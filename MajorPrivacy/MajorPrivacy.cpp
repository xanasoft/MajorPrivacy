#include "pch.h"
#include "MajorPrivacy.h"
#include "Core/PrivacyCore.h"
#include "Core/ProcessList.h"
#include "../MiscHelpers/Common/Common.h"
#include "version.h"
#include "Pages/HomePage.h"
#include "Pages/ProcessPage.h"
#include "Pages/FileSystemPage.h"
#include "Pages/RegistryPage.h"
#include "Pages/FirewallPage.h"
#include "Views/ProgramView.h"
#include "Pages/DnsPage.h"
#include "Pages/ProxyPage.h"
#include "Core/Programs/ProgramManager.h"
#include "Pages/TweakPage.h"
#include "Pages/DrivePage.h"
#include "Windows/SettingsWindow.h"
#include "../MiscHelpers/Common/MultiErrorDialog.h"
#include "../MiscHelpers/Common/ExitDialog.h"
#include "Windows/PopUpWindow.h"

CMajorPrivacy* theGUI = NULL;

HWND MainWndHandle = NULL;

CMajorPrivacy::CMajorPrivacy(QWidget *parent)
	: QMainWindow(parent)
{
	theGUI = this;

	MainWndHandle = (HWND)QWidget::winId();

	connect(theCore, SIGNAL(UnruledFwEvent(const CProgramFilePtr&, const CLogEntryPtr&)), this, SLOT(OnUnruledFwEvent(const CProgramFilePtr&, const CLogEntryPtr&)));

	//ui.setupUi(this);

	m_pMainWidget = NULL;
	
	m_pMain = menuBar()->addMenu("Privacy");
		m_pExit = m_pMain->addAction(QIcon(":/Icons/Exit.png"), tr("Exit"), this, SLOT(OnExit()));

	m_pView = menuBar()->addMenu("View");
		m_pTabLabels = m_pView->addAction(tr("Show Tab Labels"), this, SLOT(BuildGUI()));
		m_pTabLabels->setCheckable(true);
		m_pWndTopMost = m_pView->addAction(tr("Always on Top"), this, SLOT(OnAlwaysTop()));
		m_pWndTopMost->setCheckable(true);
		m_pWndTopMost->setChecked(theConf->GetBool("Options/AlwaysOnTop", false));

	m_pOptions = menuBar()->addMenu("Options");
		m_pSettings = m_pOptions->addAction(QIcon(":/Icons/Settings.png"), tr("Settings"), this, SLOT(OpenSettings()));

	m_pMenuHelp = menuBar()->addMenu(tr("&Help"));
		m_pForum = m_pMenuHelp->addAction(QIcon(":/Icons/Forum.png"), tr("Visit Support Forum"), this, SLOT(OnHelp()));
		m_pMenuHelp->addSeparator();
		//m_pUpdate = m_pMenuHelp->addAction(CSandMan::GetIcon("Update"), tr("Check for Updates"), this, SLOT(CheckForUpdates()));
		m_pMenuHelp->addSeparator();
		m_pAboutQt = m_pMenuHelp->addAction(tr("About the Qt Framework"), this, SLOT(OnAbout()));
		m_pAbout = m_pMenuHelp->addAction(QIcon(":/MajorPrivacy.png"), tr("About MajorPrivacy"), this, SLOT(OnAbout()));

	BuildGUI();

	m_pPopUpWindow = new CPopUpWindow();

	/*bool bAlwaysOnTop = theConf->GetBool("Options/AlwaysOnTop", false);
	this->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);
	m_pPopUpWindow->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);
	m_pProgressDialog->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);*/

	STATUS Status = theCore->Connect();

	CheckResults(QList<STATUS>() << Status, this);

	m_uTimerID = startTimer(250);

	// todo
}

CMajorPrivacy::~CMajorPrivacy()
{
	m_pPopUpWindow->close();
	delete m_pPopUpWindow;

	killTimer(m_uTimerID);

	StoreState();

	theCore->Disconnect();

	theGUI = NULL;
}

