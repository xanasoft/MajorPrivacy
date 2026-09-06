#pragma once
#include "../Library/Status.h"
#include "../../Library/Common/StVariant.h"
#include "../../Library/Common/FlexGuid.h"
#include <shared_mutex>
#include <map>
#include <string>

class CJSStateManager
{
public:
    CJSStateManager();
    ~CJSStateManager();

    // Size limits
    static const size_t MAX_NAME_LENGTH = 256;
    static const size_t MAX_KEY_LENGTH = 256;
    static const size_t MAX_VALUE_SIZE = 64 * 1024;  // 64KB per value
    static const size_t MAX_KEYS_PER_STATE = 1000;
    static const size_t MAX_KEYS_PER_SCRIPT = 1000;

    // Private state - only accessible by owning script (keyed by script GUID)
    StVariant GetPrivate(const CFlexGuid& ScriptGuid, const std::string& Key);
    STATUS SetPrivate(const CFlexGuid& ScriptGuid, const std::string& Key, const StVariant& Value);
    STATUS DeletePrivate(const CFlexGuid& ScriptGuid, const std::string& Key);
    StVariant GetPrivateKeys(const CFlexGuid& ScriptGuid);
    bool HasPrivate(const CFlexGuid& ScriptGuid, const std::string& Key);

    // Global/Public state - named states accessible by any script knowing the name
    // bWritable: if true, any script can modify values (not just owner)
    STATUS CreateGlobalState(const CFlexGuid& OwnerGuid, const std::string& StateName, bool bWritable = false);
    STATUS DestroyGlobalState(const CFlexGuid& CallerGuid, const std::string& StateName);
    bool GlobalStateExists(const std::string& StateName);
    bool IsGlobalStateOwner(const CFlexGuid& CallerGuid, const std::string& StateName);
    bool IsGlobalStateWritable(const std::string& StateName);
    StVariant ListGlobalStates();  // Returns list of all state names

    // Get/Set values in a global state
    StVariant GetGlobal(const std::string& StateName, const std::string& Key);
    STATUS SetGlobal(const CFlexGuid& CallerGuid, const std::string& StateName, const std::string& Key, const StVariant& Value);
    STATUS DeleteGlobal(const CFlexGuid& CallerGuid, const std::string& StateName, const std::string& Key);
    StVariant GetGlobalKeys(const std::string& StateName);
    bool HasGlobal(const std::string& StateName, const std::string& Key);

    // Cleanup when script is permanently removed
    void CleanupScriptState(const CFlexGuid& ScriptGuid);

protected:
    mutable std::shared_mutex m_Mutex;

    // Private state: m_PrivateState[ScriptGuid][Key] = StVariant
    std::map<CFlexGuid, std::map<std::string, StVariant>> m_PrivateState;

    // Global/Public state structure
    struct SGlobalState
    {
        CFlexGuid OwnerGuid;                        // Script that created this state
        bool bWritable = false;                     // If true, any script can write
        std::map<std::string, StVariant> Entries;   // Key-value pairs
    };

    // m_GlobalStates[StateName] = SGlobalState
    std::map<std::string, SGlobalState> m_GlobalStates;
};
