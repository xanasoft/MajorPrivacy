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

	if (!m_Data) {
#ifdef DEF_USE_POOL
		if(!m_pMem)
			return;
		m_Data = m_pMem->New<SAccessLog>();
#else
		m_Data = std::make_shared<SAccessLog>();
#endif
	}

#ifdef DEF_USE_POOL
	auto Ref = m_Data->ExecParents[ProgramUID];
	SExecInfo* pInfo = &(&Ref)->Infos[ProcessUID.Get()];
#else
	SExecInfo* pInfo = &m_Data->ExecParents[ProgramUID].Infos[ProcessUID.Get()];
#endif
	pInfo->bBlocked = bBlocked;
	pInfo->LastExecTime = CreateTime;
#ifdef DEF_USE_POOL
	pInfo->CommandLine.Assign(CmdLine.c_str(), CmdLine.length());
#else
	pInfo->CommandLine = CmdLine;
#endif
}

void CAccessLog::StoreAllExecParents(StVariantWriter& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts) const
{
	std::unique_lock lock(m_Mutex);

	if(!m_Data)
		return;

#ifdef DEF_USE_POOL
	for (auto I = m_Data->ExecParents.begin(); I != m_Data->ExecParents.end(); ++I)
	{
		auto& Log = *I;
		for (auto J = Log.Infos.begin(); J != Log.Infos.end(); ++J)
		{
			auto& Entry = *J;
#else
	for (auto I = m_Data->ExecParents.begin(); I != m_Data->ExecParents.end(); ++I)
	{
		auto& Log = I->second;
		for (auto J = Log.Infos.begin(); J != Log.Infos.end(); ++J)
		{
			auto& Entry = J->second;
#endif
			StVariantWriter vActor(List.Allocator());
			vActor.BeginIndex();
			if (Opts.Flags & SVarWriteOpt::eSaveToFile) {
#ifdef DEF_USE_POOL
				CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(I.Key());
#else
				CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(I->first);
#endif
				if (!pProg) continue;
				vActor.WriteVariant(API_V_ID, pProg->GetID().ToVariant(Opts, vActor.Allocator()));
			}
			else {
#ifdef DEF_USE_POOL
				vActor.Write(API_V_PROCESS_REF, (uint64)&Entry);
				vActor.Write(API_V_EVENT_ACTOR_UID, I.Key()); // Program UID
#else
				vActor.Write(API_V_PROCESS_REF, (uint64)&J->second);
				vActor.Write(API_V_EVENT_ACTOR_UID, I->first); // Program UID
#endif
			}
			if (!EnclaveGuid.IsNull()) // this is the Enclave we have been started into
				vActor.WriteVariant(API_V_EVENT_TARGET_EID, EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, vActor.Allocator()));
#ifdef DEF_USE_POOL
			vActor.Write(API_V_PID, J.Key()); // Process UID
#else
			vActor.Write(API_V_PID, J->first); // Process UID
#endif
			if (!Entry.EnclaveGuid.IsNull()) // this is the Enclave of the process starting us
				vActor.WriteVariant(API_V_EVENT_ACTOR_EID, Entry.EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, vActor.Allocator()));

			vActor.Write(API_V_LAST_ACTIVITY, Entry.LastExecTime);
			vActor.Write(API_V_WAS_BLOCKED, Entry.bBlocked);
			vActor.WriteEx(API_V_CMD_LINE, Entry.CommandLine);

			List.WriteVariant(vActor.Finish());
		}
	}
}

