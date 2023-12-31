#include "pch.h"
#include "PrivacyCore.h"
#include "ProcessList.h"
#include "Programs/ProgramManager.h"
#include "Network/NetworkManager.h"
#include "Network/NetLogEntry.h"
#include "Tweaks/TweakManager.h"

#include <phnt_windows.h>
#include <phnt.h>

#include "../Library/Helpers/AppUtil.h"
#include "../Service/ServiceAPI.h"

CSettings* theConf = NULL;
CPrivacyCore* theCore = NULL;

#ifdef _DEBUG
void XVariantTest();
#endif

CPrivacyCore::CPrivacyCore(QObject* parent)
: QObject(parent)
{
#ifdef _DEBUG
	XVariantTest();
#endif
	
	m_pProcesses = new CProcessList(this);
	m_pPrograms = new CProgramManager(this);

	m_pNetwork = new CNetworkManager(this);

	m_pTweaks = new CTweakManager(this);

	m_Service.RegisterEventHandler(SVC_API_EVENT_RULE_CHANGED, &CPrivacyCore::OnEventChanged, this);
	m_Service.RegisterEventHandler(SVC_API_EVENT_NET_ACTIVITY, &CPrivacyCore::OnEventChanged, this);
}

CPrivacyCore::~CPrivacyCore()
{
}

STATUS CPrivacyCore::Connect()
{
	STATUS Status;
#if 0
	if(IsRunningElevated())
		Status = m_Service.ConnectEngine();
	else // todo: fix me add ability to install and start service as non admin !!!!!
		Status = m_Service.ConnectSvc();
	//if (Status)
	//	Status = m_Driver.ConnectDrv();
#else
	Status = m_Service.ConnectEngine();
#endif
	return Status;
}

void CPrivacyCore::Disconnect()
{
	m_Service.DisconnectSvc();
	//m_Driver.DisconnectDrv();

	m_FwRulesUpdated = false;
}

void CPrivacyCore::Update()
{
	m_pProcesses->Update();
	m_pPrograms->Update();

	QMutexLocker Lock(&m_EventQueueMutex);
	auto RuleEvents = m_EventQueue.take(SVC_API_EVENT_RULE_CHANGED);
	auto NetEvents = m_EventQueue.take(SVC_API_EVENT_NET_ACTIVITY);
	Lock.unlock();

	if (m_FwRulesUpdated) 
	{
		foreach(const XVariant& vEvent, RuleEvents) 
		{
			QString RuleId = vEvent[SVC_API_FW_GUID].AsQStr();
			if (vEvent[SVC_API_FW_CHANGE] == SVC_API_FW_REMOVED)
				m_pNetwork->RemoveFwRule(RuleId);
			else
				m_pNetwork->UpdateFwRule(RuleId);
		}
	}
	else if(m_pNetwork->UpdateAllFwRules())
		m_FwRulesUpdated = true;

	foreach(const XVariant& Event, NetEvents)
	{
		//QJsonDocument doc(QJsonValue::fromVariant(Event.ToQVariant()).toObject());			
		//QByteArray data = doc.toJson();

		CProgramID ID;
		ID.FromVariant(Event[SVC_API_ID_PROG]);

		CProgramFilePtr pProgram = m_pPrograms->GetProgramFile(ID.GetFilePath());
		if (!pProgram)
			continue;

		CNetLogEntry* pNetEnrty = new CNetLogEntry();
		CLogEntryPtr pEntry = CLogEntryPtr(pNetEnrty);
		pEntry->FromVariant(Event[SVC_API_EVENT_DATA]);
		pProgram->TraceLogAdd(ETraceLogs::eNetLog, pEntry, Event[SVC_API_EVENT_INDEX]);

		if (pNetEnrty->GetState() == SVC_API_EVENT_UNRULED)
			emit UnruledFwEvent(pProgram, pEntry);
	}
}

void CPrivacyCore::OnEventChanged(uint32 MessageId, const CBuffer* pEvent)
{
	// WARNING: this function is invoked from a worker thread !!!

	XVariant vEvent;
	vEvent.FromPacket(pEvent);

	QMutexLocker Lock(&m_EventQueueMutex);
	m_EventQueue[MessageId].enqueue(vEvent);
}

STATUS CPrivacyCore::StartProcessInEnvlave(const QString& Command, uint32 EnclaveId)
{
	//return m_Driver.StartProcessInEnvlave(Command.toStdWString(), EnclaveId);
	return ERR(STATUS_NOT_IMPLEMENTED); // todo
}

