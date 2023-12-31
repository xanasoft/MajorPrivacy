#include "pch.h"
#include "Firewall.h"
#include "WindowsFirewall.h"
#include "../../ServiceCore.h"
//#include "../Library/API/AlpcPortServer.h"
#include "../Library/API/PipeServer.h"
#include "WindowsFwLog.h"
#include "WindowsFwGuard.h"
#include "ServiceAPI.h"
#include "../NetIsolator.h"
#include "../SocketList.h"
#include "../../Programs/ProgramManager.h"
#include "../../Processes/ProcessList.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/RegUtil.h"
#include "../NetLogEntry.h"
#include "../Library/Common/UIntX.h"

EFwProfiles CFirewall__Profiles[] = { EFwProfiles::Private, EFwProfiles::Public, EFwProfiles::Domain };

CFirewall::CFirewall()
{
	m_pLog = new CWindowsFwLog();
	m_pLog->RegisterHandler(&CFirewall::OnFwLogEvent, this);

	m_pGuard = new CWindowsFwGuard();
	m_pGuard->RegisterHandler(&CFirewall::OnFwGuardEvent, this);

	m_RegWatcher.AddKey(L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\DomainProfile");
	m_RegWatcher.AddKey(L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\PublicProfile");
	m_RegWatcher.AddKey(L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\StandardProfile");
	//static const std::wstring FwRuleKey = L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\FirewallRules";
	//m_RegWatcher.AddKey(FwRuleKey);

	m_RegWatcher.RegisterHandler([&](const std::wstring& Key){
		//if(Key == FwRuleKey)
		//	m_UpdateAllRules = false;
		//else
			m_UpdateDefaultProfiles = true;
	});

	m_RegWatcher.Start();
}

CFirewall::~CFirewall()
{
	m_RegWatcher.Stop();

	delete m_pLog;
	delete m_pGuard;
}

STATUS CFirewall::Init()
{
	m_pLog->Start();
	m_pGuard->Start();

	LoadRules();
	LoadDefaults();
	
	if(svcCore->Config()->GetBool("Service", "LoadWindowsFirewallLog", false))
		LoadFwLog();

	return OK;
}

void CFirewall::Update()
{
	if (m_UpdateDefaultProfiles)
		LoadDefaults();

	if (m_UpdateAllRules)
		LoadRules();
}

STATUS CFirewall::LoadRules()
{
	auto ret = CWindowsFirewall::Instance()->LoadRules();
	if(ret.IsError())
		return ret;

	auto pRuleList = ret.GetValue();

	std::unique_lock Lock(m_RulesMutex);

	std::map<std::wstring, CFirewallRulePtr> OldFwRules = m_FwRules;

	for (auto pRule : *pRuleList) 
	{
		CFirewallRulePtr pFwRule;
		auto I = OldFwRules.find(pRule->guid);
		if (I != OldFwRules.end())
		{
			pFwRule = I->second;
			OldFwRules.erase(I);
			if (!MatchRuleID(pRule, pFwRule))
			{
				RemoveRuleUnsafe(pFwRule);
				pFwRule.reset();
			}
		}

		if (!pFwRule) 
		{
			pFwRule = std::make_shared<CFirewallRule>(pRule);
			AddRuleUnsafe(pFwRule);
		}
		else
			pFwRule->Update(pRule);
	}

	for (auto I : OldFwRules)
		RemoveRuleUnsafe(I.second);

	// todo add an other watcher not reliable on security events by monitoring the registry key

	m_UpdateAllRules = false;
	return OK;
}

void CFirewall::LoadDefaults()
{
	for (int i = 0; i < ARRAYSIZE(CFirewall__Profiles); i++) 
	{
		m_BlockAllInbound[i] = CWindowsFirewall::Instance()->GetBlockAllInboundTraffic(CFirewall__Profiles[i]);
		m_DefaultInboundAction[i] = CWindowsFirewall::Instance()->GetDefaultInboundAction(CFirewall__Profiles[i]);
		m_DefaultoutboundAction[i] = CWindowsFirewall::Instance()->GetDefaultOutboundAction(CFirewall__Profiles[i]);
	}

	m_UpdateDefaultProfiles = false;
}

void CFirewall::AddRuleUnsafe(const CFirewallRulePtr& pFwRule)
{
	m_FwRules.insert(std::make_pair(pFwRule->GetGuid(), pFwRule));

	const CProgramID& ProgID = pFwRule->GetProgramID();

	if (!ProgID.GetFilePath().empty())
		m_FileRules.insert(std::make_pair(ProgID.GetFilePath(), pFwRule));
	if (!ProgID.GetServiceTag().empty())
		m_SvcRules.insert(std::make_pair(ProgID.GetServiceTag(), pFwRule));
	if (!ProgID.GetAppContainerSid().empty())
		m_AppRules.insert(std::make_pair(ProgID.GetAppContainerSid(), pFwRule));

	//if (ProgID.GetFilePath().empty() && ProgID.GetServiceTag().empty() && ProgID.GetAppContainerSid().empty())
	//	m_AllProgramsRules.insert(std::make_pair(pFwRule->GetGuid(), pFwRule));

	svcCore->ProgramManager()->AddFwRule(pFwRule);
}

void CFirewall::RemoveRuleUnsafe(const CFirewallRulePtr& pFwRule)
{
	m_FwRules.erase(pFwRule->GetGuid());

	const CProgramID& ProgID = pFwRule->GetProgramID();

	if (!ProgID.GetFilePath().empty())
		mmap_erase(m_FileRules, ProgID.GetFilePath(), pFwRule);
	if (!ProgID.GetServiceTag().empty())
		mmap_erase(m_SvcRules, ProgID.GetServiceTag(), pFwRule);
	if (!ProgID.GetAppContainerSid().empty())
		mmap_erase(m_AppRules, ProgID.GetAppContainerSid(), pFwRule);

	//if (ProgID.GetFilePath().empty() && ProgID.GetServiceTag().empty() && ProgID.GetAppContainerSid().empty())
	//	m_AllProgramsRules.erase(pFwRule->GetGuid())

	svcCore->ProgramManager()->RemoveFwRule(pFwRule);
}

std::map<std::wstring, CFirewallRulePtr> CFirewall::FindRules(const CProgramID& ID)
{
	std::unique_lock Lock(m_RulesMutex);

	std::map<std::wstring, CFirewallRulePtr> Found;

	switch (ID.GetType())
	{
	case CProgramID::eFile:
		for (auto I = m_FileRules.find(ID.GetFilePath()); I != m_FileRules.end() && I->first == ID.GetFilePath(); ++I) {
			Found.insert(std::make_pair(I->second->GetGuid(), I->second));
		}
		break;
	case CProgramID::eService:
		for (auto I = m_SvcRules.find(ID.GetServiceTag()); I != m_SvcRules.end() && I->first == ID.GetServiceTag(); ++I) {
			if (!ID.GetFilePath().empty() && I->second->GetProgramID().GetFilePath() != ID.GetFilePath())
				continue;
			Found.insert(std::make_pair(I->second->GetGuid(), I->second));
		}
		break;
	case CProgramID::eApp:
		for (auto I = m_AppRules.find(ID.GetAppContainerSid()); I != m_AppRules.end() && I->first == ID.GetAppContainerSid(); ++I) {
			Found.insert(std::make_pair(I->second->GetGuid(), I->second));
		}
		break;
	case CProgramID::eAll:
		//return m_AllProgramsRules;
		return m_FwRules;
	}
	return Found;
}

std::map<std::wstring, CFirewallRulePtr> CFirewall::GetAllRules()
{
	std::unique_lock Lock(m_RulesMutex);
	return m_FwRules;
}

STATUS CFirewall::SetRule(const CFirewallRulePtr& pFwRule)
{
	std::shared_ptr<struct SWindowsFwRule> pData = pFwRule->GetData();

	if (pData->Direction == EFwDirections::Bidirectiona)
	//{
	//	if(!pData->guid.empty()) // Bidirectiona is only valid when creating a new rule(s)
			return ERR(STATUS_INVALID_PARAMETER);
	//
	//	std::shared_ptr<struct SWindowsFwRule> pData2 = std::make_shared<struct SWindowsFwRule>(*pData); // copy the ruledata
	//	
	//	pData2->Direction = EFwDirections::Inbound;
	//	pData->Direction = EFwDirections::Outbound;
	//
	//	STATUS Status = CWindowsFirewall::Instance()->UpdateRule(pData2); // this may set the GUID member
	//	if (!Status)
	//		return Status;
	//
	//	UpdateFWRule(pData2, pData2->guid);
	//}

	STATUS Status = CWindowsFirewall::Instance()->UpdateRule(pData); // this may set the GUID member
	if (!Status)
		return Status;

	UpdateFWRule(pData, pData->guid);

	return OK;
}

CFirewallRulePtr CFirewall::GetRule(const std::wstring& RuleId)
{
	CFirewallRulePtr pFwRule;
	auto F = m_FwRules.find(RuleId);
	if (F != m_FwRules.end())
		pFwRule = F->second;
	return pFwRule;
}

STATUS CFirewall::DelRule(const std::wstring& RuleId)
{
	STATUS Status = CWindowsFirewall::Instance()->RemoveRule(RuleId);
	if (!Status)
		return Status;

	UpdateFWRule(NULL, RuleId);

	return OK;
}

bool CFirewall::MatchRuleID(const SWindowsFwRulePtr& pRule, const CFirewallRulePtr& pFwRule)
{
	if (!pRule || !pFwRule)
		return false;

	const CProgramID& ProgID = pFwRule->GetProgramID();

	// match fails when anythign changes
	if (NormalizeFilePath(pRule->BinaryPath) != ProgID.GetFilePath())
		return false;
	if (MkLower(pRule->ServiceTag) != ProgID.GetServiceTag())
		return false;
	if (MkLower(pRule->AppContainerSid) != ProgID.GetAppContainerSid())
		return false;

	return true;
}

void CFirewall::UpdateFWRule(const std::shared_ptr<struct SWindowsFwRule>& pRule, const std::wstring& RuleId)
{
	std::unique_lock Lock(m_RulesMutex);

	CFirewallRulePtr pFwRule;
	if (!RuleId.empty()) {
		auto F = m_FwRules.find(RuleId);
		if (F != m_FwRules.end())
			pFwRule = F->second;
	}

	if (MatchRuleID(pRule, pFwRule)) // always false on remove, when pRule == NULL
		pFwRule->Update(pRule);
	else {  // when the rule changes the programs it applyes to we remove it and tehn add it
		if (pFwRule) RemoveRuleUnsafe(pFwRule);
		if (pRule) AddRuleUnsafe(std::make_shared<CFirewallRule>(pRule));
	}
}

uint32 CFirewall::OnFwGuardEvent(const struct SWinFwGuardEvent* pEvent)
{
	SWindowsFwRulePtr pRule;
	if (pEvent->Type != SWinFwGuardEvent::EType::Removed) {
		std::vector<std::wstring> RuleIds;
		RuleIds.push_back(pEvent->RuleId);
		auto ret = CWindowsFirewall::Instance()->LoadRules(RuleIds);
		if(ret.GetValue()) pRule = (*ret.GetValue())[pEvent->RuleId]; // CWindowsFirewall::Instance()->GetRule(pEvent->RuleId);
	}

	UpdateFWRule(pRule, pEvent->RuleId);

	CVariant vEvent;
	vEvent[SVC_API_FW_GUID] = pEvent->RuleId;
	vEvent[SVC_API_FW_NAME] = pEvent->RuleName;
	switch (pEvent->Type)
	{
	case SWinFwGuardEvent::EType::Added:	vEvent[SVC_API_FW_CHANGE] = SVC_API_FW_ADDED;   break;
	case SWinFwGuardEvent::EType::Changed:	vEvent[SVC_API_FW_CHANGE] = SVC_API_FW_CHANGED; break;
	case SWinFwGuardEvent::EType::Removed:	vEvent[SVC_API_FW_CHANGE] = SVC_API_FW_REMOVED; break;
	}

	CBuffer Event;
	vEvent.ToPacket(&Event);
	svcCore->UserPipe()->BroadcastMessage(SVC_API_EVENT_RULE_CHANGED, &Event);

	return 0;
}

uint32 CFirewall::OnFwLogEvent(const SWinFwLogEvent* pEvent)
{
	CSocketPtr pSocket = svcCore->NetIsolator()->SocketList()->OnFwLogEvent(pEvent);

	ProcessFwEvent(pEvent, pSocket.get());

	return 0;
}

void CFirewall::ProcessFwEvent(const struct SWinFwLogEvent* pEvent, class CSocket* pSocket)
{
	CProcessPtr pProcess;
	CProgramFilePtr pProgram;
	CAppPackagePtr pApp;
	std::list<CWindowsServicePtr> Svcs;
	CHostNamePtr pRemoteHostName;

	SAdapterInfoPtr pNicInfo = svcCore->NetIsolator()->GetAdapterInfoByIP(pEvent->LocalAddress);

	if (_wcsicmp(pEvent->ProcessFileName.c_str(), L"System") == 0)
	{
		pProcess = svcCore->ProcessList()->GetProcess(NT_OS_KERNEL_PID, true);
		pProgram = svcCore->ProgramManager()->GetNtOsKernel();
	}
	else if(pSocket != (CSocket*)-1)
	{
		pProcess = pSocket ? pSocket->GetProcess() : svcCore->ProcessList()->GetProcess(pEvent->ProcessId, true); // todo socket should always have a process
		if (pProcess)
		{
			pProgram = pProcess->GetProgram();
			
			std::wstring AppSid = pProcess->GetAppContainerSid();
			if (!AppSid.empty())
				pApp = svcCore->ProgramManager()->GetAppPackage(AppSid);

			std::set<std::wstring> SvcTags = pProcess->GetServices();
			for (auto I : SvcTags) {
				CWindowsServicePtr pSvc = svcCore->ProgramManager()->GetService(I);
				if (pSvc) 
					Svcs.push_back(pSvc);
			}
		}
	}

	//
	// If we were unable to determine exact informations by querying the running process,
	// fall back to finding the executable file item and decuce from it the likely app package and/or services
	//

	if (!pProgram) {
		pProgram = svcCore->ProgramManager()->GetProgramFile(pEvent->ProcessFileName);
		pApp = pProgram->GetAppPackage();
		Svcs = pProgram->GetAllServices();
	}

	//
	// Evaluate firewall event
	//

	EFwEventStates EventState = EFwEventStates::Undefined;
	std::wstring ServiceTag;

	if (pSocket == (CSocket*)-1)
		 EventState = EFwEventStates::FromLog;
	else
	{
		if (pSocket) {
			ServiceTag = pSocket->GetOwnerServiceName();
			pRemoteHostName = pSocket->GetRemoteHostName();
		}

		struct SRuleAction
		{
			bool bHasRules = false;
			bool bConflict = false;

			bool TestAndSetOrClear(EFwActions& Result, const struct SWinFwLogEvent* pEvent, bool ForceSet = true) 
			{
				if (Result == EFwActions::Undefined)
					return false;

				//
				// Block rules take precedence over allow rules, encountering allow rules on a blocked event is fine
				// howe ever encuntering a block rule on a allowed event is an issue
				// 
				// todo: fix me: rules are ineffective on locak host connections take this into account
				//

				if (Result == EFwActions::Block && pEvent->Type == EFwActions::Allow) // Conflict
				{
					if (!ForceSet) 
					{
						//
						// if we dont know for sure this was an app package ...
						//	or
						// if we dont know for sure this socket belonged to a service ...
						// 
						// ... we assume it wasnt and discard the result on conflict
						//

						Result = EFwActions::Undefined; // Clear
						return false;
					}
					bConflict = true;
				}

				bHasRules = true;
				return true;
			}
		}
		RuleAction;

		//
		// We evaluate rules for the specific binary as well as rules applying to all processes
		// We also evaluate rules applying to the App package if one is present
		//

		SRuleMatch AllMatch = MatchRulesWithEvent(svcCore->ProgramManager()->GetOnlyAll()->GetFwRules(), pEvent, pNicInfo);
		RuleAction.TestAndSetOrClear(AllMatch.Result, pEvent);

		SRuleMatch FileMatch = MatchRulesWithEvent(pProgram->GetFwRules(), pEvent, pNicInfo);
		RuleAction.TestAndSetOrClear(FileMatch.Result, pEvent);

		SRuleMatch AppMatch;
		if(pApp) AppMatch = MatchRulesWithEvent(pApp->GetFwRules(), pEvent, pNicInfo);
		RuleAction.TestAndSetOrClear(AppMatch.Result, pEvent, pProcess != NULL);

		//
		// Evaluate service results, we idealy expect one result, but on windows 7 and some cases on later versions
		// one process may host multiple services, hence we may have a ambiguous situation.
		// In which case we pick the first service to have an action matching with the event
		// If we dont have any of these but one that failed, we have to pick that one.
		//

		std::list<std::pair<CWindowsServicePtr, SRuleAction>> SvcActionsPass;
		std::list<std::pair<CWindowsServicePtr, SRuleAction>> SvcActionsFail;
		for (auto pSvc : Svcs) {

			if (!ServiceTag.empty() && _wcsicmp(pSvc->GetSvcTag().c_str(), ServiceTag.c_str()) != 0)
				continue;

			SRuleMatch SvcMatch = MatchRulesWithEvent(pSvc->GetFwRules(), pEvent, pNicInfo);
			SRuleAction SvcAction = RuleAction; // copy
			if (SvcAction.TestAndSetOrClear(SvcMatch.Result, pEvent, !ServiceTag.empty())) {
				if (SvcAction.bConflict)	SvcActionsFail.push_back(std::make_pair(pSvc, SvcAction));
				else						SvcActionsPass.push_back(std::make_pair(pSvc, SvcAction));
			}
		}
		if (SvcActionsPass.size() > 0)		RuleAction = SvcActionsPass.front().second;
		else if (SvcActionsFail.size() > 0)	RuleAction = SvcActionsFail.front().second;
		
		//
		// Check if there is no specific rule yet for the observed event
		//

		if (!RuleAction.bHasRules) {
			EFwActions DefaultAction = GetDefaultAction(pEvent->Direction, pNicInfo ? pNicInfo->Profile : svcCore->NetIsolator()->GetDefaultProfile());
			if (DefaultAction != pEvent->Type) // Conflicts with default action
				RuleAction.bConflict = true;
		}

		if (RuleAction.bConflict)
			EventState = EFwEventStates::RuleError;
		else if(!RuleAction.bHasRules)
			EventState = EFwEventStates::UnRuled;
		else if (pEvent->Type == EFwActions::Allow)
			EventState = EFwEventStates::RuleAllowed;
		else if (pEvent->Type == EFwActions::Block)
			EventState = EFwEventStates::RuleBlocked;
	}

	CNetLogEntryPtr pLogEntry = std::make_shared<CNetLogEntry>(pEvent, EventState, pRemoteHostName, ServiceTag);
	uint32 LogIndex = pProgram->AddTraceLogEntry(pLogEntry, ETraceLogs::eNetLog);

	CVariant Event;
	//Event[SVC_API_EVENT_TYPE]	= SVC_API_EVENT_FW_EVENT;
	Event[SVC_API_ID_PROG]		= pProgram->GetID().ToVariant();
	Event[SVC_API_EVENT_INDEX]	= LogIndex;
	Event[SVC_API_EVENT_DATA]	= pLogEntry->ToVariant();
	CBuffer EventBuff;
	Event.ToPacket(&EventBuff);
	svcCore->UserPipe()->BroadcastMessage(SVC_API_EVENT_NET_ACTIVITY, &EventBuff);
}

EFwActions CFirewall::GetDefaultAction(EFwDirections Direction, EFwProfiles Profiles)
{
    // Note: FwProfile should have only one bit set, but just in case we can haldnel more than one, but not accurately
    int BlockRules = 0;
    int AllowRules = 0;

	for (int i = 0; i < ARRAYSIZE(CFirewall__Profiles); i++)
	{
		if (((int)Profiles & (int)CFirewall__Profiles[i]) == 0)
			continue;

		switch (Direction)
		{
		case EFwDirections::Inbound:
			if (m_BlockAllInbound[i])
				BlockRules++;
			else
				switch (m_DefaultInboundAction[i])
				{
				case EFwActions::Allow: AllowRules++; break;
				case EFwActions::Block: BlockRules++; break;
				}
			break;
		case EFwDirections::Outbound:
			switch (m_DefaultoutboundAction[i])
			{
			case EFwActions::Allow: AllowRules++; break;
			case EFwActions::Block: BlockRules++; break;
			}
			break;
		}
	}

    // Note: block rules take precedence
    if (BlockRules > 0)
        return EFwActions::Block;
    if (AllowRules > 0)
        return EFwActions::Allow;
    return EFwActions::Undefined;
}

CFirewall::SRuleMatch CFirewall::MatchRulesWithEvent(const std::set<CFirewallRulePtr>& Rules, const struct SWinFwLogEvent* pEvent, std::shared_ptr<struct SAdapterInfo> pNicInfo)
{
	SRuleMatch MatchingRules;

	for(auto pRule: Rules)
    {
		SWindowsFwRulePtr pData = pRule->GetData();

        if (!pData->Enabled)
            continue;
        if (pData->Direction != pEvent->Direction)
            continue;
        if (pData->Protocol != EFwKnownProtocols::Any && pEvent->ProtocolType != pData->Protocol)
            continue;
        if (pNicInfo && ((int)pNicInfo->Profile & pData->Profile) == 0)
            continue;
        if (pNicInfo && pData->Interface != (int)EFwInterfaces::All && (int)pNicInfo->Type != pData->Interface)
            continue;
        if (!MatchEndpoint(pData->RemoteAddresses, pData->RemotePorts, pEvent->RemoteAddress, pEvent->RemotePort, pNicInfo))
            continue;
        if (!MatchEndpoint(pData->LocalAddresses, pData->LocalPorts, pEvent->RemoteAddress, pEvent->RemotePort, pNicInfo))
            continue;

        pRule->IncrHitCount();

		if (pData->Action == EFwActions::Allow) {
			MatchingRules.AllowCount++;
#ifdef _DEBUG
			MatchingRules.AllowRules.push_back(pRule);
#endif
		}
		else if (pData->Action == EFwActions::Block) {
			MatchingRules.BlockCount++;
#ifdef _DEBUG
			MatchingRules.BlockRules.push_back(pRule);
#endif
		}
    }

    // Note: block rules take precedence
	if (MatchingRules.BlockCount > 0)
		MatchingRules.Result = EFwActions::Block;
    else if (MatchingRules.AllowCount > 0)
        MatchingRules.Result = EFwActions::Allow;
	return MatchingRules;
}

bool CFirewall::IsEmptyOrStar(const std::vector<std::wstring>& value)
{
	return value.empty() || (value.size() == 1 && (value[0].empty() || value[0] == L"*"));
}

bool CFirewall::MatchPort(uint16 Port, const std::vector<std::wstring>& Ports)
{
    for (auto Range: Ports)
    {
		auto PortRange = SplitStr(Range, L"-");
        if (PortRange.size() == 1)
        {
			uint16 SinglePort = _wtoi(PortRange[0].c_str());
            if (!SinglePort)
            {
                // todo: xxx some rule port values are strings :(
                // how can we test that?!
            }
            else if (SinglePort == Port)
                return true;
        }
        else if (PortRange.size() == 2)
        {
            uint16 BeginPort = _wtoi(PortRange[0].c_str());
            uint16 EndPort = _wtoi(PortRange[1].c_str());
            if (BeginPort && EndPort)
            {
                if (BeginPort <= Port && Port <= EndPort)
                    return true;
            }
        }
    }
    return false;
}

CUInt128 CFirewall__IpToInt(const CAddress& Address)
{
	CUInt128 IP;
	if(Address.Type() == CAddress::EAF::INET)
		IP = CUInt128(Address.ToIPv4());
	else if(Address.Type() == CAddress::EAF::INET6)
		IP = CUInt128(Address.Data());
	return IP;
}

bool CFirewall::MatchAddress(const CAddress& Address, const std::vector<std::wstring>& Addresses, std::shared_ptr<struct SAdapterInfo> pNicInfo)
{
    CUInt128 uIP = CFirewall__IpToInt(Address);

    for (auto Range: Addresses)
    {
        auto IPRange = SplitStr(Range, L"-");
        if (IPRange.size() == 1)
        {
			size_t pos = IPRange[0].find(L"/");
            if (pos != -1) // ip/net
            {
				auto IPNet = Split2(IPRange[0], L"/");
            
				CAddress BeginIP(IPNet.first);
				if (BeginIP.Type() == Address.Type())
				{
					CUInt128 uBeginIP = CFirewall__IpToInt(BeginIP);
					int shift = _wtoi(IPNet.second.c_str());
					CUInt128 uEndIP = uBeginIP + ((CUInt128(1) << (32 - shift)) - 1);

					if (uBeginIP <= uIP && uIP <= uEndIP)
						return true;
				}
            }
            else
            {
                std::vector<std::wstring> SpecialAddresses = GetSpecialNet(IPRange[0], pNicInfo);
                if (!SpecialAddresses.empty())
                    return MatchAddress(Address, SpecialAddresses);
                else
                {
					CAddress SingleIP(IPRange[0]);
					if (SingleIP.Type() == Address.Type())
					{
						CUInt128 uSingleIP = CFirewall__IpToInt(SingleIP);
						if (uSingleIP == uIP)
							return true;
					}
                }
            }
        }
        else if (IPRange.size() == 2)
        {
            CAddress BeginIP(IPRange[0]);
			CAddress EndIP(IPRange[1]);
			if (BeginIP.Type() == Address.Type() && BeginIP.Type() == EndIP.Type())
			{
				CUInt128 uBeginIP = CFirewall__IpToInt(BeginIP);
				CUInt128 uEndIP = CFirewall__IpToInt(EndIP);
				if (uBeginIP <= uIP && uIP <= uEndIP)
					return true;
			}
        }
    }
    return false;
}

bool CFirewall::MatchEndpoint(const std::vector<std::wstring>& Addresses, const std::vector<std::wstring>& Ports, const CAddress& Address, uint16 Port, std::shared_ptr<struct SAdapterInfo> pNicInfo)
{
    if (!IsEmptyOrStar(Ports) && !MatchPort(Port, Ports))
        return false;
    if (!Address.IsNull() && !IsEmptyOrStar(Addresses) && !MatchAddress(Address, Addresses, pNicInfo))
        return false;
    return true;
}

std::vector<std::wstring> CFirewall__CopyStrIPs(const std::list<CAddress>& Addresses)
{
	std::vector<std::wstring> IpRanges;
	for (auto Address : Addresses)
		IpRanges.push_back(Address.ToWString());
	return IpRanges;
}

std::vector<std::wstring> CFirewall::GetSpecialNet(std::wstring SubNet, std::shared_ptr<struct SAdapterInfo> pNicInfo)
{
    std::vector<std::wstring> IpRanges;
    if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordLocalSubnet) == 0 || _wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordIntrAnet) == 0)
    {
        // todo: create the list base on NicInfo.Addresses
		// 
        // IPv4
        IpRanges.push_back(L"10.0.0.0-10.255.255.255");
        IpRanges.push_back(L"127.0.0.0-127.255.255.255"); // localhost
        IpRanges.push_back(L"172.16.0.0-172.31.255.255");
        IpRanges.push_back(L"192.168.0.0-192.168.255.255");
        IpRanges.push_back(L"224.0.0.0-239.255.255.255"); // multicast

        // IPv6
        IpRanges.push_back(L"::1"); // localhost
        IpRanges.push_back(L"fc00::-fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"); // Unique local address
        IpRanges.push_back(L"fe80::-fe80::ffff:ffff:ffff:ffff"); //IpRanges.push_back(L"fe80::-febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff"); // Link-local address
        IpRanges.push_back(L"ff00::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"); // multicast
    }
    else if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordIntErnet) == 0)
    {
        // todo: ceate the list base on NicInfo.Addresses
        // IPv4
        IpRanges.push_back(L"0.0.0.0-9.255.255.255");
        // 10.0.0.0 - 10.255.255.255
        IpRanges.push_back(L"11.0.0.0-126.255.255.255");
        // 127.0.0.0 - 127.255.255.255 // localhost
        IpRanges.push_back(L"128.0.0.0-172.15.255.255");
        // 172.16.0.0 - 172.31.255.255
        IpRanges.push_back(L"172.32.0.0-192.167.255.255");
        // 192.168.0.0 - 192.168.255.255
        IpRanges.push_back(L"192.169.0.0-223.255.255.255");
        // 224.0.0.0-239.255.255.255 // multicast
        IpRanges.push_back(L"240.0.0.0-255.255.255.255");

        // ipv6
        //"::1" // localhost
        IpRanges.push_back(L"::2-fc00::");
        //"fc00::-fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" // Unique local address
        IpRanges.push_back(L"fc00::ffff:ffff:ffff:ffff-fe80::");
        //"fe80::-fe80::ffff:ffff:ffff:ffff" // fe80::-febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff // Link-local address 
        IpRanges.push_back(L"fe80::ffff:ffff:ffff:ffff-ff00::");
        //"ff00::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" // multicast
    }
    else if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordDNS) == 0)
    {
        IpRanges = CFirewall__CopyStrIPs(pNicInfo->DnsAddresses);
    }
    else if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordDHCP) == 0)
    {
        IpRanges = CFirewall__CopyStrIPs(pNicInfo->DhcpServerAddresses);
    }
    else if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordWINS) == 0)
    {
        IpRanges = CFirewall__CopyStrIPs(pNicInfo->WinsServersAddresses);
    }
    else if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordDefaultGateway) == 0)
    {
        IpRanges = CFirewall__CopyStrIPs(pNicInfo->GatewayAddresses);
    }
    else if (_wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordRmtIntrAnet) == 0
            || _wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordPly2Renders) == 0
            || _wcsicmp(SubNet.c_str(), SWindowsFwRule::AddrKeywordCaptivePortal) == 0)
    {
        // todo: !!!!
    }
    return IpRanges;
}