void CMajorPrivacy::changeEvent(QEvent* e)
{
	if (e->type() == QEvent::WindowStateChange) 
	{
		if (isMinimized()) 
		{
			if (m_bOnTop) {
				m_bOnTop = false;
				this->setWindowFlag(Qt::WindowStaysOnTopHint, m_bOnTop);
				SafeShow(this);
			}

			if (m_pTrayIcon->isVisible() && theConf->GetBool("Options/MinimizeToTray", false))
			{
				StoreState();
				hide();

				//if (theAPI->GetGlobalSettings()->GetBool("ForgetPassword", false))
				//	theAPI->ClearPassword();

				e->ignore();
				return;
			}
		}
	}
	QMainWindow::changeEvent(e); 
}

void CMajorPrivacy::closeEvent(QCloseEvent *e)
{
	if (!m_bExit)// && !theAPI->IsConnected())
	{
		QString OnClose = theConf->GetString("Options/OnClose", "ToTray");
		if (m_pTrayIcon->isVisible() && OnClose.compare("ToTray", Qt::CaseInsensitive) == 0)
		{
			StoreState();
			hide();

			//if (theAPI->GetGlobalSettings()->GetBool("ForgetPassword", false))
			//	theAPI->ClearPassword();

			e->ignore();
			return;
		}
		else if(OnClose.compare("Prompt", Qt::CaseInsensitive) == 0)
		{
			CExitDialog ExitDialog(tr("Do you want to close Sandboxie Manager?"));
			if (!ExitDialog.exec())
			{
				e->ignore();
				return;
			}
		}
	}

	emit Closed();

	//

	QApplication::quit();
}

void CMajorPrivacy::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() != m_uTimerID)
		return;

	theCore->Update();

	m_pProgramView->Update();
	m_ProcessPage->Update();
	m_FileSystemPage->Update();
	m_FirewallPage->Update();
	m_DnsPage->Update();
	m_ProxyPage->Update();
	m_DrivePage->Update();
	m_TweakPage->Update();
}

void CMajorPrivacy::LoadState(bool bFull)
{
	if (bFull) {
		setWindowState(Qt::WindowNoState);
		restoreGeometry(theConf->GetBlob("MainWindow/Window_Geometry"));
		restoreState(theConf->GetBlob("MainWindow/Window_State"));
	}

	if(m_pTabBar) m_pTabBar->setCurrentIndex(theConf->GetInt("MainWindow/MainTab", 0));
}

void CMajorPrivacy::StoreState()
{
	theConf->SetBlob("MainWindow/Window_Geometry", saveGeometry());
	theConf->SetBlob("MainWindow/Window_State", saveState());

	if(m_pTabBar) theConf->SetValue("MainWindow/MainTab", m_pTabBar->currentIndex());
}

