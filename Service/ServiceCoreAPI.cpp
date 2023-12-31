#include "pch.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Common/Buffer.h"
#include "ServiceCore.h"
#include "ServiceAPI.h"
#include "../Library/API/PipeServer.h"
#include "../Library/API/AlpcPortServer.h"
#include "Network/Firewall/Firewall.h"
#include "Network/NetIsolator.h"
#include "Processes/AppIsolator.h"
#include "Filesystem/FSIsolator.h"
#include "Processes/ProcessList.h"
#include "Network/SocketList.h"
#include "Programs/ProgramManager.h"
#include "Tweaks/TweakManager.h"
#include "Network/Dns/DnsInspector.h"

void CServiceCore::RegisterUserAPI()
{
	m_pUserPipe->RegisterHandler(SVC_API_GET_VERSION, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_CONFIG, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_SET_CONFIG, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_FW_RULES, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_SET_FW_RULE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_FW_RULE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_DEL_FW_RULE, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_FW_PROFILE, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_SET_FW_PROFILE, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_PROCESSES, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_PROCESS, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_PROGRAMS, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_SOCKETS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_GET_TRAFFIC, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_DNC_CACHE, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_TRACE_LOG, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_GET_TWEAKS, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_APPLY_TWEAK, &CServiceCore::OnRequest, this);
	m_pUserPipe->RegisterHandler(SVC_API_UNDO_TWEAK, &CServiceCore::OnRequest, this);

	m_pUserPipe->RegisterHandler(SVC_API_SHUTDOWN, &CServiceCore::OnRequest, this);


	// todo port
}