void CFirewall::LoadFwLog()
{
	std::wstring XmlQuery = CWindowsFwLog::GetXmlQuery();
	
	CScopedHandle hQuery(CEventLogCallback::EvtQuery(NULL, NULL, XmlQuery.c_str(), EvtQueryChannelPath), CEventLogCallback::EvtClose);
	if (!hQuery)
		return;
	
	for(;;)
	{
		EVT_HANDLE hEvent[100];

		DWORD count = 0;
		if (!CEventLogCallback::EvtNext(hQuery, ARRAYSIZE(hEvent), hEvent, 0, 0, &count)) 
		{
			DWORD status = GetLastError();
			if (status == ERROR_NO_MORE_ITEMS)
				break;
			//else {
			//	// error
			//	break;
			//}
		}

		for (DWORD i = 0; i < count; i++)
		{
			//auto test = CEventLogCallback::GetEventXml(hEvent[i]);
				
			SWinFwLogEvent Event;
			CWindowsFwLog::ReadEvent(hEvent[i], Event);

			ProcessFwEvent(&Event, (CSocket*)-1);

			CEventLogCallback::EvtClose(hEvent[i]);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// Firewall profile Configuration

std::vector<EFwProfiles> FwProfiles = { EFwProfiles::Private, EFwProfiles::Public, EFwProfiles::Domain };

void SetDefaultOutboundAction(EFwActions Action)
{
	for (auto& profile : FwProfiles)
	{
		CWindowsFirewall::Instance()->SetDefaultOutboundAction(profile, Action);
	}
}

bool TestDefaultOutboundAction(EFwActions Action)
{
	for (auto& profile : FwProfiles)
	{
		if (CWindowsFirewall::Instance()->GetDefaultOutboundAction(profile) != Action)
			return false;
	}
	return true;
}

void SetBlockAllInboundTraffic(bool Block)
{
	for (auto& profile : FwProfiles)
	{
		CWindowsFirewall::Instance()->SetBlockAllInboundTraffic(profile, Block);
	}
}

bool TestBlockAllInboundTraffic(bool Block)
{
	for (auto& profile : FwProfiles)
	{
		if (CWindowsFirewall::Instance()->GetBlockAllInboundTraffic(profile) != Block)
			return false;
	}
	return true;
}

void SetFirewallEnabled(bool Enable)
{
	for (auto& profile : FwProfiles)
	{
		CWindowsFirewall::Instance()->SetFirewallEnabled(profile, Enable);
	}
}

bool TestFirewallEnabled(bool Enable)
{
	for (auto& profile : FwProfiles)
	{
		if (CWindowsFirewall::Instance()->GetFirewallEnabled(profile) != Enable)
			return false;
	}
	return true;
}

STATUS CFirewall::SetFilteringMode(FwFilteringModes Mode)
{
	bool DoNotAllowExceptions = RegQueryDWord(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\StandardProfile", L"DoNotAllowExceptions", 0) != 0;

	SetFirewallEnabled(Mode != FwFilteringModes::NoFiltering);
	switch (Mode)
	{
	case FwFilteringModes::NoFiltering:
		break;
	case FwFilteringModes::BlockList:
		SetBlockAllInboundTraffic(DoNotAllowExceptions);
		SetDefaultOutboundAction(EFwActions::Allow);
		break;
	case FwFilteringModes::AllowList:
		SetBlockAllInboundTraffic(DoNotAllowExceptions);
		SetDefaultOutboundAction(EFwActions::Block);
		break;
	//case FwFilteringModes::BlockAll:
	//	BlockAllTrafic();
	//	break;
	}

	return OK;
}

FwFilteringModes CFirewall::GetFilteringMode()
{
	if (TestFirewallEnabled(true))
	{
		if (TestDefaultOutboundAction(EFwActions::Allow))
			return FwFilteringModes::BlockList;

		if (TestDefaultOutboundAction(EFwActions::Block))
			return FwFilteringModes::AllowList;
	}
	return FwFilteringModes::NoFiltering;
}