void CMajorPrivacy::BuildGUI()
{
	int iDark = theConf->GetInt("Options/UseDarkTheme", 2);
	int iFusion = theConf->GetInt("Options/UseFusionTheme", 2);
	bool bDark = iDark == 2 ? m_CustomTheme.IsSystemDark() : (iDark == 1);
	m_CustomTheme.SetUITheme(bDark, iFusion);
	//CPopUpWindow::SetDarkMode(bDark);

	bool bFull = true;
	if (m_pMainWidget) {
		m_pMainWidget->deleteLater();
		bFull = false;
	}

	m_pMainWidget = new QWidget();
	m_pMainLayout = new QGridLayout(m_pMainWidget);
	m_pMainLayout->setContentsMargins(1, 1, 1, 1);
	m_pMainLayout->setSpacing(0);
	m_pMainWidget->setLayout(m_pMainLayout);
	this->setCentralWidget(m_pMainWidget);

	m_pProgramView = new CProgramView();

	m_HomePage = new CHomePage(this);
	m_ProcessPage = new CProcessPage(this);
	m_FileSystemPage = new CFileSystemPage(this);
	m_RegistryPage = new CRegistryPage(this);
	m_FirewallPage = new CFirewallPage(this);
	m_DnsPage = new CDnsPage(this);
	m_ProxyPage = new CProxyPage(this);
	m_TweakPage = new CTweakPage(this);
	m_DrivePage = new CDrivePage(this);

#if 1
	m_pTabBar = new  QTabBar();
	m_pTabBar->setShape(QTabBar::RoundedWest);
	m_pTabBar->addTab(QIcon(":/Icons/Home.png"), tr("System Overview"));
	//m_pTabBar->addTab(QIcon(":/Icons/BoxConfig.png"), tr("Secure Enclaves"));
	m_pTabBar->addTab(QIcon(":/Icons/Process.png"), tr("Process Security"));
	m_pTabBar->addTab(QIcon(":/Icons/Folder.png"), tr("File System Protection"));
	m_pTabBar->addTab(QIcon(":/Icons/RegEdit.png"), tr("Registry Protection"));
	m_pTabBar->addTab(QIcon(":/Icons/Wall.png"), tr("Network Firewall"));
	m_pTabBar->addTab(QIcon(":/Icons/Network2.png"), tr("DNS Filtering"));
	m_pTabBar->addTab(QIcon(":/Icons/Proxy.png"), tr("Proxy Injection"));
	m_pTabBar->addTab(QIcon(":/Icons/SecureDisk.png"), tr("Secure Drive"));
	m_pTabBar->addTab(QIcon(":/Icons/Tweaks.png"), tr("Privacy Tweaks"));
	//m_pTabBar->addTab(QIcon(":/Icons/SetLogging.png"), tr("Privacy Log"));
	m_pMainLayout->addWidget(m_pTabBar, 0, 0);

#ifndef _DEBUG
	m_pTabBar->setTabEnabled(eFileSystem, false);
	m_pTabBar->setTabEnabled(eRegistry, false);
	m_pTabBar->setTabEnabled(eProxy, false);
	m_pTabBar->setTabEnabled(eDrives, false);
#endif

	connect(m_pTabBar, SIGNAL(currentChanged(int)), this, SLOT(OnPageChanged(int)));

	m_pPageStack = new QStackedLayout();
	m_pMainLayout->addLayout(m_pPageStack, 0, 1, 2, 1);

	m_pPageStack->addWidget(m_HomePage);

	m_pProgramWidget = new QWidget();
	m_pProgramLayout = new QVBoxLayout(m_pProgramWidget);
	m_pProgramLayout->setContentsMargins(0, 0, 0, 0);
	m_pProgramToolBar = new QToolBar();

	/*m_pProgramToolBar->addAction("start", this, [&]() {

		QString Path = QInputDialog::getText(this, "Major Privacy", tr("Run Path"));
		if (Path.isEmpty())
			return;
		quint32 EnclaveId = QInputDialog::getInt(this, "Major Privacy", tr("enclave id"));

		theCore->StartProcessInEnvlave(Path, EnclaveId);
	});

	m_pProgramToolBar->addAction("test svc", this, [&]() {
		//theCore->Service()->TestSvc();
	});

	m_pProgramToolBar->addAction("test drv", this, [&]() {
		//theCore->Driver()->TestDrv();
	});*/

	m_pProgramLayout->addWidget(m_pProgramToolBar);
	m_pProgramSplitter = new QSplitter();
	m_pProgramSplitter->setOrientation(Qt::Horizontal);
	m_pProgramLayout->addWidget(m_pProgramSplitter);
	m_pPageStack->addWidget(m_pProgramWidget);

	m_pProgramSplitter->addWidget(m_pProgramView);

	m_pPageSubWidget = new QWidget();
	m_pPageSubStack = new QStackedLayout(m_pPageSubWidget);
	m_pProgramSplitter->addWidget(m_pPageSubWidget);
	m_pProgramSplitter->setCollapsible(1, false);

	m_pPageSubStack->addWidget(m_ProcessPage);
	m_pPageSubStack->addWidget(m_FileSystemPage);
	m_pPageSubStack->addWidget(m_RegistryPage);
	m_pPageSubStack->addWidget(m_FirewallPage);

	m_pPageStack->addWidget(m_DnsPage);
	m_pPageStack->addWidget(m_ProxyPage);
	m_pPageStack->addWidget(m_TweakPage);
	m_pPageStack->addWidget(m_DrivePage);
	//m_pPageStack->addWidget(new QWidget()); // logg ???

#else
	m_pMainTabs = new QTabWidget(m_pMainWidget);
	m_pMainTabs->setTabPosition(QTabWidget::West);
	m_pMainTabs->addTab(m_HomePage, QIcon(":/Icons/Home.png"), tr("System Overview"));
	//m_pMainTabs->addTab(new QWidget(), QIcon(":/Icons/BoxConfig.png"), tr("Secure Enclaves"));
	m_pMainTabs->addTab(m_ProcessPage, QIcon(":/Icons/Ampel.png"), tr("Process Security"));
	m_pMainTabs->addTab(m_FileSystemPage, QIcon(":/Icons/Folder.png"), tr("File System Protection"));
	m_pMainTabs->addTab(m_RegistryPage, QIcon(":/Icons/RegEdit.png"), tr("Registry Protection"));
	m_pMainTabs->addTab(m_FirewallPage, QIcon(":/Icons/Wall.png"), tr("Network Firewall"));
	m_pMainTabs->addTab(m_DnsPage, QIcon(":/Icons/Network2.png"), tr("DNS Filtering"));
	m_pMainTabs->addTab(m_ProxyPage, QIcon(":/Icons/Proxy.png"), tr("Proxy Injection"));
	m_pMainTabs->addTab(m_DrivePage, QIcon(":/Icons/SecureDisk.png"), tr("Secure Drive"));
	m_pMainTabs->addTab(m_TweakPage, QIcon(":/Icons/Maintenance.png"), tr("Privacy Tweaks"));
	//m_pMainTabs->addTab(new QWidget(), QIcon(":/Icons/SetLogging.png"), tr("Privacy Log"));
	m_pMainLayout->addWidget(m_pMainTabs);

	m_pTabBar = m_pMainTabs->tabBar();
#endif

	if (!m_pTabLabels->isChecked())
	{
		m_pTabBar->setProperty("imgSize", 32);
		m_pTabBar->setProperty("noLabels", true);
		for (int i = 0; i < m_pTabBar->count(); i++)
			m_pTabBar->setTabToolTip(i, m_pTabBar->tabText(i));
	}

	CreateTrayIcon();

	LoadState(bFull);
}