bool CAccessLog::LoadExecParent(const StVariant& vData)
{
	CProgramID ID;
	ID.FromVariant(vData[API_V_ID]);
	CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
	if (!pItem) return false;

	// API_V_EVENT_TARGET_EID

	CFlexGuid EnclaveGuid;
	EnclaveGuid.FromVariant(vData[API_V_EVENT_ACTOR_EID]);
	uint64 ProcessUID = vData[API_V_PID].To<uint64>(0);

	std::unique_lock lock(m_Mutex);

	if (!m_Data) {
#ifdef DEF_USE_POOL
		if (!m_pMem)
			return true;
		m_Data = m_pMem->New<SAccessLog>();
#else
		m_Data = std::make_shared<SAccessLog>();
#endif
	}

#ifdef DEF_USE_POOL
	auto Ref = m_Data->ExecParents[pItem->GetUID()];
	SExecInfo* pInfo = &(&Ref)->Infos[ProcessUID];
#else
	SExecInfo* pInfo = &m_Data->ExecParents[pItem->GetUID()].Infos[ProcessUID];
#endif
	pInfo->bBlocked = vData[API_V_WAS_BLOCKED];
	pInfo->LastExecTime = vData[API_V_LAST_ACTIVITY];
#ifdef DEF_USE_POOL
	pInfo->CommandLine = vData[API_V_CMD_LINE].ToStringW();
#else
	pInfo->CommandLine = vData[API_V_CMD_LINE].AsStr();
#endif

	return true;
}

void CAccessLog::AddExecChild(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked)
{
	std::unique_lock lock(m_Mutex);

	if (!m_Data) {
#ifdef DEF_USE_POOL
		if(!m_pMem)
			return;
		m_Data = m_pMem->New<SAccessLog>();
#else
		m_Data = std::make_shared<SAccessLog>();
#endif
	}

#ifdef DEF_USE_POOL
	auto Ref = m_Data->ExecChildren[ProgramUID];
	SExecInfo* pInfo = &(&Ref)->Infos[ProcessUID.Get()];
#else
	SExecInfo* pInfo = &m_Data->ExecChildren[ProgramUID].Infos[ProcessUID.Get()];
#endif
	pInfo->bBlocked = bBlocked;
	pInfo->LastExecTime = CreateTime;
#ifdef DEF_USE_POOL
	pInfo->CommandLine.Assign(CmdLine.c_str(), CmdLine.length());
#else
	pInfo->CommandLine = CmdLine;
#endif
}

void CAccessLog::StoreAllExecChildren(StVariantWriter& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts, const std::wstring& SvcTag) const
{
	std::unique_lock lock(m_Mutex);

	if(!m_Data)
		return;

#ifdef DEF_USE_POOL
	for (auto I = m_Data->ExecChildren.begin(); I != m_Data->ExecChildren.end(); ++I)
	{
		auto& Log = *I;
		for (auto J = Log.Infos.begin(); J != Log.Infos.end(); ++J)
		{
			auto& Entry = *J;
#else
	for (auto I = m_Data->ExecChildren.begin(); I != m_Data->ExecChildren.end(); ++I)
	{
		auto& Log = I->second;
		for (auto J = Log.Infos.begin(); J != Log.Infos.end(); ++J)
		{
			auto& Entry = J->second;
#endif
			StVariantWriter vTarget(List.Allocator());
			vTarget.BeginIndex();
			if (Opts.Flags & SVarWriteOpt::eSaveToFile) {
#ifdef DEF_USE_POOL
				CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(I.Key());
#else
				CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(I->first);
#endif
				if (!pProg) continue;
				vTarget.WriteVariant(API_V_ID, pProg->GetID().ToVariant(Opts, vTarget.Allocator()));
			}
			else {
				vTarget.Write(API_V_PROCESS_REF, (uint64)&Entry);
#ifdef DEF_USE_POOL
				vTarget.Write(API_V_EVENT_TARGET_UID, I.Key()); // Program UID
#else
				vTarget.Write(API_V_EVENT_TARGET_UID, I->first); // Program UID
#endif
				if (!SvcTag.empty()) vTarget.WriteEx(API_V_SERVICE_TAG, SvcTag);
			}
			if (!EnclaveGuid.IsNull()) // this is the Enclave we are in while starting an otherprocess
				vTarget.WriteVariant(API_V_EVENT_ACTOR_EID, EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, vTarget.Allocator()));
#ifdef DEF_USE_POOL
			vTarget.Write(API_V_PID, J.Key()); // Process UID
#else
			vTarget.Write(API_V_PID, J->first); // Process UID
#endif
			if (!Entry.EnclaveGuid.IsNull()) // this is the Enclave of the process we are starting
				vTarget.WriteVariant(API_V_EVENT_TARGET_EID, Entry.EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, vTarget.Allocator()));

			vTarget.Write(API_V_LAST_ACTIVITY, Entry.LastExecTime);
			vTarget.Write(API_V_WAS_BLOCKED, Entry.bBlocked);
			vTarget.WriteEx(API_V_CMD_LINE, Entry.CommandLine);

			List.WriteVariant(vTarget.Finish());
		}
	}
}