XVariant CPrivacyCore::MakeIDs(const QList<const class CProgramItem*>& Nodes)
{
	XVariant IDs;
	foreach(auto Item, Nodes) 
	{
		/*XVariant ID;
		if (const CAppPackage* pApp = qobject_cast<const CAppPackage*>(Item)) {
			ID[SVC_API_ID_TYPE] = SVC_API_ID_TYPE_PACKAGE;
			ID[SVC_API_ID_APP_SID] = pApp->GetAppSid();
		}
		else if (const CAppInstallation* pSvc = qobject_cast<const CAppInstallation*>(Item)) {
			ID[SVC_API_ID_TYPE] = SVC_API_ID_TYPE_INSTALL;
			ID[SVC_API_ID_REG_KEY] = pSvc->GetRegKey();
			ID[SVC_API_ID_FILE_PATH] = pSvc->GetPath();
		}
		else if (const CWindowsService* pSvc = qobject_cast<const CWindowsService*>(Item)) {
			ID[SVC_API_ID_TYPE] = SVC_API_ID_TYPE_SERVICE;
			ID[SVC_API_ID_SVC_TAG] = pSvc->GetSvcTag();
			//ID[SVC_API_ID_FILE_PATH] = pSvc->GetPath();
		}
		else if (const CProgramPattern* pFile = qobject_cast<const CProgramPattern*>(Item)) {
			ID[SVC_API_ID_TYPE] = SVC_API_ID_TYPE_PATTERN;
			ID[SVC_API_ID_FILE_PATH] = pFile->GetPattern();
		}
		else if (const CProgramFile* pFile = qobject_cast<const CProgramFile*>(Item)) {
			ID[SVC_API_ID_TYPE] = SVC_API_ID_TYPE_FILE;
			ID[SVC_API_ID_FILE_PATH] = pFile->GetPath();
		}
		else if (const CAllProgram* pAll = qobject_cast<const CAllProgram*>(Item)) {
			ID[SVC_API_ID_TYPE] = SVC_API_ID_TYPE_All;
		}
		else {
			//Q_ASSERT(0); // valid for root entry just drop it
			continue;
		}
		IDs.Append(ID);*/
		IDs.Append(Item->GetID().ToVariant());
	}
	return IDs;
}

#define RET_AS_XVARIANT(r) \
auto Ret = r; \
if (Ret.IsError()) \
return ERR(Ret.GetStatus()); \
RETURN((XVariant&)Ret.GetValue());

#define RET_GET_XVARIANT(r, n) \
auto Ret = r; \
if (Ret.IsError()) \
return ERR(Ret.GetStatus()); \
CVariant& Res = Ret.GetValue(); \
RETURN((XVariant&)Res.Get(n));

RESULT(XVariant) CPrivacyCore::GetConfig(const QString& Name) 
{
	CVariant Request;
	Request[SVC_API_CONF_NAME] = Name.toStdString();
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_CONFIG, Request), SVC_API_CONF_VALUE);
}

STATUS CPrivacyCore::SetConfig(const QString& Name, const XVariant& Value)
{
	CVariant Request;
	Request[SVC_API_CONF_NAME] = Name.toStdString();
	Request[SVC_API_CONF_VALUE] = Value;
	return m_Service.Call(SVC_API_SET_CONFIG, Request);
}

RESULT(XVariant) CPrivacyCore::GetConfigMap(const XVariant& NameList)
{
	CVariant Request;
	Request[SVC_API_CONF_MAP] = NameList;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_CONFIG, Request), SVC_API_CONF_MAP);
}

STATUS CPrivacyCore::SetConfigMap(const XVariant& ValueMap)
{
	CVariant Request;
	Request[SVC_API_CONF_MAP] = ValueMap;
	return m_Service.Call(SVC_API_SET_CONFIG, Request);
}

RESULT(XVariant) CPrivacyCore::GetProcesses()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_PROCESSES, Request), SVC_API_PROCESSES);
}

RESULT(XVariant) CPrivacyCore::GetProcess(uint64 Pid)
{
	CVariant Request;
	Request[SVC_API_PROC_PID] = Pid;
	RET_AS_XVARIANT(m_Service.Call(SVC_API_GET_PROCESS, Request))
}

RESULT(XVariant) CPrivacyCore::GetPrograms()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_PROGRAMS, Request), SVC_API_PROGRAMS);
}

RESULT(XVariant) CPrivacyCore::GetFwRulesFor(const QList<const class CProgramItem*>& Nodes)
{
	CVariant Request;
	if(!Nodes.isEmpty())
		Request[SVC_API_PROG_IDS] = MakeIDs(Nodes);
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_FW_RULES, Request), SVC_API_RULES);
}

RESULT(XVariant) CPrivacyCore::GetAllFwRules()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_FW_RULES, Request), SVC_API_RULES);
}

