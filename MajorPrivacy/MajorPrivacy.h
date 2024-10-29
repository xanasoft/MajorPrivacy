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

	const SCurrentItems& GetCurrentItems() const {return m_CurrentItems;}
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

signals:
	void				Closed();

	void				OnMountVolume();
	void				OnUnmountAllVolumes();
	void				OnCreateVolume();

public slots:
	void				OnExecutionEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pLogEntry);
	void				OnAccessEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pLogEntry);
	void				OnUnruledFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pLogEntry);

	void				OpenSettings();

	void				UpdateSettings(bool bRebuildUI);

	//void				ClearIgnoreLists();

	void				ClearLogs();

private slots:
	void				OnMaintenance();
	void				OnExit();
	void				OnHelp();
	void				OnAbout();

	void				BuildMenu();
	void				SetUITheme();
	void				BuildGUI();

	void				OnAlwaysTop();

	void				OnShowHide();
	void				OnSysTray(QSystemTrayIcon::ActivationReason Reason);

	void				OnSignFile();
	void				OnPrivateKey();
	void				OnMakeKeyPair();
	void				OnClearKeys();

	void				OnFwProfile();

	void				OnPageChanged(int index);

	void				OnProgramsChanged(const QList<CProgramItemPtr>& Programs);

	void				OnProgSplitter(int pos, int index);

protected:
	void				closeEvent(QCloseEvent *e);
	void				changeEvent(QEvent* e);
	void				timerEvent(QTimerEvent* pEvent);
	int					m_uTimerID;

	void				SetTitle();

	STATUS				InitSigner(class CPrivateKey& PrivateKey);

	void				CreateTrayIcon();
	void				CreateTrayMenu();

	SCurrentItems		MakeCurrentItems() const;
	SCurrentItems		MakeCurrentItems(const QList<CProgramItemPtr>& Progs) const;
	SCurrentItems		m_CurrentItems;

	bool				m_bShowAll = false;
	bool				m_bWasVisible = false;
	bool				m_bOnTop = false;
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

	void				Update();

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

	QWidget*			m_pMainWidget;
	QGridLayout*		m_pMainLayout;
	//QTabWidget*			m_pMainTabs;
	QTabBar*			m_pTabBar;
	QStackedLayout*		m_pPageStack;

	
	QWidget*			m_pProgramWidget;
	QVBoxLayout*		m_pProgramLayout;
	
	//QToolBar*			m_pProgramToolBar;
	//QToolButton*		m_pBtnClear;

	QSplitter*			m_pProgramSplitter;
	QWidget*			m_pPageSubWidget;
	QStackedLayout*		m_pPageSubStack;

	QMenu*				m_pMain;
	QMenu*				m_pMaintenance;
	QAction*			m_pImportOptions;
	QAction*			m_pExportOptions;
	QAction*			m_pOpenUserFolder;
	QAction*			m_pOpenSystemFolder;
	QAction*			m_pExit;

	QMenu*				m_pView;
	QAction*			m_pTabLabels;
	QAction*			m_pWndTopMost;

	QMenu*				m_pVolumes;
	QAction*			m_pMountVolume;
	QAction*			m_pUnmountAllVolumes;
	QAction*			m_pCreateVolume;

	QMenu*				m_pSecurity;
	QAction*			m_pSignFile;
	QAction*			m_pPrivateKey;
	QAction*			m_pMakeKeyPair;
	QAction*			m_pClearKeys;

	QMenu*				m_pOptions;
	QAction*			m_pSettings;
	//QAction*			m_pClearIgnore;
	QAction*			m_pClearLogs;

	QMenu*				m_pMenuHelp;
	QAction*			m_pForum;
	//QAction*			m_pUpdate;
	QAction*			m_pAbout;
	QAction*			m_pAboutQt;

	CHomePage*			m_HomePage;

	CEnclavePage*		m_EnclavePage;
	CProcessPage*		m_ProcessPage;
	CAccessPage*		m_AccessPage;
	CNetworkPage*		m_NetworkPage;
	CDnsPage*			m_DnsPage;
	CTweakPage*			m_TweakPage;
	CVolumePage*		m_VolumePage;

	QSplitter*			m_pInfoSplitter;
	CInfoView*			m_pInfoView;
	CProgramView*		m_pProgramView;

	CPopUpWindow*		m_pPopUpWindow;

	QSystemTrayIcon*	m_pTrayIcon;
	QMenu*				m_pTrayMenu;
	QMenu*				m_pFwProfileMenu;
	QAction*			m_pFwBlockAll;
	QAction*			m_pFwAllowList;
	QAction*			m_pFwBlockList;
	QAction*			m_pFwDisabled;

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