bool CAccessLog::LoadExecChild(const StVariant& vData)
{
	CProgramID ID;
	ID.FromVariant(vData[API_V_ID]);
	CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
	if (!pItem) return false;

	// API_V_EVENT_ACTOR_EID

	CFlexGuid EnclaveGuid;
	EnclaveGuid.FromVariant(vData[API_V_EVENT_TARGET_EID]);
	uint64 ProcessUID = vData[API_V_PID].To<uint64>(0);

	std::unique_lock lock(m_Mutex);

	if (!m_Data) {
#ifdef DEF_USE_POOL
		if (!m_pMem)
			return  true;
		m_Data = m_pMem->New<SAccessLog>();
#else
		m_Data = std::make_shared<SAccessLog>();
#endif
	}

#ifdef DEF_USE_POOL
	auto Ref = m_Data->ExecChildren[pItem->GetUID()];
	SExecInfo* pInfo = &(&Ref)->Infos[ProcessUID];
#else
	SExecInfo* pInfo = &m_Data->ExecChildren[pItem->GetUID()].Infos[ProcessUID];
#endif
	pInfo->bBlocked = vData[API_V_WAS_BLOCKED];
	pInfo->LastExecTime = vData[API_V_LAST_ACTIVITY];
#ifdef DEF_USE_POOL
	pInfo->CommandLine = vData[API_V_CMD_LINE].ToStringW();
#else
	pInfo->CommandLine = vData[API_V_CMD_LINE].AsStr();
#endif

	return true;
}

void CAccessLog::AddIngressActor(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	std::unique_lock lock(m_Mutex);

	if (!m_Data) {
#ifdef DEF_USE_POOL
		if(!m_pMem)
			return;
		m_Data = m_pMem->New<SAccessLog>();
#else
		m_Data = std::make_shared<SAccessLog>();
#endif
	}

#ifdef DEF_USE_POOL
	auto Ref = m_Data->IngressActors[ProgramUID];
	SAccessInfo* pInfo = &(&Ref)->Infos[ProcessUID.Get()];
#else
	SAccessInfo* pInfo = &m_Data->IngressActors[ProgramUID].Infos[ProcessUID.Get()];
#endif
	pInfo->bBlocked = bBlocked;
	pInfo->LastAccessTime = AccessTime;
	if (bThread)
		pInfo->ThreadAccessMask |= AccessMask;
	else
		pInfo->ProcessAccessMask |= AccessMask;
}

