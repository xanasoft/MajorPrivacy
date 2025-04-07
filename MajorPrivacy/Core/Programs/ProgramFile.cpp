#include "pch.h"
#include "ProgramFile.h"
#include "../Processes/ExecLogEntry.h"
#include "./Common/QtVariant.h"
#include "../Library/API/PrivacyAPI.h"
#include "../PrivacyCore.h"
#include "../Network/NetLogEntry.h"
#include "../Access/ResLogEntry.h"
#include "../MiscHelpers/Common/Common.h"
#include "ProgramRule.h"
#include "WindowsService.h"
#include "../Enclaves/Enclave.h"


CProgramFile::CProgramFile(QObject* parent)
	: CProgramSet(parent)
{
}

QString CProgramFile::GetNameEx() const
{ 
	auto PathName = Split2(m_Path, "\\", true);
	QString FileName = PathName.second.isEmpty() ? m_Path : PathName.second;
	if(m_Name.isEmpty())
		return FileName;
	return QString("%1 (%2)").arg(FileName).arg(m_Name); // todo make switchable
}

QMultiMap<quint64, SLibraryInfo> CProgramFile::GetLibraries()
{
	if (m_LibrariesChanged)
	{
		auto Res = theCore->GetLibraryStats(m_ID);
		if (!Res.IsError())
		{
			m_Libraries.clear();
			m_LibrariesChanged = false;

			QtVariantReader(Res.GetValue()).ReadRawList([&](const FW::CVariant& vData) {
				const QtVariant& Data = *(QtVariant*)&vData;

				quint64 LibraryRef = Data.Get(API_V_LIB_REF).To<uint64>(0);

				SLibraryInfo Info;
				Info.EnclaveGuid.FromVariant(Data.Get(API_V_ENCLAVE));
				Info.LastLoadTime = Data.Get(API_V_LIB_LOAD_TIME).To<uint64>(0);
				Info.TotalLoadCount = Data.Get(API_V_LIB_LOAD_COUNT).To<uint32>(0);
				Info.SignInfo.FromVariant(Data.Get(API_V_SIGN_INFO));
				Info.LastStatus = (EEventStatus)Data.Get(API_V_LIB_STATUS).To<uint32>();
				m_Libraries.insert(LibraryRef, Info);
			});
		}
	}

	return m_Libraries;
}

QMap<quint64, CProgramFile::SExecutionInfo> CProgramFile::GetExecStats()
{
	if (m_ExecChanged)
	{
		auto Res = theCore->GetExecStats(m_ID);
		if (!Res.IsError())
		{
			m_ExecStats.clear();
			m_ExecChanged = false;

			auto Data = Res.GetValue();

			auto Targets = Data.Get(API_V_PROG_EXEC_CHILDREN);
			QtVariantReader(Targets).ReadRawList([&](const FW::CVariant& vData) {
				const QtVariant& Data = *(QtVariant*)&vData;

				quint64 Ref = Data.Get(API_V_PROCESS_REF).To<uint64>(0);

				SExecutionInfo Info;
				Info.Role = EExecLogRole::eTarget;

				Info.EnclaveGuid.FromVariant(Data.Get(API_V_EVENT_ACTOR_EID));

				Info.SubjectUID = Data.Get(API_V_EVENT_TARGET_UID).To<uint64>(0);
				Info.SubjectPID = Data.Get(API_V_PID).To<uint64>(0);
				Info.SubjectEnclave.FromVariant(Data.Get(API_V_EVENT_TARGET_EID));
				Info.ActorSvcTag = Data.Get(API_V_SERVICE_TAG).AsQStr();

				Info.LastExecTime = Data.Get(API_V_PROG_LAST_EXEC).To<uint64>(0);
				Info.bBlocked = Data.Get(API_V_WAS_BLOCKED).To<bool>(false);
				Info.CommandLine = Data.Get(API_V_CMD_LINE).AsQStr();

				m_ExecStats[Ref] = Info;
			});

			auto Actors = Data.Get(API_V_PROG_EXEC_PARENTS);
			QtVariantReader(Actors).ReadRawList([&](const FW::CVariant& vData) {
				const QtVariant& Data = *(QtVariant*)&vData;

				quint64 Ref = Data.Get(API_V_PROCESS_REF).To<uint64>(0);

				SExecutionInfo Info;
				Info.Role = EExecLogRole::eActor;

				Info.EnclaveGuid.FromVariant(Data.Get(API_V_EVENT_TARGET_EID));

				Info.SubjectUID = Data.Get(API_V_EVENT_ACTOR_UID).To<uint64>(0);
				Info.SubjectPID = Data.Get(API_V_PID).To<uint64>(0);
				Info.SubjectEnclave.FromVariant(Data.Get(API_V_EVENT_ACTOR_EID));
				Info.ActorSvcTag = Data.Get(API_V_SERVICE_TAG).AsQStr();

				Info.LastExecTime = Data.Get(API_V_PROG_LAST_EXEC).To<uint64>(0);
				Info.bBlocked = Data.Get(API_V_WAS_BLOCKED).To<bool>(false);
				Info.CommandLine = Data.Get(API_V_CMD_LINE).AsQStr();

				m_ExecStats[Ref] = Info;
			});	

		}
	}

	return m_ExecStats;
}

