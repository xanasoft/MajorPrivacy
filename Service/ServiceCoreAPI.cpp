#include "pch.h"
#include <psapi.h>
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Common/FileIO.h"
#include "../Framework/Common/Buffer.h"
#include "ServiceCore.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/IPC/PipeServer.h"
#include "../Library/IPC/AlpcPortServer.h"
#include "Network/Firewall/Firewall.h"
#include "Network/NetworkManager.h"
#include "Processes/ProcessList.h"
#include "Network/SocketList.h"
#include "Programs/ProgramManager.h"
#include "Tweaks/TweakManager.h"
#include "Network/Dns/DnsInspector.h"
#include "Network/Dns/DnsFilter.h"
#include "../Library/API/DriverAPI.h"
#include "../Library/Common/Strings.h"
#include "Volumes/VolumeManager.h"
#include "Access/HandleList.h"
#include "Access/AccessManager.h"
#include "../Library/Helpers/ScopedHandle.h"

void CServiceCore::RegisterUserAPI()
{
	m_pUserPipe->RegisterEventHandler(&CServiceCore::OnClient, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_VERSION, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_CONFIG_DIR, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_CONFIG, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_SET_CONFIG, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_CHECK_CONFIG_FILE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_CONFIG_FILE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_SET_CONFIG_FILE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_DEL_CONFIG_FILE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_LIST_CONFIG_FILES, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_CONFIG_STATUS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_COMMIT_CONFIG, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_DISCARD_CHANGES, &CServiceCore::OnRequest, this);

	// Network Manager
	m_pUserPipe->RegisterHandler(SVC_API_GET_FW_RULES, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_SET_FW_RULE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_FW_RULE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_DEL_FW_RULE, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_FW_PROFILE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_SET_FW_PROFILE, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_FW_AUDIT_MODE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_SET_FW_AUDIT_MODE, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_SOCKETS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_TRAFFIC, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_DNC_CACHE, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_FLUSH_DNS_CACHE, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_DNS_RULES, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_DNS_RULE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_SET_DNS_RULE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_DEL_DNS_RULE, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_DNS_LIST_INFO, &CServiceCore::OnRequest, this);

	// Process Manager
	m_pUserPipe->RegisterHandler(SVC_API_GET_PROCESSES, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_PROCESS, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_PROGRAMS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_LIBRARIES, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_SET_PROGRAM, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_ADD_PROGRAM, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_REMOVE_PROGRAM, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_CLEANUP_PROGRAMS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_REGROUP_PROGRAMS, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_START_SECURE, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_TRACE_LOG, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_LIBRARY_STATS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_EXEC_STATS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_INGRESS_STATS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_CLEANUP_LIBS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_ACCESS_STATS, &CServiceCore::OnRequest, this);

	// Access Manager
	m_pUserPipe->RegisterHandler(SVC_API_GET_HANDLES, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_CLEAR_LOGS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_CLEANUP_ACCESS_TREE, &CServiceCore::OnRequest, this);
	
	// Volume Manager
	m_pUserPipe->RegisterHandler(SVC_API_VOL_CREATE_IMAGE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_VOL_CHANGE_PASSWORD, &CServiceCore::OnRequest, this);
	//m_pUserPipe->RegisterHandler(SVC_API_VOL_DELETE_IMAGE, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_VOL_MOUNT_IMAGE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_VOL_DISMOUNT_VOLUME, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_VOL_DISMOUNT_ALL, &CServiceCore::OnRequest, this);

	//m_pUserPipe->RegisterHandler(SVC_API_VOL_GET_VOLUME_LIST, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_VOL_GET_ALL_VOLUMES, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_VOL_GET_VOLUME, &CServiceCore::OnRequest, this);

	// Tweak Manager
	m_pUserPipe->RegisterHandler(SVC_API_GET_TWEAKS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_APPLY_TWEAK, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_UNDO_TWEAK, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_APPROVE_TWEAK, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_SET_WATCHED_PROG, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_SVC_STATS, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_SHOW_SECURE_PROMPT, &CServiceCore::OnRequest, this);
	
	m_pUserPipe->RegisterHandler(SVC_API_SHUTDOWN, &CServiceCore::OnRequest, this);


	// todo port
}

void CServiceCore::OnClient(uint32 uEvent, struct SPipeClientInfo& pClient)
{
	std::unique_lock Lock(m_ClientsMutex);
	switch (uEvent)
	{
		case CPipeServer::eClientConnected: {
			DbgPrint("Client connected %d %d\n", pClient.PID, pClient.TID);
			m_Clients[pClient.PID] = std::make_shared<SClient>();

			if (pClient.PID != -1) {

				ProcessIdToSessionId(pClient.PID, &pClient.SessionId);

				CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pClient.PID, true);
				auto SignInfo = pProcess->GetSignInfo().GetInfo();
				//if ((pProcess->GetSecFlags() & KPH_PROCESS_STATE_MEDIUM) == KPH_PROCESS_STATE_MEDIUM)
				if (SignInfo.Authority == KphDevAuthority)
					m_Clients[pClient.PID]->bIsTrusted = true;
			}
			break;
		}
		case CPipeServer::eClientDisconnected:
			DbgPrint("Client disconnected %d %d\n", pClient.PID, pClient.TID);
			m_Clients.erase(pClient.PID);
			break;
	}
}

