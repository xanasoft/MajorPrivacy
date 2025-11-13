#include "pch.h"
#include "NetworkManager.h"
#include "../PrivacyCore.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../../MiscHelpers/Common/Common.h"

CNetworkManager::CNetworkManager(QObject *parent) 
    : QObject(parent)
{
    
}

bool CNetworkManager::UpdateAllFwRules(bool bReLoad)
{
	auto Ret = theCore->GetAllFwRules(bReLoad);
	if (Ret.IsError())
		return false;

	QtVariant& Rules = Ret.GetValue();

	QMap<QFlexGuid, CFwRulePtr> OldRules = m_FwRules;

	for (int i = 0; i < Rules.Count(); i++)
	{
		const QtVariant& Rule = Rules[i];

		QFlexGuid Guid;
		Guid.FromVariant(Rule[API_V_GUID]);

		CProgramID ID;
		ID.FromVariant(Rule[API_V_ID]);

		CFwRulePtr pRule = OldRules.value(Guid);
		if (pRule) {
			if(ID == pRule->GetProgramID())
				OldRules.remove(Guid);
			else // on ID change compeltely re add the rule as if it would be new
			{
				RemoveFwRule(pRule);
				pRule.clear();
			}
		}

		bool bAdd = false;
		if (!pRule) {
			pRule = CFwRulePtr(new CFwRule(ID));
			bAdd = true;
		} 

		pRule->FromVariant(Rule);

		if(bAdd)
			AddFwRule(pRule);
	}

	foreach(const QFlexGuid& Guid, OldRules.keys())
		RemoveFwRule(OldRules.take(Guid));

	return true;
}

bool CNetworkManager::UpdateFwRule(const QFlexGuid& Guid)
{
	auto Ret = theCore->GetFwRule(Guid);
	if (Ret.IsError())
		return false;

	QtVariant& Rule = Ret.GetValue();

	QString RuleID = Rule[API_V_GUID].AsQStr();

	CProgramID ID;
	ID.FromVariant(Rule[API_V_ID]);

	CFwRulePtr pRule = m_FwRules.value(RuleID);
	if (pRule) {
		if(ID != pRule->GetProgramID()) // on ID change compeltely re add the rule as if it would be new
		{
			RemoveFwRule(pRule);
			pRule.clear();
		}
	}

	bool bAdd = false;
	if (!pRule) {
		pRule = CFwRulePtr(new CFwRule(ID));
		bAdd = true;
	} 

	pRule->FromVariant(Rule);

	if(bAdd)
		AddFwRule(pRule);

	return true;
}

void CNetworkManager::RemoveFwRule(const QFlexGuid& Guid)
{
	CFwRulePtr pFwRule = m_FwRules.value(Guid);
	if (pFwRule)
		RemoveFwRule(pFwRule);
}

void CNetworkManager::AddFwRule(const CFwRulePtr& pFwRule)
{
	m_FwRules.insert(pFwRule->GetGuid(), pFwRule);

	//const CProgramID& ProgID = pFwRule->GetProgramID();

	//if (!ProgID.GetFilePath().isEmpty())
	//	m_FileRules.insert(ProgID.GetFilePath(), pFwRule);
	//if (!ProgID.GetServiceTag().isEmpty())
	//	m_SvcRules.insert(ProgID.GetServiceTag(), pFwRule);
	//if (!ProgID.GetAppContainerSid().isEmpty())
	//	m_AppRules.insert(ProgID.GetAppContainerSid(), pFwRule);
}

void CNetworkManager::RemoveFwRule(const CFwRulePtr& pFwRule)
{
	m_FwRules.remove(pFwRule->GetGuid());

	//const CProgramID& ProgID = pFwRule->GetProgramID();

	//if (!ProgID.GetFilePath().isEmpty())
	//	m_FileRules.remove(ProgID.GetFilePath(), pFwRule);
	//if (!ProgID.GetServiceTag().isEmpty())
	//	m_SvcRules.remove(ProgID.GetServiceTag(), pFwRule);
	//if (!ProgID.GetAppContainerSid().isEmpty())
	//	m_AppRules.remove(ProgID.GetAppContainerSid(), pFwRule);
}