QMap<quint64, CProgramFile::SIngressInfo> CProgramFile::GetIngressStats()
{
	if(m_IngressChanged)
	{
		auto Res = theCore->GetIngressStats(m_ID);
		if (!Res.IsError())
		{
			m_Ingress.clear();
			m_IngressChanged = false;

			auto Data = Res.GetValue();

			auto Targets = Data.Get(API_V_PROG_INGRESS_TARGETS);
			QtVariantReader(Targets).ReadRawList([&](const FW::CVariant& vData) {
				const QtVariant& Data = *(QtVariant*)&vData;

				quint64 Ref = Data.Get(API_V_PROCESS_REF).To<uint64>(0);

				SIngressInfo Info;
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

				m_Ingress[Ref] = Info;
			});

			auto Actors = Data.Get(API_V_PROG_INGRESS_ACTORS);
			QtVariantReader(Actors).ReadRawList([&](const FW::CVariant& vData) {
				const QtVariant& Data = *(QtVariant*)&vData;

				quint64 Ref = Data.Get(API_V_PROCESS_REF).To<uint64>(0);

				SIngressInfo Info;
				Info.Role = EExecLogRole::eActor;

				Info.EnclaveGuid.FromVariant(Data.Get(API_V_EVENT_TARGET_EID));

				Info.SubjectUID = Data.Get(API_V_EVENT_ACTOR_UID).To<uint64>(0);
				Info.SubjectPID = Data.Get(API_V_PID).To<uint64>(0);
				Info.SubjectEnclave.FromVariant(Data.Get(API_V_EVENT_ACTOR_EID));
				Info.ActorSvcTag = Data.Get(API_V_SERVICE_TAG).AsQStr();

				Info.LastAccessTime = Data.Get(API_V_LAST_ACTIVITY).To<uint64>(0);
				Info.bBlocked = Data.Get(API_V_WAS_BLOCKED).To<bool>(false);
				Info.ThreadAccessMask = Data.Get(API_V_THREAD_ACCESS_MASK).To<uint32>(0);
				Info.ProcessAccessMask = Data.Get(API_V_PROCESS_ACCESS_MASK).To<uint32>(0);

				m_Ingress[Ref] = Info;
			});
		}
	}	

	return m_Ingress;
}

void CProgramFile::TraceLogAdd(ETraceLogs Log, const CLogEntryPtr& pEntry, quint64 Index)
{ 
	STraceLogList* pLog = &m_Logs[(int)Log];

	if (Log == ETraceLogs::eResLog)
	{
		CWindowsServicePtr pActorService = GetService(pEntry->GetOwnerService());
		if (pActorService) 
			pActorService->m_AccessLastEvent = pEntry->GetTimeStamp();
		else
			m_AccessLastEvent = pEntry->GetTimeStamp();
	}

	if (Log == ETraceLogs::eExecLog)
	{
		const CExecLogEntry* pExecEntry = dynamic_cast<const CExecLogEntry*>(pEntry.constData());
		if (pExecEntry)
		{
			if(pExecEntry->GetType() == EExecLogType::eImageLoad)
				m_LibrariesChanged = true;
			else if(pExecEntry->GetType() == EExecLogType::eProcessStarted)
				m_ExecChanged = true;
			else //if(pExecEntry->GetType() == EExecLogType::eProcessAccess || pExecEntry->GetType() == EExecLogType::eThreadAccess)
				m_IngressChanged = true;
		}
	}

	// dispose of unwatched logs
	if (GetCurTick() - pLog->LastGetLog > 60 * 1000) { // 60 sec
		if (!pLog->Entries.isEmpty()) {
			pLog->Entries.clear();
			pLog->MissingCount = -1;
			pLog->IndexOffset = 0;
		}
		return;
	}

	if (pLog->MissingCount == -1) pLog->MissingCount = 0;
	if (Index != pLog->MissingCount + pLog->IndexOffset + pLog->Entries.size()) {
		pLog->Entries.clear();
		if(pLog->IndexOffset > Index) pLog->IndexOffset = 0;
		pLog->MissingCount = Index - pLog->IndexOffset;
	}
	pLog->Entries.append(pEntry); 
}

