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
	STATUS Reconnect();
	void Disconnect(bool bKeepEngine = false);

	bool IsEngineMode() const					{ return m_bEngineMode; }

	STATUS Update();

	class CProcessList* ProcessList()			{ return m_pProcessList; }
	class CEnclaveList* EnclaveList()			{ return m_pEnclaveList; }
	class CProgramManager* ProgramManager()		{ return m_pProgramManager; }
	class CAccessManager* AccessManager()		{ return m_pAccessManager; }
	class CNetworkManager* NetworkManager()		{ return m_pNetworkManager; }
	class CVolumeManager* VolumeManager()		{ return m_pVolumeManager; }
	class CTweakManager* TweakManager()			{ return m_pTweakManager; }

	STATUS StartProcessInEnvlave(const QString& Command, uint64 EnclaveId);

	CDriverAPI*		Driver() { return &m_Driver; }
	CServiceAPI*	Service() { return &m_Service; }

	QString				GetConfigDir();

	RESULT(XVariant)	GetConfig(const QString& Name);

	QString GetConfigInt(const QString& Name, const QString& defaultValue = QString()) {
		auto value = GetConfig(Name);
		if(!value.GetValue().IsValid())
			return defaultValue;
		return value.GetValue().AsQStr();
	}

	sint32 GetConfigInt(const QString& Name, qint32 defaultValue = 0) {
		auto value = GetConfig(Name);
		if(!value.GetValue().IsValid())
			return defaultValue;
		return value.GetValue().AsNum<sint32>();
	}

	uint32 GetConfigUInt(const QString& Name, quint32 defaultValue = 0) {
		auto value = GetConfig(Name);
		if(!value.GetValue().IsValid())
			return defaultValue;
		return value.GetValue().AsNum<uint32>();
	}

	int64_t GetConfigInt64(const QString& Name, qint64 defaultValue = 0) {
		auto value = GetConfig(Name);
		if(!value.GetValue().IsValid())
			return defaultValue;
		return value.GetValue().AsNum<sint64>();
	}

	uint64_t GetConfigUInt64(const QString& Name, quint64 defaultValue = 0) {
		auto value = GetConfig(Name);
		if(!value.GetValue().IsValid())
			return defaultValue;
		return value.GetValue().AsNum<uint64>();
	}

	bool GetConfigBool(const QString& Name, bool defaultValue = false) {
		auto value = GetConfig(Name);
		if(!value.GetValue().IsValid())
			return defaultValue;
		if(value.GetValue().GetType() == VAR_TYPE_UINT || value.GetValue().GetType() == VAR_TYPE_SINT)
			return value.GetValue().AsNum<uint32>() != 0;
		QString str = value.GetValue().AsQStr();
		if(str == "true" || str == "1")
			return true;
		return false;
	}

	STATUS				SetConfig(const QString& Name, const XVariant& Value);

	//RESULT(XVariant)	GetConfigMap(const XVariant& NameList);
	//STATUS				SetConfigMap(const XVariant& ValueMap);

	RESULT(XVariant)	GetSvcConfig();
	STATUS				SetSvcConfig(const XVariant& Data);

	RESULT(XVariant)	GetDrvConfig();
	STATUS				SetDrvConfig(const XVariant& Data);

	// Process Manager
	RESULT(XVariant)	GetProcesses();
	RESULT(XVariant)	GetProcess(uint64 Pid);

	// ProgramManager
	RESULT(XVariant)	GetPrograms();
	RESULT(XVariant)	GetLibraries(uint64 CacheToken = -1);

	STATUS				SetProgram(const CProgramItemPtr& pItem);
	STATUS				RemoveProgramFrom(uint64 UID, uint64 ParentUID = 0);

	STATUS				SetAllProgramRules(const XVariant& Rules);
	RESULT(XVariant)	GetAllProgramRules();
	STATUS				SetProgramRule(const XVariant& Rule);
	RESULT(XVariant)	GetProgramRule(const QString& Name);
	STATUS				DelProgramRule(const QString& Name);

	// Access Manager
	STATUS				SetAllAccessRules(const XVariant& Rules);
	RESULT(XVariant)	GetAllAccessRules();
	STATUS				SetAccessRule(const XVariant& Rule);
	RESULT(XVariant)	GetAccessRule(const QString& Name);
	STATUS				DelAccessRule(const QString& Name);

	// Network Manager
	RESULT(XVariant)	GetFwRulesFor(const QList<const class CProgramItem*>& Nodes);
	RESULT(XVariant)	GetAllFwRules();
	STATUS				SetFwRule(const XVariant& Rule);
	RESULT(XVariant)	GetFwRule(const QString& Guid);
	STATUS				DelFwRule(const QString& Guid);

	RESULT(FwFilteringModes) GetFwProfile();
	STATUS				SetFwProfile(FwFilteringModes Profile);

	RESULT(FwAuditPolicy) GetAuditPolicy();
	STATUS				SetAuditPolicy(FwAuditPolicy Mode);

	RESULT(XVariant)	GetSocketsFor(const QList<const class CProgramItem*>& Nodes);
	RESULT(XVariant)	GetAllSockets();

	RESULT(XVariant)	GetTrafficLog(const class CProgramID& ID, quint64 MinLastActivity);

	RESULT(XVariant)	GetDnsCache();

	// Access Manager
	RESULT(XVariant)	GetHandlesFor(const QList<const class CProgramItem*>& Nodes);
	RESULT(XVariant)	GetAllHandles();

	// Program Item
	RESULT(XVariant)	GetLibraryStats(const class CProgramID& ID);
	RESULT(XVariant)	GetExecStats(const class CProgramID& ID);
	RESULT(XVariant)	GetIngressStats(const class CProgramID& ID);

	RESULT(XVariant)	GetAccessStats(const class CProgramID& ID, quint64 MinLastActivity);

	RESULT(XVariant)	GetTraceLog(const class CProgramID& ID, ETraceLogs Log);

	// Volume Manager
	RESULT(XVariant)	GetVolumes();
	STATUS				MountVolume(const QString& Path, const QString& MountPoint, const QString& Password);
	STATUS				DismountVolume(const QString& MountPoint);
	STATUS				DismountAllVolumes();
	STATUS				CreateVolume(const QString& Path, const QString& Password, quint64 ImageSize = 0, const QString& Cipher = QString());
	STATUS				ChangeVolumePassword(const QString& Path, const QString& OldPassword, const QString& NewPassword);

	// Tweak Manager
	RESULT(XVariant)	GetTweaks();
	STATUS				ApplyTweak(const QString& Name);
	STATUS				UndoTweak(const QString& Name);

	//
	STATUS				SetWatchedPrograms(const QSet<CProgramItemPtr>& Programs);

	// Support
	RESULT(XVariant)	GetSupportInfo();

