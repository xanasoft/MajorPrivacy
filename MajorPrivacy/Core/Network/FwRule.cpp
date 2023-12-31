#include "pch.h"
#include "FwRule.h"
#include "../Service/ServiceAPI.h"
#include "../Library/Helpers/NtUtil.h"

CFwRule::CFwRule(const CProgramID& ID, QObject* parent)
	: QObject(parent)
{
    m_ProgramID = ID;

    m_BinaryPath = ID.GetFilePath();
    if(!m_BinaryPath.isEmpty()) 
        m_BinaryPath = QString::fromStdWString(NtPathToDosPath(m_BinaryPath.toStdWString()));
    m_ServiceTag = ID.GetServiceTag();
    m_AppContainerSid = ID.GetAppContainerSid();
}

QString CFwRule::GetProgram() const
{
    if (!m_AppContainerSid.isEmpty())
        return m_AppContainerSid;
    if (!m_ServiceTag.isEmpty())
        return m_ServiceTag;
    return m_BinaryPath;
}

CFwRule* CFwRule::Clone() const
{
	CFwRule* pRule = new CFwRule(m_ProgramID);

	pRule->m_BinaryPath = m_BinaryPath;
	pRule->m_ServiceTag = m_ServiceTag;
	pRule->m_AppContainerSid = m_AppContainerSid;
		
    pRule->m_Name = m_Name + tr(" (duplicate)");
	pRule->m_Grouping = m_Grouping;
	pRule->m_Description = m_Description;

	pRule->m_Enabled = m_Enabled;
	pRule->m_Action = m_Action;
	pRule->m_Direction = m_Direction;
	pRule->m_Profile = m_Profile;

	pRule->m_Protocol = m_Protocol;
	pRule->m_Interface = m_Interface;
	pRule->m_LocalAddresses = m_LocalAddresses;
	pRule->m_LocalPorts = m_LocalPorts;
	pRule->m_RemoteAddresses = m_RemoteAddresses;
	pRule->m_RemotePorts = m_RemotePorts;
	pRule->m_IcmpTypesAndCodes = m_IcmpTypesAndCodes;

	pRule->m_OsPlatformValidity = m_OsPlatformValidity;

	pRule->m_EdgeTraversal = m_EdgeTraversal;

	return pRule;
}

XVariant CFwRule::ToVariant() const
{
	XVariant FwRule;

    FwRule[SVC_API_FW_GUID] = m_Guid;
    FwRule[SVC_API_FW_INDEX] = m_Index;

    FwRule[SVC_API_ID_PROG] = m_ProgramID.ToVariant();

    FwRule[SVC_API_ID_FILE_PATH] = m_BinaryPath;
    FwRule[SVC_API_ID_SVC_TAG] = m_ServiceTag;
    FwRule[SVC_API_ID_APP_SID] = m_AppContainerSid;
        
    FwRule[SVC_API_FW_NAME] = m_Name;
    FwRule[SVC_API_FW_GROUP] = m_Grouping;
    FwRule[SVC_API_FW_DESCRIPTION] = m_Description;

    FwRule[SVC_API_FW_ENABLED] = m_Enabled;
    FwRule[SVC_API_FW_ACTION] = m_Action;
    FwRule[SVC_API_FW_DIRECTION] = m_Direction;
    FwRule[SVC_API_FW_PROFILE] = m_Profile;

    FwRule[SVC_API_FW_PROTOCOL] = m_Protocol;
    FwRule[SVC_API_FW_INTERFACE] = m_Interface;
    FwRule[SVC_API_FW_LOCAL_ADDR] = m_LocalAddresses;
    FwRule[SVC_API_FW_LOCAL_PORT] = m_LocalPorts;
    FwRule[SVC_API_FW_REMOTE_ADDR] = m_RemoteAddresses;
    FwRule[SVC_API_FW_REMOTE_PORT] = m_RemotePorts;
    FwRule[SVC_API_FW_ICMP_OPT] = m_IcmpTypesAndCodes;

    FwRule[SVC_API_FW_OS_P] = m_OsPlatformValidity;

    FwRule[SVC_API_FW_EDGE_T] = m_EdgeTraversal;

	return FwRule;
}

void CFwRule::FromVariant(const class XVariant& FwRule)
{
    m_Guid = FwRule[SVC_API_FW_GUID].AsQStr();
    m_Index = FwRule[SVC_API_FW_INDEX];

    // m_ProgramID; // set in constructor

    m_BinaryPath = FwRule[SVC_API_ID_FILE_PATH].AsQStr();
    m_ServiceTag = FwRule[SVC_API_ID_SVC_TAG].AsQStr();
    m_AppContainerSid = FwRule[SVC_API_ID_APP_SID].AsQStr();
     
    m_Name = FwRule[SVC_API_FW_NAME].AsQStr();
    m_Grouping = FwRule[SVC_API_FW_GROUP].AsQStr();
    m_Description = FwRule[SVC_API_FW_DESCRIPTION].AsQStr();

    m_Enabled = FwRule[SVC_API_FW_ENABLED];
    m_Action = FwRule[SVC_API_FW_ACTION].AsQStr();
    m_Direction = FwRule[SVC_API_FW_DIRECTION].AsQStr();
    m_Profile = FwRule[SVC_API_FW_PROFILE].AsQList();

    m_Protocol = FwRule[SVC_API_FW_PROTOCOL];
    m_Interface = FwRule[SVC_API_FW_INTERFACE].AsQList();
    m_LocalAddresses = FwRule[SVC_API_FW_LOCAL_ADDR].AsQList();
    m_LocalPorts = FwRule[SVC_API_FW_LOCAL_PORT].AsQList();
    m_RemoteAddresses = FwRule[SVC_API_FW_REMOTE_ADDR].AsQList();
    m_RemotePorts = FwRule[SVC_API_FW_REMOTE_PORT].AsQList();
    m_IcmpTypesAndCodes = FwRule[SVC_API_FW_ICMP_OPT].AsQList();

    m_OsPlatformValidity = FwRule[SVC_API_FW_OS_P].AsQList();

    m_EdgeTraversal = FwRule[SVC_API_FW_EDGE_T];
}