void CAccessLog::StoreAllIngressActors(StVariantWriter& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts) const
{
	std::unique_lock lock(m_Mutex);

	if(!m_Data)
		return;

#ifdef DEF_USE_POOL
	for (auto I = m_Data->IngressActors.begin(); I != m_Data->IngressActors.end(); ++I)
	{
		auto& Log = *I;
		for (auto J = Log.Infos.begin(); J != Log.Infos.end(); ++J)
		{
			auto& Entry = *J;
#else
	for (auto I = m_Data->IngressActors.begin(); I != m_Data->IngressActors.end(); ++I)
	{
		auto& Log = I->second;
		for (auto J = Log.Infos.begin(); J != Log.Infos.end(); ++J)
		{
			auto& Entry = J->second;
#endif
			StVariantWriter vActor(List.Allocator());
			vActor.BeginIndex();
			if (Opts.Flags & SVarWriteOpt::eSaveToFile) {
#ifdef DEF_USE_POOL
				CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(I.Key());
#else
				CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(I->first);
#endif
				if (!pProg) continue;
				vActor.WriteVariant(API_V_ID, pProg->GetID().ToVariant(Opts, vActor.Allocator()));
			}
			else {
				vActor.Write(API_V_PROCESS_REF, (uint64)&Entry);
#ifdef DEF_USE_POOL
				vActor.Write(API_V_EVENT_ACTOR_UID, I.Key()); // Program UID
#else
				vActor.Write(API_V_EVENT_ACTOR_UID, I->first); // Program UID
#endif
			}
			if (!EnclaveGuid.IsNull()) // this is the Enclave we are in while being accesses
				vActor.WriteVariant(API_V_EVENT_TARGET_EID, EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, vActor.Allocator()));
#ifdef DEF_USE_POOL
			vActor.Write(API_V_PID, J.Key()); // Process UID
#else
			vActor.Write(API_V_PID, J->first); // Process UID
#endif
			if (!Entry.EnclaveGuid.IsNull()) // this is the Enclave of the process accessing us
				vActor.WriteVariant(API_V_EVENT_ACTOR_EID, Entry.EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, vActor.Allocator()));

			vActor.Write(API_V_THREAD_ACCESS_MASK, Entry.ThreadAccessMask);
			vActor.Write(API_V_PROCESS_ACCESS_MASK, Entry.ProcessAccessMask);
			vActor.Write(API_V_LAST_ACTIVITY, Entry.LastAccessTime);
			vActor.Write(API_V_WAS_BLOCKED, Entry.bBlocked);

			List.WriteVariant(vActor.Finish());
		}
	}
}

bool CAccessLog::LoadIngressActor(const StVariant& vData)
{
	CProgramID ID;
	ID.FromVariant(vData[API_V_ID]);
	CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
	if (!pItem) return false;
	
	// API_V_EVENT_TARGET_EID

	CFlexGuid EnclaveGuid;
	EnclaveGuid.FromVariant(vData[API_V_EVENT_ACTOR_EID]);
	uint64 ProcessUID = vData[API_V_PID].To<uint64>(0);
	
	std::unique_lock lock(m_Mutex);
	
	if (!m_Data) {
#ifdef DEF_USE_POOL
		if (!m_pMem)
			return  true;
		m_Data = m_pMem->New<SAccessLog>();
#else
		m_Data = std::make_shared<SAccessLog>();
#endif
	}

#ifdef DEF_USE_POOL
	auto Ref = m_Data->IngressActors[pItem->GetUID()];
	SAccessInfo* pInfo = &(&Ref)->Infos[ProcessUID];
#else
	SAccessInfo* pInfo = &m_Data->IngressActors[pItem->GetUID()].Infos[ProcessUID];
#endif
	pInfo->bBlocked = vData[API_V_WAS_BLOCKED];
	pInfo->LastAccessTime = vData[API_V_LAST_ACTIVITY];
	pInfo->ThreadAccessMask = vData[API_V_THREAD_ACCESS_MASK].To<uint32>(0);
	pInfo->ProcessAccessMask = vData[API_V_PROCESS_ACCESS_MASK].To<uint32>(0);

	return true;
}

void CAccessLog::AddIngressTarget(uint64 ProgramUID, const CFlexGuid& EnclaveGuid, const SProcessUID& ProcessUID, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	std::unique_lock lock(m_Mutex);

	if (!m_Data) {
#ifdef DEF_USE_POOL
		if(!m_pMem)
			return;
		m_Data = m_pMem->New<SAccessLog>();
#else
		m_Data = std::make_shared<SAccessLog>();
#endif
	}

#ifdef DEF_USE_POOL
	auto Ref = m_Data->IngressTargets[ProgramUID];
	SAccessInfo* pInfo = &(&Ref)->Infos[ProcessUID.Get()];
#else
	SAccessInfo* pInfo = &m_Data->IngressTargets[ProgramUID].Infos[ProcessUID.Get()];
#endif
	pInfo->bBlocked = bBlocked;
	pInfo->LastAccessTime = AccessTime;
	if (bThread)
		pInfo->ThreadAccessMask |= AccessMask;
	else
		pInfo->ProcessAccessMask |= AccessMask;
}

