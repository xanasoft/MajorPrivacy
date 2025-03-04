#include "pch.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Common/FileIO.h"
#include "../Library/Common/Buffer.h"
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

	m_pUserPipe->RegisterHandler(SVC_API_SET_WATCHED_PROG, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_SHUTDOWN, &CServiceCore::OnRequest, this);


	// todo port
}

void CServiceCore::OnClient(uint32 uEvent, struct SPipeClientInfo& pClient)
{
	std::unique_lock Lock(m_ClientsMutex);
	switch (uEvent)
	{
		case CPipeServer::eClientConnected:
			DbgPrint("Client connected %d %d\n", pClient.PID, pClient.TID);
			m_Clients[pClient.PID] = std::make_shared<SClient>();
			break;
		case CPipeServer::eClientDisconnected:
			DbgPrint("Client disconnected %d %d\n", pClient.PID, pClient.TID);
			m_Clients.erase(pClient.PID);
			break;
	}
}

#define RETURN_STATUS(Status) if (Status.IsError()) { \
CVariant vRpl; vRpl[API_V_ERR_CODE] = Status.GetStatus(); \
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

	switch (msgId)
	{
		case SVC_API_GET_VERSION:
		{
			CVariant vRpl;
			vRpl[API_V_VERSION] = MY_ABI_VERSION;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_CONFIG_DIR:
		{
			CVariant vRpl;
			vRpl[API_V_VALUE] = theCore->GetDataFolder();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_GET_CONFIG:
		{
			CVariant vReq;
			vReq.FromPacket(req);
			CVariant vRpl;
			/*if (vReq.Has(API_V_CONF_MAP))
			{
				CVariant vReqMap = vReq[API_V_CONF_MAP];
				CVariant vResMap;
				vResMap.BeginIMap();
				vReqMap.ReadRawList([&](const CVariant& vData) {
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
				CVariant Data;
				Data.BeginMap();
				for(auto Section: theCore->Config()->ListSections())
				{
					CVariant Values;
					Values.BeginMap();
					for(auto Key: theCore->Config()->ListKeys(Section))
						Values.Write(Key.c_str(), theCore->Config()->GetValue(Section, Key));
					Values.Finish();
					Data.WriteVariant(Section.c_str(), Values);
				}
				Data.Finish();

				vRpl.BeginIMap();
				vRpl.WriteVariant(API_V_DATA, Data);
				vRpl.Finish();
			}
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_CONFIG:
		{
			CVariant vReq;
			vReq.FromPacket(req);
			/*if (vReq.Has(API_V_CONF_MAP))
			{
				CVariant vReqMap = vReq[API_V_CONF_MAP];
				vReqMap.ReadRawMap([&](const SVarName& Name, const CVariant& vData) { 
					auto SectionKey = Split2(std::string(Name.Buf, Name.Buf + Name.Len), "/");
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
				CVariant Data = vReq[API_V_DATA];
				Data.ReadRawMap([&](const SVarName& Section, const CVariant& Values) { 
					Values.ReadRawMap([&](const SVarName& Key, const CVariant& Value) {
						theCore->Config()->SetValue(Section.Buf, Key.Buf, Value.AsStr());
					});
				});

			}
			return STATUS_SUCCESS;
		}

		case SVC_API_CHECK_CONFIG_FILE:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			std::wstring Path = vReq[API_V_FILE_PATH];
			if (Path.empty())
				return STATUS_INVALID_PARAMETER;
			if (Path.front() != L'\\')
				Path = L"\\" + Path;
			std::wstring FilePath = theCore->GetDataFolder() + Path;

			CVariant vRpl = CVariant(FileExists(FilePath));
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_CONFIG_FILE:
		{
			CVariant vReq;
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
			CVariant vReq;
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

			CVariant vRpl;
			vRpl[API_V_DATA] = Data;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_DEL_CONFIG_FILE:
		{
			CVariant vReq;
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
			CVariant vReq;
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

			CVariant vRpl;
			vRpl[API_V_FILES] = Files;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_CONFIG_STATUS:
		{
			uint32 Status = 0;
			if(theCore->IsConfigDirty())
				Status |= CONFIG_STATUS_DIRTY;

			CVariant vRpl(Status);
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
			CVariant vReq;
			vReq.FromPacket(req);

			bool bReload = vReq.Get(API_V_RELOAD).To<bool>(false);
			if (bReload)
				m_pNetworkManager->Firewall()->LoadRules();

			std::map<std::wstring, CFirewallRulePtr> FwRules;
			if (!vReq.Has(API_V_PROG_IDS))
				FwRules = m_pNetworkManager->Firewall()->GetAllRules();
			else
			{
				CVariant IDs = vReq[API_V_PROG_IDS];
				for (uint32 i = 0; i < IDs.Count(); i++) 
				{
					CProgramID ID;
					ID.FromVariant(IDs[i]);
					auto Found = m_pNetworkManager->Firewall()->FindRules(ID);
					FwRules.merge(Found);
				}
			}

			CVariant Rules;
			for (auto I : FwRules)
				Rules.Append(I.second->ToVariant(SVarWriteOpt()));

			CVariant vRpl;
			vRpl[API_V_FW_RULES] = Rules;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_FW_RULE:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			CFirewallRulePtr FwRule = std::make_shared<CFirewallRule>();
			FwRule->FromVariant(vReq);

			STATUS Status = m_pNetworkManager->Firewall()->SetRule(FwRule);
			RETURN_STATUS(Status);
		}
		case SVC_API_GET_FW_RULE:
		{
			CVariant vReq;
			vReq.FromPacket(req);
				
			CFirewallRulePtr pRule = m_pNetworkManager->Firewall()->GetRule(vReq[API_V_GUID].AsStr());
			if (pRule) {
				CVariant vRpl = pRule->ToVariant(SVarWriteOpt());
				vRpl.ToPacket(rpl);
				return STATUS_SUCCESS;
			}
			return STATUS_NOT_FOUND;
		}
		case SVC_API_DEL_FW_RULE:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = m_pNetworkManager->Firewall()->DelRule(vReq[API_V_GUID].AsStr());
			RETURN_STATUS(Status);
		}

		case SVC_API_GET_FW_PROFILE:
		{
			FwFilteringModes Mode = m_pNetworkManager->Firewall()->GetFilteringMode();
			CVariant vRpl;
			vRpl[API_V_FW_RULE_FILTER_MODE] = (uint32)Mode;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_FW_PROFILE:
		{
			CVariant vReq;
			vReq.FromPacket(req);
			FwFilteringModes Mode = (FwFilteringModes)vReq[API_V_FW_RULE_FILTER_MODE].To<uint32>();
			STATUS Status = m_pNetworkManager->Firewall()->SetFilteringMode(Mode);
			RETURN_STATUS(Status);
		}

		case SVC_API_GET_FW_AUDIT_MODE:
		{
			FwAuditPolicy Mode = m_pNetworkManager->Firewall()->GetAuditPolicy();
			CVariant vRpl;
			vRpl[API_V_FW_AUDIT_MODE] = (uint32)Mode;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_FW_AUDIT_MODE:
		{
			CVariant vReq;
			vReq.FromPacket(req);
			FwAuditPolicy Mode = (FwAuditPolicy)vReq[API_V_FW_AUDIT_MODE].To<uint32>();
			STATUS Status = m_pNetworkManager->Firewall()->SetAuditPolicy(Mode);
			RETURN_STATUS(Status);
		}

		case SVC_API_GET_SOCKETS:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			std::set<CSocketPtr> Sockets;
			if (!vReq.Has(API_V_PROG_IDS))
				Sockets = m_pNetworkManager->SocketList()->GetAllSockets();
			else
			{
				CVariant IDs = vReq[API_V_PROG_IDS];
				for (uint32 i = 0; i < IDs.Count(); i++) 
				{
					CProgramID ID;
					ID.FromVariant(IDs[i]);
					auto Found = m_pNetworkManager->SocketList()->FindSockets(ID);
					Sockets.merge(Found);
				}
			}

			CVariant SocketList;
			for (auto pSockets : Sockets)
				SocketList.Append(pSockets->ToVariant());

			CVariant vRpl;
			vRpl[API_V_SOCKETS] = SocketList;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_TRAFFIC:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_PROG_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;

			CVariant vRpl;
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
			CVariant vRpl;
			vRpl[API_V_DNS_CACHE] = theCore->NetworkManager()->DnsInspector()->DumpDnsCache();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_FLUSH_DNS_CACHE:
		{
			theCore->NetworkManager()->DnsInspector()->FlushDnsCache();
			return STATUS_SUCCESS;
		}

		// Process Manager
		case SVC_API_GET_PROCESSES:
		{
			CVariant Processes;
			for (auto pProcess: theCore->ProcessList()->List())
				Processes.Append(pProcess.second->ToVariant(SVarWriteOpt()));

			CVariant vRpl;
			vRpl[API_V_PROCESSES] = Processes;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_GET_PROCESS:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			uint64 Pid = vReq[API_V_PID];
			CProcessPtr pProcess = theCore->ProcessList()->GetProcess(Pid, true);
			if (!pProcess)
				return STATUS_INVALID_CID;

			CVariant vRpl = pProcess->ToVariant(SVarWriteOpt());
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_PROGRAMS:
		{
			CVariant vRpl;

			//vRpl[API_V_PROGRAMS] = theCore->ProgramManager()->GetRoot()->ToVariant();

			// todo restrict to a certain type by argument

			CVariant Programs;
			Programs.BeginList();
			for (auto pItem: theCore->ProgramManager()->GetItems())
				Programs.WriteVariant(pItem.second->ToVariant(SVarWriteOpt()));
			Programs.Finish();

			vRpl[API_V_PROGRAMS] = Programs;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_LIBRARIES:
		{
			CVariant vRpl;

			//CVariant vReq;
			//vReq.FromPacket(req);

			//uint64 CacheToken = vReq.Get(API_V_CACHE_TOKEN).To<uint64>(-1);
			//if(!CacheToken || CacheToken != pClientData->LibraryCacheToken)
			//{
			//	pClientData->LibraryCacheToken = CacheToken;
			//	CacheToken = pClientData->LibraryCacheToken = rand();
			//	pClientData->LibraryCache.clear();
			//}

			//std::map<uint64, uint64> OldCache = pClientData->LibraryCache;

			CVariant Libraries;
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
			Libraries.Finish();

			//CVariant OlsLibraries;
			//OlsLibraries.BeginList();
			//for (auto I : OldCache) {
			//	pClientData->LibraryCache.erase(I.first);
			//	OlsLibraries.Write(I.first);
			//}
			//OlsLibraries.Finish();

			vRpl[API_V_LIBRARIES] = Libraries;
			//vRpl[API_V_OLD_LIBRARIES] = OlsLibraries;
			//vRpl[API_V_CACHE_TOKEN] = CacheToken;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_SET_PROGRAM:
		{
			CVariant vReq;
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
				ID.FromVariant(vReq[API_V_PROG_ID]);
				auto Ret = theCore->ProgramManager()->CreateProgram(ID);
				if (Ret.IsError()) {
					RETURN_STATUS(Ret);
				}
				pItem = Ret.GetValue();
			}

			if(vReq.Has(API_V_NAME)) pItem->SetName(vReq[API_V_NAME]);
			if(vReq.Has(API_V_ICON)) pItem->SetIcon(vReq[API_V_ICON]);
			if(vReq.Has(API_V_INFO)) pItem->SetInfo(vReq[API_V_INFO]);

			theCore->SetConfigDirty(true);

			CVariant vRpl;
			vRpl[API_V_PROG_UID] = pItem->GetUID();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_ADD_PROGRAM:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->ProgramManager()->AddProgramTo(vReq[API_V_PROG_UID], vReq[API_V_PROG_PARENT]);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_REMOVE_PROGRAM:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->ProgramManager()->RemoveProgramFrom(vReq[API_V_PROG_UID], vReq[API_V_PROG_PARENT], vReq[API_V_DEL_WITH_RULES]);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_CLEANUP_PROGRAMS:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->ProgramManager()->CleanUp(vReq[API_V_PURGE_RULES]);
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_REGROUP_PROGRAMS:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->ProgramManager()->ReGroup();
			if(Status)
				theCore->SetConfigDirty(true);
			RETURN_STATUS(Status);
		}

		case SVC_API_START_SECURE:
		{
			CVariant vReq;
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

			/*
            WTSQueryUserToken(CallerSession, &PrimaryTokenHandle);

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
			if (CreateProcessAsUserW(hDupToken, NULL, (wchar_t*)path.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
			{
				CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pi.dwProcessId, true);
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
			CVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_PROG_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;
			CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem);
			if (!pProgram)
				return STATUS_OBJECT_TYPE_MISMATCH;

			ETraceLogs Type = (ETraceLogs)vReq[API_V_LOG_TYPE].To<uint32>();
			if (Type >= ETraceLogs::eLogMax)
				return STATUS_INVALID_PARAMETER;

			CProgramFile::STraceLog TraceLog = pProgram->GetTraceLog(Type);

			CVariant LogData;
			LogData.BeginList();
			for (auto pEntry : TraceLog.Entries)
				LogData.WriteVariant(pEntry->ToVariant());
			LogData.Finish();

			CVariant vRpl;
			vRpl[API_V_LOG_OFFSET] = TraceLog.IndexOffset;
			vRpl[API_V_LOG_DATA] = LogData;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_LIBRARY_STATS:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_PROG_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;
			CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem);
			if (!pProgram)
				return STATUS_OBJECT_TYPE_MISMATCH;

			CVariant vRpl;
			vRpl[API_V_LIBRARIES] = pProgram->DumpLibraries();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_EXEC_STATS:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_PROG_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;

			CVariant vRpl;
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
			CVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_PROG_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;

			CVariant vRpl;
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
			CVariant vReq;
			vReq.FromPacket(req);

			std::set<CHandlePtr> Handles;
			if (!vReq.Has(API_V_PROG_ID))
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
				ID.FromVariant(vReq[API_V_PROG_ID]);

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
			CVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[API_V_PROG_ID]);

			CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(ID, false);
			if (!pItem)
				return STATUS_NOT_FOUND;

			CVariant vRpl;
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
			CVariant vReq;
			vReq.FromPacket(req);

			std::set<CHandlePtr> Handles;
			if (!vReq.Has(API_V_PROG_IDS))
				Handles = m_pAccessManager->HandleList()->GetAllHandles();
			else
			{
				CVariant IDs = vReq[API_V_PROG_IDS];
				for (uint32 i = 0; i < IDs.Count(); i++) 
				{
					CProgramID ID;
					ID.FromVariant(IDs[i]);
					auto Found = m_pAccessManager->HandleList()->FindHandles(ID);
					Handles.merge(Found);
				}
			}

			CVariant HandleList;
			for (auto pHandles : Handles)
				HandleList.Append(pHandles->ToVariant());

			CVariant vRpl;
			vRpl[API_V_HANDLES] = HandleList;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_CLEAR_LOGS:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			ETraceLogs Log = (ETraceLogs)vReq[API_V_LOG_TYPE].To<int>();

			std::set<CHandlePtr> Handles;
			if (!vReq.Has(API_V_PROG_ID))
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
				ID.FromVariant(vReq[API_V_PROG_ID]);

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
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->VolumeManager()->CreateImage(vReq[API_V_VOL_PATH], vReq[API_V_VOL_PASSWORD], vReq.Get(API_V_VOL_SIZE).To<uint64>(0), vReq.Get(API_V_VOL_CIPHER).AsStr());
			RETURN_STATUS(Status);
		}
		case SVC_API_VOL_CHANGE_PASSWORD:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->VolumeManager()->ChangeImagePassword(vReq[API_V_VOL_PATH], vReq[API_V_VOL_OLD_PASS], vReq[API_V_VOL_NEW_PASS]);
			RETURN_STATUS(Status);
		}
		//case SVC_API_VOL_DELETE_IMAGE:
		//{
		//	CVariant vReq;
		//	vReq.FromPacket(req);
		//
		//  STATUS Status = theCore->VolumeManager()->DeleteImage(vReq[API_V_VOL_PATH]);
		//	RETURN_STATUS(Status);
		//}

		case SVC_API_VOL_MOUNT_IMAGE:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->VolumeManager()->MountImage(vReq[API_V_VOL_PATH], vReq[API_V_VOL_MOUNT_POINT], vReq[API_V_VOL_PASSWORD], vReq[API_V_VOL_PROTECT]);
			RETURN_STATUS(Status);
		}
		case SVC_API_VOL_DISMOUNT_VOLUME:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->VolumeManager()->DismountVolume(vReq[API_V_VOL_MOUNT_POINT]);
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
				
			CVariant List;
			List.BeginList();
			for(std::wstring& DevicePath : Ret.GetValue())
				List.Write(DevicePath);
			List.Finish();

			CVariant vRpl;
			vRpl[API_V_VOL_LIST] = List;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}*/
		case SVC_API_VOL_GET_ALL_VOLUMES:
		{
			auto Ret = theCore->VolumeManager()->GetAllVolumes();
			if (Ret.IsError())
				return Ret.GetStatus();
				
			CVariant List;
			List.BeginList();
			for(auto& pVolume : Ret.GetValue())
				List.WriteVariant(pVolume->ToVariant());
			List.Finish();

			CVariant vRpl;
			vRpl[API_V_VOLUMES] = List;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_VOL_GET_VOLUME:
		{
			CVariant vReq;
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
			CVariant vRpl;
			vRpl[API_V_TWEAKS] = theCore->TweakManager()->GetRoot()->ToVariant(SVarWriteOpt());
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_APPLY_TWEAK:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->TweakManager()->ApplyTweak(vReq[API_V_NAME]);
			RETURN_STATUS(Status);
		}
		case SVC_API_UNDO_TWEAK:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = theCore->TweakManager()->UndoTweak(vReq[API_V_NAME]);
			RETURN_STATUS(Status);
		}

		case SVC_API_SET_WATCHED_PROG:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			std::unique_lock Lock(pClientData->Mutex);
			pClientData->bWatchAllPrograms = false;
			pClientData->WatchedPrograms.clear();
			if (vReq.Has(API_V_PROG_UIDS))
			{
				uint64 AllID = m_pProgramManager->GetAllItem()->GetUID();
				CVariant IDs = vReq[API_V_PROG_UIDS];
				for (uint32 i = 0; i < IDs.Count(); i++) {
					uint64 UID = IDs[i];
					if(UID == AllID)
						pClientData->bWatchAllPrograms = true;
					pClientData->WatchedPrograms.insert(UID);
				}
			}
			return STATUS_SUCCESS;
		}

		case SVC_API_SHUTDOWN:
		{
			if (theCore->m_bEngineMode) {
				CServiceCore::Shutdown(false);
				return STATUS_SUCCESS;
			}
			return STATUS_INVALID_DEVICE_REQUEST;
		}

		default:
			return STATUS_UNSUCCESSFUL;
	}
}

void CServiceCore::BroadcastMessage(uint32 MessageID, const CVariant& MessageData, const std::shared_ptr<class CProgramFile>& pProgram)
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