uint32 CServiceCore::OnRequest(uint32 msgId, const CBuffer* req, CBuffer* rpl, uint32 PID, uint32 TID)
{
//#ifdef _DEBUG
//	DbgPrint("CServiceCore::OnRequest %d\n", msgId);
//#endif

	switch (msgId)
	{
		case SVC_API_GET_VERSION:
		{
			//CVariant vReq;
			//vReq.FromPacket(req);
			CVariant vRpl;
			vRpl[(uint32)'ver'] = 0x1234; // todo full version
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_CONFIG:
		{
			CVariant vReq;
			vReq.FromPacket(req);
			CVariant vRpl;
			if (vReq.Has(SVC_API_CONF_MAP))
			{
				CVariant vReqMap = vReq[SVC_API_CONF_MAP];
				CVariant vResMap;
				vResMap.BeginIMap();
				vReqMap.ReadRawList([&](const CVariant& vData) { 
					const char* Name = (const char*)vData.GetData();
					auto SectionKey = Split2(std::string(Name, Name + vData.GetSize()), "/");
					vResMap[Name] = svcCore->Config()->GetValue(SectionKey.first, SectionKey.second);
				});
				vResMap.Finish();
				vRpl[SVC_API_CONF_MAP] = vResMap;
			}
			else 
			{
				auto SectionKey = Split2(vReq[SVC_API_CONF_NAME], "/");
				vRpl[SVC_API_CONF_VALUE] = svcCore->Config()->GetValue(SectionKey.first, SectionKey.second);
			}
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_CONFIG:
		{
			CVariant vReq;
			vReq.FromPacket(req);
			if (vReq.Has(SVC_API_CONF_MAP))
			{
				CVariant vReqMap = vReq[SVC_API_CONF_MAP];
				vReqMap.ReadRawMap([&](const SVarName& Name, const CVariant& vData) { 
					auto SectionKey = Split2(std::string(Name.Buf, Name.Buf + Name.Len), "/");
					svcCore->Config()->SetValue(SectionKey.first, SectionKey.second, vData.AsStr());
				});
			}
			else 
			{
				auto SectionKey = Split2(vReq[SVC_API_CONF_NAME], "/");
				svcCore->Config()->SetValue(SectionKey.first, SectionKey.second, vReq[SVC_API_CONF_VALUE]);
			}
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_FW_RULES:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			std::map<std::wstring, CFirewallRulePtr> FwRules;
			if (!vReq.Has(SVC_API_PROG_IDS))
				FwRules = m_pNetIsolator->Firewall()->GetAllRules();
			else
			{
				CVariant IDs = vReq[SVC_API_PROG_IDS];
				for (uint32 i = 0; i < IDs.Count(); i++) 
				{
					CProgramID ID;
					ID.FromVariant(IDs[i]);
					auto Found = m_pNetIsolator->Firewall()->FindRules(ID);
					FwRules.merge(Found);
				}
			}

			CVariant Rules;
			for (auto I : FwRules)
				Rules.Append(I.second->ToVariant());

			CVariant vRpl;
			vRpl[SVC_API_RULES] = Rules;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_FW_RULE:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			CFirewallRulePtr FwRule = std::make_shared<CFirewallRule>();
			FwRule->FromVariant(vReq);

			STATUS Status = m_pNetIsolator->Firewall()->SetRule(FwRule);
			if (Status.IsError()) {
				CVariant vRpl;
				vRpl[SVC_API_ERR_CODE] = Status.GetStatus();
				vRpl[SVC_API_ERR_MSG] = Status.GetMessageText();
				vRpl.ToPacket(rpl);
			}
			return STATUS_SUCCESS;
		}
		case SVC_API_GET_FW_RULE:
		{
			CVariant vReq;
			vReq.FromPacket(req);
				
			CFirewallRulePtr pRule = m_pNetIsolator->Firewall()->GetRule(vReq[SVC_API_FW_GUID].AsStr());
			if (pRule) {
				CVariant vRpl = pRule->ToVariant();
				vRpl.ToPacket(rpl);
				return STATUS_SUCCESS;
			}
			return STATUS_NOT_FOUND;
		}
		case SVC_API_DEL_FW_RULE:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = m_pNetIsolator->Firewall()->DelRule(vReq[SVC_API_FW_GUID].AsStr());
			if (Status.IsError()) {
				CVariant vRpl;
				vRpl[SVC_API_ERR_CODE] = Status.GetStatus();
				vRpl[SVC_API_ERR_MSG] = Status.GetMessageText();
				vRpl.ToPacket(rpl);
			}
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_FW_PROFILE:
		{
			FwFilteringModes Mode = m_pNetIsolator->Firewall()->GetFilteringMode();
			CVariant vRpl;
			switch(Mode)
			{
				case FwFilteringModes::NoFiltering: vRpl[SVC_API_FW_PROFILE] = SVC_API_FW_NONE; break;
				case FwFilteringModes::BlockList: vRpl[SVC_API_FW_PROFILE] = SVC_API_FW_BLOCK; break;
				case FwFilteringModes::AllowList: vRpl[SVC_API_FW_PROFILE] = SVC_API_FW_ALLOW; break;
			}
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_SET_FW_PROFILE:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			if (vReq[SVC_API_FW_PROFILE].To<std::string>() == SVC_API_FW_NONE) 
				m_pNetIsolator->Firewall()->SetFilteringMode(FwFilteringModes::NoFiltering);
			else if (vReq[SVC_API_FW_PROFILE].To<std::string>() == SVC_API_FW_BLOCK) 
				m_pNetIsolator->Firewall()->SetFilteringMode(FwFilteringModes::BlockList);
			else if (vReq[SVC_API_FW_PROFILE].To<std::string>() == SVC_API_FW_ALLOW) 
				m_pNetIsolator->Firewall()->SetFilteringMode(FwFilteringModes::AllowList);
			else 
				return STATUS_INVALID_PARAMETER;
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_PROCESSES:
		{
			CVariant Processes;
			for (auto pProcess: svcCore->ProcessList()->List())
				Processes.Append(pProcess.second->ToVariant());

			CVariant vRpl;
			vRpl[SVC_API_PROCESSES] = Processes;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_GET_PROCESS:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			uint64 Pid = vReq[SVC_API_PROC_PID];
			CProcessPtr pProcess = svcCore->ProcessList()->GetProcess(Pid);
			if (!pProcess)
				return STATUS_INVALID_CID;

			CVariant vRpl = pProcess->ToVariant();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_PROGRAMS:
		{
			CVariant vRpl;

			//vRpl[SVC_API_PROGRAMS] = svcCore->ProgramManager()->GetRoot()->ToVariant();
			
			// todo restrict to a certain type by argument

			CVariant Programs;
			Programs.BeginList();
			for (auto pItem: svcCore->ProgramManager()->GetItems())
				Programs.WriteVariant(pItem.second->ToVariant());
			Programs.Finish();

			vRpl[SVC_API_PROGRAMS] = Programs;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_SOCKETS:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			std::set<CSocketPtr> Sockets;
			if (!vReq.Has(SVC_API_PROG_IDS))
				Sockets = m_pNetIsolator->SocketList()->GetAllSockets();
			else
			{
				CVariant IDs = vReq[SVC_API_PROG_IDS];
				for (uint32 i = 0; i < IDs.Count(); i++) 
				{
					CProgramID ID;
					ID.FromVariant(IDs[i]);
					auto Found = m_pNetIsolator->SocketList()->FindSockets(ID);
					Sockets.merge(Found);
				}
			}

			CVariant SocketList;
			for (auto pSockets : Sockets)
				SocketList.Append(pSockets->ToVariant());

			CVariant vRpl;
			vRpl[SVC_API_SOCKETS] = SocketList;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_TRAFFIC:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[SVC_API_ID_PROG]);

			CProgramItemPtr pItem = svcCore->ProgramManager()->GetProgramByID(ID, CProgramManager::eDontAdd);
			if (!pItem)
				return STATUS_NOT_FOUND;

			CTrafficLog* pTrafficLog = NULL;
			CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem);
			if (pProgram)
				pTrafficLog = pProgram->TrafficLog();
			else {
				CWindowsServicePtr pService = std::dynamic_pointer_cast<CWindowsService>(pItem);
				if (pService) pTrafficLog = pService->TrafficLog();
			}
			if(!pTrafficLog)
				return STATUS_OBJECT_TYPE_MISMATCH;

			CVariant vRpl;
			vRpl[SVC_API_NET_TRAFFIC] = pTrafficLog->ToVariant(vReq.Get(SVC_API_NET_LAST_ACT));
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_DNC_CACHE:
		{
			CVariant vRpl;
			vRpl[SVC_API_DNS_CACHE] = svcCore->NetIsolator()->DnsInspector()->DumpDnsCache();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_TRACE_LOG:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			CProgramID ID;
			ID.FromVariant(vReq[SVC_API_ID_PROG]);

			CProgramItemPtr pItem = svcCore->ProgramManager()->GetProgramByID(ID, CProgramManager::eDontAdd);
			if (!pItem)
				return STATUS_NOT_FOUND;
			CProgramFilePtr pProgram = std::dynamic_pointer_cast<CProgramFile>(pItem);
			if (!pProgram)
				return STATUS_OBJECT_TYPE_MISMATCH;

			CVariant LogType = vReq[SVC_API_LOG_TYPE];
			ETraceLogs Type = ETraceLogs::eLogMax;
			if (LogType == SVC_API_EXEC_LOG)		Type = ETraceLogs::eExecLog;
			else if (LogType == SVC_API_NET_LOG)	Type = ETraceLogs::eNetLog;
			else if (LogType == SVC_API_FS_LOG)		Type = ETraceLogs::eFSLog;
			else if (LogType == SVC_API_REG_LOG)	Type = ETraceLogs::eRegLog;
			if (Type >= ETraceLogs::eLogMax)
				return STATUS_INVALID_PARAMETER;

			std::vector<CTraceLogEntryPtr> TraceLog = pProgram->GetTraceLog(Type);

			CVariant LogData;
			LogData.BeginList();
			for (auto pEntry : TraceLog)
				LogData.WriteVariant(pEntry->ToVariant());
			LogData.Finish();

			CVariant vRpl;
			vRpl[SVC_API_LOG_DATA] = LogData;
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}

		case SVC_API_GET_TWEAKS:
		{
			CVariant vRpl;
			vRpl[SVC_API_TWEAKS] = svcCore->TweakManager()->GetRoot()->ToVariant();
			vRpl.ToPacket(rpl);
			return STATUS_SUCCESS;
		}
		case SVC_API_APPLY_TWEAK:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = svcCore->TweakManager()->ApplyTweak(vReq[SVC_API_TWEAK_NAME]);
			if (Status.IsError()) {
				CVariant vRpl;
				vRpl[SVC_API_ERR_CODE] = Status.GetStatus();
				vRpl[SVC_API_ERR_MSG] = Status.GetMessageText();
				vRpl.ToPacket(rpl);
			}
			return STATUS_SUCCESS;
		}
		case SVC_API_UNDO_TWEAK:
		{
			CVariant vReq;
			vReq.FromPacket(req);

			STATUS Status = svcCore->TweakManager()->UndoTweak(vReq[SVC_API_TWEAK_NAME]);
			if (Status.IsError()) {
				CVariant vRpl;
				vRpl[SVC_API_ERR_CODE] = Status.GetStatus();
				vRpl[SVC_API_ERR_MSG] = Status.GetMessageText();
				vRpl.ToPacket(rpl);
			}
			return STATUS_SUCCESS;
		}

		case SVC_API_SHUTDOWN:
		{
			if (svcCore->m_bEngineMode) {
				svcCore->m_Terminate = true;
				return STATUS_SUCCESS;
			}
			return STATUS_INVALID_DEVICE_REQUEST;
		}

		default:
			return STATUS_UNSUCCESSFUL;
	}
}