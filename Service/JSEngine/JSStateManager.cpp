#include "pch.h"
#include "JSStateManager.h"

CJSStateManager::CJSStateManager()
{
}

CJSStateManager::~CJSStateManager()
{
}

//////////////////////////////////////////////////////////////////////////
// Private State

StVariant CJSStateManager::GetPrivate(const CFlexGuid& ScriptGuid, const std::string& Key)
{
    std::shared_lock Lock(m_Mutex);

    auto itScript = m_PrivateState.find(ScriptGuid);
    if (itScript == m_PrivateState.end())
        return StVariant();

    auto itKey = itScript->second.find(Key);
    if (itKey == itScript->second.end())
        return StVariant();

    return itKey->second;
}

STATUS CJSStateManager::SetPrivate(const CFlexGuid& ScriptGuid, const std::string& Key, const StVariant& Value)
{
    // Validate key length
    if (Key.length() > MAX_KEY_LENGTH)
        return ERR(STATUS_NAME_TOO_LONG);

    // Validate value size (approximate check)
    if (Value.GetSize() > MAX_VALUE_SIZE)
        return ERR(STATUS_BUFFER_OVERFLOW);

    std::unique_lock Lock(m_Mutex);

    auto& scriptState = m_PrivateState[ScriptGuid];

    // Check key count limit (only for new keys)
    if (scriptState.find(Key) == scriptState.end() && scriptState.size() >= MAX_KEYS_PER_SCRIPT)
        return ERR(STATUS_QUOTA_EXCEEDED);

    scriptState[Key] = Value;
    return OK;
}

STATUS CJSStateManager::DeletePrivate(const CFlexGuid& ScriptGuid, const std::string& Key)
{
    std::unique_lock Lock(m_Mutex);

    auto itScript = m_PrivateState.find(ScriptGuid);
    if (itScript == m_PrivateState.end())
        return ERR(STATUS_NOT_FOUND);

    auto itKey = itScript->second.find(Key);
    if (itKey == itScript->second.end())
        return ERR(STATUS_NOT_FOUND);

    itScript->second.erase(itKey);

    // Remove script entry if empty
    if (itScript->second.empty())
        m_PrivateState.erase(itScript);

    return OK;
}

StVariant CJSStateManager::GetPrivateKeys(const CFlexGuid& ScriptGuid)
{
    std::shared_lock Lock(m_Mutex);

    StVariant keys(nullptr, VAR_TYPE_LIST);

    auto itScript = m_PrivateState.find(ScriptGuid);
    if (itScript != m_PrivateState.end())
    {
        for (const auto& pair : itScript->second)
        {
            keys.Append(StVariant(pair.first.c_str()));
        }
    }

    return keys;
}

bool CJSStateManager::HasPrivate(const CFlexGuid& ScriptGuid, const std::string& Key)
{
    std::shared_lock Lock(m_Mutex);

    auto itScript = m_PrivateState.find(ScriptGuid);
    if (itScript == m_PrivateState.end())
        return false;

    return itScript->second.find(Key) != itScript->second.end();
}

//////////////////////////////////////////////////////////////////////////
// Global/Public State

STATUS CJSStateManager::CreateGlobalState(const CFlexGuid& OwnerGuid, const std::string& StateName, bool bWritable)
{
    // Validate name length
    if (StateName.empty() || StateName.length() > MAX_NAME_LENGTH)
        return ERR(STATUS_NAME_TOO_LONG);

    std::unique_lock Lock(m_Mutex);

    // Check if state already exists
    if (m_GlobalStates.find(StateName) != m_GlobalStates.end())
        return ERR(STATUS_OBJECT_NAME_COLLISION);

    // Create new state
    SGlobalState& state = m_GlobalStates[StateName];
    state.OwnerGuid = OwnerGuid;
    state.bWritable = bWritable;

    return OK;
}

STATUS CJSStateManager::DestroyGlobalState(const CFlexGuid& CallerGuid, const std::string& StateName)
{
    std::unique_lock Lock(m_Mutex);

    auto it = m_GlobalStates.find(StateName);
    if (it == m_GlobalStates.end())
        return ERR(STATUS_NOT_FOUND);

    // Only owner can destroy
    if (it->second.OwnerGuid != CallerGuid)
        return ERR(STATUS_ACCESS_DENIED);

    m_GlobalStates.erase(it);
    return OK;
}

bool CJSStateManager::GlobalStateExists(const std::string& StateName)
{
    std::shared_lock Lock(m_Mutex);
    return m_GlobalStates.find(StateName) != m_GlobalStates.end();
}