void CMajorPrivacy::CreateTrayIcon()
{
	m_pTrayIcon = new QSystemTrayIcon(QIcon(":/MajorPrivacy.png"), this);
	m_pTrayIcon->setToolTip("Major Privacy");
	connect(m_pTrayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(OnSysTray(QSystemTrayIcon::ActivationReason)));

	CreateTrayMenu();

	bool bAutoRun = QApplication::arguments().contains("-autorun");

	m_pTrayIcon->show(); // Note: qt bug; hide does not work if not showing first :/
	if(!bAutoRun && theConf->GetInt("Options/SysTrayIcon", 1) == 0)
		m_pTrayIcon->hide();
}

void CMajorPrivacy::CreateTrayMenu()
{
	m_pTrayMenu = new QMenu();
	QAction* pShowHide = m_pTrayMenu->addAction(QIcon(":/MajorPrivacy.png"), tr("Show/Hide"), this, SLOT(OnShowHide()));
	QFont f = pShowHide->font();
	f.setBold(true);
	pShowHide->setFont(f);
	m_pTrayMenu->addSeparator();

	m_pFwProfileMenu = m_pTrayMenu->addMenu(tr("Firewall Profile"));
	//m_pFwBlockAll = m_pFwProfileMenu->addAction(tr("Block All Traffic"), this, SLOT(OnFwProfile()));
	//m_pFwBlockAll->setCheckable(true);
	m_pFwAllowList = m_pFwProfileMenu->addAction(tr("Use Allow List"), this, SLOT(OnFwProfile()));
	m_pFwAllowList->setCheckable(true);
	m_pFwBlockList = m_pFwProfileMenu->addAction(tr("Use Block List"), this, SLOT(OnFwProfile()));
	m_pFwBlockList->setCheckable(true);
	m_pFwDisabled = m_pFwProfileMenu->addAction(tr("Disabled Firewall"), this, SLOT(OnFwProfile()));
	m_pFwDisabled->setCheckable(true);

	m_pTrayMenu->addSeparator();
	m_pTrayMenu->addAction(m_pExit);
}

