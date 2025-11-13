#include "pch.h"
#include <psapi.h>
#include <wtsapi32.h>
#include <userenv.h>
#include "../Library/Helpers/NtObj.h"
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
#include "../Library/Helpers/NtIO.h"
#include "Common/EventLog.h"
#include "../Library/Helpers/TokenUtil.h"
#include "Enclaves/EnclaveManager.h"
#include "Presets/PresetManager.h"

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
	m_pUserPipe->RegisterHandler(SVC_API_GET_PROGRAM, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_LIBRARIES, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_LIBRARY, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_SET_PROGRAM, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_ADD_PROGRAM, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_REMOVE_PROGRAM, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_REFRESH_PROGRAMS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_CLEANUP_PROGRAMS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_REGROUP_PROGRAMS, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_START_SECURE, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_RUN_UPDATER, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_TRACE_LOG, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_LIBRARY_STATS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_EXEC_STATS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_INGRESS_STATS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_CLEANUP_LIBS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_ACCESS_STATS, &CServiceCore::OnRequest, this);

	// Access Manager
	m_pUserPipe->RegisterHandler(SVC_API_GET_HANDLES, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_CLEAR_LOGS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_CLEAR_RECORDS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_CLEANUP_ACCESS_TREE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_SET_ACCESS_EVENT_ACTION, &CServiceCore::OnRequest, this);
	
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
	m_pUserPipe->RegisterHandler(SVC_API_VOL_SET_VOLUME, &CServiceCore::OnRequest, this);

	// Tweak Manager
	m_pUserPipe->RegisterHandler(SVC_API_GET_TWEAKS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_APPLY_TWEAK, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_UNDO_TWEAK, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_APPROVE_TWEAK, &CServiceCore::OnRequest, this);

	// PresetManager
	m_pUserPipe->RegisterHandler(SVC_API_SET_PRESETS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_PRESETS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_PRESET, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_SET_PRESET, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_DEL_PRESET, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_ACTIVATE_PRESET, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_DEACTIVATE_PRESET, &CServiceCore::OnRequest, this);
	

	m_pUserPipe->RegisterHandler(SVC_API_SET_WATCHED_PROG, &CServiceCore::OnRequest, this);
	
	m_pUserPipe->RegisterHandler(SVC_API_SET_DAT_FILE, &CServiceCore::OnRequest, this);
	//m_pUserPipe->RegisterHandler(SVC_API_GET_DAT_FILE, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_EVENT_LOG, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_CLEAR_EVENT_LOG, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_SVC_STATS, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_SCRIPT_LOG, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_CLEAR_SCRIPT_LOG, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_CALL_SCRIPT_FUNC, &CServiceCore::OnRequest, this);

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
				//if ((pProcess->GetSecFlags() & KPH_PROCESS_STATE_MEDIUM) == KPH_PROCESS_STATE_MEDIUM)
				if (pProcess->GetSignInfo().GetAuthority() == KphDevAuthority)
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
StVariant vRpl(m_pMemPool); vRpl[API_V_ERR_CODE] = Status.GetStatus(); \
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
			StVariant vRpl(m_pMemPool);
			vRpl[API_V_VERSION] = MY_ABI_VERSION;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_CONFIG_DIR:
		{
			StVariant vRpl(m_pMemPool);
			vRpl[API_V_VALUE] = theCore->GetDataFolder();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_GET_CONFIG:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);
			StVariant vRpl(m_pMemPool);
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
				if(SectionKey.first == "Service")
					theCore->RefreshConfig(SectionKey.second);
				bool bOk;
				std::wstring Value = theCore->Config()->GetValue(SectionKey.first, SectionKey.second, L"", &bOk);
				if (bOk) vRpl[API_V_VALUE] = Value;
			}
			else
			{
				StVariantWriter Data(m_pMemPool);
				Data.BeginMap();
				for(auto Section: theCore->Config()->ListSections())
				{
					StVariantWriter Values(m_pMemPool);
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
			StVariant vReq(m_pMemPool);
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
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			std::wstring Path = vReq[API_V_FILE_PATH];
			if (Path.empty())
				return STATUS_INVALID_PARAMETER;
			if (Path.front() != L'\\')
				Path = L"\\" + Path;
			std::wstring FilePath = theCore->GetDataFolder() + Path;

			StVariant vRpl = StVariant(m_pMemPool, FileExists(FilePath));
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_CONFIG_FILE:
		{
			StVariant vReq(m_pMemPool);
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
			StVariant vReq(m_pMemPool);
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

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_DATA] = Data;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_DEL_CONFIG_FILE:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			std::wstring Path = vReq[API_V_FILE_PATH];
			if (Path.empty())
				return STATUS_INVALID_PARAMETER;
			if (Path.front() != L'\\')
				Path = L"\\" + Path;
			std::wstring FilePath = theCore->GetDataFolder() + Path;

			if (FilePath.at(FilePath.length() - 1) == L'\\')
			{
				SNtObject ntOld(L"\\??\\" + FilePath);
				return NtIo_DeleteFolderRecursively(&ntOld.attr, NULL, NULL);
			}

			return RemoveFile(FilePath) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
		}
		case SVC_API_LIST_CONFIG_FILES:
		{
			StVariant vReq(m_pMemPool);
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

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_FILES] = Files;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_CONFIG_STATUS:
		{
			uint32 Status = 0;
			if(theCore->IsConfigDirty())
				Status |= CONFIG_STATUS_DIRTY;

			StVariant vRpl(m_pMemPool, Status);
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
			StVariant vReq(m_pMemPool);
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

			StVariant Rules(m_pMemPool);
			for (auto I : FwRules)
				Rules.Append(I.second->ToVariant(SVarWriteOpt(), m_pMemPool));

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_RULES] = Rules;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_FW_RULE:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			CFirewallRulePtr pFwRule = std::make_shared<CFirewallRule>();
			pFwRule->FromVariant(vReq[API_V_RULE]);

			STATUS Status = m_pNetworkManager->Firewall()->SetRule(pFwRule);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}
		case SVC_API_GET_FW_RULE:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);
				
			CFirewallRulePtr pRule = m_pNetworkManager->Firewall()->GetRule(vReq[API_V_GUID].AsStr());
			if (pRule) {
				StVariant vRpl(m_pMemPool);
				vRpl[API_V_RULE] = pRule->ToVariant(SVarWriteOpt(), m_pMemPool);
				vRpl.ToPacket(rpl);
				return STATUS_SUCCESS;
			}
			return STATUS_NOT_FOUND;
		}
		case SVC_API_DEL_FW_RULE:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			STATUS Status = m_pNetworkManager->Firewall()->DelRule(vReq[API_V_GUID].AsStr());
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_GET_FW_PROFILE:
		{
			FwFilteringModes Mode = m_pNetworkManager->Firewall()->GetFilteringMode();
			StVariant vRpl(m_pMemPool);
			vRpl[API_V_FW_RULE_FILTER_MODE] = (uint32)Mode;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_FW_PROFILE:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);
			FwFilteringModes Mode = (FwFilteringModes)vReq[API_V_FW_RULE_FILTER_MODE].To<uint32>();
			STATUS Status = m_pNetworkManager->Firewall()->SetFilteringMode(Mode);
			RETURN_STATUS(Status);
		}

		case SVC_API_GET_FW_AUDIT_MODE:
		{
			FwAuditPolicy Mode = m_pNetworkManager->Firewall()->GetAuditPolicy();
			StVariant vRpl(m_pMemPool);
			vRpl[API_V_FW_AUDIT_MODE] = (uint32)Mode;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_FW_AUDIT_MODE:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);
			FwAuditPolicy Mode = (FwAuditPolicy)vReq[API_V_FW_AUDIT_MODE].To<uint32>();
			STATUS Status = m_pNetworkManager->Firewall()->SetAuditPolicy(Mode);
			RETURN_STATUS(Status);
		}

		case SVC_API_GET_SOCKETS:
		{
			StVariant vReq(m_pMemPool);
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

			StVariant SocketList(m_pMemPool);
			for (auto pSockets : Sockets)
				SocketList.Append(pSockets->ToVariant(m_pMemPool));

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_SOCKETS] = SocketList;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_TRAFFIC:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;

			StVariant vRpl(m_pMemPool);
			if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
				vRpl[API_V_TRAFFIC_LOG] = pProgram->TrafficLog()->ToVariant(vReq.Get(API_V_SOCK_LAST_NET_ACT), m_pMemPool);
			else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
				vRpl[API_V_TRAFFIC_LOG] = pService->TrafficLog()->ToVariant(vReq.Get(API_V_SOCK_LAST_NET_ACT), m_pMemPool);
			else
				return STATUS_OBJECT_TYPE_MISMATCH;

			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_DNC_CACHE:
		{
			StVariant vRpl(m_pMemPool);
			vRpl[API_V_DNS_CACHE] = theCore->NetworkManager()->DnsInspector()->DumpDnsCache(m_pMemPool);
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
			StVariant vRpl(m_pMemPool);
			SVarWriteOpt Opts;
			Opts.Flags |= SVarWriteOpt::eSaveAll;
			vRpl[API_V_RULES] = theCore->NetworkManager()->DnsFilter()->SaveEntries(Opts, m_pMemPool);
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_DNS_RULE:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);
			auto Ret = theCore->NetworkManager()->DnsFilter()->GetEntry(vReq[API_V_GUID].AsStr(), SVarWriteOpt(), m_pMemPool);
			if (Ret.IsError()) {
				RETURN_STATUS(Ret);
			}

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_RULE] = Ret.GetValue();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_SET_DNS_RULE:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			auto Ret = theCore->NetworkManager()->DnsFilter()->SetEntry(vReq[API_V_RULE]);
			if (Ret.IsError()) {
				RETURN_STATUS(Ret);
			}

			theCore->SetConfigDirty(true);

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_GUID] = Ret.GetValue();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_DEL_DNS_RULE:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);
			STATUS Status = theCore->NetworkManager()->DnsFilter()->DelEntry(vReq[API_V_GUID].AsStr());
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_GET_DNS_LIST_INFO:
		{
			StVariant vRpl(m_pMemPool);
			vRpl[API_V_DNS_LIST_INFO] = theCore->NetworkManager()->DnsFilter()->GetBlockListInfo(m_pMemPool);
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		// Process Manager
		case SVC_API_GET_PROCESSES:
		{
			StVariant Processes(m_pMemPool);
			for (auto pProcess: theCore->ProcessList()->List())
				Processes.Append(pProcess.second->ToVariant(SVarWriteOpt(), m_pMemPool));

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_PROCESSES] = Processes;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_GET_PROCESS:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			uint64 Pid = vReq[API_V_PID];
			CProcessPtr pProcess = theCore->ProcessList()->GetProcess(Pid, true);
			if (!pProcess)
				return STATUS_INVALID_CID;

			StVariant vRpl = pProcess->ToVariant(SVarWriteOpt(), m_pMemPool);
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_PROGRAMS:
		{
			StVariant vRpl(m_pMemPool);

			//vRpl[API_V_PROGRAMS] = theCore->ProgramManager()->GetRoot()->ToVariant();

			// todo restrict to a certain type by argument

			StVariantWriter Programs(m_pMemPool);
			Programs.BeginList();
			for (auto pItem: theCore->ProgramManager()->GetItems())
				Programs.WriteVariant(pItem.second->ToVariant(SVarWriteOpt(), m_pMemPool));

			vRpl[API_V_PROGRAMS] = Programs.Finish();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_GET_PROGRAM:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			CProgramItemPtr pItem;
			if (uint64 UID = vReq.Get(API_V_PROG_UID)) // get by UID
			{
				pItem = theCore->ProgramManager()->GetItem(vReq[API_V_PROG_UID]);
			}
			else // get by ID
			{
				CProgramID ID;
				ID.FromVariant(vReq[API_V_ID]);
				pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			}
			if (!pItem)
				return STATUS_NOT_FOUND;

			StVariant vRpl = pItem->ToVariant(SVarWriteOpt(), m_pMemPool);
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_LIBRARIES:
		{
			StVariant vRpl(m_pMemPool);

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

			StVariantWriter Libraries(m_pMemPool);
			Libraries.BeginList();
			for (auto pItem : theCore->ProgramManager()->GetLibraries()) {
				//auto F = OldCache.find(pItem.first);
				//if (F != OldCache.end()) {
				//	OldCache.erase(F);
				//	continue;
				//}
				//pClientData->LibraryCache[pItem.first] = 0;
				Libraries.WriteVariant(pItem.second->ToVariant(SVarWriteOpt(), m_pMemPool));
			}

			//StVariantWriter OldLibraries(m_pMemPool);
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
		case SVC_API_GET_LIBRARY:
		{
			return STATUS_NOT_IMPLEMENTED; // todo
		}

		case SVC_API_SET_PROGRAM:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			CProgramItemPtr pItem;
			if (uint64 UID = vReq.Get(API_V_PROG_UID)) // edit existing
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

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_PROG_UID] = pItem->GetUID();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_ADD_PROGRAM:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			STATUS Status = theCore->ProgramManager()->AddProgramTo(vReq[API_V_PROG_UID], vReq[API_V_PROG_PARENT]);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}
		case SVC_API_REMOVE_PROGRAM:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			STATUS Status = theCore->ProgramManager()->RemoveProgramFrom(vReq[API_V_PROG_UID], vReq.Get(API_V_PROG_PARENT).To<uint64>(0), vReq.Get(API_V_DEL_WITH_RULES).To<bool>(false), vReq.Get(API_V_KEEP_ONE).To<bool>(false));
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_REFRESH_PROGRAMS:
		{
			theCore->ProgramManager()->CheckProgramFiles();
			return STATUS_SUCCESS;
		}
		case SVC_API_CLEANUP_PROGRAMS:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			STATUS Status = theCore->ProgramManager()->CleanUp(vReq[API_V_PURGE_RULES]);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}
		case SVC_API_REGROUP_PROGRAMS:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			STATUS Status = theCore->ProgramManager()->ReGroup();
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_START_SECURE:
		{
			StVariant vReq(m_pMemPool);
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

#if 1
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

			CScopedHandle lpEnvironment((LPVOID)0, DestroyEnvironmentBlock);
			if (!CreateEnvironmentBlock(&lpEnvironment, hDupToken, FALSE)) {
				Status = ERR(STATUS_VARIABLE_NOT_FOUND);
				RETURN_STATUS(Status);
			}

			STARTUPINFOW si = { 0 };
			si.cb = sizeof(si);
			si.dwFlags = STARTF_FORCEOFFFEEDBACK;
			si.wShowWindow = SW_SHOWNORMAL;
			PROCESS_INFORMATION pi = { 0 };

			//
			// Note: If we are running in engine mode, We are admin but not system and are missing SeAssignPrimaryTokenPrivilege
			//

			if (CreateProcessAsUserW(m_bEngineMode ? NULL : *&hDupToken, NULL, (wchar_t*)path.c_str(), NULL, NULL, FALSE, CREATE_UNICODE_ENVIRONMENT, lpEnvironment, NULL, &si, &pi))
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
				Status = ERR(GetLastWin32ErrorAsNtStatus());
				
#else
			
			uint32 dwProcessId = 0;
			Status = theCore->CreateUserProcess(path, pClient.PID, CServiceCore::eExec_AsCallerUser, L"", &dwProcessId);
			if (Status.IsSuccess())
			{
				CProcessPtr pProcess = theCore->ProcessList()->GetProcess(dwProcessId, true);

				KPH_PROCESS_SFLAGS SecFlags;
				SecFlags.SecFlags = pProcess->GetSecFlags();

				if(SecFlags.EjectFromEnclave)
					Status = ERR(STATUS_ERR_PROC_EJECTED);
			}
#endif

			RETURN_STATUS(Status);
		}

		case SVC_API_RUN_UPDATER:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			std::wstring Cmd = L"\"" + theCore->GetAppDir() + L"\\UpdUtil.exe \" " + vReq[API_V_CMD_LINE].AsStr();
			uint32 Elevate = vReq.Get(API_V_ELEVATE).To<uint32>(0);

			CScopedHandle<HANDLE, BOOL(*)(HANDLE)> CallerProcessHandle(OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, FALSE, pClient.PID), CloseHandle);
			if (!CallerProcessHandle) 
				return STATUS_UNSUCCESSFUL;
			
			ULONG CallerSession = pClient.SessionId;

			CScopedHandle<HANDLE, BOOL(*)(HANDLE)> PrimaryTokenHandle(NULL, CloseHandle);

			if (Elevate == 2) 
			{
				//
				// run as system, works also for non administrative users
				//

				const ULONG TOKEN_RIGHTS = TOKEN_QUERY  | TOKEN_DUPLICATE
								| TOKEN_ADJUST_DEFAULT	| TOKEN_ADJUST_SESSIONID
								| TOKEN_ADJUST_GROUPS	| TOKEN_ASSIGN_PRIMARY;

				if (!OpenProcessToken(GetCurrentProcess(), TOKEN_RIGHTS, &PrimaryTokenHandle))
					return STATUS_UNSUCCESSFUL;
					
				CScopedHandle<HANDLE, BOOL(*)(HANDLE)> hNewToken(NULL, CloseHandle);
				if (!DuplicateTokenEx(PrimaryTokenHandle, TOKEN_RIGHTS, NULL, SecurityAnonymous, TokenPrimary, &hNewToken))
					return STATUS_UNSUCCESSFUL;

				PrimaryTokenHandle.Set(hNewToken.Detach());

				if (SetTokenInformation(PrimaryTokenHandle, TokenSessionId, &CallerSession, sizeof(ULONG)))
					return STATUS_UNSUCCESSFUL;
			} 
			else 
			{
				//
				// get calling user's token
				//

				if (!WTSQueryUserToken(CallerSession, &PrimaryTokenHandle))
					return STATUS_UNSUCCESSFUL;

				if (Elevate == 1 && !TokenIsAdmin(PrimaryTokenHandle, true)) 
				{
					//
					// run elevated as the current user, if the user is not in the admin group
					// this will fail, and the process started as normal user
					//

					ULONG returnLength;
					TOKEN_LINKED_TOKEN linkedToken = {0};
					NtQueryInformationToken(PrimaryTokenHandle, (TOKEN_INFORMATION_CLASS)TokenLinkedToken, &linkedToken, sizeof(TOKEN_LINKED_TOKEN), &returnLength);

					PrimaryTokenHandle.Set(linkedToken.LinkedToken);
				}
			}

			if (!PrimaryTokenHandle)
				return STATUS_UNSUCCESSFUL;

			PROCESS_INFORMATION pi = {0};
			STARTUPINFO si = {0};
			si.cb = sizeof(STARTUPINFO);
			si.dwFlags = STARTF_FORCEOFFFEEDBACK;
			si.wShowWindow = SW_SHOWNORMAL;
			if (!CreateProcessAsUser(PrimaryTokenHandle, NULL, (wchar_t*)Cmd.c_str(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
				return STATUS_UNSUCCESSFUL;

			//
			// Filter Handles to prevent privilege escalation in case 
			// a signed but hijacked agent requested the start of a utility process
			// and would subsequenty try to hijack the utility process.
			//

			HANDLE hCreatedProcess = NULL;
			// Note: PROCESS_SUSPEND_RESUME is enough to start a debugging session which will give a full access handle in the first debug event (diversenok)
			DWORD dwRead =  STANDARD_RIGHTS_READ | SYNCHRONIZE |
				PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | //PROCESS_SUSPEND_RESUME | unlike THREAD_SUSPEND_RESUME this one is dangerous
				PROCESS_QUERY_LIMITED_INFORMATION;
			DuplicateHandle(GetCurrentProcess(), pi.hProcess, CallerProcessHandle, &hCreatedProcess, dwRead, FALSE, 0);
		
			//HANDLE hCreatedThread = NULL;
			//DWORD dwRead =  STANDARD_RIGHTS_READ | SYNCHRONIZE |
			//	THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME | 
			//	THREAD_QUERY_LIMITED_INFORMATION;
			//DuplicateHandle(GetCurrentProcess(), pi.hThread, CallerProcessHandle, &hCreatedThread, dwRead, FALSE, 0);
	
			ResumeThread(pi.hThread);

			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_HANDLE] = (uint64)hCreatedProcess;
			vRpl[API_V_PID] = (uint32)pi.dwProcessId;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_TRACE_LOG:
		{
			StVariant vReq(m_pMemPool);
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

			StVariantWriter LogData(m_pMemPool);
			LogData.BeginList();
			for (auto pEntry : TraceLog->Entries)
				LogData.WriteVariant(pEntry->ToVariant(m_pMemPool));

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_LOG_OFFSET] = TraceLog->IndexOffset;
			vRpl[API_V_LOG_DATA] = LogData.Finish();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_LIBRARY_STATS:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;
			CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem);
			if (!pProgram)
				return STATUS_OBJECT_TYPE_MISMATCH;

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_LIBRARIES] = pProgram->DumpLibraries(m_pMemPool);
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_EXEC_STATS:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;

			StVariant vRpl(m_pMemPool);
			if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
				vRpl = pProgram->DumpExecStats(m_pMemPool);
			else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
				vRpl = pService->DumpExecStats(m_pMemPool);
			else
				return STATUS_OBJECT_TYPE_MISMATCH;

			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_INGRESS_STATS:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;

			StVariant vRpl(m_pMemPool);
			if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
				vRpl = pProgram->DumpIngress(m_pMemPool);
			else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
				vRpl = pService->DumpIngress(m_pMemPool);
			else
				return STATUS_OBJECT_TYPE_MISMATCH;

			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_CLEANUP_LIBS:
		{
			StVariant vReq(m_pMemPool);
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
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;

			StVariant vRpl(m_pMemPool);
			if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem))
				vRpl[API_V_PROG_RESOURCE_ACCESS] = pProgram->DumpResAccess(vReq.Get(API_V_LAST_ACTIVITY), m_pMemPool);
			else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
				vRpl[API_V_PROG_RESOURCE_ACCESS] = pService->DumpResAccess(vReq.Get(API_V_LAST_ACTIVITY), m_pMemPool);
			else
				return STATUS_OBJECT_TYPE_MISMATCH;
			
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		// Access Manager
		case SVC_API_GET_HANDLES:
		{
			StVariant vReq(m_pMemPool);
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

			StVariantWriter HandleList(m_pMemPool);
			HandleList.BeginList();
			for (auto pHandles : Handles)
				HandleList.WriteVariant(pHandles->ToVariant(m_pMemPool));

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_HANDLES] = HandleList.Finish();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_CLEAR_LOGS:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			ETraceLogs Log = (ETraceLogs)vReq[API_V_LOG_TYPE].To<int>();

			std::set<CHandlePtr> Handles;
			if (!vReq.Has(API_V_ID))
			{
				for (auto pItem : theCore->ProgramManager()->GetItems())
				{
					if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem.second))
						pProgram->ClearTraceLog(Log);
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
					pProgram->ClearTraceLog(Log);
				else
					return STATUS_OBJECT_TYPE_MISMATCH;
			}
			return STATUS_SUCCESS;
		}
		case SVC_API_CLEAR_RECORDS:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			ETraceLogs Log = (ETraceLogs)vReq[API_V_LOG_TYPE].To<int>();

			std::set<CHandlePtr> Handles;
			if (!vReq.Has(API_V_ID))
			{
				for (auto pItem : theCore->ProgramManager()->GetItems())
				{
					if (CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem.second))
						pProgram->ClearRecords(Log);
					else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem.second))
						pService->ClearRecords(Log);
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
					pProgram->ClearRecords(Log);
				else if(CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem))
					pService->ClearRecords(Log);
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

		case SVC_API_SET_ACCESS_EVENT_ACTION:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);
			uint64 EventId = vReq[API_V_EVENT_REF].To<uint64>(0);
			EAccessRuleType Action = (EAccessRuleType)vReq[API_V_EVENT_ACTION].To<int>();
			STATUS Status = theCore->ProcessList()->SetAccessEventAction(EventId, Action);
			RETURN_STATUS(Status);
		}

		// Volume Manager
		case SVC_API_VOL_CREATE_IMAGE:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			STATUS Status = m_pVolumeManager->CreateImage(vReq[API_V_VOL_PATH], vReq[API_V_VOL_PASSWORD], vReq.Get(API_V_VOL_SIZE).To<uint64>(0), vReq.Get(API_V_VOL_CIPHER).AsStr());
			RETURN_STATUS(Status);
		}
		case SVC_API_VOL_CHANGE_PASSWORD:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			STATUS Status = m_pVolumeManager->ChangeImagePassword(vReq[API_V_VOL_PATH], vReq[API_V_VOL_OLD_PASS], vReq[API_V_VOL_NEW_PASS]);
			RETURN_STATUS(Status);
		}
		//case SVC_API_VOL_DELETE_IMAGE:
		//{
		//	StVariant vReq(m_pMemPool);
		//	vReq.FromPacket(req);
		//
		//  STATUS Status = m_pVolumeManager->DeleteImage(vReq[API_V_VOL_PATH]);
		//	RETURN_STATUS(Status);
		//}

		case SVC_API_VOL_MOUNT_IMAGE:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			STATUS Status = m_pVolumeManager->MountImage(vReq[API_V_VOL_PATH], vReq[API_V_VOL_MOUNT_POINT], vReq[API_V_VOL_PASSWORD], vReq.Get(API_V_VOL_PROTECT).To<bool>(), vReq.Get(API_V_VOL_LOCKDOWN).To<bool>());
			RETURN_STATUS(Status);
		}
		case SVC_API_VOL_DISMOUNT_VOLUME:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			STATUS Status = m_pVolumeManager->DismountVolume(vReq[API_V_VOL_MOUNT_POINT].ToWString());
			RETURN_STATUS(Status);
		}
		case SVC_API_VOL_DISMOUNT_ALL:
		{
			STATUS Status = m_pVolumeManager->DismountAll();
			RETURN_STATUS(Status);
		}
		
		/*case SVC_API_VOL_GET_VOLUME_LIST:
		{
			auto Ret = m_pVolumeManager->GetVolumeList();
			if (Ret.IsError())
				return Ret.GetStatus();
				
			StVariant List(m_pMemPool);
			List.BeginList();
			for(std::wstring& DevicePath : Ret.GetValue())
				List.Write(DevicePath);
			List.Finish();

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_VOL_LIST] = List;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}*/
		case SVC_API_VOL_GET_ALL_VOLUMES:
		{
			auto Ret = m_pVolumeManager->GetAllVolumes();
			if (Ret.IsError())
				return Ret.GetStatus();
				
			StVariantWriter List(m_pMemPool);
			List.BeginList();
			for(auto& pVolume : Ret.GetValue())
				List.WriteVariant(pVolume->ToVariant(SVarWriteOpt(), m_pMemPool));

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_VOLUMES] = List.Finish();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_VOL_GET_VOLUME:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			CVolumePtr pVolume = m_pVolumeManager->GetVolume(vReq[API_V_GUID].AsStr());
			if (pVolume) {
				StVariant vRpl = pVolume->ToVariant(SVarWriteOpt(), m_pMemPool);
				vRpl.ToPacket(rpl);
				return STATUS_SUCCESS;
			}
			return STATUS_NOT_FOUND;
		}
		case SVC_API_VOL_SET_VOLUME:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			CVolumePtr pVolume = std::make_shared<CVolume>();
			pVolume->FromVariant(vReq);

			STATUS Status = m_pVolumeManager->SetVolume(pVolume);
			RETURN_STATUS(Status);
		}

		// Tweak Manager
		case SVC_API_GET_TWEAKS:
		{
			StVariant vRpl(m_pMemPool);
			SVarWriteOpt Opts;
			Opts.Flags |= SVarWriteOpt::eSaveAll;
			vRpl[API_V_TWEAKS] = theCore->TweakManager()->GetTweaks(pClient.PID, Opts, m_pMemPool);
			vRpl[API_V_REVISION] = theCore->TweakManager()->GetRevision();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_APPLY_TWEAK:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			STATUS Status = theCore->TweakManager()->ApplyTweak(vReq[API_V_ID], pClient.PID);
			RETURN_STATUS(Status);
		}
		case SVC_API_UNDO_TWEAK:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			STATUS Status = theCore->TweakManager()->UndoTweak(vReq[API_V_ID], pClient.PID);
			RETURN_STATUS(Status);
		}
		case SVC_API_APPROVE_TWEAK:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			STATUS Status = theCore->TweakManager()->ApproveTweak(vReq[API_V_ID], pClient.PID);
			RETURN_STATUS(Status);
		}

		case SVC_API_SET_PRESETS:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			std::set<CFlexGuid> ActivePresets = theCore->PresetManager()->GetActivePresets();
			for (const auto& PresetGuid : ActivePresets)
			{
				STATUS Status = theCore->PresetManager()->DeactivatePreset(PresetGuid, pClient.PID);
				if (Status.IsError())
					theCore->Log()->LogEventLine(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG,
						L"Failed to deactivate Preset %s before import", PresetGuid.ToWString().c_str());
			}

			STATUS Status = theCore->PresetManager()->SetPresets(vReq[API_V_PRESETS]);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}
		case SVC_API_GET_PRESETS:
		{
			StVariant vRpl(m_pMemPool);
			SVarWriteOpt Opts;
			Opts.Flags |= SVarWriteOpt::eSaveAll;
			vRpl[API_V_PRESETS] = theCore->PresetManager()->SaveEntries(Opts, m_pMemPool);
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_GET_PRESET:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);
			auto Ret = theCore->PresetManager()->GetEntry(vReq[API_V_GUID].AsStr(), SVarWriteOpt(), m_pMemPool);
			if (Ret.IsError()) {
				RETURN_STATUS(Ret);
			}

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_PRESET] = Ret.GetValue();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_PRESET:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			auto Status = theCore->PresetManager()->SetEntry(vReq[API_V_PRESET]);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}
		case SVC_API_DEL_PRESET:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);
			STATUS Status = theCore->PresetManager()->DelEntry(vReq[API_V_GUID].AsStr());
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}
		case SVC_API_ACTIVATE_PRESET:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);
			bool bForce = vReq.Get(API_V_FORCE).To<bool>(false);
			STATUS Status = theCore->PresetManager()->ActivatePreset(vReq[API_V_GUID].AsStr(), pClient.PID, bForce);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}
		case SVC_API_DEACTIVATE_PRESET:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);
			STATUS Status = theCore->PresetManager()->DeactivatePreset(vReq[API_V_GUID].AsStr(), pClient.PID);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_SET_WATCHED_PROG:
		{
			StVariant vReq(m_pMemPool);
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

		case SVC_API_SET_DAT_FILE:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			std::wstring FileName = vReq[API_V_NAME].AsStr();

			const wchar_t* ext = wcsrchr(FileName.c_str(), L'.');
			if (!ext || (_wcsicmp(ext, L".dat") != 0) || wcsstr(FileName.c_str(), L"..") != NULL)
				return STATUS_INVALID_FILE_FOR_SECTION;

			SNtObject Obj(L"\\??\\" + theCore->GetAppDir() + L"\\" + FileName);

			CBuffer Data = vReq[API_V_DATA];

			if (Data.GetSize() == 0) {
				NtDeleteFile(Obj.Get());
				return STATUS_SUCCESS;
			}

			IO_STATUS_BLOCK IoStatusBlock;
			CScopedHandle<HANDLE, NTSTATUS(*)(HANDLE)> handle(NULL, NtClose);
			NTSTATUS status = NtCreateFile(&handle, FILE_GENERIC_WRITE, Obj.Get(), &IoStatusBlock, NULL, 0, 0x00000007/*FILE_SHARE_VALID_FLAGS*/, FILE_OVERWRITE_IF, FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE, NULL, 0);
			if (NT_SUCCESS(status)) 
				status = NtWriteFile(handle, NULL, NULL, NULL, &IoStatusBlock, Data.GetBuffer(), (ULONG)Data.GetSize(), NULL, NULL);
	
			return status;
		}
		case SVC_API_GET_DAT_FILE:
		{
			return STATUS_NOT_IMPLEMENTED;
		}

		case SVC_API_GET_EVENT_LOG:
		{
			StVariant vRpl(m_pMemPool);

			SVarWriteOpt Opts;
			vRpl[API_V_EVENT_LOG] = m_pEventLog->SaveEntries(Opts, m_pMemPool);

			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_CLEAR_EVENT_LOG:
		{
			m_pEventLog->ClearEvents();
			RETURN_STATUS(OK);
		}

		case SVC_API_GET_SVC_STATS:
		{
			StVariant vRpl(m_pMemPool);

			PROCESS_MEMORY_COUNTERS pmc;
			if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
				vRpl[API_V_SVC_MEM_WS] = pmc.WorkingSetSize;
				vRpl[API_V_SVC_MEM_PB] = pmc.PagefileUsage;	
			}
			vRpl[API_V_LOG_MEM_USAGE] = theCore->ProgramManager()->GetLogMemUsage();

			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_SCRIPT_LOG:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			CFlexGuid Guid;
			Guid.FromVariant(vReq[API_V_GUID]);
			CJSEnginePtr pScript = theCore->GetScript(Guid, (EItemType)vReq[API_V_TYPE].To<uint32>());
			if(!pScript)
				return STATUS_INVALID_PARAMETER_1;

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_EVENT_LOG] = pScript->DumpLog(vReq.Get(API_V_LAST_ACTIVITY).To<uint32>(0), m_pMemPool);
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_CLEAR_SCRIPT_LOG:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			CFlexGuid Guid;
			Guid.FromVariant(vReq[API_V_GUID]);
			CJSEnginePtr pScript = theCore->GetScript(Guid, (EItemType)vReq[API_V_TYPE].To<uint32>());
			if(!pScript)
				return STATUS_INVALID_PARAMETER_1;

			pScript->ClearLog();
			return STATUS_SUCCESS;
		}

		case SVC_API_CALL_SCRIPT_FUNC:
		{
			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			CFlexGuid Guid;
			Guid.FromVariant(vReq[API_V_GUID]);
			CJSEnginePtr pScript = theCore->GetScript(Guid, (EItemType)vReq[API_V_TYPE].To<uint32>());
			if(!pScript)
				return STATUS_INVALID_PARAMETER_1;

			auto Ret = pScript->CallFunction(vReq[API_V_NAME].ToStringA().ConstData(), vReq.Get(API_V_PARAMS), pClient.PID);
			if (Ret.IsError()) {
				RETURN_STATUS(Ret);
			}
			StVariant vRpl(m_pMemPool);
			vRpl[API_V_DATA] = Ret.GetValue();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_SHOW_SECURE_PROMPT:
		{
			NTSTATUS status = STATUS_UNSUCCESSFUL;

			StVariant vReq(m_pMemPool);
			vReq.FromPacket(req);

			std::wstring Text = vReq[API_V_MB_TEXT].AsStr();
			//std::wstring Title = vReq.Get(API_V_MB_TITLE).AsStr();
			//if(Title.empty()) Title = L"MajorPrivacy";
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
							if (WaitForSingleObject(pi.hProcess, INFINITE) == 0)
							{
								DWORD Code = 0;
								if (GetExitCodeProcess(pi.hProcess, &Code))
								{
									StVariant vRpl(m_pMemPool);
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
				CServiceCore::Shutdown(CServiceCore::eShutdown_NoWait);

			StVariant vRpl(m_pMemPool);
			vRpl[API_V_PID] = (uint32)GetCurrentProcessId();
			vRpl.ToPacket(rpl);
			
			return Status.GetStatus();
		}

		default:
			return STATUS_UNSUCCESSFUL;
	}
}

int CServiceCore::BroadcastMessage(uint32 MessageID, const StVariant& MessageData, const std::shared_ptr<class CProgramFile>& pProgram)
{
	CBuffer MessageBuffer;
	MessageData.ToPacket(&MessageBuffer);

	if (!pProgram)
		return theCore->UserPipe()->BroadcastMessage(MessageID, &MessageBuffer);

	std::unique_lock Lock(m_ClientsMutex);
	auto Clients = m_Clients;
	Lock.unlock();

	int Success = 0;
	for(auto& I : Clients) {
		SClientPtr pClientData = I.second;
		std::unique_lock Lock(pClientData->Mutex);
		if(!pClientData->bWatchAllPrograms && pClientData->WatchedPrograms.find(pProgram->GetUID()) == pClientData->WatchedPrograms.end())
			continue;
		if(theCore->UserPipe()->BroadcastMessage(MessageID, &MessageBuffer, I.first))
			Success++;
	}
	return Success != 0;
}