void CAccessLog::StoreAllIngressTargets(StVariantWriter& List, const CFlexGuid& EnclaveGuid, const SVarWriteOpt& Opts, const std::wstring& SvcTag) const
{
	std::unique_lock lock(m_Mutex);

	if(!m_Data)
		return;

#ifdef DEF_USE_POOL
	for (auto I = m_Data->IngressTargets.begin(); I != m_Data->IngressTargets.end(); ++I)
	{
		auto& Log = *I;
		for (auto J = Log.Infos.begin(); J != Log.Infos.end(); ++J)
		{
			auto& Entry = *J;
#else
	for (auto I = m_Data->IngressTargets.begin(); I != m_Data->IngressTargets.end(); ++I)
	{
		auto& Log = I->second;
		for (auto J = Log.Infos.begin(); J != Log.Infos.end(); ++J)
		{
			auto& Entry = J->second;
#endif
			StVariantWriter vTarget(List.Allocator());
			vTarget.BeginIndex();
			if (Opts.Flags & SVarWriteOpt::eSaveToFile) {
#ifdef DEF_USE_POOL
				CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(I.Key());
#else
				CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(I->first);
#endif
				if (!pProg) continue;
				vTarget.WriteVariant(API_V_ID, pProg->GetID().ToVariant(Opts, vTarget.Allocator()));
			}
			else {
				vTarget.Write(API_V_PROCESS_REF, (uint64)&Entry);
#ifdef DEF_USE_POOL
				vTarget.Write(API_V_EVENT_TARGET_UID, I.Key()); // Program UID
#else
				vTarget.Write(API_V_EVENT_TARGET_UID, I->first); // Program UID
#endif
				if (!SvcTag.empty()) vTarget.WriteEx(API_V_SERVICE_TAG, SvcTag);
			}
			if (!EnclaveGuid.IsNull()) // this is the Enclave we are in while accessign an other process
				vTarget.WriteVariant(API_V_EVENT_ACTOR_EID, EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, vTarget.Allocator()));
#ifdef DEF_USE_POOL
			vTarget.Write(API_V_PID, J.Key()); // Process UID
#else
			vTarget.Write(API_V_PID, J->first); // Process UID
#endif
			if (!Entry.EnclaveGuid.IsNull()) // this is the Enclave of the process we are accessing
				vTarget.WriteVariant(API_V_EVENT_TARGET_EID, Entry.EnclaveGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, vTarget.Allocator()));

			vTarget.Write(API_V_THREAD_ACCESS_MASK, Entry.ThreadAccessMask);
			vTarget.Write(API_V_PROCESS_ACCESS_MASK, Entry.ProcessAccessMask);
			vTarget.Write(API_V_LAST_ACTIVITY, Entry.LastAccessTime);
			vTarget.Write(API_V_WAS_BLOCKED, Entry.bBlocked);

			List.WriteVariant(vTarget.Finish());
		}
	}
}

bool CAccessLog::LoadIngressTarget(const StVariant& vData)
{
	CProgramID ID;
	ID.FromVariant(vData[API_V_ID]);
	CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
	if (!pItem) return false;
	
	// API_V_EVENT_ACTOR_EID

	CFlexGuid EnclaveGuid;
	EnclaveGuid.FromVariant(vData[API_V_EVENT_TARGET_EID]);
	uint64 ProcessUID = vData[API_V_PID].To<uint64>(0);
	
	std::unique_lock lock(m_Mutex);
	
	if (!m_Data) {
#ifdef DEF_USE_POOL
		if (!m_pMem)
			return true;
		m_Data = m_pMem->New<SAccessLog>();
#else
		m_Data = std::make_shared<SAccessLog>();
#endif
	}

#ifdef DEF_USE_POOL
	auto Ref = m_Data->IngressTargets[pItem->GetUID()];
	SAccessInfo* pInfo = &(&Ref)->Infos[ProcessUID];
#else
	SAccessInfo* pInfo = &m_Data->IngressTargets[pItem->GetUID()].Infos[ProcessUID];
#endif
	pInfo->bBlocked = vData[API_V_WAS_BLOCKED];
	pInfo->LastAccessTime = vData[API_V_LAST_ACTIVITY];
	pInfo->ThreadAccessMask = vData[API_V_THREAD_ACCESS_MASK].To<uint32>(0);
	pInfo->ProcessAccessMask = vData[API_V_PROCESS_ACCESS_MASK].To<uint32>(0);

	return true;
}

