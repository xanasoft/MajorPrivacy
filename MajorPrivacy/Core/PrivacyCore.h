#pragma once

#include "../MiscHelpers/Common/Settings.h"
#include "../../Library/API/DriverAPI.h"
#include "../../Library/API/ServiceAPI.h"
#include "./Common/QtVariant.h"
#include "TraceLogEntry.h"
#include "Programs/ProgramFile.h"
#include "../Helpers/SidResolver.h"
#include "./Common/QtFlexGuid.h"
#include "../../Library/Helpers/EvtUtil.h"
#include "../../Framework/Core/MemoryPool.h"

struct CIString 
{
	QString s;


	CIString() = default;
	CIString(const QString& str) : s(str) {}
	CIString(QString&& str) noexcept : s(std::move(str)) {}

	// ASCII-only case-insensitive equality to match the hash
	bool operator==(const CIString& o) const noexcept {
		if (s.size() != o.s.size()) return false;
		const ushort* a = s.utf16();
		const ushort* b = o.s.utf16();
		for (qsizetype i = 0, n = s.size(); i < n; ++i) {
			uint16_t ca = static_cast<uint16_t>(a[i]);
			uint16_t cb = static_cast<uint16_t>(b[i]);

			// fold ASCII A..Z -> a..z
			if (ca >= 'A' && ca <= 'Z') ca = static_cast<uint16_t>(ca + 0x20);
			if (cb >= 'A' && cb <= 'Z') cb = static_cast<uint16_t>(cb + 0x20);

			if (ca != cb) return false;
		}
		return true;
	}

	// ASCII-only case-insensitive hash over UTF-16 code units
	friend size_t qHash(const CIString& key, size_t seed = 0) noexcept {
		const ushort* p = key.s.utf16();
		const qsizetype n = key.s.size();

		// FNV-1a (64-bit when size_t is 64, else 32-bit)
		if constexpr (sizeof(size_t) == 8) {
			uint64_t h = 1469598103934665603ull ^ static_cast<uint64_t>(seed);
			for (qsizetype i = 0; i < n; ++i) {
				uint16_t c = static_cast<uint16_t>(p[i]);
				if (c >= 'A' && c <= 'Z') c = static_cast<uint16_t>(c + 0x20);
				h ^= static_cast<uint64_t>(c);
				h *= 1099511628211ull;
			}
			return static_cast<size_t>(h);
		} else {
			uint32_t h = 2166136261u ^ static_cast<uint32_t>(seed);
			for (qsizetype i = 0; i < n; ++i) {
				uint16_t c = static_cast<uint16_t>(p[i]);
				if (c >= 'A' && c <= 'Z') c = static_cast<uint16_t>(c + 0x20);
				h ^= static_cast<uint32_t>(c);
				h *= 16777619u;
			}
			return static_cast<size_t>(h);
		}
	}
};

class CPrivacyCore : public QThread
{
	Q_OBJECT
public:
	CPrivacyCore(QObject* parent = nullptr);
	~CPrivacyCore();
	
	STATUS Start();
	STATUS Stop();
	STATUS Install();
	STATUS Uninstall();
	bool IsInstalled();
	bool SvcIsRunning();

	STATUS Connect(bool bCanStart = false, bool bEngineMode = false);
	void Disconnect(bool bKeepEngine = false);

	bool IsEngineMode() const					{ return m_bEngineMode; }

	uint32 GetGuiSecState() const				{ return m_GuiSecState; }
	bool IsGuiHighSecurity() const;
	bool IsGuiMaxSecurity() const;
	uint32 GetSvcSecState() const				{ return m_SvcSecState; }
	bool IsSvcHighSecurity() const;
	bool IsSvcMaxSecurity() const;

	STATUS Update();
	void ProcessEvents();

	void Clear();

