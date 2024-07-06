#include "pch.h"
#include "ProgramGroup.h"
#include "../Library/Common/XVariant.h"
#include "../Library/API/PrivacyAPI.h"
#include "../PrivacyCore.h"
#include "../Processes/ProcessList.h"

///////////////////////////////////////////////////////////////////////////////////////
// CAllPrograms

void CAllPrograms::CountStats()
{
	m_Stats.ProcessCount = theCore->ProcessList()->GetCount();
	m_Stats.SocketCount = theCore->ProcessList()->GetSocketCount();
}

void CAllPrograms::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_SOCK_LAST_ACT:	m_Stats.LastActivity = Data; break;

	case API_V_SOCK_UPLOAD:		m_Stats.Upload = Data; break;
	case API_V_SOCK_DOWNLOAD:	m_Stats.Download = Data; break;
	case API_V_SOCK_UPLOADED:	m_Stats.Uploaded = Data; break;
	case API_V_SOCK_DOWNLOADED:	m_Stats.Downloaded = Data; break;

	default: CProgramSet::ReadIValue(Index, Data);
	}
}

void CAllPrograms::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ACT))			m_Stats.LastActivity = Data;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOAD))		m_Stats.Upload = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOAD))	m_Stats.Download = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOADED))	m_Stats.Uploaded = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOADED))	m_Stats.Downloaded = Data;

	else CProgramSet::ReadMValue(Name, Data);
}

///////////////////////////////////////////////////////////////////////////////////////
// CProgramList

void CProgramList::CountStats()
{
	m_Stats.ProcessCount = m_Stats.ProgRuleCount = m_Stats.ResRuleCount = m_Stats.FwRuleCount = m_Stats.SocketCount = 0;
	m_Stats.LastActivity = m_Stats.Upload = m_Stats.Download = m_Stats.Uploaded = m_Stats.Downloaded = 0;

	m_Stats.ProgRuleCount = m_ProgRuleIDs.count();

	m_Stats.ResRuleCount = m_ResRuleIDs.count();

	m_Stats.FwRuleCount = m_FwRuleIDs.count();

	foreach(auto pNode, m_Nodes) 
	{
		pNode->CountStats();
		const SProgramStats* pStats = pNode->GetStats();

		m_Stats.ProcessCount += pStats->ProcessCount;
		m_Stats.ProgRuleCount += pStats->ProgRuleCount;

		m_Stats.ResRuleCount += pStats->ResRuleCount;

		m_Stats.FwRuleCount += pStats->FwRuleCount;
		m_Stats.SocketCount += pStats->SocketCount;

		if(m_Stats.LastActivity < pStats->LastActivity)
			m_Stats.LastActivity = pStats->LastActivity;

		m_Stats.Upload += pStats->Upload;
		m_Stats.Download += pStats->Download;
		m_Stats.Uploaded += pStats->Uploaded;
		m_Stats.Downloaded += pStats->Downloaded;
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// CProgramGroup

QIcon CProgramGroup::DefaultIcon() const
{
	return QIcon(":/Icons/Group.png");
}

