#pragma once

#include "../MiscHelpers/Common/Settings.h"
#include "../../Library/API/DriverAPI.h"
#include "../../Library/API/ServiceAPI.h"
#include "../Library/Common/XVariant.h"
#include "TraceLogEntry.h"
#include "Programs/ProgramFile.h"

class CPrivacyCore : public QObject
{
	Q_OBJECT
public:
	CPrivacyCore(QObject* parent = nullptr);
	~CPrivacyCore();
	
	STATUS Connect();
	void Disconnect();

	void Update();

	class CProcessList* Processes()		{ return m_pProcesses; }
	class CProgramManager* Programs()	{ return m_pPrograms; }
	class CNetworkManager* Network()	{ return m_pNetwork; }
	class CTweakManager* Tweaks()		{ return m_pTweaks; }

	STATUS StartProcessInEnvlave(const QString& Command, uint32 EnclaveId);

	//CDriverAPI*		Driver() { return &m_Driver; }
	//CServiceAPI*	Service() { return &m_Service; }

	RESULT(XVariant)	GetConfig(const QString& Name);
	STATUS				SetConfig(const QString& Name, const XVariant& Value);

	RESULT(XVariant)	GetConfigMap(const XVariant& NameList);
	STATUS				SetConfigMap(const XVariant& ValueMap);

	RESULT(XVariant)	GetProcesses();
	RESULT(XVariant)	GetProcess(uint64 Pid);

	RESULT(XVariant)	GetPrograms();

	RESULT(XVariant)	GetFwRulesFor(const QList<const class CProgramItem*>& Nodes);
	RESULT(XVariant)	GetAllFwRules();
	STATUS				SetFwRule(const XVariant& FwRule);
	RESULT(XVariant)	GetFwRule(const QString& Guid);
	STATUS				DelFwRule(const QString& Guid);

	RESULT(FwFilteringModes) GetFwProfile();
	STATUS				SetFwProfile(FwFilteringModes Profile);

	RESULT(XVariant)	GetSocketsFor(const QList<const class CProgramItem*>& Nodes);
	RESULT(XVariant)	GetAllSockets();

	RESULT(XVariant)	GetTraceLog(const class CProgramID& ID, ETraceLogs Log);

	RESULT(XVariant)	GetTrafficLog(const class CProgramID& ID, quint64 MinLastActivity);

	RESULT(XVariant)	GetDnsCache();

	RESULT(XVariant)	GetTweaks();
	STATUS				ApplyTweak(const QString& Name);
	STATUS				UndoTweak(const QString& Name);

signals:
	void				UnruledFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);
	//void				FwRuleChanged();


protected:
	//friend class CProcessList;
	//friend class CServiceList;
	//friend class CPackageList;
	//friend class CProcess;
	friend class CProgramManager;

	void OnEventChanged(uint32 MessageId, const CBuffer* pEvent);

	static XVariant MakeIDs(const QList<const class CProgramItem*>& Nodes);

	//CDriverAPI	m_Driver;
	CServiceAPI m_Service;

	class CProcessList* m_pProcesses = NULL;
	class CProgramManager* m_pPrograms = NULL;
	class CNetworkManager* m_pNetwork = NULL;
	bool m_FwRulesUpdated = false;
	class CTweakManager* m_pTweaks = NULL;

	QMutex m_EventQueueMutex;
	QMap<quint32, QQueue<XVariant>> m_EventQueue;
};

extern CSettings* theConf;
extern CPrivacyCore* theCore;