	class CProcessList* ProcessList()			{ return m_pProcessList; }
	class CEnclaveManager* EnclaveManager()		{ return m_pEnclaveManager; }
	class CHashDB* HashDB()						{ return m_pHashDB; }
	class CProgramManager* ProgramManager()		{ return m_pProgramManager; }
	class CAccessManager* AccessManager()		{ return m_pAccessManager; }
	class CNetworkManager* NetworkManager()		{ return m_pNetworkManager; }
	class CVolumeManager* VolumeManager()		{ return m_pVolumeManager; }
	class CTweakManager* TweakManager()			{ return m_pTweakManager; }

	class CIssueManager* IssueManager()			{ return m_pIssueManager; }

	CDriverAPI*		Driver() { return &m_Driver; }
	CServiceAPI*	Service() { return &m_Service; }

	class CEventLogger*	Log()					{ return m_pSysLog; }
	class CEventLog*	EventLog()				{ return m_pEventLog; }


	static QString		NormalizePath(QString sFilePath, bool bForID = false);

	/*STATUS 				SignFiles(const QStringList& Paths, const class CPrivateKey* pPrivateKey);
	bool				HasFileSignature(const QString& Path, const QByteArray& Hash);
	STATUS 				RemoveFileSignature(const QStringList& Paths);
	STATUS 				SignCerts(const QMap<QByteArray, QString>& Certs, const class CPrivateKey* pPrivateKey);
	bool				HasCertSignature(const QString& Subject, const QByteArray& SignerHash);
	STATUS 				RemoveCertSignature(const QMap<QByteArray, QString>& Certs);*/

	std::shared_ptr<struct SEmbeddedCIInfo> GetEmbeddedCIInfo(const std::wstring& filePath);
	std::shared_ptr<struct SCatalogCIInfo> GetCatalogCIInfo(const std::wstring& filePath);
	void				ClearCIInfoCache();

	QString				GetConfigDir() { return m_ConfigDir; }
	QString				GetAppDir() { return m_AppDir; }
	QString				GetWinDir() { return m_WinDir; }

	RESULT(QtVariant)	GetConfig(const QString& Name);

	QString GetConfigStr(const QString& Name, const QString& defaultValue = QString()) {
		auto value = GetConfig(Name);
		if(!value.GetValue().IsValid())
			return defaultValue;
		return value.GetValue().AsQStr();
	}

	qint32 GetConfigInt(const QString& Name, qint32 defaultValue = 0) {
		auto value = GetConfig(Name);
		if(!value.GetValue().IsValid())
			return defaultValue;
		return value.GetValue().AsNum<sint32>();
	}

	quint32 GetConfigUInt(const QString& Name, quint32 defaultValue = 0) {
		auto value = GetConfig(Name);
		if(!value.GetValue().IsValid())
			return defaultValue;
		return value.GetValue().AsNum<uint32>();
	}

	qint64 GetConfigInt64(const QString& Name, qint64 defaultValue = 0) {
		auto value = GetConfig(Name);
		if(!value.GetValue().IsValid())
			return defaultValue;
		return value.GetValue().AsNum<sint64>();
	}

