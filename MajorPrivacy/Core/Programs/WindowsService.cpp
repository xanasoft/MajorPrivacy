#include "pch.h"
#include "WindowsService.h"
#include "../Library/Common/XVariant.h"
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

QMap<quint64, SAccessStatsPtr> CWindowsService::GetAccessStats()
{
	auto Res = theCore->GetAccessStats(m_ID, m_TrafficLogLastActivity);
	if (!Res.IsError()) {
		XVariant Root = Res.GetValue();
		m_TrafficLogLastActivity = ReadAccessBranch(m_AccessStats, Root);
	}
	return m_AccessStats;
}

QMap<QString, CTrafficEntryPtr>	CWindowsService::GetTrafficLog()
{
	auto Res = theCore->GetTrafficLog(m_ID, m_TrafficLogLastActivity);
	if (!Res.IsError())
		m_TrafficLogLastActivity = CTrafficEntry__LoadList(m_TrafficLog, Res.GetValue());
	return m_TrafficLog;
}