QSet<QFlexGuid> CNetworkManager::GetFwRuleIDs() const 
{	
	return ListToSet(m_FwRules.keys()); 
}

//QList<CFwRulePtr> CNetworkManager::GetFwRulesFor(const QList<const class CProgramItem*>& Nodes)
//{
//	return QList<CFwRulePtr>();
//}

QList<CFwRulePtr> CNetworkManager::GetFwRules(const QSet<QFlexGuid>& FwRuleIDs)
{
	QList<CFwRulePtr> List;
	foreach(const QFlexGuid& Guid, FwRuleIDs) {
		CFwRulePtr pFwRule = m_FwRules.value(Guid);
		if (pFwRule)List.append(pFwRule);
	}
	return List;
}

CFwRulePtr CNetworkManager::GetFwRule(const QFlexGuid& Guid)
{
	CFwRulePtr pFwRule = m_FwRules.value(Guid);
	if (!pFwRule && UpdateFwRule(Guid))
		pFwRule = m_FwRules.value(Guid);
	return pFwRule;
}

RESULT(QFlexGuid) CNetworkManager::SetFwRule(const CFwRulePtr& pRule)
{
	SVarWriteOpt Opts;
	Opts.Flags = SVarWriteOpt::eTextGuids;

	return theCore->SetFwRule(pRule->ToVariant(Opts));
}

//RESULT(CFwRulePtr) CNetworkManager::GetFwRule(const QFlexGuid& Guid)
//{
//	auto Ret = theCore->GetFwRule(Guid);
//	if (Ret.IsError())
//		return Ret;
//
//	QtVariant& Rule = Ret.GetValue();
//
//	CProgramID ID;
//	ID.FromVariant(Rule[API_V_ID]);
//
//	CFwRulePtr pRule = CFwRulePtr(new CFwRule(ID));
//	pRule->FromVariant(Rule);
//
//	RETURN(pRule);
//}

STATUS CNetworkManager::DelFwRule(const CFwRulePtr& pRule)
{
	return theCore->DelFwRule(pRule->GetGuid());
}

STATUS CNetworkManager::CreateFwRule(const QFlexGuid& Id, const QString& Name, const QString& Description, const CProgramID& ProgramID, EFwActions Action, EFwDirections Direction, EFwKnownProtocols Protocol, EFwProfiles Profile, const QStringList& LPort, const QStringList& RPort, const QStringList& RAddr)
{
	CFwRulePtr pRule = CFwRulePtr(new CFwRule(ProgramID));
	pRule->m_Guid = Id;
	pRule->m_Name = Name;
	pRule->m_Description = Description;
	pRule->m_Action = Action;
	pRule->m_Direction = Direction;
	pRule->m_Protocol = Protocol;
	pRule->m_Profile = (int)Profile;
	pRule->m_LocalPorts = LPort;
	pRule->m_RemotePorts = RPort;
	pRule->m_RemoteAddresses = RAddr;

	pRule->SetApproved();
	pRule->SetSource(EFwRuleSource::eMajorPrivacy);
	return SetFwRule(pRule);
}

