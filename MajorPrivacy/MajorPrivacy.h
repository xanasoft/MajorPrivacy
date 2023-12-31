#pragma once

#include <QtWidgets/QMainWindow>
//#include "ui_MajorPrivacy.h"
#include "Core/Programs/WindowsService.h"
#include "Core/TraceLogEntry.h"
#include "../Library/Status.h"
#include "../MiscHelpers/Common/CustomTheme.h"

class CHomePage;
class CProcessPage;
class CFileSystemPage;
class CRegistryPage;
class CFirewallPage;
class CProgramView;
class CDnsPage;
class CProxyPage;
class CTweakPage;
class CDrivePage;

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

		QMap<quint64, CProcessPtr> Processes;
		QSet<CProgramFilePtr> Programs;
		QSet<CWindowsServicePtr> ServicesEx; // explicitly sellected services
		QSet<CWindowsServicePtr> ServicesIm; // implicitly sellected services

		bool bAllPrograms = false;
	};

	SCurrentItems		GetCurrentItems() const;

	QString				FormatError(const STATUS& Error);
	void				CheckResults(QList<STATUS> Results, QWidget* pParent, bool bAsync = false);

	bool				IsAlwaysOnTop() const;

signals:
	void				Closed();

public slots:
	void				OnUnruledFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);

	void				OpenSettings();

private slots:
	void				OnExit();
	void				OnHelp();
	void				OnAbout();

	void				BuildGUI();

	void				OnAlwaysTop();

	void				OnShowHide();
	void				OnSysTray(QSystemTrayIcon::ActivationReason Reason);

	void				OnFwProfile();

	void				OnPageChanged(int index);

protected:
	void				closeEvent(QCloseEvent *e);
	void				changeEvent(QEvent* e);
	void				timerEvent(QTimerEvent* pEvent);
	int					m_uTimerID;

	void				CreateTrayIcon();
	void				CreateTrayMenu();

	bool				m_bOnTop = false;
	bool				m_bExit = false;

private:

	SCurrentItems GetCurrentItems(const QList<CProgramItemPtr>& Progs) const;

	void				LoadState(bool bFull = true);
	void				StoreState();

	//Ui::CMajorPrivacyClass ui;

	enum ETabs
	{
		eHome = 0,
		//eEnclaves,
		eProcesses,
		eFileSystem,
		eRegistry,
		eFirewall,
		eDNS,
		eProxy,
		eDrives,
		eTweaks,
		//eLog,
		eTabCount
	};

	QWidget*			m_pMainWidget;
	QGridLayout*		m_pMainLayout;
	//QTabWidget*			m_pMainTabs;
	QTabBar*			m_pTabBar;
	QStackedLayout*		m_pPageStack;

	
	QWidget*			m_pProgramWidget;
	QVBoxLayout*		m_pProgramLayout;
	QToolBar*			m_pProgramToolBar;
	QSplitter*			m_pProgramSplitter;
	QWidget*			m_pPageSubWidget;
	QStackedLayout*		m_pPageSubStack;

	QMenu*				m_pMain;
	QAction*			m_pExit;

	QMenu*				m_pView;
	QAction*			m_pTabLabels;
	QAction*			m_pWndTopMost;

	QMenu*				m_pOptions;
	QAction*			m_pSettings;

	QMenu*				m_pMenuHelp;
	QAction*			m_pForum;
	//QAction*			m_pUpdate;
	QAction*			m_pAbout;
	QAction*			m_pAboutQt;

	CHomePage*			m_HomePage;

	CProcessPage*		m_ProcessPage;
	CFileSystemPage*	m_FileSystemPage;
	CRegistryPage*		m_RegistryPage;
	CFirewallPage*		m_FirewallPage;
	CDnsPage*			m_DnsPage;
	CProxyPage*			m_ProxyPage;
	CTweakPage*			m_TweakPage;
	CDrivePage*			m_DrivePage;

	CProgramView*		m_pProgramView;

	CPopUpWindow*		m_pPopUpWindow;

	QSystemTrayIcon*	m_pTrayIcon;
	QMenu*				m_pTrayMenu;
	QMenu*				m_pFwProfileMenu;
	QAction*			m_pFwBlockAll;
	QAction*			m_pFwAllowList;
	QAction*			m_pFwBlockList;
	QAction*			m_pFwDisabled;

	CCustomTheme		m_CustomTheme;
};

extern CMajorPrivacy* theGUI;
