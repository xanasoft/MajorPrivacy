#include "pch.h"
#include "ProgramFile.h"
#include "../Library/Common/XVariant.h"
#include "../Service/ServiceAPI.h"
#include "../PrivacyCore.h"
#include "../Network/NetLogEntry.h"

CProgramFile::CProgramFile(QObject* parent)
	: CProgramSet(parent)
{
}

QString CProgramFile::GetNameEx() const
{ 
	QString FileName = QFileInfo(m_FileName).fileName();
	if(m_Name.isEmpty())
		return FileName;
	return QString("%1 (%2)").arg(m_Name).arg(FileName); // todo make switchable
}

void CProgramFile::TraceLogAdd(ETraceLogs Log, const CLogEntryPtr& pEntry, quint32 Index)
{ 
	STraceLogList* pLog = &m_Logs[(int)Log];
	if (pLog->MissingIndex == -1) pLog->MissingIndex = 0;
	if (Index != pLog->MissingIndex + pLog->Entries.size()) {
		pLog->Entries.clear();
		pLog->MissingIndex = Index;
	}
	pLog->Entries.append(pEntry); 
}

STraceLogList* CProgramFile::GetTraceLog(ETraceLogs Log)
{ 
	STraceLogList* pLog = &m_Logs[(int)Log];
	if (pLog->MissingIndex)
	{
		auto Ret  = theCore->GetTraceLog(m_ID, Log);
		if (Ret.IsError())
			return pLog;
		
		pLog->Entries.clear();
		pLog->MissingIndex = 0;
		XVariant& Entries = Ret.GetValue();

		switch (Log)
		{
		case ETraceLogs::eNetLog:
			Entries.ReadRawList([&](const CVariant& vData) {
				const XVariant& Entry = *(XVariant*)&vData;

				CLogEntryPtr pEntry = CLogEntryPtr(new CNetLogEntry());
				pEntry->FromVariant(Entry);
				pLog->Entries.append(pEntry);
			});
			break;
		//todo: other
		}
	}

	return pLog;
}

void CProgramFile::CountStats()
{
	foreach(auto pNode, m_Nodes)
		pNode->CountStats();

	m_Stats.ProcessCount = m_ProcessPids.count();
	
	m_Stats.FwRuleCount = m_FwRuleIDs.count();
	m_Stats.SocketCount = m_SocketRefs.count();
}

QMap<QString, CTrafficEntryPtr>	CProgramFile::GetTrafficLog()
{
	auto Res = theCore->GetTrafficLog(m_ID, m_TrafficLogLastActivity);
	if (!Res.IsError())
		m_TrafficLogLastActivity = CTrafficEntry__LoadList(m_TrafficLog, Res.GetValue());
	return m_TrafficLog;
}

void CProgramFile::ReadValue(const SVarName& Name, const XVariant& Data)
{
		 if (VAR_TEST_NAME(Name, SVC_API_PROG_FILE))		m_FileName = Data.AsQStr();

	else if (VAR_TEST_NAME(Name, SVC_API_PROG_PROC_PIDS))	m_ProcessPids = Data.AsQSet<quint64>();

	else if (VAR_TEST_NAME(Name, SVC_API_PROG_SOCKETS))		m_SocketRefs = Data.AsQSet<quint64>();

	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_LAST_ACT))	m_Stats.LastActivity = Data;

	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_UPLOAD))		m_Stats.Upload = Data;
	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_DOWNLOAD))	m_Stats.Download = Data;
	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_UPLOADED))	m_Stats.Uploaded = Data;
	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_DOWNLOADED))	m_Stats.Downloaded = Data;

	else CProgramSet::ReadValue(Name, Data);
}