	quint64 GetConfigUInt64(const QString& Name, quint64 defaultValue = 0) {
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

	STATUS				SetConfig(const QString& Name, const QtVariant& Value);

	QString				QueryConfigDir();
	bool				CheckConfigFile(const QString& Name);
	RESULT(QByteArray)	ReadConfigFile(const QString& Name);
	STATUS				WriteConfigFile(const QString& Name, const QByteArray& Data);
	STATUS				RemoveConfigFile(const QString& Name);
	RESULT(QStringList)	ListConfigFiles(const QString& Name);

	RESULT(QtVariant)	GetSvcConfig();
	STATUS				SetSvcConfig(const QtVariant& Data);

	//RESULT(QtVariant)	GetDrvConfig(const QString& Name);
	//STATUS				SetDrvConfig(const QString& Name, const QtVariant& Value);

	RESULT(QtVariant)	GetDrvConfig();
	STATUS				SetDrvConfig(const QtVariant& Data);

	// Process Manager
	RESULT(QtVariant)	GetProcesses();
	RESULT(QtVariant)	GetProcess(uint64 Pid);
	STATUS				StartProcessBySvc(const QString& Command);
	STATUS				TerminateProcess(uint64 Pid);
	RESULT(int)			RunUpdateUtility(const QStringList& Params, quint32 Elevate = 0, bool Wait = false);

	// Secure Enclaves
	STATUS				SetAllEnclaves(const QtVariant& Enclaves);
	RESULT(QtVariant)	GetAllEnclaves();
	STATUS				SetEnclave(const QtVariant& Enclave);
	RESULT(QtVariant)	GetEnclave(const QFlexGuid& Guid);
	STATUS				DelEnclave(const QFlexGuid& Guid);
	STATUS				StartProcessInEnclave(const QString& Command, const QFlexGuid& Guid);

	// HashDB
	STATUS				SetAllHashes(const QtVariant& Enclaves);
	RESULT(QtVariant)	GetAllHashes();
	STATUS				SetHashEntry(const QtVariant& Enclave);
	RESULT(QtVariant)	GetHashEntry(const QByteArray& HashValue);
	STATUS				DelHashEntry(const QByteArray& HashValue);


	// ProgramManager
	RESULT(QtVariant)	GetPrograms();
	RESULT(QtVariant)	GetProgram(const class CProgramID& ID);
	RESULT(QtVariant)	GetProgram(uint64 UID);
	RESULT(QtVariant)	GetLibraries(uint64 CacheToken = -1);

	RESULT(quint64)		SetProgram(const CProgramItemPtr& pItem);
	STATUS				AddProgramTo(uint64 UID, uint64 ParentUID);
	STATUS				RemoveProgramFrom(uint64 UID, uint64 ParentUID = 0, bool bDelRules = false, bool bKeepOne = false);

	STATUS				RefreshPrograms();
	STATUS				CleanUpPrograms(bool bPurgeRules = false);
	STATUS				ReGroupPrograms();

	STATUS				SetAllProgramRules(const QtVariant& Rules);
	RESULT(QtVariant)	GetAllProgramRules();
	STATUS				SetProgramRule(const QtVariant& Rule);
	RESULT(QtVariant)	GetProgramRule(const QFlexGuid& Guid);
	STATUS				DelProgramRule(const QFlexGuid& Guid);

	// Access Manager
	STATUS				SetAllAccessRules(const QtVariant& Rules);
	RESULT(QtVariant)	GetAllAccessRules();
	STATUS				SetAccessRule(const QtVariant& Rule);
	RESULT(QtVariant)	GetAccessRule(const QFlexGuid& Guid);
	STATUS				DelAccessRule(const QFlexGuid& Guid);
	STATUS				SetAccessEventAction(uint64 EventId, EAccessRuleType Action);

	// Network Manager
	RESULT(QtVariant)	GetFwRulesFor(const QList<const class CProgramItem*>& Nodes);
	RESULT(QtVariant)	GetAllFwRules(bool bReLoad = false);
	STATUS				SetFwRule(const QtVariant& Rule);
	RESULT(QtVariant)	GetFwRule(const QFlexGuid& Guid);
	STATUS				DelFwRule(const QFlexGuid& Guid);

	RESULT(FwFilteringModes) GetFwProfile();
	STATUS				SetFwProfile(FwFilteringModes Profile);

	RESULT(FwAuditPolicy) GetAuditPolicy();
	STATUS				SetAuditPolicy(FwAuditPolicy Mode);

	RESULT(QtVariant)	GetSocketsFor(const QList<const class CProgramItem*>& Nodes);
	RESULT(QtVariant)	GetAllSockets();

	RESULT(QtVariant)	GetTrafficLog(const class CProgramID& ID, quint64 MinLastActivity);

	RESULT(QtVariant)	GetAllDnsRules();
	STATUS				SetDnsRule(const QtVariant& Rule);
	RESULT(QtVariant)	GetDnsRule(const QFlexGuid& Guid);
	STATUS				DelDnsRule(const QFlexGuid& Guid);

	RESULT(QtVariant)	GetDnsCache();

	STATUS				FlushDnsCache();

	RESULT(QtVariant)	GetBlockListInfo();

	// Access Manager
	RESULT(QtVariant)	GetHandlesFor(const QList<const class CProgramItem*>& Nodes);
	RESULT(QtVariant)	GetAllHandles();
	STATUS				ClearTraceLog(ETraceLogs Log, const CProgramItemPtr& pItem = CProgramItemPtr());
	STATUS				ClearRecords(ETraceLogs Log, const CProgramItemPtr& pItem = CProgramItemPtr());

	STATUS				CleanUpAccessTree();

	// Program Item
	RESULT(QtVariant)	GetLibraryStats(const class CProgramID& ID);
	STATUS				CleanUpLibraries(const CProgramItemPtr& pItem = CProgramItemPtr());
	RESULT(QtVariant)	GetExecStats(const class CProgramID& ID);
	RESULT(QtVariant)	GetIngressStats(const class CProgramID& ID);

	RESULT(QtVariant)	GetAccessStats(const class CProgramID& ID, quint64 MinLastActivity);

	RESULT(QtVariant)	GetTraceLog(const class CProgramID& ID, ETraceLogs Log);

	// Volume Manager
	RESULT(QtVariant)	GetVolumes();
	RESULT(QtVariant)	GetVolume(const QFlexGuid& Guid);
	STATUS				SetVolume(const QtVariant& Volume);
	STATUS				MountVolume(const QString& Path, const QString& MountPoint, const QString& Password, bool bProtect, bool bLockdown);
	STATUS				DismountVolume(const QString& MountPoint);
	STATUS				DismountAllVolumes();
	STATUS				CreateVolume(const QString& Path, const QString& Password, quint64 ImageSize = 0, const QString& Cipher = QString());
	STATUS				ChangeVolumePassword(const QString& Path, const QString& OldPassword, const QString& NewPassword);

	// Tweak Manager
	RESULT(QtVariant)	GetTweaks(uint32* pRevision = nullptr);
	STATUS				ApplyTweak(const QString& Name);
	STATUS				UndoTweak(const QString& Name);
	STATUS				ApproveTweak(const QString& Id);

	// Other
	void				ClearPrivacyLog();

	RESULT(QtVariant)	GetServiceStats();

	RESULT(QtVariant)	GetScriptLog(const QFlexGuid& Guid, EScriptTypes Type, quint32 LastID = 0);
	STATUS				ClearScriptLog(const QFlexGuid& Guid, EScriptTypes Type);
	RESULT(QtVariant)	CallScriptFunc(const QFlexGuid& Guid, EScriptTypes Type, const QString& Name, const QtVariant& Params = QtVariant());

	//
	STATUS				SetWatchedPrograms(const QSet<CProgramItemPtr>& Programs);

	// Support
	RESULT(QtVariant)	GetSupportInfo(bool bRefresh = false);

	STATUS				SetSecureParam(const QString& Name, const void* data, size_t size);
	STATUS				GetSecureParam(const QString& Name, void* data, size_t size, quint32* size_out = NULL, bool bVerify = false);

	bool				TestSignature(const QByteArray& Data, const QByteArray& Signature);

	STATUS				SetDatFile(const QString& FileName, const QByteArray& Data);
	//RESULT(QByteArray) GetDatFile(const QString& FileName);

	// 

	STATUS				HashFile(const QString& Path, CBuffer& Hash);

	void				OnClearTraceLog(const CProgramItemPtr& pItem, ETraceLogs Log);
	void				OnClearRecords(const CProgramItemPtr& pItem, ETraceLogs Log);


	virtual size_t		GetTotalMemUsage() const { return m_TotalMemoryUsed; }
	virtual size_t		GetLogMemUsage() const { return m_LogMemoryUsed; }

	CSidResolver*		GetSidResolver() {return m_pSidResolver;}

signals:
	void				ProgramsAdded();
	void				UnruledFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);
	void				FwChangeEvent(const QString& RuleId, qint32 iEventType);
	
