#include "pch.h"
#include "ProgramGroup.h"
#include "../Library/Common/XVariant.h"
#include "../Library/API/PrivacyAPI.h"
#include "../PrivacyCore.h"
#include "../Processes/ProcessList.h"


///////////////////////////////////////////////////////////////////////////////////////
// CProgramSet

//void CProgramSet::MergeStats()
//{
//
//}


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
	case API_V_SOCK_LAST_ACT:	m_Stats.LastNetActivity = Data; break;

	case API_V_SOCK_LAST_ALLOW:	m_Stats.LastFwAllowed = Data; break;
	case API_V_SOCK_LAST_BLOCK:	m_Stats.LastFwBlocked = Data; break;

	case API_V_SOCK_UPLOAD:		m_Stats.Upload = Data; break;
	case API_V_SOCK_DOWNLOAD:	m_Stats.Download = Data; break;
	case API_V_SOCK_UPLOADED:	m_Stats.Uploaded = Data; break;
	case API_V_SOCK_DOWNLOADED:	m_Stats.Downloaded = Data; break;

	default: CProgramSet::ReadIValue(Index, Data);
	}
}

void CAllPrograms::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ACT))			m_Stats.LastNetActivity = Data;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ALLOW))	m_Stats.LastFwAllowed = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_BLOCK))	m_Stats.LastFwBlocked = Data;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOAD))		m_Stats.Upload = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOAD))		m_Stats.Download = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOADED))		m_Stats.Uploaded = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOADED))	m_Stats.Downloaded = Data;

	else CProgramSet::ReadMValue(Name, Data);
}

///////////////////////////////////////////////////////////////////////////////////////
// CProgramList

void CProgramList::CountStats()
{
	m_Stats.ProgramsCount = m_Stats.ServicesCount = m_Stats.AppsCount = m_Stats.GroupCount = 0;

	m_Stats.ProcessCount = m_Stats.SocketCount = 0;
	m_Stats.LastExecution = 0;
	m_Stats.LastNetActivity = m_Stats.LastFwAllowed = m_Stats.LastFwBlocked = 0;
	m_Stats.Upload = m_Stats.Download = m_Stats.Uploaded = m_Stats.Downloaded = 0;

	m_Stats.ProgRuleTotal = m_Stats.ProgRuleCount = m_ProgRuleIDs.count();

	m_Stats.ResRuleTotal = m_Stats.ResRuleCount = m_ResRuleIDs.count();

	m_Stats.FwRuleTotal = m_Stats.FwRuleCount = m_FwRuleIDs.count();

	foreach(auto pNode, m_Nodes) 
	{
		pNode->CountStats();
		const SProgramStats* pStats = pNode->GetStats();

		switch (pNode->GetType())
		{
		case EProgramType::eProgramFile:	m_Stats.ProgramsCount++; break;
		case EProgramType::eAllPrograms:	break;
		case EProgramType::eWindowsService:	m_Stats.ServicesCount++; break;
		case EProgramType::eAppPackage:		m_Stats.AppsCount++; break;
		//case EProgramType::eProgramSet:
		//case EProgramType::eProgramList:
		case EProgramType::eFilePattern:
		case EProgramType::eAppInstallation:
		case EProgramType::eProgramGroup:	m_Stats.GroupCount++; break;
		}

		m_Stats.ProgramsCount += pStats->ProgramsCount;
		m_Stats.ServicesCount += pStats->ServicesCount;
		m_Stats.AppsCount += pStats->AppsCount;
		m_Stats.GroupCount += pStats->GroupCount;

		if(m_Stats.LastExecution < pStats->LastExecution)
			m_Stats.LastExecution = pStats->LastExecution;

		m_Stats.ProcessCount += pStats->ProcessCount;
		m_Stats.ProgRuleTotal += pStats->ProgRuleTotal;

		m_Stats.ResRuleTotal += pStats->ResRuleTotal;

		m_Stats.FwRuleTotal += pStats->FwRuleTotal;
		m_Stats.SocketCount += pStats->SocketCount;

		if(m_Stats.LastNetActivity < pStats->LastNetActivity)
			m_Stats.LastNetActivity = pStats->LastNetActivity;

		if (m_Stats.LastFwAllowed < pStats->LastFwAllowed)
			m_Stats.LastFwAllowed = pStats->LastFwAllowed;
		if (m_Stats.LastFwBlocked < pStats->LastFwBlocked)
			m_Stats.LastFwBlocked = pStats->LastFwBlocked;

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