STraceLogList* CProgramFile::GetTraceLog(ETraceLogs Log)
{ 
	STraceLogList* pLog = &m_Logs[(int)Log];

	pLog->LastGetLog = GetCurTick();

	if (pLog->MissingCount)
	{
		auto Ret  = theCore->GetTraceLog(m_ID, Log);
		if (Ret.IsError())
			return pLog;
		
		pLog->Entries.clear();
		pLog->MissingCount = 0;
		QtVariant& LogData = Ret.GetValue();
		QtVariant Entries = LogData.Get(API_V_LOG_DATA);
		pLog->IndexOffset = LogData.Get(API_V_LOG_OFFSET).To<uint64>(0);

		switch (Log)
		{
		case ETraceLogs::eNetLog:
			QtVariantReader(Entries).ReadRawList([&](const FW::CVariant& vData) {
				const QtVariant& Entry = *(QtVariant*)&vData;

				CLogEntryPtr pEntry = CLogEntryPtr(new CNetLogEntry());
				pEntry->FromVariant(Entry);
				pLog->Entries.append(pEntry);
			});
			break;

		case ETraceLogs::eExecLog:
			QtVariantReader(Entries).ReadRawList([&](const FW::CVariant& vData) {
				const QtVariant& Entry = *(QtVariant*)&vData;

				CLogEntryPtr pEntry = CLogEntryPtr(new CExecLogEntry());
				pEntry->FromVariant(Entry);
				pLog->Entries.append(pEntry);
			});
			break;

		case ETraceLogs::eResLog:
			QtVariantReader(Entries).ReadRawList([&](const FW::CVariant& vData) {
				const QtVariant& Entry = *(QtVariant*)&vData;

				CLogEntryPtr pEntry = CLogEntryPtr(new CResLogEntry());
				pEntry->FromVariant(Entry);
				pLog->Entries.append(pEntry);
			});
			break;
		}
	}

	return pLog;
}

void CProgramFile::ClearTraceLog(ETraceLogs Log)
{
	if (Log == ETraceLogs::eLogMax) 
	{
		for (int i = 0; i < (int)ETraceLogs::eLogMax; i++)
		{
			STraceLogList* pLog = &m_Logs[i];
			pLog->Entries.clear();
			pLog->MissingCount = -1;
			pLog->IndexOffset = 0;
			pLog->LastGetLog = 0;
		}
	}
	else 
	{
		STraceLogList* pLog = &m_Logs[(int)Log];
		pLog->Entries.clear();
		pLog->MissingCount = -1;
		pLog->IndexOffset = 0;
		pLog->LastGetLog = 0;
	}
}

void CProgramFile::ClearProcessLogs()
{
	m_ExecStats.clear();
	m_ExecChanged = true;

	m_Ingress.clear();
	m_IngressChanged = true;
}

void CProgramFile::ClearLogs(ETraceLogs Log)
{
	ClearTraceLog(Log);

	if(Log == ETraceLogs::eLogMax || Log == ETraceLogs::eResLog)
		ClearAccessLog();
	if(Log == ETraceLogs::eLogMax || Log == ETraceLogs::eExecLog)
		ClearProcessLogs();
	if(Log == ETraceLogs::eLogMax || Log == ETraceLogs::eNetLog)
		ClearTrafficLog();
}

void CProgramFile::ClearAccessLog()
{
	m_AccessStats.clear();
	m_AccessLastActivity = 0;
}

