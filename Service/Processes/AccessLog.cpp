#include "pch.h"
#include "AccessLog.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Programs/ProgramManager.h"
#include "../ServiceCore.h"

CAccessLog::CAccessLog()
{
}

CAccessLog::~CAccessLog()
{
}

void CAccessLog::AddExecActor(uint64 UID, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked)
{
	SExecInfo& Info = m_ExecActors[UID];
	Info.bBlocked = bBlocked;
	Info.LastExecTime = CreateTime;
	Info.CommandLine = CmdLine;
}

CVariant CAccessLog::StoreExecActors(const SVarWriteOpt& Opts) const
{
	CVariant Actors;
	Actors.BeginList();
	DumpExecActors(Actors, Opts);
	Actors.Finish();
	return Actors;

}

void CAccessLog::LoadExecActors(const CVariant& Data)
{
	Data.ReadRawList([&](const CVariant& vData) {
		CProgramID ID;
		ID.FromVariant(vData[API_V_PROG_ID]);
		CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
		if (!pItem) return;
		AddExecActor(pItem->GetUID(), Data[API_V_PROC_EVENT_CMD_LINE].AsStr(), Data[API_V_PROC_EVENT_LAST_EXEC], Data[API_V_PROC_EVENT_BLOCKED]);
	});
}

void CAccessLog::DumpExecActors(CVariant& Actors, const SVarWriteOpt& Opts) const
{
	for (auto& pItem : m_ExecActors)
	{
		CVariant vActor;
		vActor.BeginIMap();
		if (Opts.Flags & SVarWriteOpt::eSaveToFile) {
			CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(pItem.first);
			if(!pProg) continue;
			vActor.WriteVariant(API_V_PROG_ID, pProg->GetID().ToVariant(Opts));
		}
		else {
			vActor.Write(API_V_PROC_REF, (uint64)&pItem.second);
			vActor.Write(API_V_PROC_EVENT_ACTOR, pItem.first);
		}
		vActor.Write(API_V_PROC_EVENT_LAST_EXEC, pItem.second.LastExecTime);
		vActor.Write(API_V_PROC_EVENT_BLOCKED, pItem.second.bBlocked);
		vActor.Write(API_V_PROC_EVENT_CMD_LINE, pItem.second.CommandLine);
		vActor.Finish();
		Actors.WriteVariant(vActor);
	}
}

void CAccessLog::AddExecTarget(uint64 UID, const std::wstring& CmdLine, uint64 CreateTime, bool bBlocked)
{
	SExecInfo& Info = m_ExecTargets[UID];
	Info.bBlocked = bBlocked;
	Info.LastExecTime = CreateTime;
	Info.CommandLine = CmdLine;
}

CVariant CAccessLog::StoreExecTargets(const SVarWriteOpt& Opts) const
{
	CVariant Targets;
	Targets.BeginList();
	DumpExecTarget(Targets, Opts, L"");
	Targets.Finish();
	return Targets;
}

void CAccessLog::LoadExecTargets(const CVariant& Data)
{
	Data.ReadRawList([&](const CVariant& vData) {
		CProgramID ID;
		ID.FromVariant(vData[API_V_PROG_ID]);
		CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
		if (!pItem) return;
		AddExecTarget(pItem->GetUID(), Data[API_V_PROC_EVENT_CMD_LINE].AsStr(), Data[API_V_PROC_EVENT_LAST_EXEC], Data[API_V_PROC_EVENT_BLOCKED]);
	});
}

void CAccessLog::DumpExecTarget(CVariant& Targets, const SVarWriteOpt& Opts, const std::wstring& SvcTag) const
{
	for (auto& pItem : m_ExecTargets)
	{
		CVariant vTarget;
		vTarget.BeginIMap();
		if (Opts.Flags & SVarWriteOpt::eSaveToFile) {
			CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(pItem.first);
			if(!pProg) continue;
			vTarget.WriteVariant(API_V_PROG_ID, pProg->GetID().ToVariant(Opts));
		}
		else {
			vTarget.Write(API_V_PROC_REF, (uint64)&pItem.second);
			vTarget.Write(API_V_PROC_EVENT_TARGET, pItem.first);
			if(!SvcTag.empty()) vTarget.Write(API_V_PROG_SVC_TAG, SvcTag);
		}
		vTarget.Write(API_V_PROC_EVENT_LAST_EXEC, pItem.second.LastExecTime);
		vTarget.Write(API_V_PROC_EVENT_BLOCKED, pItem.second.bBlocked);
		vTarget.Write(API_V_PROC_EVENT_CMD_LINE, pItem.second.CommandLine);
		vTarget.Finish();
		Targets.WriteVariant(vTarget);
	}
}

void CAccessLog::AddIngressActor(uint64 UID, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	SAccessInfo& Info = m_IngressActors[UID];
	Info.bBlocked = bBlocked;
	Info.LastAccessTime = AccessTime;
	if (bThread)
		Info.ThreadAccessMask |= AccessMask;
	else
		Info.ProcessAccessMask |= AccessMask;

}