void CMajorPrivacy::OnShowHide()
{
	if (isVisible()) {
		StoreState();
		hide();
	} else
		show();
}

void CMajorPrivacy::OnAlwaysTop()
{
	m_bOnTop = false;

	StoreState();
	bool bAlwaysOnTop = m_pWndTopMost->isChecked();
	theConf->SetValue("Options/AlwaysOnTop", bAlwaysOnTop);
	this->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);
	LoadState();
	SafeShow(this); // why is this needed?

	m_pPopUpWindow->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);
}

bool CMajorPrivacy::IsAlwaysOnTop() const
{
	return m_bOnTop || theConf->GetBool("Options/AlwaysOnTop", false);
}

void CMajorPrivacy::OnFwProfile()
{
	QAction* pAction = (QAction*)sender();
	STATUS Status;
	/*if (pAction == m_pFwBlockAll)
		Status = theCore->SetFwProfile(FwFilteringModes::BlockAll);
	else*/ if (pAction == m_pFwAllowList)
		Status = theCore->SetFwProfile(FwFilteringModes::AllowList);
	else if (pAction == m_pFwBlockList)
		Status = theCore->SetFwProfile(FwFilteringModes::BlockList);
	else if (pAction == m_pFwDisabled)
		Status = theCore->SetFwProfile(FwFilteringModes::NoFiltering);
	CheckResults(QList<STATUS>() << Status, this);
}

