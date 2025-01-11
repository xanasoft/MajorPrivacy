#include "pch.h"
#include "AccessLog.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Programs/ProgramManager.h"
#include "../Library/Helpers/NtUtil.h"
#include "../ServiceCore.h"

CAccessLog::CAccessLog()
{
}

CAccessLog::~CAccessLog()
{
}

void CAccessLog::AddExecParent(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked)
{
	std::unique_lock lock(m_Mutex);

	SExecInfo* pInfo = &m_ExecParents[ProgramUID].Infos[ProcessUID.Get()];
	pInfo->bBlocked = bBlocked;
	pInfo->LastExecTime = CreateTime;
	pInfo->CommandLine = CmdLine;
}

void CAccessLog::StoreAllExecParents(CVariant& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts) const
{
	std::unique_lock lock(m_Mutex);

	for (auto& Log : m_ExecParents)
	{
		for (auto& Entry : Log.second.Infos)
		{
			CVariant vActor;
			vActor.BeginIMap();
			if (Opts.Flags & SVarWriteOpt::eSaveToFile) {
				CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(Log.first);
				if (!pProg) continue;
				vActor.WriteVariant(API_V_PROG_ID, pProg->GetID().ToVariant(Opts));
			}
			else {
				vActor.Write(API_V_PROCESS_REF, (uint64)&Entry.second);
				vActor.Write(API_V_EVENT_ACTOR_UID, Log.first); // Program UID
			}
			if (!EnclaveGuid.IsNull()) // this is the Enclave we have been started into
				vActor.WriteVariant(API_V_EVENT_TARGET_EID, EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids));
			vActor.Write(API_V_PID, Entry.first); // Process UID
			if (!Entry.second.EnclaveGuid.IsNull()) // this is the Enclave of the process starting us
				vActor.WriteVariant(API_V_EVENT_ACTOR_EID, Entry.second.EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids));

			vActor.Write(API_V_LAST_ACTIVITY, Entry.second.LastExecTime);
			vActor.Write(API_V_WAS_BLOCKED, Entry.second.bBlocked);
			vActor.Write(API_V_CMD_LINE, Entry.second.CommandLine);

			vActor.Finish();
			List.WriteVariant(vActor);
		}
	}
}

bool CAccessLog::LoadExecParent(const CVariant& vData)
{
	CProgramID ID;
	ID.FromVariant(vData[API_V_PROG_ID]);
	CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
	if (!pItem) return false;

	// API_V_EVENT_TARGET_EID

	CFlexGuid EnclaveGuid;
	EnclaveGuid.FromVariant(vData[API_V_EVENT_ACTOR_EID]);
	uint64 ProcessUID = vData[API_V_PID].To<uint64>(0);

	std::unique_lock lock(m_Mutex);

	SExecInfo* pInfo = &m_ExecParents[pItem->GetUID()].Infos[ProcessUID];
	pInfo->bBlocked = vData[API_V_WAS_BLOCKED];
	pInfo->LastExecTime = vData[API_V_LAST_ACTIVITY];
	pInfo->CommandLine = vData[API_V_CMD_LINE].AsStr();

	return true;
}

void CAccessLog::AddExecChild(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked)
{
	std::unique_lock lock(m_Mutex);

	SExecInfo* pInfo = &m_ExecChildren[ProgramUID].Infos[ProcessUID.Get()];
	pInfo->bBlocked = bBlocked;
	pInfo->LastExecTime = CreateTime;
	pInfo->CommandLine = CmdLine;
}

void CAccessLog::StoreAllExecChildren(CVariant& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts, const std::wstring& SvcTag) const
{
	std::unique_lock lock(m_Mutex);

	for (auto& Log : m_ExecChildren)
	{
		for (auto& Entry : Log.second.Infos)
		{
			CVariant vTarget;
			vTarget.BeginIMap();
			if (Opts.Flags & SVarWriteOpt::eSaveToFile) {
				CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(Log.first);
				if (!pProg) continue;
				vTarget.WriteVariant(API_V_PROG_ID, pProg->GetID().ToVariant(Opts));
			}
			else {
				vTarget.Write(API_V_PROCESS_REF, (uint64)&Entry.second);
				vTarget.Write(API_V_EVENT_TARGET_UID, Log.first); // Program UID
				if (!SvcTag.empty()) vTarget.Write(API_V_SERVICE_TAG, SvcTag);
			}
			if (!EnclaveGuid.IsNull()) // this is the Enclave we are in while starting an otherprocess
				vTarget.WriteVariant(API_V_EVENT_ACTOR_EID, EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids));
			vTarget.Write(API_V_PID, Entry.first); // Process UID
			if (!Entry.second.EnclaveGuid.IsNull()) // this is the Enclave of the process we are starting
				vTarget.WriteVariant(API_V_EVENT_TARGET_EID, Entry.second.EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids));

			vTarget.Write(API_V_LAST_ACTIVITY, Entry.second.LastExecTime);
			vTarget.Write(API_V_WAS_BLOCKED, Entry.second.bBlocked);
			vTarget.Write(API_V_CMD_LINE, Entry.second.CommandLine);

			vTarget.Finish();
			List.WriteVariant(vTarget);
		}
	}
}

