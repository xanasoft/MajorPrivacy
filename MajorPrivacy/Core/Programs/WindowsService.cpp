#include "pch.h"
#include "WindowsService.h"
#include "../Library/Common/XVariant.h"
#include "../Service/ServiceAPI.h"
#include "../PrivacyCore.h"

CWindowsService::CWindowsService(QObject* parent)
	: CProgramItem(parent)
{
}

QIcon CWindowsService::DefaultIcon() const
{
	QIcon DllIcon(":/dll16.png");
	DllIcon.addFile(":/dll32.png");
	DllIcon.addFile(":/dll48.png");
	DllIcon.addFile(":/dll64.png");
	//return QIcon(":/Icons/Process.png");
	return DllIcon;
}

QString CWindowsService::GetNameEx() const
{
	if(m_Name.isEmpty())
		return m_ServiceId;
	if(m_Name.contains(m_ServiceId, Qt::CaseInsensitive))
		return m_Name;
	return QString("%1 (%2)").arg(m_Name).arg(m_ServiceId); // todo advanced view only
}

CProgramFilePtr CWindowsService::GetProgramFile() const
{
	CProgramFilePtr pProgram;
	if (!m_Groups.isEmpty())
		pProgram = m_Groups.first().lock().objectCast<CProgramFile>();
	return pProgram;
}

void CWindowsService::CountStats()
{
	m_Stats.ProcessCount = m_ProcessId != 0 ? 1 : 0;

	m_Stats.FwRuleCount = m_FwRuleIDs.count();
	m_Stats.SocketCount = m_SocketRefs.count();
}

QMap<QString, CTrafficEntryPtr>	CWindowsService::GetTrafficLog()
{
	auto Res = theCore->GetTrafficLog(m_ID, m_TrafficLogLastActivity);
	if (!Res.IsError())
		m_TrafficLogLastActivity = CTrafficEntry__LoadList(m_TrafficLog, Res.GetValue());
	return m_TrafficLog;
}

void CWindowsService::ReadValue(const SVarName& Name, const XVariant& Data)
{
		 if (VAR_TEST_NAME(Name, SVC_API_PROG_SVC_TAG))		m_ServiceId = Data.AsQStr();

	else if (VAR_TEST_NAME(Name, SVC_API_PROG_PROC_PID))	m_ProcessId = Data;

	else if (VAR_TEST_NAME(Name, SVC_API_PROG_SOCKETS))		m_SocketRefs = Data.AsQSet<quint64>();

	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_LAST_ACT))	m_Stats.LastActivity = Data;

	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_UPLOAD))		m_Stats.Upload = Data;
	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_DOWNLOAD))	m_Stats.Download = Data;
	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_UPLOADED))	m_Stats.Uploaded = Data;
	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_DOWNLOADED))	m_Stats.Downloaded = Data;

	else CProgramItem::ReadValue(Name, Data);
}
