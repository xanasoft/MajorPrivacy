#include "pch.h"
#include "WindowsService.h"
#include "./Common/QtVariant.h"
#include "../Library/API/PrivacyAPI.h"
#include "../PrivacyCore.h"
#include "ProgramLibrary.h"

CWindowsService::CWindowsService(QObject* parent)
	: CProgramItem(parent)
{
}

QIcon CWindowsService::DefaultIcon() const
{
	return CProgramLibrary::DefaultIcon();
}

QString CWindowsService::GetNameEx() const
{
	QReadLocker Lock(&m_Mutex); 

	if(m_Name.isEmpty())
		return m_ServiceTag;
	if(m_Name.contains(m_ServiceTag, Qt::CaseInsensitive))
		return m_Name;
	return QString("%1 (%2)").arg(m_Name).arg(m_ServiceTag); // todo advanced view only
}

QString CWindowsService::GetPath() const
{
	CProgramFilePtr pProg = GetProgramFile();
	if(pProg)
		return pProg->GetPath();
	return "";
}

CProgramFilePtr CWindowsService::GetProgramFile() const
{
	QReadLocker Lock(&m_Mutex); 

	CProgramFilePtr pProgram;
	if (!m_Groups.isEmpty())
		pProgram = m_Groups.first().lock().objectCast<CProgramFile>();
	return pProgram;
}

void CWindowsService::CountStats()
{
	QWriteLocker Lock(&m_Mutex); 

	m_Stats.LastExecution = m_LastExec;

	m_Stats.ProcessCount = m_ProcessId != 0 ? 1 : 0;

	m_Stats.ProgRuleTotal = m_Stats.ProgRuleCount = m_ProgRuleIDs.count();

	m_Stats.ResRuleTotal = m_Stats.ResRuleCount = m_ResRuleIDs.count();
	m_Stats.AccessCount = m_AccessCount;
	m_Stats.HandleCount = m_HandleCount;

	m_Stats.FwRuleTotal = m_Stats.FwRuleCount = m_FwRuleIDs.count();
	m_Stats.SocketCount = m_SocketRefs.count();
}

QMap<quint64, CProgramFile::SExecutionInfo> CWindowsService::GetExecStats()
{
	QReadLocker Lock(&m_Mutex); 
	if (m_ExecChanged)
	{
		Lock.unlock();
		auto Res = theCore->GetExecStats(m_ID);
		if (!Res.IsError())
		{
			QWriteLocker Lock(&m_Mutex); 

			m_ExecStats.clear();
			m_ExecChanged = false;

			auto Data = Res.GetValue();

			auto Targets = Data.Get(API_V_PROG_EXEC_CHILDREN);
			QtVariantReader(Targets).ReadRawList([&](const FW::CVariant& vData) {
				const QtVariant& Data = *(QtVariant*)&vData;

				quint64 Ref = Data.Get(API_V_PROCESS_REF).To<uint64>(0);

				CProgramFile::SExecutionInfo Info;
				Info.Role = EExecLogRole::eTarget;

				Info.EnclaveGuid.FromVariant(Data.Get(API_V_EVENT_ACTOR_EID));

				Info.SubjectUID = Data.Get(API_V_EVENT_TARGET_UID).To<uint64>(0);
				Info.SubjectEnclave.FromVariant(Data.Get(API_V_EVENT_TARGET_EID));
				Info.ActorSvcTag = Data.Get(API_V_SERVICE_TAG).AsQStr();

				Info.LastExecTime = Data.Get(API_V_LAST_ACTIVITY).To<uint64>(0);
				Info.bBlocked = Data.Get(API_V_WAS_BLOCKED).To<bool>(false);
				Info.CommandLine = Data.Get(API_V_CMD_LINE).AsQStr();

				m_ExecStats[Ref] = Info;
			});
		}
		Lock.relock();
	}

	return m_ExecStats;
}

QMap<quint64, CProgramFile::SIngressInfo> CWindowsService::GetIngressStats()
{
	QReadLocker Lock(&m_Mutex);

	if(m_IngressChanged)
	{
		Lock.unlock();
		auto Res = theCore->GetIngressStats(m_ID);
		if (!Res.IsError())
		{
			QWriteLocker Lock(&m_Mutex);

			m_Ingress.clear();
			m_IngressChanged = false;

			auto Data = Res.GetValue();

			auto Targets = Data.Get(API_V_PROG_INGRESS_TARGETS);
			QtVariantReader(Targets).ReadRawList([&](const FW::CVariant& vData) {
				const QtVariant& Data = *(QtVariant*)&vData;

				quint64 Ref = Data.Get(API_V_PROCESS_REF).To<uint64>(0);

				CProgramFile::SIngressInfo Info;
				Info.Role = EExecLogRole::eTarget;

				Info.EnclaveGuid.FromVariant(Data.Get(API_V_EVENT_ACTOR_EID));

				Info.SubjectUID = Data.Get(API_V_EVENT_TARGET_UID).To<uint64>(0);
				Info.SubjectEnclave.FromVariant(Data.Get(API_V_EVENT_TARGET_EID));
				Info.ActorSvcTag = Data.Get(API_V_SERVICE_TAG).AsQStr();

				Info.LastAccessTime = Data.Get(API_V_LAST_ACTIVITY).To<uint64>(0);
				Info.bBlocked = Data.Get(API_V_WAS_BLOCKED).To<bool>(false);
				Info.ThreadAccessMask = Data.Get(API_V_THREAD_ACCESS_MASK).To<uint32>(0);
				Info.ProcessAccessMask = Data.Get(API_V_PROCESS_ACCESS_MASK).To<uint32>(0);

				m_Ingress[Ref] = Info;
			});
		}
		Lock.relock();
	}	

	return m_Ingress;
}

QMap<quint64, SAccessStatsPtr> CWindowsService::GetAccessStats()
{
	auto Res = theCore->GetAccessStats(m_ID, m_AccessLastActivity);
	QWriteLocker Lock(&m_Mutex);
	if (!Res.IsError()) {
		QtVariant Root = Res.GetValue();
		m_AccessLastActivity = ReadAccessBranch(m_AccessStats, Root);
	}
	return m_AccessStats;
}

QHash<QString, CTrafficEntryPtr> CWindowsService::GetTrafficLog()
{
	auto Res = theCore->GetTrafficLog(m_ID, m_TrafficLogLastActivity);
	QWriteLocker Lock(&m_Mutex);
	if (!Res.IsError())
		m_TrafficLogLastActivity = CTrafficEntry__LoadList(m_TrafficLog, m_Unresolved, Res.GetValue());
	return m_TrafficLog;
}

void CWindowsService::ClearProcessLogs()
{
	QWriteLocker Lock(&m_Mutex);

	m_ExecStats.clear();
	m_ExecChanged = true;

	m_Ingress.clear();
	m_IngressChanged = true;
}

void CWindowsService::ClearAccessLog()
{
	QWriteLocker Lock(&m_Mutex);

	m_AccessStats.clear();
	m_AccessLastActivity = 0;
}

void CWindowsService::ClearTrafficLog()
{
	QWriteLocker Lock(&m_Mutex);

	m_TrafficLog.clear();
	m_Unresolved.clear();
	m_TrafficLogLastActivity = 0;
}