bool CAccessLog::LoadExecChild(const CVariant& vData)
{
	CProgramID ID;
	ID.FromVariant(vData[API_V_PROG_ID]);
	CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
	if (!pItem) return false;

	// API_V_EVENT_ACTOR_EID

	CFlexGuid EnclaveGuid;
	EnclaveGuid.FromVariant(vData[API_V_EVENT_TARGET_EID]);
	uint64 ProcessUID = vData[API_V_PID].To<uint64>(0);

	std::unique_lock lock(m_Mutex);

	SExecInfo* pInfo = &m_ExecChildren[pItem->GetUID()].Infos[ProcessUID];
	pInfo->bBlocked = vData[API_V_WAS_BLOCKED];
	pInfo->LastExecTime = vData[API_V_LAST_ACTIVITY];
	pInfo->CommandLine = vData[API_V_CMD_LINE].AsStr();

	return true;
}

void CAccessLog::AddIngressActor(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	std::unique_lock lock(m_Mutex);

	SAccessInfo* pInfo = &m_IngressActors[ProgramUID].Infos[ProcessUID.Get()];
	pInfo->bBlocked = bBlocked;
	pInfo->LastAccessTime = AccessTime;
	if (bThread)
		pInfo->ThreadAccessMask |= AccessMask;
	else
		pInfo->ProcessAccessMask |= AccessMask;
}

void CAccessLog::StoreAllIngressActors(CVariant& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts) const
{
	std::unique_lock lock(m_Mutex);

	for (auto& Log : m_IngressActors)
	{
		for (auto& Entry : Log.second.Infos)
		{
			CVariant vActor;
			vActor.BeginIMap();
			if (Opts.Flags & SVarWriteOpt::eSaveToFile) {
				CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(Log.first);
				if (!pProg) continue;
				vActor.WriteVariant(API_V_PROG_ID, pProg->GetID().ToVariant(Opts));
			}
			else {
				vActor.Write(API_V_PROCESS_REF, (uint64)&Entry.second);
				vActor.Write(API_V_EVENT_ACTOR_UID, Log.first); // Program UID
			}
			if (!EnclaveGuid.IsNull()) // this is the Enclave we are in while being accesses
				vActor.WriteVariant(API_V_EVENT_TARGET_EID, EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids));
			vActor.Write(API_V_PID, Entry.first); // Process UID
			if (!Entry.second.EnclaveGuid.IsNull()) // this is the Enclave of the process accessing us
				vActor.WriteVariant(API_V_EVENT_ACTOR_EID, Entry.second.EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids));

			vActor.Write(API_V_THREAD_ACCESS_MASK, Entry.second.ThreadAccessMask);
			vActor.Write(API_V_PROCESS_ACCESS_MASK, Entry.second.ProcessAccessMask);
			vActor.Write(API_V_LAST_ACTIVITY, Entry.second.LastAccessTime);
			vActor.Write(API_V_WAS_BLOCKED, Entry.second.bBlocked);

			vActor.Finish();
			List.WriteVariant(vActor);
		}
	}
}

bool CAccessLog::LoadIngressActor(const CVariant& vData)
{
	CProgramID ID;
	ID.FromVariant(vData[API_V_PROG_ID]);
	CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
	if (!pItem) return false;
	
	// API_V_EVENT_TARGET_EID

	CFlexGuid EnclaveGuid;
	EnclaveGuid.FromVariant(vData[API_V_EVENT_ACTOR_EID]);
	uint64 ProcessUID = vData[API_V_PID].To<uint64>(0);
	
	std::unique_lock lock(m_Mutex);
	
	SAccessInfo* pInfo = &m_IngressActors[pItem->GetUID()].Infos[ProcessUID];
	pInfo->bBlocked = vData[API_V_WAS_BLOCKED];
	pInfo->LastAccessTime = vData[API_V_LAST_ACTIVITY];
	pInfo->ThreadAccessMask = vData[API_V_THREAD_ACCESS_MASK].To<uint32>(0);
	pInfo->ProcessAccessMask = vData[API_V_PROCESS_ACCESS_MASK].To<uint32>(0);

	return true;
}

void CAccessLog::AddIngressTarget(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	std::unique_lock lock(m_Mutex);

	SAccessInfo* pInfo = &m_IngressTargets[ProgramUID].Infos[ProcessUID.Get()];
	pInfo->bBlocked = bBlocked;
	pInfo->LastAccessTime = AccessTime;
	if (bThread)
		pInfo->ThreadAccessMask |= AccessMask;
	else
		pInfo->ProcessAccessMask |= AccessMask;
}