STATUS CPrivacyCore::SetFwRule(const XVariant& FwRule)
{
	return m_Service.Call(SVC_API_SET_FW_RULE, FwRule);
}

RESULT(XVariant) CPrivacyCore::GetFwRule(const QString& Guid)
{
	CVariant Request;
	Request[SVC_API_FW_GUID] = Guid.toStdWString();
	RET_AS_XVARIANT(m_Service.Call(SVC_API_GET_FW_RULE, Request));
}

STATUS CPrivacyCore::DelFwRule(const QString& Guid)
{
	CVariant Request;
	Request[SVC_API_FW_GUID] = Guid.toStdWString();
	return m_Service.Call(SVC_API_DEL_FW_RULE, Request);
}

RESULT(FwFilteringModes) CPrivacyCore::GetFwProfile()
{
	CVariant Request;
	auto Ret = m_Service.Call(SVC_API_GET_FW_PROFILE, Request);
	if (Ret.IsError())
		return Ret;
	CVariant& Response = Ret.GetValue();
	std::string Profile = Response.Get(SVC_API_FW_PROFILE).To<std::string>();
	if (Profile == SVC_API_FW_NONE)			return FwFilteringModes::NoFiltering;
	else if (Profile == SVC_API_FW_BLOCK)	return FwFilteringModes::BlockList;
	else if (Profile == SVC_API_FW_ALLOW)	return FwFilteringModes::AllowList;
	return FwFilteringModes::NoFiltering;
}

STATUS CPrivacyCore::SetFwProfile(FwFilteringModes Profile)
{
	CVariant Request;
	switch(Profile)
	{
	case FwFilteringModes::NoFiltering: Request[SVC_API_FW_PROFILE] = SVC_API_FW_NONE; break;
	case FwFilteringModes::BlockList: Request[SVC_API_FW_PROFILE] = SVC_API_FW_BLOCK; break;
	case FwFilteringModes::AllowList: Request[SVC_API_FW_PROFILE] = SVC_API_FW_ALLOW; break;
	}
	return m_Service.Call(SVC_API_SET_FW_PROFILE, Request);
}

RESULT(XVariant) CPrivacyCore::GetSocketsFor(const QList<const class CProgramItem*>& Nodes)
{
	CVariant Request;
	if(!Nodes.isEmpty())
		Request[SVC_API_PROG_IDS] = MakeIDs(Nodes);
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_SOCKETS, Request), SVC_API_SOCKETS);
}

RESULT(XVariant) CPrivacyCore::GetAllSockets()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_SOCKETS, Request), SVC_API_SOCKETS);
}

RESULT(XVariant) CPrivacyCore::GetTraceLog(const class CProgramID& ID, ETraceLogs Log)
{
	CVariant LogType;
	switch (Log)
	{
	case ETraceLogs::eExecLog:	LogType = SVC_API_EXEC_LOG; break;
	case ETraceLogs::eNetLog:	LogType = SVC_API_NET_LOG; break;
	case ETraceLogs::eFSLog:	LogType = SVC_API_FS_LOG; break;
	case ETraceLogs::eRegLog:	LogType = SVC_API_REG_LOG; break;
	}
	CVariant Request;
	Request[SVC_API_ID_PROG] = ID.ToVariant();
	Request[SVC_API_LOG_TYPE] = LogType;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_TRACE_LOG, Request), SVC_API_LOG_DATA);	
}

RESULT(XVariant) CPrivacyCore::GetTrafficLog(const class CProgramID& ID, quint64 MinLastActivity)
{
	CVariant Request;
	Request[SVC_API_ID_PROG] = ID.ToVariant();
	if(MinLastActivity) 
		Request[SVC_API_NET_LAST_ACT] = MinLastActivity;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_TRAFFIC, Request), SVC_API_NET_TRAFFIC);
}

RESULT(XVariant) CPrivacyCore::GetDnsCache()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_DNC_CACHE, Request), SVC_API_DNS_CACHE);
}

RESULT(XVariant) CPrivacyCore::GetTweaks()
{
	CVariant Request;
	RET_GET_XVARIANT(m_Service.Call(SVC_API_GET_TWEAKS, Request), SVC_API_TWEAKS);
}

STATUS CPrivacyCore::ApplyTweak(const QString& Name)
{
	CVariant Request;
	Request[SVC_API_TWEAK_NAME] = Name.toStdWString();
	return m_Service.Call(SVC_API_APPLY_TWEAK, Request);
}

STATUS CPrivacyCore::UndoTweak(const QString& Name)
{
	CVariant Request;
	Request[SVC_API_TWEAK_NAME] = Name.toStdWString();
	return m_Service.Call(SVC_API_UNDO_TWEAK, Request);
}