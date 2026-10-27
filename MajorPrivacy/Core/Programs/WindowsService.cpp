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
	QWriteLocker Lock(&m_Mutex);
	// at most one fetch per interval, while the flag stays set the next call tries again
	if (m_ExecChanged && GetCurTick() - m_ExecFetched >= PROG_STATS_REFETCH_INTERVAL)
	{
		// cleared before the request, so an event arriving while it is in flight sets it again
		m_ExecChanged = false;
		m_ExecFetched = GetCurTick();
		Lock.unlock();
		auto Res = theCore->GetExecStats(m_ID);
		Lock.relock();
		if (Res.IsError())
			m_ExecChanged = true;
		else
		{
			m_ExecStats.clear();

			auto Data = Res.GetValue();

			auto Targets = Data.Get(API_V_PROG_EXEC_CHILDREN);
			QtVariantReader(Targets).ReadRawList([&](const FW::CVariant& vData) {
				const QtVariant& Data = *(QtVariant*)&vData;

				CProgramFile::SExecutionInfo Info;
				Info.Role = EExecLogRole::eTarget;

				Info.EnclaveGuid.FromVariant(Data.Get(API_V_EVENT_ACTOR_EID));

				Info.SubjectUID = Data.Get(API_V_EVENT_TARGET_UID).To<uint64>(0);
				Info.SubjectPID = Data.Get(API_V_PID).To<uint64>(0);
				Info.SubjectEnclave.FromVariant(Data.Get(API_V_EVENT_TARGET_EID));
				Info.ActorSvcTag = Data.Get(API_V_SERVICE_TAG).AsQStr();

				Info.LastExecTime = Data.Get(API_V_LAST_ACTIVITY).To<uint64>(0);
				Info.bBlocked = Data.Get(API_V_WAS_BLOCKED).To<bool>(false);
				Info.CommandLine = Data.Get(API_V_CMD_LINE).AsQStr();

				m_ExecStats.insert(m_ExecStats.size(), Info);
			});
			m_ExecRevision++;
		}
	}

	return m_ExecStats;
}

QMap<quint64, CProgramFile::SIngressInfo> CWindowsService::GetIngressStats()
{
	QWriteLocker Lock(&m_Mutex);
	// at most one fetch per interval, while the flag stays set the next call tries again
	if (m_IngressChanged && GetCurTick() - m_IngressFetched >= PROG_STATS_REFETCH_INTERVAL)
	{
		// cleared before the request, so an event arriving while it is in flight sets it again
		m_IngressChanged = false;
		m_IngressFetched = GetCurTick();
		Lock.unlock();
		auto Res = theCore->GetIngressStats(m_ID);
		Lock.relock();
		if (Res.IsError())
			m_IngressChanged = true;
		else
		{
			m_Ingress.clear();

			auto Data = Res.GetValue();

			auto Targets = Data.Get(API_V_PROG_INGRESS_TARGETS);
			QtVariantReader(Targets).ReadRawList([&](const FW::CVariant& vData) {
				const QtVariant& Data = *(QtVariant*)&vData;

				CProgramFile::SIngressInfo Info;
				Info.Role = EExecLogRole::eTarget;

				Info.EnclaveGuid.FromVariant(Data.Get(API_V_EVENT_ACTOR_EID));

				Info.SubjectUID = Data.Get(API_V_EVENT_TARGET_UID).To<uint64>(0);
				Info.SubjectPID = Data.Get(API_V_PID).To<uint64>(0);
				Info.SubjectEnclave.FromVariant(Data.Get(API_V_EVENT_TARGET_EID));
				Info.ActorSvcTag = Data.Get(API_V_SERVICE_TAG).AsQStr();

				Info.LastAccessTime = Data.Get(API_V_LAST_ACTIVITY).To<uint64>(0);
				Info.bBlocked = Data.Get(API_V_WAS_BLOCKED).To<bool>(false);
				Info.ThreadAccessMask = Data.Get(API_V_THREAD_ACCESS_MASK).To<uint32>(0);
				Info.ProcessAccessMask = Data.Get(API_V_PROCESS_ACCESS_MASK).To<uint32>(0);

				m_Ingress.insert(m_Ingress.size(), Info);
			});
			m_IngressRevision++;
		}
	}	

	return m_Ingress;
}

QMap<quint64, SAccessStatsPtr> CWindowsService::GetAccessStats()
{
	auto Res = theCore->GetAccessStats(m_ID, m_AccessLastActivity);
	QWriteLocker Lock(&m_Mutex);
	if (!Res.IsError()) {
		QtVariant Root = Res.GetValue();
		// same as for the traffic log, an empty dump must not reset the watermark
		quint64 LastActivity = ReadAccessBranch(m_AccessLog, Root);
		if (LastActivity > m_AccessLastActivity)
			m_AccessLastActivity = LastActivity;
	}
	return m_AccessLog.Changes;
}

QHash<QString, CTrafficEntryPtr> CWindowsService::GetTrafficLog(QList<CTrafficEntryPtr>* pChanged)
{
	auto Res = theCore->GetTrafficLog(m_ID, m_TrafficLogLastActivity);
	QWriteLocker Lock(&m_Mutex);
	if (!Res.IsError()) {
		// an empty reply returns 0, which must not reset the watermark or the next call fetches the whole log again
		quint64 LastActivity = CTrafficEntry__LoadList(m_TrafficLog, m_Unresolved, Res.GetValue(), pChanged);
		if (LastActivity > m_TrafficLogLastActivity)
			m_TrafficLogLastActivity = LastActivity;
	}
	return m_TrafficLog;
}

void CWindowsService::ClearProcessLogs()
{
	QWriteLocker Lock(&m_Mutex);

	m_ExecStats.clear();
	m_ExecChanged = true;
	m_ExecRevision++;

	m_Ingress.clear();
	m_IngressChanged = true;
	m_IngressRevision++;
}

void CWindowsService::ClearAccessLog()
{
	QWriteLocker Lock(&m_Mutex);

	m_AccessLog.Stats.clear();
	m_AccessLog.Changes.clear();
	m_AccessLastActivity = 0;
}

void CWindowsService::ClearTrafficLog()
{
	QWriteLocker Lock(&m_Mutex);

	m_TrafficLog.clear();
	m_Unresolved.clear();
	m_TrafficLogLastActivity = 0;
	m_TrafficLogGeneration++;
}