signals:
	void				UnruledFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);
	//void				FwRuleChanged();
	void				ExecutionEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);
	void				AccessEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);


protected:
	//friend class CProcessList;
	//friend class CServiceList;
	//friend class CPackageList;
	//friend class CProcess;
	friend class CProgramManager;

	void OnSvcEvent(uint32 MessageId, const CBuffer* pEvent);
	//void OnDrvEvent(const std::wstring& Guid, ERuleEvent Event, ERuleType Type);

	static XVariant MakeIDs(const QList<const class CProgramItem*>& Nodes);

	CDriverAPI	m_Driver;
	CServiceAPI m_Service;
	bool m_bEngineMode = false;

	class CProcessList* m_pProcessList = NULL;
	class CEnclaveList* m_pEnclaveList = NULL;
	class CProgramManager* m_pProgramManager = NULL;
	bool m_ProgramRulesUpdated = false;
	class CAccessManager* m_pAccessManager = NULL;
	bool m_AccessRulesUpdated = false;
	class CNetworkManager* m_pNetworkManager = NULL;
	bool m_FwRulesUpdated = false;
	class CVolumeManager* m_pVolumeManager = NULL;
	class CTweakManager* m_pTweakManager = NULL;

	QMutex m_EventQueueMutex;
	QMap<quint32, QQueue<XVariant>> m_SvcEventQueue;
	//struct SDrvRuleEvent
	//{
	//	QString Guid;
	//	ERuleType Type;
	//	ERuleEvent Event;
	//};
	//QMap<ERuleType, QQueue<SDrvRuleEvent>> m_DrvEventQueue;
};

extern CSettings* theConf;
extern CPrivacyCore* theCore;