void CMajorPrivacy::OnSysTray(QSystemTrayIcon::ActivationReason Reason)
{
	static bool TriggerSet = false;
	static bool NullifyTrigger = false;

	if (theConf->GetBool("Options/TraySingleClick", false) && Reason == QSystemTrayIcon::Trigger)
		Reason = QSystemTrayIcon::DoubleClick;

	switch(Reason)
	{
	case QSystemTrayIcon::Context:
	{
		auto Result = theCore->GetFwProfile();
		if(!Result.IsError()){
			FwFilteringModes Profile = Result.GetValue();
			//m_pFwBlockAll->setChecked(Profile == FwFilteringModes::BlockAll);
			m_pFwAllowList->setChecked(Profile == FwFilteringModes::AllowList);
			m_pFwBlockList->setChecked(Profile == FwFilteringModes::BlockList);
			m_pFwDisabled->setChecked(Profile == FwFilteringModes::NoFiltering);
		}

		m_pTrayMenu->popup(QCursor::pos());	
		break;
	}
	case QSystemTrayIcon::DoubleClick:
		if (isVisible())
		{
			if(TriggerSet)
				NullifyTrigger = true;

			StoreState();
			hide();

			//if (theAPI->GetGlobalSettings()->GetBool("ForgetPassword", false))
			//	theAPI->ClearPassword();

			break;
		}
		//CheckSupport();
		show();
	case QSystemTrayIcon::Trigger:
		if (isVisible() && !TriggerSet)
		{
			TriggerSet = true;
			QTimer::singleShot(100, [this]() { 
				TriggerSet = false;
				if (NullifyTrigger) {
					NullifyTrigger = false;
					return;
				}
				this->setWindowState((this->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
				SetForegroundWindow(MainWndHandle);
				} );
		}
		m_pPopUpWindow->Poke();
		break;
	}
}

void CMajorPrivacy::OnPageChanged(int index)
{
	switch (index)
	{
	case eHome: m_pPageStack->setCurrentWidget(m_HomePage); break;
	//case eEnclaves: m_pPageStack->setCurrentWidget(m_); break;
	case eProcesses: 
	case eFileSystem:
	case eRegistry:
	case eFirewall:
		m_pPageStack->setCurrentWidget(m_pProgramWidget); 
		switch (index)
		{
		case eProcesses: m_pPageSubStack->setCurrentWidget(m_ProcessPage); break;
		case eFileSystem: m_pPageSubStack->setCurrentWidget(m_FileSystemPage); break;
		case eRegistry: m_pPageSubStack->setCurrentWidget(m_RegistryPage); break;
		case eFirewall: m_pPageSubStack->setCurrentWidget(m_FirewallPage); break;
		}
		break;
	case eDNS: m_pPageStack->setCurrentWidget(m_DnsPage); break;
	case eProxy: m_pPageStack->setCurrentWidget(m_ProxyPage); break;
	case eDrives: m_pPageStack->setCurrentWidget(m_DrivePage); break;
	case eTweaks: m_pPageStack->setCurrentWidget(m_TweakPage); break;
	//case eLog: m_pPageStack->setCurrentWidget(m_); break;
	}
}

CMajorPrivacy::SCurrentItems CMajorPrivacy::GetCurrentItems() const
{
	QList<CProgramItemPtr> Progs;
	bool bShowAll = m_pProgramSplitter->sizes().at(0) < 10;
	if (bShowAll) Progs.append(theCore->Programs()->GetRoot());
	else Progs = m_pProgramView->GetCurrentProgs();

	SCurrentItems Current = GetCurrentItems(Progs);

	// special case where the all programs item is selected
	if (!bShowAll && Current.bAllPrograms)
	{
		SCurrentItems All = GetCurrentItems(QList<CProgramItemPtr>() << theCore->Programs()->GetRoot());
	
		Current.Processes = All.Processes;
		Current.Programs = All.Programs;
		Current.ServicesEx.clear();
		Current.ServicesIm = All.ServicesIm;
	}

	return Current;
}

CMajorPrivacy::SCurrentItems CMajorPrivacy::GetCurrentItems(const QList<CProgramItemPtr>& Progs) const
{
	SCurrentItems Current;

	std::function<void(const CProgramItemPtr&)> AddItem = [&](const CProgramItemPtr& pItem) 
	{
		CWindowsServicePtr pService = pItem.objectCast<CWindowsService>();
		if (pService) {
			Current.ServicesIm.insert(pService);
			return;
		}

		CProgramSetPtr pSet = pItem.objectCast<CProgramSet>();
		if (!pSet) return;
		foreach(const CProgramItemPtr& pSubItem, pSet->GetNodes())
		{
			Current.Items.insert(pSubItem); // implicite

			AddItem(pSubItem);
		}
	};

	foreach(const CProgramItemPtr& pItem, Progs) 
	{
		Current.Items.insert(pItem); // explicite

		CWindowsServicePtr pService = pItem.objectCast<CWindowsService>();
		if (pService) {
			Current.ServicesEx.insert(pService);
			continue;
		}

		AddItem(pItem);

		if(!Current.bAllPrograms && pItem->inherits("CAllProgram")) // must be on root level
			Current.bAllPrograms = true;
	}

	Current.ServicesEx.subtract(Current.ServicesIm);
	

	foreach(CProgramItemPtr pItem, Current.Items)
	{
		CProgramFilePtr pProgram = pItem.objectCast<CProgramFile>();
		if (!pProgram)continue;

		Current.Programs.insert(pProgram);

		foreach(quint64 Pid, pProgram->GetProcessPids()) {
			CProcessPtr pProcess = theCore->Processes()->GetProcess(Pid);
			if (pProcess) Current.Processes.insert(Pid, pProcess);
		}
	}

	return Current;
}

QString CMajorPrivacy::FormatError(const STATUS& Error)
{
	const wchar_t* pMsg = Error.GetMessageText();
	if (!pMsg || !*pMsg) 
	{
		wchar_t* messageBuffer = nullptr;
		DWORD bufferLength = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, RtlNtStatusToDosError(Error.GetStatus()), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&messageBuffer, 0, nullptr);

		if (bufferLength) {
			QString msg = QString::fromWCharArray(messageBuffer);
			LocalFree(messageBuffer);
			return msg;
		}
		return tr("Error 0x%1").arg(QString::number(Error.GetStatus(), 16));
	}
	return QString::fromWCharArray(pMsg);
}

void CMajorPrivacy::CheckResults(QList<STATUS> Results, QWidget* pParent, bool bAsync)
{
	QStringList Errors;
	for (QList<STATUS>::iterator I = Results.begin(); I != Results.end(); ++I) {
		if (I->IsError())// && I->GetStatus() != OP_CANCELED)
			Errors.append(FormatError(*I));
	}

	/*if (bAsync) {
		foreach(const QString &Error, Errors)
			theGUI->OnLogMessage(Error, true);
	}
	else*/ if (Errors.count() == 1)
		QMessageBox::warning(pParent ? pParent : this, tr("Sandboxie-Plus - Error"), Errors.first());
	else if (Errors.count() > 1) {
		CMultiErrorDialog Dialog("MajorPrivacy", tr("Operation failed for %1 item(s).").arg(Errors.size()), Errors, pParent ? pParent : this);
		Dialog.exec();
	}
}

void CMajorPrivacy::OnUnruledFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry)
{
	if(theConf->GetBool("Firewall/ShowNotifications", true))
		m_pPopUpWindow->PushFwEvent(pProgram, pEntry);
}