bool CJSStateManager::IsGlobalStateOwner(const CFlexGuid& CallerGuid, const std::string& StateName)
{
    std::shared_lock Lock(m_Mutex);

    auto it = m_GlobalStates.find(StateName);
    if (it == m_GlobalStates.end())
        return false;

    return it->second.OwnerGuid == CallerGuid;
}

bool CJSStateManager::IsGlobalStateWritable(const std::string& StateName)
{
    std::shared_lock Lock(m_Mutex);

    auto it = m_GlobalStates.find(StateName);
    if (it == m_GlobalStates.end())
        return false;

    return it->second.bWritable;
}

StVariant CJSStateManager::ListGlobalStates()
{
    std::shared_lock Lock(m_Mutex);

    StVariant names(nullptr, VAR_TYPE_LIST);
    for (const auto& pair : m_GlobalStates)
    {
        names.Append(StVariant(pair.first.c_str()));
    }

    return names;
}

StVariant CJSStateManager::GetGlobal(const std::string& StateName, const std::string& Key)
{
    std::shared_lock Lock(m_Mutex);

    auto itState = m_GlobalStates.find(StateName);
    if (itState == m_GlobalStates.end())
        return StVariant();

    auto itKey = itState->second.Entries.find(Key);
    if (itKey == itState->second.Entries.end())
        return StVariant();

    return itKey->second;
}

STATUS CJSStateManager::SetGlobal(const CFlexGuid& CallerGuid, const std::string& StateName, const std::string& Key, const StVariant& Value)
{
    // Validate key length
    if (Key.length() > MAX_KEY_LENGTH)
        return ERR(STATUS_NAME_TOO_LONG);

    // Validate value size
    if (Value.GetSize() > MAX_VALUE_SIZE)
        return ERR(STATUS_BUFFER_OVERFLOW);

    std::unique_lock Lock(m_Mutex);

    auto itState = m_GlobalStates.find(StateName);
    if (itState == m_GlobalStates.end())
        return ERR(STATUS_NOT_FOUND);

    // Check write permission: owner can always write, others only if writable
    bool isOwner = (itState->second.OwnerGuid == CallerGuid);
    if (!isOwner && !itState->second.bWritable)
        return ERR(STATUS_ACCESS_DENIED);

    // Check key count limit (only for new keys)
    auto& entries = itState->second.Entries;
    if (entries.find(Key) == entries.end() && entries.size() >= MAX_KEYS_PER_STATE)
        return ERR(STATUS_QUOTA_EXCEEDED);

    entries[Key] = Value;
    return OK;
}

STATUS CJSStateManager::DeleteGlobal(const CFlexGuid& CallerGuid, const std::string& StateName, const std::string& Key)
{
    std::unique_lock Lock(m_Mutex);

    auto itState = m_GlobalStates.find(StateName);
    if (itState == m_GlobalStates.end())
        return ERR(STATUS_NOT_FOUND);

    // Check write permission: owner can always delete, others only if writable
    bool isOwner = (itState->second.OwnerGuid == CallerGuid);
    if (!isOwner && !itState->second.bWritable)
        return ERR(STATUS_ACCESS_DENIED);

    auto itKey = itState->second.Entries.find(Key);
    if (itKey == itState->second.Entries.end())
        return ERR(STATUS_NOT_FOUND);

    itState->second.Entries.erase(itKey);
    return OK;
}

StVariant CJSStateManager::GetGlobalKeys(const std::string& StateName)
{
    std::shared_lock Lock(m_Mutex);

    StVariant keys(nullptr, VAR_TYPE_LIST);

    auto itState = m_GlobalStates.find(StateName);
    if (itState != m_GlobalStates.end())
    {
        for (const auto& pair : itState->second.Entries)
        {
            keys.Append(StVariant(pair.first.c_str()));
        }
    }

    return keys;
}

bool CJSStateManager::HasGlobal(const std::string& StateName, const std::string& Key)
{
    std::shared_lock Lock(m_Mutex);

    auto itState = m_GlobalStates.find(StateName);
    if (itState == m_GlobalStates.end())
        return false;

    return itState->second.Entries.find(Key) != itState->second.Entries.end();
}

//////////////////////////////////////////////////////////////////////////
// Cleanup

void CJSStateManager::CleanupScriptState(const CFlexGuid& ScriptGuid)
{
    std::unique_lock Lock(m_Mutex);

    // Remove private state
    m_PrivateState.erase(ScriptGuid);

    // Remove all global states owned by this script
    for (auto it = m_GlobalStates.begin(); it != m_GlobalStates.end(); )
    {
        if (it->second.OwnerGuid == ScriptGuid)
            it = m_GlobalStates.erase(it);
        else
            ++it;
    }
}
