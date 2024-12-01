#pragma once

#include <QtWidgets/QMainWindow>
//#include "ui_MajorPrivacy.h"
#include "Core/Programs/WindowsService.h"
#include "Core/TraceLogEntry.h"
#include "../Library/Status.h"
#include "../MiscHelpers/Common/CustomTheme.h"
#include <QTranslator>

class CHomePage;
class CEnclavePage;
class CProcessPage;
class CAccessPage;
class CNetworkPage;
class CProgramView;
class CDnsPage;
class CTweakPage;
class CVolumePage;
class CInfoView;

class CPopUpWindow;

class CMajorPrivacy : public QMainWindow
{
	Q_OBJECT

public:
	CMajorPrivacy(QWidget *parent = Q_NULLPTR);
	~CMajorPrivacy();

	static QString		GetVersion();

	struct SCurrentItems
	{
		QSet<CProgramItemPtr> Items; // all items including not sellected children

		//QMap<quint64, CProcessPtr> Processes;
		QSet<CProgramFilePtr> Programs;
		QSet<CWindowsServicePtr> ServicesEx; // explicitly sellected services
		QSet<CWindowsServicePtr> ServicesIm; // implicitly sellected services

		bool bAllPrograms = false;
	};

	const SCurrentItems& GetCurrentItems() const;
	QMap<quint64, CProcessPtr> GetCurrentProcesses() const;

	int					SafeExec(QDialog* pDialog);

	QString				FormatError(const STATUS& Error);
	int 				CheckResults(QList<STATUS> Results, QWidget* pParent, bool bAsync = false);

	void				UpdateTheme();

	bool				IsAlwaysOnTop() const;

	STATUS				SignFiles(const QStringList& Paths);

	void				IgnoreEvent(ERuleType Type, const CProgramFilePtr& pProgram, const QString& Path = QString());
	bool				IsEventIgnored(ERuleType Type, const CProgramFilePtr& pProgram, const CLogEntryPtr& pLogEntry) const;

	void				LoadIgnoreList();
	void				StoreIgnoreList();

	void				ShowNotification(const QString& Title, const QString& Message, QSystemTrayIcon::MessageIcon Icon, int Timeout = 0);

	static QString		GetResourceStr(const QString& Name);

	quint64				GetRecentLimit() const { return m_pBtnTime->isChecked() ? m_pCmbRecent->currentData().toULongLong() : 0; }

signals:
	void				Closed();

	void				Clear();

	void				OnMountVolume();
	void				OnUnmountAllVolumes();
	void				OnCreateVolume();

public slots:
	void				OnProgramsAdded();
	void				OnExecutionEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pLogEntry);
	void				OnAccessEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pLogEntry);
	void				OnUnruledFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pLogEntry);

	void				CleanUpPrograms();
	void				ReGroupPrograms();

	void				OpenSettings();

	void				UpdateSettings(bool bRebuildUI);

	void				ClearIgnoreLists();
	void				ResetPrompts();

	void				ClearTraceLogs();

private slots:
	void				OnMaintenance();
	void				OnExit();
	void				OnHelp();
	void				OnAbout();

	void				BuildMenu();
	void				SetUITheme();
	void				RebuildGUI();
	void				BuildGUI();

	void				OnSplitColumns();
	void				OnAlwaysTop();

	void				OnShowHide();
	void				OnSysTray(QSystemTrayIcon::ActivationReason Reason);

	void				OnSignFile();
	void				OnPrivateKey();
	void				OnMakeKeyPair();
	void				OnClearKeys();

	void				OnToggleTree();
	void				OnStackPanels();
	void				OnMergePanels();

	void				OnFwProfile();

	void				OnPageChanged(int index);

	void				OnProgramsChanged(const QList<CProgramItemPtr>& Programs);

	void				OnProgSplitter(int pos, int index);

	void				OnCleanUpDone();
	void				OnCleanUpProgress(quint64 Done, quint64 Total);

protected:
	void				closeEvent(QCloseEvent *e);
	void				changeEvent(QEvent* e);
	void				timerEvent(QTimerEvent* pEvent);
	int					m_uTimerID;

	void				SetTitle();

	STATUS				Connect();
	void				Disconnect();

	STATUS				InitSigner(class CPrivateKey& PrivateKey);

	void				CreateTrayIcon();
	void				CreateTrayMenu();

	SCurrentItems		MakeCurrentItems() const;
	SCurrentItems		MakeCurrentItems(const QList<CProgramItemPtr>& Progs, std::function<bool(const CProgramItemPtr&)> Filter = nullptr) const;
	SCurrentItems		m_CurrentItems;

	bool				m_bShowAll = false;
	bool				m_bWasVisible = false;
	bool				m_bOnTop = false;
	bool				m_bWasConnected = false;
	bool				m_bExit = false;

	void				LoadIgnoreList(ERuleType Type);
	void				StoreIgnoreList(ERuleType Type);

	struct SIgnoreEvent
	{
		bool bAllPaths = false;
		QSet<QString> Paths;
	};

	QMap<QString, SIgnoreEvent> m_IgnoreEvents[(int)ERuleType::eMax - 1];

