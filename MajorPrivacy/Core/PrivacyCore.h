#pragma once

#include "../MiscHelpers/Common/Settings.h"
#include "../../Library/API/DriverAPI.h"
#include "../../Library/API/ServiceAPI.h"
#include "../Library/Common/XVariant.h"
#include "TraceLogEntry.h"
#include "Programs/ProgramFile.h"
#include "../Helpers/SidResolver.h"
#include "../../Library/Common/FlexGuid.h"

class CPrivacyCore : public QObject
{
	Q_OBJECT
public:
	CPrivacyCore(QObject* parent = nullptr);
	~CPrivacyCore();
	
	STATUS Install();
	STATUS Uninstall();
	bool IsInstalled();
	bool SvcIsRunning();

	STATUS Connect(bool bEngineMode);
	void Disconnect(bool bKeepEngine);

	bool IsEngineMode() const					{ return m_bEngineMode; }

	STATUS Update();
	void ProcessEvents();

	void Clear();

	class CProcessList* ProcessList()			{ return m_pProcessList; }
	class CEnclaveManager* EnclaveManager()		{ return m_pEnclaveManager; }
	class CProgramManager* ProgramManager()		{ return m_pProgramManager; }
	class CAccessManager* AccessManager()		{ return m_pAccessManager; }
	class CNetworkManager* NetworkManager()		{ return m_pNetworkManager; }
	class CVolumeManager* VolumeManager()		{ return m_pVolumeManager; }
	class CTweakManager* TweakManager()			{ return m_pTweakManager; }

	CDriverAPI*		Driver() { return &m_Driver; }
	CServiceAPI*	Service() { return &m_Service; }

	static QString		NormalizePath(QString sFilePath, bool bForID = false);

	STATUS 				SignFiles(const QStringList& Paths, const class CPrivateKey* pPrivateKey);
	bool				HasFileSignature(const QString& Path);
	STATUS 				RemoveFileSignature(const QStringList& Paths);
	STATUS 				SignCerts(const QMap<QByteArray, QString>& Certs, const class CPrivateKey* pPrivateKey);
	bool				HasCertSignature(const QString& Subject, const QByteArray& SignerHash);
	STATUS 				RemoveCertSignature(const QMap<QByteArray, QString>& Certs);

	QString				GetConfigDir() { return m_ConfigDir; }

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

	QString				QueryConfigDir();
	bool				CheckConfigFile(const QString& Name);
	RESULT(QByteArray)	ReadConfigFile(const QString& Name);
	STATUS				WriteConfigFile(const QString& Name, const QByteArray& Data);
	STATUS				RemoveConfigFile(const QString& Name);
	RESULT(QStringList)	ListConfigFiles(const QString& Name);

	RESULT(XVariant)	GetSvcConfig();
	STATUS				SetSvcConfig(const XVariant& Data);

	RESULT(XVariant)	GetDrvConfig();
	STATUS				SetDrvConfig(const XVariant& Data);

	// Process Manager
	RESULT(XVariant)	GetProcesses();
	RESULT(XVariant)	GetProcess(uint64 Pid);
	STATUS				TerminateProcess(uint64 Pid);

	// Secure Enclaves
	STATUS				SetAllEnclaves(const XVariant& Enclaves);
	RESULT(XVariant)	GetAllEnclaves();
	STATUS				SetEnclave(const XVariant& Enclave);
	RESULT(XVariant)	GetEnclave(const QFlexGuid& Guid);
	STATUS				DelEnclave(const QFlexGuid& Guid);
	STATUS				StartProcessInEnclave(const QString& Command, const QFlexGuid& Guid);

	// ProgramManager
	RESULT(XVariant)	GetPrograms();
	RESULT(XVariant)	GetLibraries(uint64 CacheToken = -1);

	RESULT(quint64)		SetProgram(const CProgramItemPtr& pItem);
	STATUS				AddProgramTo(uint64 UID, uint64 ParentUID);
	STATUS				RemoveProgramFrom(uint64 UID, uint64 ParentUID = 0, bool bDelRules = false);

	STATUS				CleanUpPrograms(bool bPurgeRules = false);
	STATUS				ReGroupPrograms();

	STATUS				SetAllProgramRules(const XVariant& Rules);
	RESULT(XVariant)	GetAllProgramRules();
	STATUS				SetProgramRule(const XVariant& Rule);
	RESULT(XVariant)	GetProgramRule(const QFlexGuid& Guid);
	STATUS				DelProgramRule(const QFlexGuid& Guid);

	// Access Manager
	STATUS				SetAllAccessRules(const XVariant& Rules);
	RESULT(XVariant)	GetAllAccessRules();
	STATUS				SetAccessRule(const XVariant& Rule);
	RESULT(XVariant)	GetAccessRule(const QFlexGuid& Guid);
	STATUS				DelAccessRule(const QFlexGuid& Guid);