void CAccessLog::StoreAllIngressTargets(CVariant& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts, const std::wstring& SvcTag) const
{
	std::unique_lock lock(m_Mutex);

	for (auto& Log : m_IngressTargets)
	{
		for (auto& Entry : Log.second.Infos)
		{
			CVariant vTarget;
			vTarget.BeginIMap();
			if (Opts.Flags & SVarWriteOpt::eSaveToFile) {
				CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(Log.first);
				if (!pProg) continue;
				vTarget.WriteVariant(API_V_PROG_ID, pProg->GetID().ToVariant(Opts));
			}
			else {
				vTarget.Write(API_V_PROCESS_REF, (uint64)&Entry.second);
				vTarget.Write(API_V_EVENT_TARGET_UID, Log.first); // Program UID
				if (!SvcTag.empty()) vTarget.Write(API_V_SERVICE_TAG, SvcTag);
			}
			if (!EnclaveGuid.IsNull()) // this is the Enclave we are in while accessign an other process
				vTarget.WriteVariant(API_V_EVENT_ACTOR_EID, EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids));
			vTarget.Write(API_V_PID, Entry.first); // Process UID
			if (!Entry.second.EnclaveGuid.IsNull()) // this is the Enclave of the process we are accessing
				vTarget.WriteVariant(API_V_EVENT_TARGET_EID, Entry.second.EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids));

			vTarget.Write(API_V_THREAD_ACCESS_MASK, Entry.second.ThreadAccessMask);
			vTarget.Write(API_V_PROCESS_ACCESS_MASK, Entry.second.ProcessAccessMask);
			vTarget.Write(API_V_LAST_ACTIVITY, Entry.second.LastAccessTime);
			vTarget.Write(API_V_WAS_BLOCKED, Entry.second.bBlocked);

			vTarget.Finish();
			List.WriteVariant(vTarget);
		}
	}
}

bool CAccessLog::LoadIngressTarget(const CVariant& vData)
{
	CProgramID ID;
	ID.FromVariant(vData[API_V_PROG_ID]);
	CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
	if (!pItem) return false;
	
	// API_V_EVENT_ACTOR_EID

	CFlexGuid EnclaveGuid;
	EnclaveGuid.FromVariant(vData[API_V_EVENT_TARGET_EID]);
	uint64 ProcessUID = vData[API_V_PID].To<uint64>(0);
	
	std::unique_lock lock(m_Mutex);
	
	SAccessInfo* pInfo = &m_IngressTargets[pItem->GetUID()].Infos[ProcessUID];
	pInfo->bBlocked = vData[API_V_WAS_BLOCKED];
	pInfo->LastAccessTime = vData[API_V_LAST_ACTIVITY];
	pInfo->ThreadAccessMask = vData[API_V_THREAD_ACCESS_MASK].To<uint32>(0);
	pInfo->ProcessAccessMask = vData[API_V_PROCESS_ACCESS_MASK].To<uint32>(0);

	return true;
}

void CAccessLog::Clear()
{
	std::unique_lock lock(m_Mutex);

	m_ExecParents.clear();
	m_ExecChildren.clear();

	m_IngressActors.clear();
	m_IngressTargets.clear();
}

void CAccessLog::Truncate()
{
	uint64 CleanupDateMinutes = theCore->Config()->GetUInt64("Service", "TraceLogRetentionMinutes", 60 * 24 * 14); // default 14 days
	uint64 CleanupDate = GetCurrentTimeAsFileTime() - (CleanupDateMinutes * 60 * 10000000ULL);

	std::unique_lock lock(m_Mutex);


	for (auto I = m_ExecParents.begin(); I != m_ExecParents.end();) 
	{
		for (auto J = I->second.Infos.begin(); J != I->second.Infos.end();)
		{
			if (J->second.LastExecTime < CleanupDate)
				I->second.Infos.erase(J++);
			else
				++J;
		}

		if (I->second.Infos.empty())
			m_ExecParents.erase(I++);
		else
			++I;
	}

	for (auto I = m_ExecChildren.begin(); I != m_ExecChildren.end();)
	{
		for (auto J = I->second.Infos.begin(); J != I->second.Infos.end();)
		{
			if (J->second.LastExecTime < CleanupDate)
				I->second.Infos.erase(J++);
			else
				++J;
		}

		if (I->second.Infos.empty())
			m_ExecChildren.erase(I++);
		else
			++I;
	}

	for (auto I = m_IngressActors.begin(); I != m_IngressActors.end();)
	{
		for (auto J = I->second.Infos.begin(); J != I->second.Infos.end();)
		{
			if (J->second.LastAccessTime < CleanupDate)
				I->second.Infos.erase(J++);
			else
				++J;
		}

		if (I->second.Infos.empty())
			m_IngressActors.erase(I++);
		else
			++I;
	}

	for (auto I = m_IngressTargets.begin(); I != m_IngressTargets.end();)
	{
		for (auto J = I->second.Infos.begin(); J != I->second.Infos.end();)
		{
			if (J->second.LastAccessTime < CleanupDate)
				I->second.Infos.erase(J++);
			else
				++J;
		}

		if (I->second.Infos.empty())
			m_IngressTargets.erase(I++);
		else
			++I;
	}
}