#define RETURN_STATUS(Status) if (Status.IsError()) { \
StVariant vRpl; vRpl[API_V_ERR_CODE] = Status.GetStatus(); \
vRpl[API_V_ERR_MSG] = Status.GetMessageText(); \
vRpl.ToPacket(rpl); \
} return STATUS_SUCCESS;

uint32 CServiceCore::OnRequest(uint32 msgId, const CBuffer* req, CBuffer* rpl, const struct SPipeClientInfo& pClient)
{
#ifdef _DEBUG
	//DbgPrint("CServiceCore::OnRequest %d\n", msgId);
#endif

	std::unique_lock Lock(m_ClientsMutex);
	auto F = m_Clients.find(pClient.PID);
	if(F == m_Clients.end())
		return STATUS_BAD_KEY;
	SClientPtr pClientData = F->second;
	Lock.unlock();

	if(!pClientData->bIsTrusted) {
		DbgPrint("Client not trusted %d %d\n", pClient.PID, pClient.TID);
		return STATUS_ACCESS_DENIED;
	}

	switch (msgId)
	{
		case SVC_API_GET_VERSION:
		{
			StVariant vRpl;
			vRpl[API_V_VERSION] = MY_ABI_VERSION;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_CONFIG_DIR:
		{
			StVariant vRpl;
			vRpl[API_V_VALUE] = theCore->GetDataFolder();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_GET_CONFIG:
		{
			StVariant vReq;
			vReq.FromPacket(req);
			StVariant vRpl;
			/*if (vReq.Has(API_V_CONF_MAP))
			{
				StVariant vReqMap = vReq[API_V_CONF_MAP];
				StVariant vResMap;
				vResMap.BeginIndex();
				vReqMap.ReadRawList([&](const StVariant& vData) {
					const char* Name = (const char*)vData.GetData();
					auto SectionKey = Split2(std::string(Name, Name + vData.GetSize()), "/");
					bool bOk;
					std::wstring Value = theCore->Config()->GetValue(SectionKey.first, SectionKey.second, L"", &bOk);
					if (bOk) vResMap[Name] = Value;
				});
				vResMap.Finish();
				vRpl[API_V_CONF_MAP] = vResMap;
			}
			else*/ if (vReq.Has(API_V_KEY))
			{
				auto SectionKey = Split2(vReq[API_V_KEY], "/");
				bool bOk;
				std::wstring Value = theCore->Config()->GetValue(SectionKey.first, SectionKey.second, L"", &bOk);
				if (bOk) vRpl[API_V_VALUE] = Value;
			}
			else
			{
				StVariantWriter Data;
				Data.BeginMap();
				for(auto Section: theCore->Config()->ListSections())
				{
					StVariantWriter Values;
					Values.BeginMap();
					for(auto Key: theCore->Config()->ListKeys(Section))
						Values.WriteEx(Key.c_str(), theCore->Config()->GetValue(Section, Key));
					Data.WriteVariant(Section.c_str(), Values.Finish());
				}
				
				vRpl[API_V_DATA] = Data.Finish();
			}
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_CONFIG:
		{
			StVariant vReq;
			vReq.FromPacket(req);
			/*if (vReq.Has(API_V_CONF_MAP))
			{
				StVariant vReqMap = vReq[API_V_CONF_MAP];
				vReqMap.ReadRawMap([&](const SVarName& Name, const StVariant& vData) { 
					auto SectionKey = Split2(std::string(Name.Buf, Name.Len), "/");
					theCore->Config()->SetValue(SectionKey.first, SectionKey.second, vData.AsStr());
				});
			}
			else*/ if (vReq.Has(API_V_KEY))
			{
				auto SectionKey = Split2(vReq[API_V_KEY], "/");
				theCore->Config()->SetValue(SectionKey.first, SectionKey.second, vReq[API_V_VALUE].AsStr());
				if(SectionKey.first == "Service")
					theCore->Reconfigure(SectionKey.second);
			}
			else
			{
				StVariant Data = vReq[API_V_DATA];
				StVariantReader(Data).ReadRawMap([&](const SVarName& Section, const StVariant& Values) { 
					StVariantReader(Values).ReadRawMap([&](const SVarName& Key, const StVariant& Value) {
						theCore->Config()->SetValue(std::string(Section.Buf, Section.Len), std::string(Key.Buf, Key.Len), Value.AsStr());
					});
				});

			}
			return STATUS_SUCCESS;
		}

		case SVC_API_CHECK_CONFIG_FILE:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			std::wstring Path = vReq[API_V_FILE_PATH];
			if (Path.empty())
				return STATUS_INVALID_PARAMETER;
			if (Path.front() != L'\\')
				Path = L"\\" + Path;
			std::wstring FilePath = theCore->GetDataFolder() + Path;

			StVariant vRpl = StVariant(FileExists(FilePath));
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_CONFIG_FILE:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			std::wstring Path = vReq[API_V_FILE_PATH];
			if (Path.empty())
				return STATUS_INVALID_PARAMETER;
			if(Path.front() != L'\\')
				Path = L"\\" + Path;
			std::wstring FilePath = theCore->GetDataFolder() + Path;

			if(!CreateFullPath(Split2(FilePath, L"\\", true).first))
				return STATUS_UNSUCCESSFUL;

			std::vector<BYTE> Data = vReq[API_V_DATA].AsBytes();
			return WriteFile(FilePath, Data) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
		}
		case SVC_API_GET_CONFIG_FILE:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			std::wstring Path = vReq[API_V_FILE_PATH];
			if (Path.empty())
				return STATUS_INVALID_PARAMETER;
			if (Path.front() != L'\\')
				Path = L"\\" + Path;
			std::wstring FilePath = theCore->GetDataFolder() + Path;

			std::vector<BYTE> Data;
			if (!ReadFile(FilePath, Data))
				return STATUS_UNSUCCESSFUL;

			StVariant vRpl;
			vRpl[API_V_DATA] = Data;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_DEL_CONFIG_FILE:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			std::wstring Path = vReq[API_V_FILE_PATH];
			if (Path.empty())
				return STATUS_INVALID_PARAMETER;
			if (Path.front() != L'\\')
				Path = L"\\" + Path;
			std::wstring FilePath = theCore->GetDataFolder() + Path;

			return RemoveFile(FilePath) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
		}
		case SVC_API_LIST_CONFIG_FILES:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			std::wstring Path = vReq[API_V_FILE_PATH];
			if (Path.empty())
				return STATUS_INVALID_PARAMETER;
			if (Path.front() != L'\\')
				Path = L"\\" + Path;
			std::wstring FilePath = theCore->GetDataFolder() + Path;

			std::vector<std::wstring> FullPaths;
			if (!ListDir(FilePath, FullPaths))
				return STATUS_UNSUCCESSFUL;

			std::vector<std::wstring> Files;
			for (auto FullPath : FullPaths)
				Files.push_back(FullPath.substr(theCore->GetDataFolder().length()));

			StVariant vRpl;
			vRpl[API_V_FILES] = Files;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_CONFIG_STATUS:
		{
			uint32 Status = 0;
			if(theCore->IsConfigDirty())
				Status |= CONFIG_STATUS_DIRTY;

			StVariant vRpl(Status);
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_COMMIT_CONFIG:
		{
			STATUS Status = theCore->CommitConfig();
			RETURN_STATUS(Status);
		}
		case SVC_API_DISCARD_CHANGES:
		{
			STATUS Status = theCore->DiscardConfig();
			RETURN_STATUS(Status);
		}

		// Network Manager
		case SVC_API_GET_FW_RULES:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			bool bReload = vReq.Get(API_V_RELOAD).To<bool>(false);
			if (bReload)
				m_pNetworkManager->Firewall()->UpdateRules();

			std::map<CFlexGuid, CFirewallRulePtr> FwRules;
			if (!vReq.Has(API_V_IDS))
				FwRules = m_pNetworkManager->Firewall()->GetAllRules();
			else
			{
				StVariant IDs = vReq[API_V_IDS];
				for (uint32 i = 0; i < IDs.Count(); i++) 
				{
					CProgramID ID;
					ID.FromVariant(IDs[i]);
					auto Found = m_pNetworkManager->Firewall()->FindRules(ID);
					FwRules.merge(Found);
				}
			}

			StVariant Rules;
			for (auto I : FwRules)
				Rules.Append(I.second->ToVariant(SVarWriteOpt()));

			StVariant vRpl;
			vRpl[API_V_FW_RULES] = Rules;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_FW_RULE:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			CFirewallRulePtr FwRule = std::make_shared<CFirewallRule>();
			FwRule->FromVariant(vReq);

			STATUS Status = m_pNetworkManager->Firewall()->SetRule(FwRule);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}
		case SVC_API_GET_FW_RULE:
		{
			StVariant vReq;
			vReq.FromPacket(req);
				
			CFirewallRulePtr pRule = m_pNetworkManager->Firewall()->GetRule(vReq[API_V_GUID].AsStr());
			if (pRule) {
				StVariant vRpl = pRule->ToVariant(SVarWriteOpt());
				vRpl.ToPacket(rpl);
				return STATUS_SUCCESS;
			}
			return STATUS_NOT_FOUND;
		}
		case SVC_API_DEL_FW_RULE:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = m_pNetworkManager->Firewall()->DelRule(vReq[API_V_GUID].AsStr());
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_GET_FW_PROFILE:
		{
			FwFilteringModes Mode = m_pNetworkManager->Firewall()->GetFilteringMode();
			StVariant vRpl;
			vRpl[API_V_FW_RULE_FILTER_MODE] = (uint32)Mode;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_FW_PROFILE:
		{
			StVariant vReq;
			vReq.FromPacket(req);
			FwFilteringModes Mode = (FwFilteringModes)vReq[API_V_FW_RULE_FILTER_MODE].To<uint32>();
			STATUS Status = m_pNetworkManager->Firewall()->SetFilteringMode(Mode);
			RETURN_STATUS(Status);
		}

		case SVC_API_GET_FW_AUDIT_MODE:
		{
			FwAuditPolicy Mode = m_pNetworkManager->Firewall()->GetAuditPolicy();
			StVariant vRpl;
			vRpl[API_V_FW_AUDIT_MODE] = (uint32)Mode;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_FW_AUDIT_MODE:
		{
			StVariant vReq;
			vReq.FromPacket(req);
			FwAuditPolicy Mode = (FwAuditPolicy)vReq[API_V_FW_AUDIT_MODE].To<uint32>();
			STATUS Status = m_pNetworkManager->Firewall()->SetAuditPolicy(Mode);
			RETURN_STATUS(Status);
		}

		case SVC_API_GET_SOCKETS:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			std::set<CSocketPtr> Sockets;
			if (!vReq.Has(API_V_IDS))
				Sockets = m_pNetworkManager->SocketList()->GetAllSockets();
			else
			{
				StVariant IDs = vReq[API_V_IDS];
				for (uint32 i = 0; i < IDs.Count(); i++) 
				{
					CProgramID ID;
					ID.FromVariant(IDs[i]);
					auto Found = m_pNetworkManager->SocketList()->FindSockets(ID);
					Sockets.merge(Found);
				}
			}

			StVariant SocketList;
			for (auto pSockets : Sockets)
				SocketList.Append(pSockets->ToVariant());

			StVariant vRpl;
			vRpl[API_V_SOCKETS] = SocketList;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_TRAFFIC:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;

			StVariant vRpl;
			if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
				vRpl[API_V_TRAFFIC_LOG] = pProgram->TrafficLog()->ToVariant(vReq.Get(API_V_SOCK_LAST_NET_ACT));
			else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
				vRpl[API_V_TRAFFIC_LOG] = pService->TrafficLog()->ToVariant(vReq.Get(API_V_SOCK_LAST_NET_ACT));
			else
				return STATUS_OBJECT_TYPE_MISMATCH;

			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_DNC_CACHE:
		{
			StVariant vRpl;
			vRpl[API_V_DNS_CACHE] = theCore->NetworkManager()->DnsInspector()->DumpDnsCache();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_FLUSH_DNS_CACHE:
		{
			theCore->NetworkManager()->DnsInspector()->FlushDnsCache();
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_DNS_RULES:
		{
			StVariant vRpl;
			SVarWriteOpt Opts;
			Opts.Flags |= SVarWriteOpt::eSaveAll;
			vRpl[API_V_DNS_RULES] = theCore->NetworkManager()->DnsFilter()->SaveEntries(Opts);
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_DNS_RULE:
		{
			StVariant vReq;
			vReq.FromPacket(req);
			auto Ret = theCore->NetworkManager()->DnsFilter()->GetEntry(vReq[API_V_GUID].AsStr(), SVarWriteOpt());
			if (Ret.IsError()) {
				RETURN_STATUS(Ret);
			}

			Ret.GetValue().ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_SET_DNS_RULE:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			auto Ret = theCore->NetworkManager()->DnsFilter()->SetEntry(vReq);
			if (Ret.IsError()) {
				RETURN_STATUS(Ret);
			}

			theCore->SetConfigDirty(true);

			StVariant vRpl;
			vRpl[API_V_GUID] = Ret.GetValue();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_DEL_DNS_RULE:
		{
			StVariant vReq;
			vReq.FromPacket(req);
			STATUS Status = theCore->NetworkManager()->DnsFilter()->DelEntry(vReq[API_V_GUID].AsStr());
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_GET_DNS_LIST_INFO:
		{
			StVariant vRpl;
			vRpl[API_V_DNS_LIST_INFO] = theCore->NetworkManager()->DnsFilter()->GetBlockListInfo();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		// Process Manager
		case SVC_API_GET_PROCESSES:
		{
			StVariant Processes;
			for (auto pProcess: theCore->ProcessList()->List())
				Processes.Append(pProcess.second->ToVariant(SVarWriteOpt()));

			StVariant vRpl;
			vRpl[API_V_PROCESSES] = Processes;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_GET_PROCESS:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			uint64 Pid = vReq[API_V_PID];
			CProcessPtr pProcess = theCore->ProcessList()->GetProcess(Pid, true);
			if (!pProcess)
				return STATUS_INVALID_CID;

			StVariant vRpl = pProcess->ToVariant(SVarWriteOpt());
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_PROGRAMS:
		{
			StVariant vRpl;

			//vRpl[API_V_PROGRAMS] = theCore->ProgramManager()->GetRoot()->ToVariant();

			// todo restrict to a certain type by argument

			StVariantWriter Programs;
			Programs.BeginList();
			for (auto pItem: theCore->ProgramManager()->GetItems())
				Programs.WriteVariant(pItem.second->ToVariant(SVarWriteOpt()));

			vRpl[API_V_PROGRAMS] = Programs.Finish();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_LIBRARIES:
		{
			StVariant vRpl;

			//StVariant vReq;
			//vReq.FromPacket(req);

			//uint64 CacheToken = vReq.Get(API_V_CACHE_TOKEN).To<uint64>(-1);
			//if(!CacheToken || CacheToken != pClientData->LibraryCacheToken)
			//{
			//	pClientData->LibraryCacheToken = CacheToken;
			//	CacheToken = pClientData->LibraryCacheToken = rand();
			//	pClientData->LibraryCache.clear();
			//}

			//std::map<uint64, uint64> OldCache = pClientData->LibraryCache;

			StVariantWriter Libraries;
			Libraries.BeginList();
			for (auto pItem : theCore->ProgramManager()->GetLibraries()) {
				//auto F = OldCache.find(pItem.first);
				//if (F != OldCache.end()) {
				//	OldCache.erase(F);
				//	continue;
				//}
				//pClientData->LibraryCache[pItem.first] = 0;
				Libraries.WriteVariant(pItem.second->ToVariant(SVarWriteOpt()));
			}

			//StVariantWriter OldLibraries;
			//OldLibraries.BeginList();
			//for (auto I : OldCache) {
			//	pClientData->LibraryCache.erase(I.first);
			//	OldLibraries.Write(I.first);
			//}

			vRpl[API_V_LIBRARIES] = Libraries.Finish();
			//vRpl[API_V_OLD_LIBRARIES] = OldLibraries.Finish();
			//vRpl[API_V_CACHE_TOKEN] = CacheToken;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_SET_PROGRAM:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			CProgramItemPtr pItem;
			if (uint64 UID = vReq[API_V_PROG_UID]) // edit existing
			{
				pItem = theCore->ProgramManager()->GetItem(vReq[API_V_PROG_UID]);
				if (!pItem)
					return STATUS_NOT_FOUND;
			}
			else // add new
			{
				CProgramID ID;
				ID.FromVariant(vReq[API_V_ID]);
				auto Ret = theCore->ProgramManager()->CreateProgram(ID);
				if (Ret.IsError()) {
					RETURN_STATUS(Ret);
				}
				pItem = Ret.GetValue();
			}

			if(vReq.Has(API_V_NAME)) pItem->SetName(vReq[API_V_NAME]);
			if(vReq.Has(API_V_ICON)) pItem->SetIcon(vReq[API_V_ICON]);
			if(vReq.Has(API_V_INFO)) pItem->SetInfo(vReq[API_V_INFO]);

			if(vReq.Has(API_V_EXEC_TRACE)) pItem->SetExecTrace((ETracePreset)vReq[API_V_EXEC_TRACE].To<int>());
			if(vReq.Has(API_V_RES_TRACE))  pItem->SetResTrace((ETracePreset)vReq[API_V_RES_TRACE].To<int>());
			if(vReq.Has(API_V_NET_TRACE))  pItem->SetNetTrace((ETracePreset)vReq[API_V_NET_TRACE].To<int>());
			if(vReq.Has(API_V_SAVE_TRACE)) pItem->SetSaveTrace((ESavePreset)vReq[API_V_SAVE_TRACE].To<int>());

			theCore->SetConfigDirty(true);

			StVariant vRpl;
			vRpl[API_V_PROG_UID] = pItem->GetUID();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_ADD_PROGRAM:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->ProgramManager()->AddProgramTo(vReq[API_V_PROG_UID], vReq[API_V_PROG_PARENT]);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_REMOVE_PROGRAM:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->ProgramManager()->RemoveProgramFrom(vReq[API_V_PROG_UID], vReq[API_V_PROG_PARENT], vReq[API_V_DEL_WITH_RULES]);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_CLEANUP_PROGRAMS:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->ProgramManager()->CleanUp(vReq[API_V_PURGE_RULES]);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_REGROUP_PROGRAMS:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->ProgramManager()->ReGroup();
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_START_SECURE:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			STATUS Status;

			if (vReq.Has(API_V_ENCLAVE)) {
				CFlexGuid EnclaveGuid;
				EnclaveGuid.FromVariant(vReq[API_V_ENCLAVE]);

				Status = theCore->Driver()->PrepareEnclave(EnclaveGuid);
				if (Status.IsError())
					RETURN_STATUS(Status);
			}

			std::wstring path = vReq[API_V_CMD_LINE];

			/*WTSQueryUserToken(CallerSession, &PrimaryTokenHandle);

            if (req->elevate == 1) {

                //
                // run elevated as the current user, if the user is not in the admin group
                // this will fail, and the process started as normal user
                //

                ULONG returnLength;
                TOKEN_LINKED_TOKEN linkedToken = {0};
                NtQueryInformationToken(PrimaryTokenHandle, (TOKEN_INFORMATION_CLASS)TokenLinkedToken,
                    &linkedToken, sizeof(TOKEN_LINKED_TOKEN), &returnLength);

                CloseHandle(PrimaryTokenHandle);
                PrimaryTokenHandle = linkedToken.LinkedToken;                
            }
			*/

			CScopedHandle<HANDLE, BOOL(*)(HANDLE)> hProcess(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, FALSE, pClient.PID), CloseHandle);

			CScopedHandle<HANDLE, BOOL(*)(HANDLE)> hToken(NULL, CloseHandle);
			OpenProcessToken(hProcess, TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY, &hToken);

			CScopedHandle<HANDLE, BOOL(*)(HANDLE)> hDupToken(NULL, CloseHandle);
			DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, NULL, SecurityAnonymous, TokenPrimary, &hDupToken);

			STARTUPINFOW si = { 0 };
			si.cb = sizeof(si);
			si.dwFlags = STARTF_FORCEOFFFEEDBACK;
			si.wShowWindow = SW_SHOWNORMAL;
			PROCESS_INFORMATION pi = { 0 };
			if (CreateProcessAsUserW(hDupToken, NULL, (wchar_t*)path.c_str(), NULL, NULL, FALSE, 0/*CREATE_SUSPENDED*/, NULL, NULL, &si, &pi))
			{
				CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pi.dwProcessId, true);
				//ResumeThread(pi.hThread);
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);

				KPH_PROCESS_SFLAGS SecFlags;
				SecFlags.SecFlags = pProcess->GetSecFlags();

				if(SecFlags.EjectFromEnclave)
					Status = ERR(STATUS_ERR_PROC_EJECTED);
			}
			else
				Status = ERR(STATUS_UNSUCCESSFUL); // todo make a better error code

			RETURN_STATUS(Status);
		}

		case SVC_API_GET_TRACE_LOG:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;
			CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem);
			if (!pProgram)
				return STATUS_OBJECT_TYPE_MISMATCH;

			ETraceLogs Type = (ETraceLogs)vReq[API_V_LOG_TYPE].To<uint32>();
			if (Type >= ETraceLogs::eLogMax)
				return STATUS_INVALID_PARAMETER;

			CProgramFile::STraceLogPtr TraceLog = pProgram->GetTraceLog(Type);
			std::shared_lock LogLock(TraceLog->Mutex);

			StVariantWriter LogData;
			LogData.BeginList();
			for (auto pEntry : TraceLog->Entries)
				LogData.WriteVariant(pEntry->ToVariant());

			StVariant vRpl;
			vRpl[API_V_LOG_OFFSET] = TraceLog->IndexOffset;
			vRpl[API_V_LOG_DATA] = LogData.Finish();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_LIBRARY_STATS:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;
			CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem);
			if (!pProgram)
				return STATUS_OBJECT_TYPE_MISMATCH;

			StVariant vRpl;
			vRpl[API_V_LIBRARIES] = pProgram->DumpLibraries();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_EXEC_STATS:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;

			StVariant vRpl;
			if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
				vRpl = pProgram->DumpExecStats();
			else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
				vRpl = pService->DumpExecStats();
			else
				return STATUS_OBJECT_TYPE_MISMATCH;

			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_INGRESS_STATS:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;

			StVariant vRpl;
			if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
				vRpl = pProgram->DumpIngress();
			else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
				vRpl = pService->DumpIngress();
			else
				return STATUS_OBJECT_TYPE_MISMATCH;

			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_CLEANUP_LIBS:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			std::set<CHandlePtr> Handles;
			if (!vReq.Has(API_V_ID))
			{
				for (auto pItem : theCore->ProgramManager()->GetItems())
				{
					if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem.second))
						pProgram->CleanUpLibraries();
				}
			}
			else
			{
				CProgramID ID;
				ID.FromVariant(vReq[API_V_ID]);

				CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
				if (!pItem)
					return STATUS_NOT_FOUND;
				if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
					pProgram->CleanUpLibraries();
				else
					return STATUS_OBJECT_TYPE_MISMATCH;
			}
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_ACCESS_STATS:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;

			StVariant vRpl;
			if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
				vRpl[API_V_PROG_RESOURCE_ACCESS] = pProgram->DumpResAccess(vReq.Get(API_V_LAST_ACTIVITY));
			else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
				vRpl[API_V_PROG_RESOURCE_ACCESS] = pService->DumpResAccess(vReq.Get(API_V_LAST_ACTIVITY));
			else
				return STATUS_OBJECT_TYPE_MISMATCH;
			
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		// Access Manager
		case SVC_API_GET_HANDLES:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			std::set<CHandlePtr> Handles;
			if (!vReq.Has(API_V_IDS))
				Handles = m_pAccessManager->HandleList()->GetAllHandles();
			else
			{
				StVariant IDs = vReq[API_V_IDS];
				for (uint32 i = 0; i < IDs.Count(); i++) 
				{
					CProgramID ID;
					ID.FromVariant(IDs[i]);
					auto Found = m_pAccessManager->HandleList()->FindHandles(ID);
					Handles.merge(Found);
				}
			}

			StVariant HandleList;
			for (auto pHandles : Handles)
				HandleList.Append(pHandles->ToVariant());

			StVariant vRpl;
			vRpl[API_V_HANDLES] = HandleList;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_CLEAR_LOGS:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			ETraceLogs Log = (ETraceLogs)vReq[API_V_LOG_TYPE].To<int>();

			std::set<CHandlePtr> Handles;
			if (!vReq.Has(API_V_ID))
			{
				for (auto pItem : theCore->ProgramManager()->GetItems())
				{
					if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem.second))
						pProgram->ClearLogs(Log);
					else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem.second))
						pService->ClearLogs(Log);
				}
			}
			else
			{
				CProgramID ID;
				ID.FromVariant(vReq[API_V_ID]);

				CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
				if (!pItem)
					return STATUS_NOT_FOUND;
				if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
					pProgram->ClearLogs(Log);
				else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
					pService->ClearLogs(Log);
				else
					return STATUS_OBJECT_TYPE_MISMATCH;
			}
			return STATUS_SUCCESS;
		}

		case SVC_API_CLEANUP_ACCESS_TREE:
		{
			STATUS Status = theCore->AccessManager()->CleanUp();
			RETURN_STATUS(Status);
		}

		// Volume Manager
		case SVC_API_VOL_CREATE_IMAGE:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->VolumeManager()->CreateImage(vReq[API_V_VOL_PATH], vReq[API_V_VOL_PASSWORD], vReq.Get(API_V_VOL_SIZE).To<uint64>(0), vReq.Get(API_V_VOL_CIPHER).AsStr());
			RETURN_STATUS(Status);
		}
		case SVC_API_VOL_CHANGE_PASSWORD:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->VolumeManager()->ChangeImagePassword(vReq[API_V_VOL_PATH], vReq[API_V_VOL_OLD_PASS], vReq[API_V_VOL_NEW_PASS]);
			RETURN_STATUS(Status);
		}
		//case SVC_API_VOL_DELETE_IMAGE:
		//{
		//	StVariant vReq;
		//	vReq.FromPacket(req);
		//
		//  STATUS Status = theCore->VolumeManager()->DeleteImage(vReq[API_V_VOL_PATH]);
		//	RETURN_STATUS(Status);
		//}

		case SVC_API_VOL_MOUNT_IMAGE:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->VolumeManager()->MountImage(vReq[API_V_VOL_PATH], vReq[API_V_VOL_MOUNT_POINT], vReq[API_V_VOL_PASSWORD], vReq[API_V_VOL_PROTECT]);
			RETURN_STATUS(Status);
		}
		case SVC_API_VOL_DISMOUNT_VOLUME:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->VolumeManager()->DismountVolume(vReq[API_V_VOL_MOUNT_POINT].ToWString());
			RETURN_STATUS(Status);
		}
		case SVC_API_VOL_DISMOUNT_ALL:
		{
			STATUS Status = theCore->VolumeManager()->DismountAll();
			RETURN_STATUS(Status);
		}
		
		/*case SVC_API_VOL_GET_VOLUME_LIST:
		{
			auto Ret = theCore->VolumeManager()->GetVolumeList();
			if (Ret.IsError())
				return Ret.GetStatus();
				
			StVariant List;
			List.BeginList();
			for(std::wstring& DevicePath : Ret.GetValue())
				List.Write(DevicePath);
			List.Finish();

			StVariant vRpl;
			vRpl[API_V_VOL_LIST] = List;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}*/
		case SVC_API_VOL_GET_ALL_VOLUMES:
		{
			auto Ret = theCore->VolumeManager()->GetAllVolumes();
			if (Ret.IsError())
				return Ret.GetStatus();
				
			StVariantWriter List;
			List.BeginList();
			for(auto& pVolume : Ret.GetValue())
				List.WriteVariant(pVolume->ToVariant());

			StVariant vRpl;
			vRpl[API_V_VOLUMES] = List.Finish();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_VOL_GET_VOLUME:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			auto Ret = theCore->VolumeManager()->GetVolumeInfo(vReq[API_V_VOL_MOUNT_POINT]);
			if (Ret.IsError())
				return Ret.GetStatus();

			Ret.GetValue()->ToVariant().ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		// Tweak Manager
		case SVC_API_GET_TWEAKS:
		{
			StVariant vRpl;
			SVarWriteOpt Opts;
			Opts.Flags |= SVarWriteOpt::eSaveAll;
			vRpl[API_V_TWEAKS] = theCore->TweakManager()->GetTweaks(Opts, pClient.PID);
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_APPLY_TWEAK:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->TweakManager()->ApplyTweak(vReq[API_V_ID], pClient.PID);
			RETURN_STATUS(Status);
		}
		case SVC_API_UNDO_TWEAK:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->TweakManager()->UndoTweak(vReq[API_V_ID], pClient.PID);
			RETURN_STATUS(Status);
		}
		case SVC_API_APPROVE_TWEAK:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->TweakManager()->ApproveTweak(vReq[API_V_ID], pClient.PID);
			RETURN_STATUS(Status);
		}

		case SVC_API_SET_WATCHED_PROG:
		{
			StVariant vReq;
			vReq.FromPacket(req);

			std::unique_lock Lock(pClientData->Mutex);
			pClientData->bWatchAllPrograms = false;
			pClientData->WatchedPrograms.clear();
			if (vReq.Has(API_V_PROG_UIDS))
			{
				uint64 AllID = m_pProgramManager->GetAllItem()->GetUID();
				StVariant IDs = vReq[API_V_PROG_UIDS];
				for (uint32 i = 0; i < IDs.Count(); i++) {
					uint64 UID = IDs[i];
					if(UID == AllID)
						pClientData->bWatchAllPrograms = true;
					pClientData->WatchedPrograms.insert(UID);
				}
			}
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_SVC_STATS:
		{
			StVariant vRpl;

			PROCESS_MEMORY_COUNTERS pmc;
			if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
				vRpl[API_V_SVC_MEM_WS] = pmc.WorkingSetSize;
				vRpl[API_V_SVC_MEM_PB] = pmc.PagefileUsage;	
			}
			vRpl[API_V_LOG_MEM_USAGE] = theCore->ProgramManager()->GetLogMemUsage();

			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_SHOW_SECURE_PROMPT:
		{
			NTSTATUS status = STATUS_UNSUCCESSFUL;

			StVariant vReq;
			vReq.FromPacket(req);

			std::wstring Text = vReq[API_V_MB_TEXT].AsStr();
			//std::wstring Title = vReq.Get(API_V_MB_TITLE).AsStr();
			//if(Title.empty()) Title = L"Major Privacy";
			uint32 Type = vReq[API_V_MB_TYPE].To<uint32>();
		
			CScopedHandle<HANDLE, BOOL(*)(HANDLE)> hToken(NULL, CloseHandle);
			if (OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hToken)) 
			{
				CScopedHandle<HANDLE, BOOL(*)(HANDLE)> hNewToken(NULL, CloseHandle);
				if (DuplicateTokenEx(hToken, MAXIMUM_ALLOWED, nullptr, SecurityIdentification, TokenPrimary, &hNewToken)) 
				{
					// Set the new session ID on the duplicated token
					if (SetTokenInformation(hNewToken, TokenSessionId, (LPVOID)&pClient.SessionId, sizeof(pClient.SessionId))) 
					{
						wchar_t szPath[MAX_PATH];
						GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath));
						std::wstring CmdLine = L"\"" + std::wstring(szPath) + L"\"";

						CmdLine += L" \"-MSGBOX:";

						switch (Type & 0xF) {
						case MB_OK: CmdLine += L"OK"; break;
						case MB_OKCANCEL: CmdLine += L"OKCANCEL"; break;
						case MB_ABORTRETRYIGNORE: CmdLine += L"ABORTRETRYIGNORE"; break;
						case MB_YESNOCANCEL: CmdLine += L"YESNOCANCEL"; break;
						case MB_YESNO: CmdLine += L"YESNO"; break;
						case MB_RETRYCANCEL: CmdLine += L"RETRYCANCEL"; break;
						case MB_CANCELTRYCONTINUE: CmdLine += L"CANCELTRYCONTINUE"; break;
						}

						switch (Type & 0xF0) {
						case MB_ICONHAND: CmdLine += L"-STOP"; break;
						case MB_ICONQUESTION: CmdLine += L"-QUESTION"; break;
						case MB_ICONEXCLAMATION: CmdLine += L"-EXCLAMATION"; break;
						case MB_ICONASTERISK: CmdLine += L"-INFORMATION"; break;
						}

						CmdLine += L":" + Text + L"\"";

						STARTUPINFOW si = { 0 };
						si.cb = sizeof(si);
						si.dwFlags = STARTF_FORCEOFFFEEDBACK;
						si.wShowWindow = SW_SHOWNORMAL;
						PROCESS_INFORMATION pi = { 0 };
						if (CreateProcessAsUserW(hNewToken, NULL, (wchar_t*)CmdLine.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
						{
							if (WaitForSingleObject(pi.hProcess, 100 * 1000) == 0)
							{
								DWORD Code = 0;
								if (GetExitCodeProcess(pi.hProcess, &Code))
								{
									StVariant vRpl;
									vRpl[API_V_MB_CODE] = (uint32)Code;
									vRpl.ToPacket(rpl);

									status = STATUS_SUCCESS;
								}
							}

							CloseHandle(pi.hProcess);
							CloseHandle(pi.hThread);
						}
					}
				}
			}

			return status;
		}

		case SVC_API_SHUTDOWN:
		{
			STATUS Status = theCore->PrepareShutdown();

			if (theCore->m_bEngineMode)
				CServiceCore::Shutdown(false);

			return Status.GetStatus();
		}

		default:
			return STATUS_UNSUCCESSFUL;
	}
}

void CServiceCore::BroadcastMessage(uint32 MessageID, const StVariant& MessageData, const std::shared_ptr<class CProgramFile>& pProgram)
{
	CBuffer MessageBuffer;
	MessageData.ToPacket(&MessageBuffer);

	if (!pProgram) {
		theCore->UserPipe()->BroadcastMessage(MessageID, &MessageBuffer);
		return;
	}

	std::unique_lock Lock(m_ClientsMutex);
	auto Clients = m_Clients;
	Lock.unlock();

	for(auto& I : Clients) {
		SClientPtr pClientData = I.second;
		std::unique_lock Lock(pClientData->Mutex);
		if(!pClientData->bWatchAllPrograms && pClientData->WatchedPrograms.find(pProgram->GetUID()) == pClientData->WatchedPrograms.end())
			continue;
		theCore->UserPipe()->BroadcastMessage(MessageID, &MessageBuffer, I.first);
	}
}