	// Network Manager
	RESULT(XVariant)	GetFwRulesFor(const QList<const class CProgramItem*>& Nodes);
	RESULT(XVariant)	GetAllFwRules(bool bReLoad = false);
	STATUS				SetFwRule(const XVariant& Rule);
	RESULT(XVariant)	GetFwRule(const QFlexGuid& Guid);
	STATUS				DelFwRule(const QFlexGuid& Guid);

	RESULT(FwFilteringModes) GetFwProfile();
	STATUS				SetFwProfile(FwFilteringModes Profile);

	RESULT(FwAuditPolicy) GetAuditPolicy();
	STATUS				SetAuditPolicy(FwAuditPolicy Mode);

	RESULT(XVariant)	GetSocketsFor(const QList<const class CProgramItem*>& Nodes);
	RESULT(XVariant)	GetAllSockets();

	RESULT(XVariant)	GetTrafficLog(const class CProgramID& ID, quint64 MinLastActivity);

	RESULT(XVariant)	GetDnsCache();

	STATUS				FlushDnsCache();

	// Access Manager
	RESULT(XVariant)	GetHandlesFor(const QList<const class CProgramItem*>& Nodes);
	RESULT(XVariant)	GetAllHandles();
	STATUS				ClearTraceLog(ETraceLogs Log, const CProgramItemPtr& pItem = CProgramItemPtr());

	STATUS				CleanUpAccessTree();

	// Program Item
	RESULT(XVariant)	GetLibraryStats(const class CProgramID& ID);
	STATUS				CleanUpLibraries(const CProgramItemPtr& pItem = CProgramItemPtr());
	RESULT(XVariant)	GetExecStats(const class CProgramID& ID);
	RESULT(XVariant)	GetIngressStats(const class CProgramID& ID);

	RESULT(XVariant)	GetAccessStats(const class CProgramID& ID, quint64 MinLastActivity);

	RESULT(XVariant)	GetTraceLog(const class CProgramID& ID, ETraceLogs Log);

	// Volume Manager
	RESULT(XVariant)	GetVolumes();
	STATUS				MountVolume(const QString& Path, const QString& MountPoint, const QString& Password, bool bProtect);
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

	// 


	void				OnClearTraceLog(const CProgramItemPtr& pItem, ETraceLogs Log);

	CSidResolver*		GetSidResolver() {return m_pSidResolver;}

signals:
	void				ProgramsAdded();
	void				UnruledFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);
	
	void				EnclavesChanged();
	void				ExecRulesChanged();
	void				ResRulesChanged();
	void				FwRulesChanged();
	
	void				ExecutionEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);
	void				AccessEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);

	void				CleanUpDone();
	void				CleanUpProgress(quint64 Done, quint64 Total);

	void				DevicesChanged();

protected:
	//friend class CProcessList;
	//friend class CServiceList;
	//friend class CPackageList;
	//friend class CProcess;
	friend class CProgramManager;

	//void OnProgEvent(uint32 MessageId, const CBuffer* pEvent);

	void				OnSvcEvent(uint32 MessageId, const CBuffer* pEvent);
	//void				OnDrvEvent(const std::wstring& Guid, EConfigEvent Event, ERuleType Type);

	STATUS				HashFile(const QString& Path, CBuffer& Hash);
	QString				GetSignatureFilePath(const QString& Path);

	void				OnCleanUpDone(uint32 MessageId, const CBuffer* pEvent);
	
	static void			DeviceChangedCallback(void* param);

	static XVariant		MakeIDs(const QList<const class CProgramItem*>& Nodes);

	CDriverAPI	m_Driver;
	CServiceAPI m_Service;
	bool m_bEngineMode = false;
	QString m_ConfigDir;

	class CProcessList* m_pProcessList = NULL;
	class CEnclaveManager* m_pEnclaveManager = NULL;
	class CProgramManager* m_pProgramManager = NULL;
	class CAccessManager* m_pAccessManager = NULL;
	class CNetworkManager* m_pNetworkManager = NULL;
	class CVolumeManager* m_pVolumeManager = NULL;
	class CTweakManager* m_pTweakManager = NULL;

	bool m_EnclavesUpToDate = false;
	bool m_ProgramRulesUpToDate = false;
	bool m_AccessRulesUpToDate = false;
	bool m_FwRulesUpToDate = false;

	QMutex m_EventQueueMutex;
	QMap<quint32, QQueue<XVariant>> m_SvcEventQueue;
	//struct SDrvRuleEvent
	//{
	//	QString Guid;
	//	ERuleType Type;
	//	EConfigEvent Event;
	//};
	//QMap<ERuleType, QQueue<SDrvRuleEvent>> m_DrvEventQueue;

	CSidResolver* m_pSidResolver;

	QHash<QString, int> m_SigFileCache;
};

extern CSettings* theConf;
extern CPrivacyCore* theCore;