CVariant CAccessLog::StoreIngressActors(const SVarWriteOpt& Opts) const
{
	CVariant Actors;
	Actors.BeginList();
	DumpIngressActors(Actors, Opts);
	Actors.Finish();
	return Actors;
}

void CAccessLog::LoadIngressActors(const CVariant& Data)
{
	Data.ReadRawList([&](const CVariant& vData) {
		CProgramID ID;
		ID.FromVariant(vData[API_V_PROG_ID]);
		CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
		if (!pItem) return;
		uint32 uThreadAccessMask = vData[API_V_THREAD_ACCESS_MASK].To<uint32>(0);
		uint32 uProcessAccessMask = vData[API_V_PROCESS_ACCESS_MASK].To<uint32>(0);
		bool bThread = uThreadAccessMask != 0;
		AddIngressActor(pItem->GetUID(), bThread, bThread ? uThreadAccessMask : uProcessAccessMask, vData[API_V_PROC_EVENT_LAST_EXEC], vData[API_V_PROC_EVENT_BLOCKED]);
	});
}

void CAccessLog::DumpIngressActors(CVariant& Actors, const SVarWriteOpt& Opts) const
{
	for (auto& pItem : m_IngressActors)
	{
		CVariant vActor;
		vActor.BeginIMap();
		if (Opts.Flags & SVarWriteOpt::eSaveToFile) {
			CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(pItem.first);
			if(!pProg) continue;
			vActor.WriteVariant(API_V_PROG_ID, pProg->GetID().ToVariant(Opts));
		}
		else {
			vActor.Write(API_V_PROC_REF, (uint64)&pItem.second);
			vActor.Write(API_V_PROC_EVENT_ACTOR, pItem.first);
		}
		vActor.Write(API_V_THREAD_ACCESS_MASK, pItem.second.ThreadAccessMask);
		vActor.Write(API_V_PROCESS_ACCESS_MASK, pItem.second.ProcessAccessMask);
		vActor.Write(API_V_PROC_EVENT_LAST_ACT, pItem.second.LastAccessTime);
		vActor.Write(API_V_PROC_EVENT_BLOCKED, pItem.second.bBlocked);
		vActor.Finish();
		Actors.WriteVariant(vActor);
	}
}

void CAccessLog::AddIngressTarget(uint64 UID, bool bThread, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{
	SAccessInfo& Info = m_IngressTargets[UID];
	Info.bBlocked = bBlocked;
	Info.LastAccessTime = AccessTime;
	if (bThread)
		Info.ThreadAccessMask |= AccessMask;
	else
		Info.ProcessAccessMask |= AccessMask;
}

CVariant CAccessLog::StoreIngressTargets(const SVarWriteOpt& Opts) const
{
	CVariant Targets;
	Targets.BeginList();
	DumpIngressTargets(Targets, Opts);
	Targets.Finish();
	return Targets;
}

void CAccessLog::LoadIngressTargets(const CVariant& Data)
{
	Data.ReadRawList([&](const CVariant& vData) {
		CProgramID ID;
		ID.FromVariant(vData[API_V_PROG_ID]);
		CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID);
		if (!pItem) return;
		uint32 uThreadAccessMask = vData[API_V_THREAD_ACCESS_MASK].To<uint32>(0);
		uint32 uProcessAccessMask = vData[API_V_PROCESS_ACCESS_MASK].To<uint32>(0);
		bool bThread = uThreadAccessMask != 0;
		AddIngressTarget(pItem->GetUID(), bThread, bThread ? uThreadAccessMask : uProcessAccessMask, vData[API_V_PROC_EVENT_LAST_EXEC], vData[API_V_PROC_EVENT_BLOCKED]);
	});
}

void CAccessLog::DumpIngressTargets(CVariant& Targets, const SVarWriteOpt& Opts, const std::wstring& SvcTag) const
{
	for (auto& pItem : m_IngressTargets)
	{
		CVariant vTarget;
		vTarget.BeginIMap();
		if (Opts.Flags & SVarWriteOpt::eSaveToFile) {
			CProgramItemPtr pProg = theCore->ProgramManager()->GetItem(pItem.first);
			if(!pProg) continue;
			vTarget.WriteVariant(API_V_PROG_ID, pProg->GetID().ToVariant(Opts));
		}
		else {
			vTarget.Write(API_V_PROC_REF, (uint64)&pItem.second);
			vTarget.Write(API_V_PROC_EVENT_TARGET, pItem.first);
			if(!SvcTag.empty()) vTarget.Write(API_V_PROG_SVC_TAG, SvcTag);
		}
		vTarget.Write(API_V_THREAD_ACCESS_MASK, pItem.second.ThreadAccessMask);
		vTarget.Write(API_V_PROCESS_ACCESS_MASK, pItem.second.ProcessAccessMask);
		vTarget.Write(API_V_PROC_EVENT_LAST_ACT, pItem.second.LastAccessTime);
		vTarget.Write(API_V_PROC_EVENT_BLOCKED, pItem.second.bBlocked);
		vTarget.Finish();
		Targets.WriteVariant(vTarget);
	}
}

void CAccessLog::Clear()
{
	m_ExecActors.clear();
	m_ExecTargets.clear();

	m_IngressActors.clear();
	m_IngressTargets.clear();
}