void CAccessLog::Clear()
{
	std::unique_lock lock(m_Mutex);

	m_Data.reset();
}

void CAccessLog::Truncate()
{
	uint64 CleanupDateMinutes = theCore->Config()->GetUInt64("Service", "TraceLogRetentionMinutes", 60 * 24 * 14); // default 14 days
	uint64 CleanupDate = GetCurrentTimeAsFileTime() - (CleanupDateMinutes * 60 * 10000000ULL);

	std::unique_lock lock(m_Mutex);

	if(!m_Data)
		return;

	for (auto I = m_Data->ExecParents.begin(); I != m_Data->ExecParents.end();) 
	{
#ifdef DEF_USE_POOL
		auto& Log = *I;
#else
		auto& Log = I->second;
#endif
		for (auto J = Log.Infos.begin(); J != Log.Infos.end();)
		{
#ifdef DEF_USE_POOL
			if (J->LastExecTime < CleanupDate)
#else
			if (J->second.LastExecTime < CleanupDate)
#endif
				J = Log.Infos.erase(J);
			else
				++J;
		}

#ifdef DEF_USE_POOL
		if (Log.Infos.IsEmpty())
#else
		if (Log.Infos.empty())
#endif
			I = m_Data->ExecParents.erase(I);
		else
			++I;
	}

	for (auto I = m_Data->ExecChildren.begin(); I != m_Data->ExecChildren.end();)
	{
#ifdef DEF_USE_POOL
		auto& Log = *I;
#else
		auto& Log = I->second;
#endif
		for (auto J = Log.Infos.begin(); J != Log.Infos.end();)
		{
#ifdef DEF_USE_POOL
			if (J->LastExecTime < CleanupDate)
#else
			if (J->second.LastExecTime < CleanupDate)
#endif
				J = Log.Infos.erase(J);
			else
				++J;
		}

#ifdef DEF_USE_POOL
		if (Log.Infos.IsEmpty())
#else
		if (Log.Infos.empty())
#endif
			I = m_Data->ExecChildren.erase(I);
		else
			++I;
	}

	for (auto I = m_Data->IngressActors.begin(); I != m_Data->IngressActors.end();)
	{
#ifdef DEF_USE_POOL
		auto& Log = *I;
#else
		auto& Log = I->second;
#endif
		for (auto J = Log.Infos.begin(); J != Log.Infos.end();)
		{
#ifdef DEF_USE_POOL
			if (J->LastAccessTime < CleanupDate)
#else
			if (J->second.LastAccessTime < CleanupDate)
#endif
				J = Log.Infos.erase(J);
			else
				++J;
		}

		if (Log.Infos.empty())
			I = m_Data->IngressActors.erase(I);
		else
			++I;
	}

	for (auto I = m_Data->IngressTargets.begin(); I != m_Data->IngressTargets.end();)
	{
#ifdef DEF_USE_POOL
		auto& Log = *I;
#else
		auto& Log = I->second;
#endif
		for (auto J = Log.Infos.begin(); J != Log.Infos.end();)
		{
#ifdef DEF_USE_POOL
			if (J->LastAccessTime < CleanupDate)
#else
			if (J->second.LastAccessTime < CleanupDate)
#endif
				J = Log.Infos.erase(J);
			else
				++J;
		}

		if (Log.Infos.empty())
			I = m_Data->IngressTargets.erase(I);
		else
			++I;
	}
}