void CMajorPrivacy::OpenSettings()
{
	static CSettingsWindow* pSettingsWindow = NULL;
	if (pSettingsWindow == NULL) {
		pSettingsWindow = new CSettingsWindow();
		connect(this, SIGNAL(Closed()), pSettingsWindow, SLOT(close()));
		connect(pSettingsWindow, SIGNAL(OptionsChanged(bool)), this, SLOT(UpdateSettings(bool)));
		connect(pSettingsWindow, &CSettingsWindow::Closed, [this]() {
			pSettingsWindow = NULL;
			});
		SafeShow(pSettingsWindow);
	}
	else {
		pSettingsWindow->setWindowState((pSettingsWindow->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
		SetForegroundWindow((HWND)pSettingsWindow->winId());
	}
}

void CMajorPrivacy::OnExit()
{
	m_bExit = true;
	close();
}

void CMajorPrivacy::OnHelp()
{
	if (sender() == m_pForum)
		QDesktopServices::openUrl(QUrl("https://xanasoft.com/go.php?to=forum"));
	else
		QDesktopServices::openUrl(QUrl("https://xanasoft.com/go.php?to=patreon"));
}

void CMajorPrivacy::OnAbout()
{
	if (sender() == m_pAbout)
	{
		QString AboutCaption = tr(
			"<h3>About MajorPrivacy</h3>"
			"<p>Version %1</p>"
			"<p>Copyright (c) 2023-2024 by DavidXanatos</p>"
		).arg(theGUI->GetVersion());


		QString AboutText = tr("GUI Data Folder: \n%1\n\nCore Data Folder: \n%2\n\n")
			.arg(theConf->GetConfigDir())
			.arg(theCore->GetConfigDir());

		QMessageBox *msgBox = new QMessageBox(this);
		msgBox->setAttribute(Qt::WA_DeleteOnClose);
		msgBox->setWindowTitle(tr("About MajorPrivacy"));
		msgBox->setText(AboutCaption);
		msgBox->setInformativeText(AboutText);

		QIcon ico(QLatin1String(":/MajorPrivacy.png"));
		
		QPixmap pix(128, 160);
		pix.fill(Qt::transparent);

		QPainter painter(&pix);
		painter.drawPixmap(0, 0, ico.pixmap(128, 128));

		msgBox->setIconPixmap(pix);

		SafeShow(msgBox);
		msgBox->exec();
	}
	else if (sender() == m_pAboutQt)
		QMessageBox::aboutQt(this);
}

QString CMajorPrivacy::GetVersion()
{
	QString Version = QString::number(VERSION_MJR) + "." + QString::number(VERSION_MIN) //.rightJustified(2, '0')
//#if VERSION_REV > 0 || VERSION_MJR == 0
		+ "." + QString::number(VERSION_REV)
//#endif
#if VERSION_UPD > 0
		+ QChar('a' + VERSION_UPD - 1)
#endif
		;
	return Version;
}