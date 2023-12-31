#include "pch.h"
#include "ProgramGroup.h"
#include "../Library/Common/XVariant.h"
#include "../Service/ServiceAPI.h"
#include "../PrivacyCore.h"
#include "../ProcessList.h"

///////////////////////////////////////////////////////////////////////////////////////
// CAllProgram

void CAllProgram::CountStats()
{
	m_Stats.ProcessCount = theCore->Processes()->GetCount();
	m_Stats.SocketCount = theCore->Processes()->GetSocketCount();
}

void CAllProgram::ReadValue(const SVarName& Name, const XVariant& Data)
{
		 if (VAR_TEST_NAME(Name, SVC_API_SOCK_LAST_ACT))	m_Stats.LastActivity = Data;

	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_UPLOAD))		m_Stats.Upload = Data;
	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_DOWNLOAD))	m_Stats.Download = Data;
	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_UPLOADED))	m_Stats.Uploaded = Data;
	else if (VAR_TEST_NAME(Name, SVC_API_SOCK_DOWNLOADED))	m_Stats.Downloaded = Data;

	else CProgramSet::ReadValue(Name, Data);
}

///////////////////////////////////////////////////////////////////////////////////////
// CProgramList

void CProgramList::CountStats()
{
	m_Stats.ProcessCount = m_Stats.FwRuleCount = m_Stats.SocketCount = 0;
	m_Stats.LastActivity = m_Stats.Upload = m_Stats.Download = m_Stats.Uploaded = m_Stats.Downloaded = 0;

	m_Stats.FwRuleCount = m_FwRuleIDs.count();

	foreach(auto pNode, m_Nodes) 
	{
		pNode->CountStats();
		const SProgramStats* pStats = pNode->GetStats();

		m_Stats.ProcessCount += pStats->ProcessCount;
	
		m_Stats.FwRuleCount += pStats->FwRuleCount;
		m_Stats.SocketCount += pStats->SocketCount;

		m_Stats.LastActivity += pStats->LastActivity;

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