QList<STATUS> CNetworkManager::CreateRecommendedFwRules()
{
	CProgramID System = CProgramID::FromPath(theCore->NormalizePath("\\SystemRoot\\System32\\ntoskrnl.exe", true));

	QList<STATUS> Results;

	Results << CreateFwRule(QFlexGuid("MajorPrivacy-SSDP-In"), tr("Network Discovery (SSDP-In) - Default MajorPrivacy Rule"), tr(""), 
		CProgramID::FromService(theCore->NormalizePath("\\SystemRoot\\System32\\svchost.exe", true), "SSDPSRV"), 
		EFwActions::Allow, EFwDirections::Inbound, EFwKnownProtocols::UDP, EFwProfiles::PrivateAndDomain, 
		QStringList() << "1900", QStringList(), QStringList() << "LocalSubnet");
	Results << CreateFwRule(QFlexGuid("MajorPrivacy-SSDP-Out"), tr("Network Discovery (SSDP-Out) - Default MajorPrivacy Rule"), tr(""), 
		CProgramID::FromService(theCore->NormalizePath("\\SystemRoot\\System32\\svchost.exe", true), "SSDPSRV"), 
		EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::UDP, EFwProfiles::PrivateAndDomain, 
		QStringList(), QStringList() << "1900", QStringList() << "LocalSubnet");

	Results << CreateFwRule(QFlexGuid("MajorPrivacy-NB-Name-In"), tr("Network Discovery (NB-Name-In) - Default MajorPrivacy Rule"), tr(""), 
		System, EFwActions::Allow, EFwDirections::Inbound, EFwKnownProtocols::UDP, EFwProfiles::PrivateAndDomain, 
		QStringList() << "137", QStringList(), QStringList() << "LocalSubnet");
	Results << CreateFwRule(QFlexGuid("MajorPrivacy-NB-Name-Out"), tr("Network Discovery (NB-Name-Out) - Default MajorPrivacy Rule"), tr(""), 
		System, EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::UDP, EFwProfiles::PrivateAndDomain, 
		QStringList(), QStringList() << "137", QStringList() << "LocalSubnet");
	Results << CreateFwRule(QFlexGuid("MajorPrivacy-NB-Session-Out"), tr("File and Printer Sharing (NB-Session-Out) - Default MajorPrivacy Rule"), tr(""), 
		System, EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::TCP, EFwProfiles::PrivateAndDomain, 
		QStringList(), QStringList() << "139", QStringList() << "LocalSubnet");

	Results << CreateFwRule(QFlexGuid("MajorPrivacy-SMB-In"), tr("File and Printer Sharing (SMB-In) - Default MajorPrivacy Rule"), tr(""), 
		System, EFwActions::Allow, EFwDirections::Inbound, EFwKnownProtocols::TCP, EFwProfiles::PrivateAndDomain, 
		QStringList() << "445", QStringList(), QStringList() << "LocalSubnet");
	Results << CreateFwRule(QFlexGuid("MajorPrivacy-SMB-Out"), tr("File and Printer Sharing (SMB-Out) - Default MajorPrivacy Rule"), tr(""), 
		System, EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::TCP, EFwProfiles::PrivateAndDomain, 
		QStringList(), QStringList() << "445", QStringList() << "LocalSubnet");

	Results << CreateFwRule(QFlexGuid("MajorPrivacy-Spooler-Out"), tr("File and Printer Sharing (Spooler-Out) - Default MajorPrivacy Rule"), tr(""), 
		CProgramID::FromPath(theCore->NormalizePath("\\SystemRoot\\System32\\spoolsv.exe", true)), 
		EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::Any, EFwProfiles::PrivateAndDomain, 
		QStringList(), QStringList(), QStringList() << "LocalSubnet");

	Results << CreateFwRule(QFlexGuid("MajorPrivacy-wuauserv"), tr("Windows Update - Default MajorPrivacy Rule"), tr(""), 
		CProgramID::FromPath(theCore->NormalizePath("\\SystemRoot\\System32\\svchost.exe", true)), 
		EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::TCP, EFwProfiles::All, 
		QStringList(), QStringList() << "80" << "443");
	Results << CreateFwRule(QFlexGuid("MajorPrivacy-W32Time"), tr("Windows Time Service - Default MajorPrivacy Rule"), tr(""), 
		CProgramID::FromService(theCore->NormalizePath("\\SystemRoot\\System32\\svchost.exe", true), "W32Time"),
		EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::UDP, EFwProfiles::All, 
		QStringList(), QStringList() << "123");
	Results << CreateFwRule(QFlexGuid("MajorPrivacy-wwahost"), tr("Microsoft WWA Host - Default MajorPrivacy Rule"), tr(""), 
		CProgramID::FromPath(theCore->NormalizePath("\\SystemRoot\\System32\\wwahost.exe", true)), 
		EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::TCP, EFwProfiles::All, 
		QStringList(), QStringList() << "443");
	
	Results << CreateFwRule(QFlexGuid("MajorPrivacy-DHCP-Out"), tr("Core Networking - Dynamic Host Configuration Protocol (DHCP-Out) - Default MajorPrivacy Rule"), tr(""), 
		CProgramID::FromService(theCore->NormalizePath("\\SystemRoot\\System32\\svchost.exe", true), "Dhcp"), 
		EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::UDP, EFwProfiles::All, 
		QStringList() << "68", QStringList() << "67");
	
	Results << CreateFwRule(QFlexGuid("MajorPrivacy-DNS-UDP-Out"), tr("Core Networking - Domain Name System (DNS-UDP-Out) - Default MajorPrivacy Rule"), tr(""), 
		CProgramID::FromService(theCore->NormalizePath("\\SystemRoot\\System32\\svchost.exe", true), "Dnscache"), 
		EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::UDP, EFwProfiles::All, 
		QStringList(), QStringList() << "53");
	Results << CreateFwRule(QFlexGuid("MajorPrivacy-DNS-TCP-Out"), tr("Core Networking - Domain Name System (DNS-TCP-Out) - Default MajorPrivacy Rule"), tr(""),
		CProgramID::FromService(theCore->NormalizePath("\\SystemRoot\\System32\\svchost.exe", true), "Dnscache"),
		EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::TCP, EFwProfiles::All,
		QStringList(), QStringList() << "53");

	Results << CreateFwRule(QFlexGuid("MajorPrivacy-ICMPv6-In"), tr("Internet Control Message Protocol (ICMPv6-In) - Default MajorPrivacy Rule"), tr(""), 
		System, EFwActions::Allow, EFwDirections::Inbound, EFwKnownProtocols::ICMPv6, EFwProfiles::All);
	Results << CreateFwRule(QFlexGuid("MajorPrivacy-ICMPv4-In"), tr("Internet Control Message Protocol (ICMPv4-In) - Default MajorPrivacy Rule"), tr(""), 
		System, EFwActions::Allow, EFwDirections::Inbound, EFwKnownProtocols::ICMP, EFwProfiles::All);
	Results << CreateFwRule(QFlexGuid("MajorPrivacy-ICMPv6-Out"), tr("Internet Control Message Protocol (ICMPv6-Out) - Default MajorPrivacy Rule"), tr(""), 
		System, EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::ICMPv6, EFwProfiles::All);
	Results << CreateFwRule(QFlexGuid("MajorPrivacy-ICMPv4-Out"), tr("Internet Control Message Protocol (ICMPv4-Out) - Default MajorPrivacy Rule"), tr(""), 
		System, EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::ICMP, EFwProfiles::All);



	Results << CreateFwRule(QFlexGuid("MajorPrivacy-GUI"), tr("MajorPrivacy Network Access (GUI) - Default MajorPrivacy Rule"), tr(""), 
		CProgramID::FromPath(theCore->GetAppDir() + QString(("\\MajorPrivacy.exe"))), 
		EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::Any, EFwProfiles::All);

	Results << CreateFwRule(QFlexGuid("MajorPrivacy-PrivacyAgent"), tr("MajorPrivacy Service Network Access (PrivacyAgent) - Default MajorPrivacy Rule"), tr(""),
		CProgramID::FromPath(theCore->GetAppDir() + QString(("\\" API_SERVICE_NAME ".exe"))), 
		EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::Any, EFwProfiles::All);

	Results << CreateFwRule(QFlexGuid("MajorPrivacy-DNS-Filter-Out"), tr("MajorPrivacy Local DNS Filter (DNS-Filter-Out) - Default MajorPrivacy Rule"), tr(""),
		CProgramID::FromPath(theCore->GetAppDir() + QString(("\\" API_SERVICE_NAME ".exe"))), 
		EFwActions::Allow, EFwDirections::Outbound, EFwKnownProtocols::UDP, EFwProfiles::All,
		QStringList(), QStringList() << "53");
	Results << CreateFwRule(QFlexGuid("MajorPrivacy-DNS-Filter-In"), tr("MajorPrivacy Local DNS Filter (DNS-Filter-In) - Default MajorPrivacy Rule"), tr(""),
		CProgramID::FromPath(theCore->GetAppDir() + QString(("\\" API_SERVICE_NAME ".exe"))), 
		EFwActions::Allow, EFwDirections::Inbound, EFwKnownProtocols::UDP, EFwProfiles::PrivateAndDomain,
		QStringList() << "53");

	return Results;
}