	void				EnclavesChanged();
	void				HashDBChanged();
	void				ExecRulesChanged();
	void				ResRulesChanged();
	void				FwRulesChanged();
	void				DnsRulesChanged();
	
	void				ExecutionEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);
	void				AccessEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry, uint32 TimeOut);

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
	void				OnDrvEvent(const std::wstring& Guid, enum class EConfigEvent Event, enum class EConfigGroup Type, uint64 PID);

	QString				GetSignatureFilePath(const QString& Path, QByteArray Hash = QByteArray());

	void				OnCleanUpDone(uint32 MessageId, const CBuffer* pEvent);
	
	static void			DeviceChangedCallback(void* param);

	static QtVariant	MakeIDs(const QList<const class CProgramItem*>& Nodes);

	STATUS				InitHooks();

	class CEventLogger*	m_pSysLog = NULL;
	class CEventLog* m_pEventLog = NULL;

	class CPrivacyWorker* m_pWorker = NULL;

	CDriverAPI	m_Driver;
	CServiceAPI m_Service;
	FW::MemoryPool* m_pMemPool = NULL;
	bool m_bEngineMode = false;
	QString m_ConfigDir;
	QString m_AppDir;
	QString m_WinDir;

	uint32 m_GuiSecState = 0;
	uint32 m_SvcSecState = 0;

	class CProcessList* m_pProcessList = NULL;
	class CEnclaveManager* m_pEnclaveManager = NULL;
	class CHashDB* m_pHashDB = NULL;
	class CProgramManager* m_pProgramManager = NULL;
	class CAccessManager* m_pAccessManager = NULL;
	class CNetworkManager* m_pNetworkManager = NULL;
	class CVolumeManager* m_pVolumeManager = NULL;
	class CTweakManager* m_pTweakManager = NULL;

	class CIssueManager* m_pIssueManager = NULL;

	bool m_EnclavesUpToDate = false;
	bool m_HashDBUpToDate = false;
	bool m_ProgramRulesUpToDate = false;
	bool m_AccessRulesUpToDate = false;
	bool m_FwRulesUpToDate = false;
	bool m_DnsRulesUpToDate = false;

	quint64 m_TotalMemoryUsed = 0;
	quint64 m_LogMemoryUsed = 0;

	QMutex m_EventQueueMutex;
	QMap<quint32, QQueue<QtVariant>> m_SvcEventQueue;
	//struct SDrvRuleEvent
	//{
	//	QString Guid;
	//	ERuleType Type;
	//	EConfigEvent Event;
	//};
	//QMap<ERuleType, QQueue<SDrvRuleEvent>> m_DrvEventQueue;

	CSidResolver* m_pSidResolver;

	//QHash<CIString, int> m_SigFileCache;

	struct SEmbeddedSigInfo
	{
		std::shared_ptr<struct SEmbeddedCIInfo> Info;
	};

	struct SCatalogSigInfo
	{
		std::shared_ptr<struct SCatalogCIInfo> Info;
	};

	std::unordered_map<std::wstring, SEmbeddedSigInfo> m_EmbeddedSigCache;
	std::unordered_map<std::wstring, SCatalogSigInfo> m_CatalogSigCache;
};

class CPrivacyWorker : public QObject
{
	Q_OBJECT
public:
	CPrivacyWorker(QObject* parent = nullptr);
	~CPrivacyWorker();

private slots:
	void DoUpdate();

};

extern CSettings* theConf;
extern CPrivacyCore* theCore;