void CProgramFile::ClearTrafficLog()
{
	m_TrafficLog.clear();
	m_TrafficLogLastActivity = 0;
}

void CProgramFile::CountStats()
{
	m_Stats.ServicesCount = m_Nodes.count(); // a CProgramFile can only have CWindowsService nodes

	m_Stats.LastExecution = m_LastExec;
	m_Stats.ProcessCount = m_ProcessPids.count();
	m_Stats.ProgRuleTotal = m_Stats.ProgRuleCount = m_ProgRuleIDs.count();

	m_Stats.ResRuleTotal = m_Stats.ResRuleCount = m_ResRuleIDs.count();
	m_Stats.AccessCount = m_AccessCount;

	m_Stats.FwRuleTotal = m_Stats.FwRuleCount = m_FwRuleIDs.count();
	m_Stats.SocketCount = m_SocketRefs.count();

	foreach(auto pNode, m_Nodes) 
	{
		pNode->CountStats();
		const SProgramStats* pStats = pNode->GetStats();

		m_Stats.ProgRuleTotal += pStats->ProgRuleTotal;

		m_Stats.ResRuleTotal += pStats->ResRuleTotal;

		m_Stats.FwRuleTotal += pStats->FwRuleTotal;
	}
}

QMap<quint64, SAccessStatsPtr> CProgramFile::GetAccessStats()
{
	auto Res = theCore->GetAccessStats(m_ID, m_AccessLastActivity);
	if (!Res.IsError()) {
		QtVariant Root = Res.GetValue();
//#ifdef _DEBUG
//		int oldCount = m_AccessStats.count();
//#endif
		m_AccessLastActivity = ReadAccessBranch(m_AccessStats, Root);
//#ifdef _DEBUG
//		if (oldCount < m_AccessStats.count())
//			qDebug() << "AccessStats count for" << GetName() << "added" << m_AccessStats.count() - oldCount;
//#endif
	}
	return m_AccessStats;
}

QMap<QString, CTrafficEntryPtr>	CProgramFile::GetTrafficLog()
{
	auto Res = theCore->GetTrafficLog(m_ID, m_TrafficLogLastActivity);
	if (!Res.IsError())
		m_TrafficLogLastActivity = CTrafficEntry__LoadList(m_TrafficLog, Res.GetValue());
	return m_TrafficLog;
}

//
// Self-signed
//
#define MINCRYPT_POLICY_NO_ROOT           0x0001
//
// Microsoft Authenticode Root Authority
// Microsoft Root Authority
// Microsoft Root Certificate Authority
//
#define MINCRYPT_POLICY_MICROSOFT_ROOT    0x0002
//
// Microsoft Test Root Authority
//
#define MINCRYPT_POLICY_TEST_ROOT         0x0004
//
// Microsoft Code Verification Root
//
#define MINCRYPT_POLICY_CODE_ROOT         0x0008
//
// Win 10 RS5 started using Unknown Root
//
#define MINCRYPT_POLICY_UNKNOWN_ROOT      0x0010
//
// Microsoft Digital Media Authority 2005
// Microsoft Digital Media Authority 2005 for preview releases
//
#define MINCRYPT_POLICY_DMD_ROOT          0x0020
//
// MS Protected Media Test Root
//
#define MINCRYPT_POLICY_DMD_TEST_ROOT     0x0040
//
// Win 8.1/10 flags
//
#define MINCRYPT_POLICY_3RD_PARTY_ROOT    0x0080
#define MINCRYPT_POLICY_TRUSTED_BOOT_ROOT 0x0100
#define MINCRYPT_POLICY_UEFI_ROOT         0x0200
#define MINCRYPT_POLICY_FLIGHT_ROOT       0x0400
#define MINCRYPT_POLICY_PRS_WIN81_ROOT    0x0800
#define MINCRYPT_POLICY_TEST_WIN81_ROOT   0x1000
#define MINCRYPT_POLICY_OTHER_ROOT        0x2000