bool CNetworkManager::UpdateAllDnsRules()
{
	auto Ret = theCore->GetAllDnsRules();
	if (Ret.IsError())
		return false;

	QtVariant& Rules = Ret.GetValue();

	QMap<QFlexGuid, CDnsRulePtr> OldRules = m_DnsRules;

	for (int i = 0; i < Rules.Count(); i++)
	{
		const QtVariant& Rule = Rules[i];

		QFlexGuid Guid;
		Guid.FromVariant(Rule[API_V_GUID]);

		//CProgramID ID;
		//ID.FromVariant(Rule[API_V_ID]);

		CDnsRulePtr pRule = OldRules.value(Guid);
		if (pRule) {
			//if(ID == pRule->GetProgramID())
				OldRules.remove(Guid);
			//else // on ID change compeltely re add the rule as if it would be new
			//{
			//	m_DnsRules.remove(pRule->GetGuid());
			//	pRule.clear();
			//}
		}

		bool bAdd = false;
		if (!pRule) {
			pRule = CDnsRulePtr(new CDnsRule());
			bAdd = true;
		} 

		pRule->FromVariant(Rule);

		if(bAdd)
			m_DnsRules.insert(pRule->GetGuid(), pRule);
	}

	foreach(const QFlexGuid& Guid, OldRules.keys())
		m_DnsRules.remove(Guid);

	return true;
}