private:
	void				LoadState(bool bFull = true);
	void				StoreState();

	//Ui::CMajorPrivacyClass ui;

	enum ETabs
	{
		eHome = 0,
		eProcesses,
		eEnclaves,
		eResource,
		eDrives,
		eFirewall,
		eDNS,
		eTweaks,
		eTabCount
	};

	QWidget*			m_pMainWidget = nullptr;
	QGridLayout*		m_pMainLayout = nullptr;
	//QToolBar*			m_pToolBar = nullptr;
	//QTabWidget*			m_pMainTabs = nullptr;
	QTabBar*			m_pTabBar = nullptr;
	QStackedLayout*		m_pPageStack = nullptr;

	
	QWidget*			m_pProgramWidget = nullptr;
	QVBoxLayout*		m_pProgramLayout = nullptr;
	
	QToolBar*			m_pProgramToolBar = nullptr;
	//QToolButton*		m_pBtnClear = nullptr;
	QToolButton*		m_pBtnTime = nullptr;
	QComboBox*			m_pCmbRecent = nullptr;

	QSplitter*			m_pProgramSplitter = nullptr;
	QWidget*			m_pPageSubWidget = nullptr;
	QStackedLayout*		m_pPageSubStack = nullptr;

	QMenu*				m_pMain = nullptr;
	QMenu*				m_pMaintenance = nullptr;
	QAction*			m_pImportOptions = nullptr;
	QAction*			m_pExportOptions = nullptr;
	QAction*			m_pVariantEditor = nullptr;
	QAction*			m_pOpenUserFolder = nullptr;
	QAction*			m_pOpenSystemFolder = nullptr;
	QMenu*				m_pMaintenanceItems = nullptr;
	QAction*			m_pConnect = nullptr;
	QAction*			m_pDisconnect = nullptr;
	QAction*			m_pInstallService = nullptr;
	QAction*			m_pRemoveService = nullptr;
	QAction*			m_pExit = nullptr;

	QMenu*				m_pView = nullptr;
	QAction*			m_pProgTree = nullptr;
	QAction*			m_pStackPanels = nullptr;
	QAction*			m_pMergePanels = nullptr;
	//QAction*			m_pShowMenu = nullptr;
	QAction*			m_pSplitColumns = nullptr;
	QAction*			m_pTabLabels = nullptr;
	QAction*			m_pWndTopMost = nullptr;

	QMenu*				m_pVolumes = nullptr;
	QAction*			m_pMountVolume = nullptr;
	QAction*			m_pUnmountAllVolumes = nullptr;
	QAction*			m_pCreateVolume = nullptr;

	QMenu*				m_pSecurity = nullptr;
	QAction*			m_pSignFile = nullptr;
	QAction*			m_pPrivateKey = nullptr;
	QAction*			m_pMakeKeyPair = nullptr;
	QAction*			m_pClearKeys = nullptr;

	QMenu*				m_pTools = nullptr;
	QAction*			m_pCleanUpProgs = nullptr;
	QAction*			m_pReGroupProgs = nullptr;
	QMenu*				m_pExpTools = nullptr;
	QAction*			m_pMountMgr = nullptr;
	QAction*			m_pImDiskCpl = nullptr;
	QAction*			m_pClearLogs = nullptr;

	QMenu*				m_pOptions = nullptr;
	QAction*			m_pSettings = nullptr;
	QAction*			m_pClearIgnore = nullptr;
	QAction*			m_pResetPrompts = nullptr;

	QMenu*				m_pMenuHelp = nullptr;
	QAction*			m_pForum = nullptr;
	//QAction*			m_pUpdate = nullptr;
	QAction*			m_pAbout = nullptr;
	QAction*			m_pAboutQt = nullptr;

	CHomePage*			m_HomePage = nullptr;

	CEnclavePage*		m_EnclavePage = nullptr;
	CProcessPage*		m_ProcessPage = nullptr;
	CAccessPage*		m_AccessPage = nullptr;
	CNetworkPage*		m_NetworkPage = nullptr;
	CDnsPage*			m_DnsPage = nullptr;
	CTweakPage*			m_TweakPage = nullptr;
	CVolumePage*		m_VolumePage = nullptr;

	QSplitter*			m_pInfoSplitter = nullptr;
	CInfoView*			m_pInfoView = nullptr;
	CProgramView*		m_pProgramView = nullptr;

	CPopUpWindow*		m_pPopUpWindow = nullptr;

	QSystemTrayIcon*	m_pTrayIcon = nullptr;
	QMenu*				m_pTrayMenu = nullptr;
	QMenu*				m_pFwProfileMenu = nullptr;
	QAction*			m_pFwBlockAll = nullptr;
	QAction*			m_pFwAllowList = nullptr;
	QAction*			m_pFwBlockList = nullptr;
	QAction*			m_pFwDisabled = nullptr;

protected:
	friend class CNativeEventFilter;
	CCustomTheme		m_CustomTheme;

	bool				m_ThemeUpdatePending = false;

private:
	void				LoadLanguage();
	void				LoadLanguage(const QString& Lang, const QString& Module, int Index);
	QTranslator			m_Translator[2];

	QString				m_Language;
	//quint32				m_LanguageId;
};

#define IGNORE_LIST_GROUP "IgnoreList"
QString GetIgnoreTypeName(ERuleType Type);
ERuleType GetIgnoreType(const QString& Key);

extern CMajorPrivacy* theGUI;