QString CProgramFile::GetSignatureInfoStr(UCISignInfo SignInfo)
{
	QString Str = CEnclave::GetSignatureLevelStr((KPH_VERIFY_AUTHORITY)SignInfo.Authority);

	switch (SignInfo.Level) {
	case SE_SIGNING_LEVEL_UNSIGNED:			Str += tr(" (CI: Level 0)"); break;
	case SE_SIGNING_LEVEL_ENTERPRISE:		Str += tr(" (CI: Enterprise)"); break;
	case SE_SIGNING_LEVEL_DEVELOPER:		Str += tr(" (CI: Developer)"); break;
	case SE_SIGNING_LEVEL_AUTHENTICODE:		Str += tr(" (CI: Authenticode)"); break;
	case SE_SIGNING_LEVEL_CUSTOM_2:			Str += tr(" (CI: Level 2)"); break;
	case SE_SIGNING_LEVEL_STORE:			Str += tr(" (CI: Store)"); break;
	case SE_SIGNING_LEVEL_ANTIMALWARE:		Str += tr(" (CI: Antimalware)"); break;
	case SE_SIGNING_LEVEL_MICROSOFT:		Str += tr(" (CI: Microsoft)"); break;
	case SE_SIGNING_LEVEL_CUSTOM_4:			Str += tr(" (CI: Level 4)"); break;
	case SE_SIGNING_LEVEL_CUSTOM_5:			Str += tr(" (CI: Level 5)"); break;
	case SE_SIGNING_LEVEL_DYNAMIC_CODEGEN:	Str += tr(" (CI: Code Gene.)"); break;
	case SE_SIGNING_LEVEL_WINDOWS:			Str += tr(" (CI: Windows)"); break;
	case SE_SIGNING_LEVEL_CUSTOM_7:			Str += tr(" (CI: Level 7)"); break;
	case SE_SIGNING_LEVEL_WINDOWS_TCB:		Str += tr(" (CI: Windows TCB)"); break;
	case SE_SIGNING_LEVEL_CUSTOM_6:			Str += tr(" (CI: Level 6)"); break;
	}

	if (SignInfo.Policy) 
	{
		QStringList Policy;
		if (SignInfo.Policy & MINCRYPT_POLICY_NO_ROOT)				Policy += tr("None"); // self signed
		if (SignInfo.Policy & MINCRYPT_POLICY_MICROSOFT_ROOT)		Policy += tr("Microsoft");
		if (SignInfo.Policy & MINCRYPT_POLICY_TEST_ROOT)			Policy += tr("Test");
		if (SignInfo.Policy & MINCRYPT_POLICY_CODE_ROOT)			Policy += tr("Code");
		if (SignInfo.Policy & MINCRYPT_POLICY_UNKNOWN_ROOT)			Policy += tr("Unknown");
		if (SignInfo.Policy & MINCRYPT_POLICY_DMD_ROOT)				Policy += tr("DMD");
		if (SignInfo.Policy & MINCRYPT_POLICY_DMD_TEST_ROOT)		Policy += tr("DMD Test");
		if (SignInfo.Policy & MINCRYPT_POLICY_3RD_PARTY_ROOT)		Policy += tr("3rd Party");
		if (SignInfo.Policy & MINCRYPT_POLICY_TRUSTED_BOOT_ROOT)	Policy += tr("Trusted Boot");
		if (SignInfo.Policy & MINCRYPT_POLICY_UEFI_ROOT)			Policy += tr("UEFI");
		if (SignInfo.Policy & MINCRYPT_POLICY_FLIGHT_ROOT)			Policy += tr("Flight");
		//if (SignInfo.Policy & MINCRYPT_POLICY_PRS_WIN81_ROOT)		Policy += tr("PRS Win81");
		//if (SignInfo.Policy & MINCRYPT_POLICY_TEST_WIN81_ROOT)		Policy += tr("Test Win81");
		if (SignInfo.Policy & MINCRYPT_POLICY_OTHER_ROOT)			Policy += tr("Other");

		if(SignInfo.Policy & 0xFFFF0000)
			Policy += tr("Error: %1").arg(SignInfo.Policy >> 16);

		if(Policy.size() == 1)
			Str += tr(" (Root: %1)").arg(Policy[0]);
		else if(Policy.size() > 1)
			Str += tr(" (Roots: %1)").arg(Policy.join(", "));
	}

	return Str;
}

QSharedPointer<class CWindowsService> CProgramFile::GetService(const QString& SvcTag) const
{
	for (auto pNode : m_Nodes) {
		CWindowsServicePtr pSvc = pNode.objectCast<CWindowsService>();
		if (pSvc && pSvc->GetServiceTag() == SvcTag)
			return pSvc;
	}
	return nullptr;
}