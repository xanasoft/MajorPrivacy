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
	QReadLocker Lock(&m_Mutex); 

	auto PathName = Split2(m_Path, "\\", true);
	QString FileName = PathName.second.isEmpty() ? m_Path : PathName.second;
	if(m_Name.isEmpty())
		return FileName;
	return QString("%1 (%2)").arg(FileName).arg(m_Name); // todo make switchable
}

QString CProgramFile::GetPublisher() const
{
	return GetSignInfo().GetSignerName();
}

QMultiMap<quint64, SLibraryInfo> CProgramFile::GetLibraries()
{
	QReadLocker Lock(&m_Mutex); 
	if (m_LibrariesChanged)
	{
		Lock.unlock();
		auto Res = theCore->GetLibraryStats(m_ID);
		if (!Res.IsError())
		{
			QWriteLocker WriteLock(&m_Mutex);
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
		Lock.relock();
	}

	return m_Libraries;
}

QMap<quint64, CProgramFile::SExecutionInfo> CProgramFile::GetExecStats()
{
	QReadLocker Lock(&m_Mutex);
	if (m_ExecChanged)
	{
		Lock.unlock();
		auto Res = theCore->GetExecStats(m_ID);
		if (!Res.IsError())
		{
			QWriteLocker WriteLock(&m_Mutex);
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

				Info.LastExecTime = Data.Get(API_V_LAST_ACTIVITY).To<uint64>(0);
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

QMap<quint64, CProgramFile::SIngressInfo> CProgramFile::GetIngressStats()
{
	QReadLocker Lock(&m_Mutex);
	if(m_IngressChanged)
	{
		Lock.unlock();
		auto Res = theCore->GetIngressStats(m_ID);
		if (!Res.IsError())
		{
			QWriteLocker WriteLock(&m_Mutex);
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
		Lock.relock();
	}	

	return m_Ingress;
}

void CProgramFile::TraceLogAdd(ETraceLogs Log, const CLogEntryPtr& pEntry, quint64 Index)
{
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

	QWriteLocker Lock(&m_Mutex);

	STraceLogList* pLog = &m_Logs[(int)Log];

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
	QReadLocker Lock(&m_Mutex); 
	STraceLogList* pLog = &m_Logs[(int)Log];

	pLog->LastGetLog = GetCurTick();

	if (pLog->MissingCount)
	{
		Lock.unlock();
		auto Ret  = theCore->GetTraceLog(m_ID, Log);
		if (!Ret.IsError())
		{
			QWriteLocker WriteLock(&m_Mutex);
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
		Lock.relock();
	}

	return pLog;
}

void CProgramFile::ClearTraceLog(ETraceLogs Log)
{
	QWriteLocker Lock(&m_Mutex);
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
	QWriteLocker Lock(&m_Mutex);
	m_ExecStats.clear();
	m_ExecChanged = true;

	m_Ingress.clear();
	m_IngressChanged = true;
}

void CProgramFile::ClearAccessLog()
{
	QWriteLocker Lock(&m_Mutex);
	m_AccessStats.clear();
	m_AccessLastActivity = 0;
}

void CProgramFile::ClearTrafficLog()
{
	QWriteLocker Lock(&m_Mutex);
	m_TrafficLog.clear();
	m_Unresolved.clear();
	m_TrafficLogLastActivity = 0;
}

void CProgramFile::CountStats()
{
	QWriteLocker Lock(&m_Mutex);
	m_Stats.ServicesCount = m_Nodes.count(); // a CProgramFile can only have CWindowsService nodes

	m_Stats.LastExecution = m_LastExec;
	m_Stats.ProcessCount = m_ProcessPids.count();
	m_Stats.ProgRuleTotal = m_Stats.ProgRuleCount = m_ProgRuleIDs.count();

	m_Stats.ResRuleTotal = m_Stats.ResRuleCount = m_ResRuleIDs.count();
	m_Stats.AccessCount = m_AccessCount;
	m_Stats.HandleCount = m_HandleCount;

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
	QWriteLocker Lock(&m_Mutex);
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

QHash<QString, CTrafficEntryPtr> CProgramFile::GetTrafficLog()
{
	auto Res = theCore->GetTrafficLog(m_ID, m_TrafficLogLastActivity);
	QWriteLocker Lock(&m_Mutex);
	if (!Res.IsError())
		m_TrafficLogLastActivity = CTrafficEntry__LoadList(m_TrafficLog, m_Unresolved, Res.GetValue());
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

QString CProgramFile::GetSignatureInfoStr(const CImageSignInfo& SignInfo)
{
	QStringList Signs;
	if(SignInfo.m_Signatures.Developer)
		Signs += tr("Developer Signed");
	else if(SignInfo.m_Signatures.User)
		Signs += tr("User Signed");

	//if (SignInfo.m_HashStatus == EHashStatus::eHashFail)
	//	Signs.append(tr("Embedded Signature INVALID"));

	if (SignInfo.m_SignPolicyBits) 
	{
		QStringList Policy;

		// 8 bit
		if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_NO_ROOT)			Policy += tr("NONE"); // self signed
		if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_MICROSOFT_ROOT)		Policy += tr("Microsoft");
		if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_TEST_ROOT)			Policy += tr("Test");
		if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_CODE_ROOT)			Policy += tr("Code");
		if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_UNKNOWN_ROOT)		Policy += tr("UNKNOWN");
		if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_DMD_ROOT)			Policy += tr("DMD");
		if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_DMD_TEST_ROOT)		Policy += tr("DMD Test");
		if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_3RD_PARTY_ROOT)		Policy += tr("3rd Party");

		// 16 bit
		//if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_TRUSTED_BOOT_ROOT)	Policy += tr("Trusted Boot");
		//if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_UEFI_ROOT)			Policy += tr("UEFI");
		//if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_FLIGHT_ROOT)		Policy += tr("Flight");
		//if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_PRS_WIN81_ROOT)		Policy += tr("PRS Win81");
		//if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_TEST_WIN81_ROOT)	Policy += tr("Test Win81");
		if (SignInfo.m_SignPolicyBits & MINCRYPT_POLICY_OTHER_ROOT)			Policy += tr("Other");

		if(Policy.size() == 1)
			Signs += tr("Root: %1").arg(Policy[0]);
		else if(Policy.size() > 1)
			Signs += tr("Roots: %1").arg(Policy.join(", "));

#ifdef _DEBUG
		// 32 bit
		if (SignInfo.m_SignPolicyBits & 0xFFFF0000)							Signs += tr(" (Error: %1)").arg(SignInfo.m_SignPolicyBits >> 16);
#endif
	}

	// 4 bit
	switch (SignInfo.m_SignLevel) { 
	case SE_SIGNING_LEVEL_UNSIGNED:			Signs += tr("CI: Level 0"); break;
	case SE_SIGNING_LEVEL_ENTERPRISE:		Signs += tr("CI: Enterprise"); break;
	case SE_SIGNING_LEVEL_DEVELOPER:		Signs += tr("CI: Developer"); break;
	case SE_SIGNING_LEVEL_AUTHENTICODE:		Signs += tr("CI: Authenticode"); break;	// CAUTION: this ine is also true for self-signed images
	case SE_SIGNING_LEVEL_CUSTOM_2:			Signs += tr("CI: Level 2"); break;
	case SE_SIGNING_LEVEL_STORE:			Signs += tr("CI: Store"); break;
	case SE_SIGNING_LEVEL_ANTIMALWARE:		Signs += tr("CI: Antimalware"); break;
	case SE_SIGNING_LEVEL_MICROSOFT:		Signs += tr("CI: Microsoft"); break;
	case SE_SIGNING_LEVEL_CUSTOM_4:			Signs += tr("CI: Level 4"); break;
	case SE_SIGNING_LEVEL_CUSTOM_5:			Signs += tr("CI: Level 5"); break;
	case SE_SIGNING_LEVEL_DYNAMIC_CODEGEN:	Signs += tr("CI: Code Gen."); break;
	case SE_SIGNING_LEVEL_WINDOWS:			Signs += tr("CI: Windows"); break;
	case SE_SIGNING_LEVEL_CUSTOM_7:			Signs += tr("CI: Level 7"); break;
	case SE_SIGNING_LEVEL_WINDOWS_TCB:		Signs += tr("CI: Windows TCB"); break;
	case SE_SIGNING_LEVEL_CUSTOM_6:			Signs += tr("CI: Level 6"); break;
	}

	if (SignInfo.m_StatusFlags & MP_VERIFY_FLAG_COHERENCY_FAIL)
		Signs.prepend(tr("Image INCOCHERENT"));

	return Signs.join(" | ");
}

QSharedPointer<class CWindowsService> CProgramFile::GetService(const QString& SvcTag) const
{
	QReadLocker Lock(&m_Mutex); 

	for (auto pNode : m_Nodes) {
		CWindowsServicePtr pSvc = pNode.objectCast<CWindowsService>();
		if (pSvc && pSvc->GetServiceTag() == SvcTag)
			return pSvc;
	}
	return nullptr;
}