bool CNetworkManager::UpdateDnsRule(const QFlexGuid& Guid)
{
	auto Ret = theCore->GetDnsRule(Guid);
	if (Ret.IsError())
		return false;

	QtVariant& Rule = Ret.GetValue();

	QString RuleID = Rule[API_V_GUID].AsQStr();

	CDnsRulePtr pRule = m_DnsRules.value(RuleID);
	//if (pRule) {
	//	if(ID != pRule->GetProgramID()) // on ID change compeltely re add the rule as if it would be new
	//	{
	//		RemoveFwRule(pRule);
	//		pRule.clear();
	//	}
	//}

	bool bAdd = false;
	if (!pRule) {
		pRule = CDnsRulePtr(new CDnsRule());
		bAdd = true;
	} 

	pRule->FromVariant(Rule);

	if(bAdd)
		m_DnsRules.insert(pRule->GetGuid(), pRule);

	return true;
}

void CNetworkManager::RemoveDnsRule(const QFlexGuid& Guid)
{
	m_DnsRules.remove(Guid);
}

STATUS CNetworkManager::SetDnsRule(const CDnsRulePtr& pRule)
{
	SVarWriteOpt Opts;
	Opts.Flags = SVarWriteOpt::eTextGuids;

	return theCore->SetDnsRule(pRule->ToVariant(Opts));
}

STATUS CNetworkManager::DelDnsRule(const CDnsRulePtr& pRule)
{
	m_DnsRules.remove(pRule->GetGuid());
	return theCore->DelDnsRule(pRule->GetGuid());
}

void CNetworkManager::UpdateDnsCache()
{
	auto Ret = theCore->GetDnsCache();
	if (Ret.IsError())
		return;

	QtVariant& Cache = Ret.GetValue();

	QMap<quint64, CDnsCacheEntryPtr> OldCache = m_DnsCache;

	QtVariantReader(Cache).ReadRawList([&](const FW::CVariant& vData) {
		const QtVariant& Data = *(QtVariant*)&vData;

		quint64 CacheRef = Data[API_V_DNS_CACHE_REF];

		CDnsCacheEntryPtr pEntry = OldCache.take(CacheRef);
		if (!pEntry) {
			pEntry = CDnsCacheEntryPtr(new CDnsCacheEntry());
			m_DnsCache.insert(CacheRef, pEntry);
		}

		pEntry->FromVariant(Data);
	});

	foreach(const quint64 & CacheRef, OldCache.keys())
		m_DnsCache.remove(CacheRef);
}