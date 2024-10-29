#include "pch.h"
#include "MajorPrivacy.h"
#include "Core/PrivacyCore.h"
#include "Core/Processes/ProcessList.h"
#include "../MiscHelpers/Common/Common.h"
#include "version.h"
#include "Pages/HomePage.h"
#include "Pages/ProcessPage.h"
#include "Pages/AccessPage.h"
#include "Pages/NetworkPage.h"
#include "Views/ProgramView.h"
#include "Pages/DnsPage.h"
#include "Core/Programs/ProgramManager.h"
#include "Pages/TweakPage.h"
#include "Pages/VolumePage.h"
#include "Pages/EnclavePage.h"
#include "Windows/SettingsWindow.h"
#include "../MiscHelpers/Common/MultiErrorDialog.h"
#include "../MiscHelpers/Common/ExitDialog.h"
#include "Windows/PopUpWindow.h"
#include "Windows/VolumeWindow.h"
#include "../Library/Crypto/Encryption.h"
#include "../Library/Crypto/PrivateKey.h"
#include "../Library/Crypto/PublicKey.h"
#include "../Library/Crypto/HashFunction.h"
#include "Views/InfoView.h"
#include <QAbstractNativeEventFilter>
#include "Core/Access/ResLogEntry.h"
#include "Core/Access/AccessManager.h"
#include "Core/Network/NetworkManager.h"
#include "../MiscHelpers/Common/OtherFunctions.h"
#include "Windows/OptionsTransferWnd.h"
#include "Helpers/WinHelper.h"
#include "../Library/Helpers/AppUtil.h"


CMajorPrivacy* theGUI = NULL;

class CNativeEventFilter : public QAbstractNativeEventFilter
{
public:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	virtual bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
#else
	virtual bool nativeEventFilter(const QByteArray &eventType, void *message, long *result)
#endif
	{
		if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG")
		{
			MSG *msg = static_cast<MSG *>(message);

			//if(msg->message != 275 && msg->message != 1025)
			//	qDebug() << msg->message;

			if (msg->message == WM_NOTIFY)
			{
				//return true;
			}
			else if (msg->message == WM_SETTINGCHANGE)
			{
				if (theGUI && theConf->GetInt("Options/UseDarkTheme", 2) == 2)
					theGUI->UpdateTheme();
			}
			else if (msg->message == WM_SHOWWINDOW && msg->wParam)
			{
				QWidget* pWidget = QWidget::find((WId)msg->hwnd);
				if (theGUI && pWidget && (pWidget->windowType() | Qt::Dialog) == Qt::Dialog)
					theGUI->m_CustomTheme.SetTitleTheme(msg->hwnd);
			}
		}
		return false;
	}
};

HWND MainWndHandle = NULL;

CMajorPrivacy::CMajorPrivacy(QWidget *parent)
	: QMainWindow(parent)
{
	theGUI = this;

#if defined(Q_OS_WIN)
	MainWndHandle = (HWND)winId();

	QApplication::instance()->installNativeEventFilter(new CNativeEventFilter);
#endif

	connect(theCore, SIGNAL(UnruledFwEvent(const CProgramFilePtr&, const CLogEntryPtr&)), this, SLOT(OnUnruledFwEvent(const CProgramFilePtr&, const CLogEntryPtr&)));
	connect(theCore, SIGNAL(ExecutionEvent(const CProgramFilePtr&, const CLogEntryPtr&)), this, SLOT(OnExecutionEvent(const CProgramFilePtr&, const CLogEntryPtr&)));
	connect(theCore, SIGNAL(AccessEvent(const CProgramFilePtr&, const CLogEntryPtr&)), this, SLOT(OnAccessEvent(const CProgramFilePtr&, const CLogEntryPtr&)));

	//ui.setupUi(this);

	m_pMainWidget = NULL;
	
	LoadLanguage();

	CFinder::m_CaseInsensitiveIcon = QIcon(":/Icons/CaseSensitive.png");
	CFinder::m_RegExpStrIcon = QIcon(":/Icons/RegExp.png");
	CFinder::m_HighlightIcon = QIcon(":/Icons/Highlight.png");

	BuildMenu();

	BuildGUI();

	m_pPopUpWindow = new CPopUpWindow();

	bool bAlwaysOnTop = theConf->GetBool("Options/AlwaysOnTop", false);
	this->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);
	//m_pPopUpWindow->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);
	//m_pProgressDialog->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);

	LoadIgnoreList();

	SafeShow(this);

	if(!IsRunningElevated())
		QMessageBox::information(this, tr("Major Privacy"), tr("Major Privacy will now attempt to start the Privacy Agent Service, please confirm the UAC prompt."));

	STATUS Status = theCore->Connect();
	CheckResults(QList<STATUS>() << Status, this);

	auto Res = theCore->GetSupportInfo();
	if (Res) {
		XVariant Info = Res.GetValue();
		g_CertName = Info[API_V_SUPPORT_NAME].AsQStr();
		g_CertInfo.State = Info[API_V_SUPPORT_STATUS].To<uint64>();
	}

	if (!g_CertInfo.active)
		QMessageBox::critical(this, tr("Major Privacy"), tr("Major Privacy is not activated - driver protections disabled!"));

	SetTitle();

#ifdef _DEBUG
	m_uTimerID = startTimer(500);
#else
	m_uTimerID = startTimer(250);
#endif
}

CMajorPrivacy::~CMajorPrivacy()
{
	m_pPopUpWindow->close();
	delete m_pPopUpWindow;

	killTimer(m_uTimerID);

	StoreState();

	theGUI = NULL;
}

void CMajorPrivacy::SetTitle()
{
	QString Title = "Major Privacy";
	QString Version = GetVersion();
	Title += " v" + Version;

	if (theCore->Driver()->IsConnected())
	{
		auto Ret = theCore->Driver()->GetUserKey();

		m_pSignFile->setEnabled(!Ret.IsError());
		m_pClearKeys->setEnabled(!Ret.IsError());
		m_pMakeKeyPair->setEnabled(Ret.IsError());

		if (!Ret.IsError()) 
		{
			m_pMakeKeyPair->setEnabled(false);

			auto pInfo = Ret.GetValue();

			//
			// We want to show the private key fingerprint in the title bar
			//

			if (theConf->GetBool("Options/ShowShortKey", true))
			{
				
				//
				// We want it to be short enough to be easily recognizable. 
				// Hence we use a 64 bit hash of the private key as the fingerprint.
				// To improve security we use a key derivation function to make it harder to brute force.
				//

				CBuffer FP(8); // 64 bits
				CEncryption::GetKeyFromPW(pInfo->PubKey, FP, 1048576); // 2^20 iterations

				Title += "   -   USER KEY: " + QByteArray((char*)FP.GetBuffer(), (int)FP.GetSize()).toHex().toUpper();
			}
			else
			{
				//
				// Alternatively we can also show the full SHA246 Hash of the private key.
				//

				CBuffer Hash(64);
				CHashFunction::Hash(pInfo->PubKey, Hash);

				Title += "   -   USER KEY: " + QByteArray((char*)Hash.GetBuffer(), (int)Hash.GetSize()).toHex().toUpper();
			}
		}
	}

	setWindowTitle(Title);
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
			CExitDialog ExitDialog(tr("Do you want to close Major Privacy?"));
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

	bool bVisible = isVisible() && !isMinimized();
	if (!bVisible) {

		if (m_bWasVisible)
			theCore->SetWatchedPrograms(QSet<CProgramItemPtr>());
	}
	else {

		Update();

		if (!m_bWasVisible)
			m_CurrentItems = MakeCurrentItems();
	}
	m_bWasVisible = bVisible;
}

void CMajorPrivacy::Update()
{
	STATUS Status = theCore->Update();
	if (!Status)
		theCore->Reconnect();

	m_EnclavePage->Update();
	m_pProgramView->Update();
	m_ProcessPage->Update();
	m_AccessPage->Update();
	m_NetworkPage->Update();
	m_DnsPage->Update();
	m_VolumePage->Update();
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

void CMajorPrivacy::BuildMenu()
{
	m_pMain = menuBar()->addMenu("Privacy");
	//m_pMain->addSeparator();
	m_pMaintenance = m_pMain->addMenu(QIcon(":/Icons/Maintenance.png"), tr("&Maintenance"));
		m_pImportOptions = m_pMaintenance->addAction(QIcon(":/Icons/Import.png"), tr("Import Options"), this, SLOT(OnMaintenance()));
		m_pExportOptions = m_pMaintenance->addAction(QIcon(":/Icons/Export.png"), tr("Export Options"), this, SLOT(OnMaintenance()));
		m_pMaintenance->addSeparator();
		m_pOpenUserFolder = m_pMaintenance->addAction(QIcon(":/Icons/Folder.png"), tr("Open User Data Folder"), this, SLOT(OnMaintenance()));
		m_pOpenSystemFolder = m_pMaintenance->addAction(QIcon(":/Icons/Folder.png"), tr("Open System Data Folder"), this, SLOT(OnMaintenance()));
	m_pMain->addSeparator();
	m_pExit = m_pMain->addAction(QIcon(":/Icons/Exit.png"), tr("Exit"), this, SLOT(OnExit()));


	m_pView = menuBar()->addMenu("View");
	m_pTabLabels = m_pView->addAction(tr("Show Tab Labels"), this, SLOT(BuildGUI()));
	m_pTabLabels->setCheckable(true);
	m_pWndTopMost = m_pView->addAction(tr("Always on Top"), this, SLOT(OnAlwaysTop()));
	m_pWndTopMost->setCheckable(true);
	m_pWndTopMost->setChecked(theConf->GetBool("Options/AlwaysOnTop", false));

	m_pVolumes = menuBar()->addMenu("Volumes");
	m_pMountVolume = m_pVolumes->addAction(QIcon(":/Icons/AddVolume.png"), tr("Mount Volume"), this, SIGNAL(OnMountVolume()));
	m_pUnmountAllVolumes = m_pVolumes->addAction(QIcon(":/Icons/UnmountVolume.png"), tr("Unmount All Volumes"), this, SIGNAL(OnUnmountAllVolumes()));
	m_pVolumes->addSeparator();
	m_pCreateVolume = m_pVolumes->addAction(QIcon(":/Icons/MountVolume.png"), tr("Create Volume"), this, SIGNAL(OnCreateVolume()));

	m_pSecurity = menuBar()->addMenu("Security");
	m_pSignFile = m_pSecurity->addAction(QIcon(":/Icons/SignFile.png"), tr("Sign File"), this, SLOT(OnSignFile()));
	//m_pPrivateKey = m_pSecurity->addAction(tr("Unlock Key"), this, SLOT(OnPrivateKey())); // todo
	m_pMakeKeyPair = m_pSecurity->addAction(QIcon(":/Icons/AddKey.png"), tr("Setup User Key"), this, SLOT(OnMakeKeyPair()));
	m_pClearKeys = m_pSecurity->addAction(QIcon(":/Icons/RemoveKey.png"), tr("Remove User Key"), this, SLOT(OnClearKeys()));

	m_pOptions = menuBar()->addMenu("Options");
	m_pSettings = m_pOptions->addAction(QIcon(":/Icons/Settings.png"), tr("Settings"), this, SLOT(OpenSettings()));
	//m_pClearIgnore = m_pOptions->addAction(QIcon(":/Icons/Clean.png"), tr("Clear Ignore List"), this, SLOT(ClearIgnoreLists()));
	m_pClearLogs = m_pOptions->addAction(QIcon(":/Icons/Clean.png"), tr("Clear Logs"), this, SLOT(ClearLogs()));

	m_pMenuHelp = menuBar()->addMenu(tr("&Help"));
	m_pForum = m_pMenuHelp->addAction(QIcon(":/Icons/Forum.png"), tr("Visit Support Forum"), this, SLOT(OnHelp()));
	m_pMenuHelp->addSeparator();
	//m_pUpdate = m_pMenuHelp->addAction(CSandMan::GetIcon("Update"), tr("Check for Updates"), this, SLOT(CheckForUpdates()));
	m_pMenuHelp->addSeparator();
	m_pAboutQt = m_pMenuHelp->addAction(tr("About the Qt Framework"), this, SLOT(OnAbout()));
	m_pAbout = m_pMenuHelp->addAction(QIcon(":/MajorPrivacy.png"), tr("About MajorPrivacy"), this, SLOT(OnAbout()));
}

void CMajorPrivacy::UpdateTheme()
{
	if (!m_ThemeUpdatePending) {
		m_ThemeUpdatePending = true;
		QTimer::singleShot(500, this, [&]() {
			m_ThemeUpdatePending = false;
			SetUITheme();
		});
	}
}

void CMajorPrivacy::SetUITheme()
{
	int iDark = theConf->GetInt("Options/UseDarkTheme", 2);
	int iFusion = theConf->GetInt("Options/UseFusionTheme", 1);
	bool bDark = iDark == 2 ? m_CustomTheme.IsSystemDark() : (iDark == 1);
	m_CustomTheme.SetUITheme(bDark, iFusion);
	//CPopUpWindow::SetDarkMode(bDark);
}

void CMajorPrivacy::BuildGUI()
{
	SetUITheme();

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

	connect(m_pProgramView, SIGNAL(ProgramsChanged(const QList<CProgramItemPtr>&)), this, SLOT(OnProgramsChanged(const QList<CProgramItemPtr>&)));

	m_HomePage = new CHomePage(this);
	m_EnclavePage = new CEnclavePage(this);
	m_ProcessPage = new CProcessPage(this);
	m_AccessPage = new CAccessPage(this);
	m_NetworkPage = new CNetworkPage(this);
	m_DnsPage = new CDnsPage(this);
	m_TweakPage = new CTweakPage(this);
	m_VolumePage = new CVolumePage(this);

	m_pTabBar = new  QTabBar();
	m_pTabBar->setShape(QTabBar::RoundedWest);
	m_pTabBar->addTab(QIcon(":/Icons/Home.png"), tr("System Overview"));
	m_pTabBar->addTab(QIcon(":/Icons/Process.png"), tr("Process Security"));
	m_pTabBar->addTab(QIcon(":/Icons/Enclave.png"), tr("Secure Enclaves"));
	m_pTabBar->addTab(QIcon(":/Icons/Ampel.png"), tr("Resource Protection"));
	m_pTabBar->addTab(QIcon(":/Icons/SecureDisk.png"), tr("Secure Drive"));
	m_pTabBar->addTab(QIcon(":/Icons/Wall3.png"), tr("Network Firewall"));
	m_pTabBar->addTab(QIcon(":/Icons/Network2.png"), tr("DNS Filtering"));
	m_pTabBar->addTab(QIcon(":/Icons/Tweaks.png"), tr("Privacy Tweaks"));
	m_pMainLayout->addWidget(m_pTabBar, 0, 0);

	connect(m_pTabBar, SIGNAL(currentChanged(int)), this, SLOT(OnPageChanged(int)));

	m_pPageStack = new QStackedLayout();
	m_pMainLayout->addLayout(m_pPageStack, 0, 1, 2, 1);

	m_pPageStack->addWidget(m_HomePage);

	m_pPageStack->addWidget(m_EnclavePage);

	m_pProgramWidget = new QWidget();
	m_pProgramLayout = new QVBoxLayout(m_pProgramWidget);
	m_pProgramLayout->setContentsMargins(0, 0, 0, 0);
	/*m_pProgramToolBar = new QToolBar();

	m_pBtnClear = new QToolButton();
	m_pBtnClear->setIcon(QIcon(":/Icons/Clean.png"));
	m_pBtnClear->setToolTip(tr("Cleanup Program List"));
	m_pBtnClear->setFixedHeight(22);
	m_pProgramToolBar->addWidget(m_pBtnClear);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pProgramToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pProgramView->m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setFixedHeight(22);
	m_pProgramToolBar->addWidget(pBtnSearch);


	m_pProgramLayout->addWidget(m_pProgramToolBar);*/
	m_pProgramSplitter = new QSplitter();
	m_pProgramSplitter->setOrientation(Qt::Horizontal);
	m_pProgramLayout->addWidget(m_pProgramSplitter);
	m_pPageStack->addWidget(m_pProgramWidget);

	m_pInfoSplitter = new QSplitter();
	m_pInfoSplitter->setOrientation(Qt::Vertical);
	m_pInfoSplitter->addWidget(m_pProgramView);
	m_pInfoView = new CInfoView();
	m_pInfoSplitter->addWidget(m_pInfoView);
	m_pProgramSplitter->addWidget(m_pInfoSplitter);
	m_pInfoSplitter->setStretchFactor(0, 1);
	m_pInfoSplitter->setStretchFactor(1, 0);
	auto Sizes = m_pInfoSplitter->sizes();
	Sizes[1] = 0;
	m_pInfoSplitter->setSizes(Sizes);

	m_pPageSubWidget = new QWidget();
	m_pPageSubStack = new QStackedLayout(m_pPageSubWidget);
	m_pProgramSplitter->addWidget(m_pPageSubWidget);
	m_pProgramSplitter->setCollapsible(1, false);

	connect(m_pProgramSplitter, SIGNAL(splitterMoved(int, int)), this, SLOT(OnProgSplitter(int, int)));

	m_pPageSubStack->addWidget(m_ProcessPage);
	m_pPageSubStack->addWidget(m_AccessPage);
	m_pPageSubStack->addWidget(m_NetworkPage);

	m_pPageStack->addWidget(m_DnsPage);
	m_pPageStack->addWidget(m_TweakPage);
	m_pPageStack->addWidget(m_VolumePage);
	//m_pPageStack->addWidget(new QWidget()); // logg ???

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

STATUS CMajorPrivacy::InitSigner(class CPrivateKey& PrivateKey)
{
	auto Ret = theCore->Driver()->GetUserKey();
	if(Ret.IsError())
		return Ret.GetStatus();
	auto pInfo = Ret.GetValue();

	CVolumeWindow window(CVolumeWindow::eGetPW, this);
	if (theGUI->SafeExec(&window) != 1)
		return OK;
	QString Password = window.GetPassword();
	if(Password.isEmpty())
		return OK;

	STATUS Status;
	do {
		CEncryption Encryption;
		Status = Encryption.SetPassword(CBuffer(Password.utf16(), Password.length() * sizeof(ushort), true));
		if(Status.IsError()) break;

		CBuffer KeyBlob;
		Encryption.Decrypt(pInfo->EncryptedBlob, KeyBlob);
		if(Status.IsError()) break;

		CVariant KeyData;
		KeyData.FromPacket(&KeyBlob);

		CBuffer PrivKey = KeyData[API_S_PUB_KEY];
		CBuffer Hash = KeyData[API_S_HASH];

		CBuffer NewHash;
		Status = CHashFunction::Hash(PrivKey, NewHash);
		if(Status.IsError()) break;	
		if(Hash.Compare(NewHash) != 0) {
			Status = ERR(STATUS_ERR_WRONG_PASSWORD);
			break;
		}

		Status = PrivateKey.SetPrivateKey(PrivKey);

	} while(0);

	return Status;
}

void CMajorPrivacy::OnSignFile()
{
	QStringList Paths = QFileDialog::getOpenFileNames(this, tr("Select Files"), QString(), tr("All Files (*.*)"));
	if (Paths.isEmpty())
		return;

	STATUS Status = SignFiles(Paths);
	CheckResults(QList<STATUS>() << Status, this);
}

STATUS CMajorPrivacy::SignFiles(const QStringList& Paths)
{
	CPrivateKey PrivateKey;
	STATUS Status = InitSigner(PrivateKey);
	if (!PrivateKey.IsPrivateKeySet())
		return Status;
	
	foreach(const QString & Path, Paths)
	{
		QFile File(Path);
		if (!File.open(QIODevice::ReadOnly))
			continue;

		CHashFunction HashFunction;
		Status = HashFunction.InitHash();
		if (Status.IsError()) continue;

		CBuffer Buffer(0x1000);
		for (;;) {
			int Read = File.read((char*)Buffer.GetBuffer(), Buffer.GetCapacity());
			if (Read <= 0) break;
			Buffer.SetSize(Read);
			Status = HashFunction.UpdateHash(Buffer);
			if (Status.IsError()) break;
		}
		if (Status.IsError()) continue;

		CBuffer Hash;
		Status = HashFunction.FinalizeHash(Hash);
		if (Status.IsError()) continue;

		CBuffer Signature;
		Status = PrivateKey.Sign(Hash, Signature);
		if (Status.IsError()) continue;

		QString TempPath = QDir::tempPath() + "/" + Split2(Path, "/", true).second + ".sig";
		QFile SignatureFile(TempPath);
		if (SignatureFile.open(QIODevice::WriteOnly)) {
			SignatureFile.write((char*)Signature.GetBuffer(), Signature.GetSize());
			SignatureFile.close();
		}

		QString SignaturePath = Split2(Path, ".", true).first + ".sig";
		WindowsMoveFile(TempPath.replace("/", "\\"), SignaturePath.replace("/", "\\"));
	}

	return OK;
}

void CMajorPrivacy::OnPrivateKey()
{
}

void CMajorPrivacy::OnMakeKeyPair()
{
	CVolumeWindow window(CVolumeWindow::eSetPW, this);
	if (theGUI->SafeExec(&window) != 1)
		return;
	QString Password = window.GetPassword();
	if(Password.isEmpty())
		return;

	STATUS Status;
	do {
		CPrivateKey PrivateKey;
		CPublicKey PublicKey;
		Status = PrivateKey.MakeKeyPair(PublicKey);
		if(Status.IsError()) break;

		CBuffer PrivKey;
		PrivateKey.GetPrivateKey(PrivKey);

		CEncryption Encryption;
		Status = Encryption.SetPassword(CBuffer(Password.utf16(), Password.length() * sizeof(ushort), true));
		if(Status.IsError()) break;

		CBuffer Hash;
		CHashFunction::Hash(PrivKey, Hash);
		if(Status.IsError()) break;

		CVariant KeyData;
		KeyData[API_S_PUB_KEY] = PrivKey;
		KeyData[API_S_HASH] = Hash;

		CBuffer KeyBlob;
		KeyData.ToPacket(&KeyBlob);

		CBuffer EncryptedBlob;
		Status = Encryption.Encrypt(KeyBlob, EncryptedBlob);
		if(Status.IsError()) break;

		CBuffer PubKey;
		PublicKey.GetPublicKey(PubKey);
		Status = theCore->Driver()->SetUserKey(PubKey, EncryptedBlob);

	} while(0);
	CheckResults(QList<STATUS>() << Status, this);

	SetTitle();
}

void CMajorPrivacy::OnClearKeys()
{
	CPrivateKey PrivateKey;
	STATUS Status = InitSigner(PrivateKey);
	if (!PrivateKey.IsPrivateKeySet()) { // eider error or user canceled
		CheckResults(QList<STATUS>() << Status, this);
		return;
	}

	do {
		CBuffer Challenge;
		Status = theCore->Driver()->GetChallenge(Challenge);
		if(Status.IsError()) break;

		CBuffer Hash;
		CHashFunction::Hash(Challenge, Hash);

		CBuffer Response;
		PrivateKey.Sign(Hash, Response);

		Status = theCore->Driver()->ClearUserKey(Response);

	} while(0);
	CheckResults(QList<STATUS>() << Status, this);

	SetTitle();
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
	case eProcesses: 
	case eResource:
	case eFirewall:
		m_pPageStack->setCurrentWidget(m_pProgramWidget); 
		switch (index)
		{
		case eProcesses: m_pPageSubStack->setCurrentWidget(m_ProcessPage); break;
		case eResource: m_pPageSubStack->setCurrentWidget(m_AccessPage); break;
		case eFirewall: m_pPageSubStack->setCurrentWidget(m_NetworkPage); break;
		}
		break;
	case eEnclaves: m_pPageStack->setCurrentWidget(m_EnclavePage); break;
	case eDrives: m_pPageStack->setCurrentWidget(m_VolumePage); break;
	case eDNS: m_pPageStack->setCurrentWidget(m_DnsPage); break;
	case eTweaks: m_pPageStack->setCurrentWidget(m_TweakPage); break;
	//case eLog: m_pPageStack->setCurrentWidget(m_); break;
	}
}

void CMajorPrivacy::OnProgramsChanged(const QList<CProgramItemPtr>& Programs)
{
	m_pInfoView->Sync(m_pProgramView->GetCurrentProgs());
	m_CurrentItems = MakeCurrentItems();
}

void CMajorPrivacy::OnProgSplitter(int pos, int index)
{
	bool bShowAll = m_pProgramSplitter->sizes().at(0) < 10;
	if(m_bShowAll != bShowAll) {
		m_bShowAll = bShowAll;
		m_CurrentItems = MakeCurrentItems();
	}
}

CMajorPrivacy::SCurrentItems CMajorPrivacy::MakeCurrentItems() const
{
	QList<CProgramItemPtr> Progs = m_pProgramView->GetCurrentProgs();
	bool bShowAll = m_pProgramSplitter->sizes().at(0) < 10;
	if (bShowAll || Progs.isEmpty()) Progs.append(theCore->ProgramManager()->GetRoot());

	SCurrentItems Current = MakeCurrentItems(Progs);

	// special case where the all programs item is selected
	if (!bShowAll && Current.bAllPrograms)
	{
		SCurrentItems All = MakeCurrentItems(QList<CProgramItemPtr>() << theCore->ProgramManager()->GetRoot());
	
		//Current.Processes = All.Processes;
		Current.Programs = All.Programs;
		Current.ServicesEx.clear();
		Current.ServicesIm = All.ServicesIm;
	}

	theCore->SetWatchedPrograms(Current.Items);

	return Current;
}

CMajorPrivacy::SCurrentItems CMajorPrivacy::MakeCurrentItems(const QList<CProgramItemPtr>& Progs) const
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

		if(!Current.bAllPrograms && pItem->inherits("CAllPrograms")) // must be on root level
			Current.bAllPrograms = true;
	}

	Current.ServicesEx.subtract(Current.ServicesIm);
	

	foreach(CProgramItemPtr pItem, Current.Items)
	{
		CProgramFilePtr pProgram = pItem.objectCast<CProgramFile>();
		if (!pProgram)continue;

		Current.Programs.insert(pProgram);

		/*foreach(quint64 Pid, pProgram->GetProcessPids()) {
			CProcessPtr pProcess = theCore->ProcessList()->GetProcess(Pid);
			if (pProcess) Current.Processes.insert(Pid, pProcess);
		}*/
	}

	return Current;
}

QMap<quint64, CProcessPtr> CMajorPrivacy::GetCurrentProcesses() const
{
	QMap<quint64, CProcessPtr> Processes;

	foreach(CProgramFilePtr pProgram, m_CurrentItems.Programs) {
		foreach(quint64 Pid, pProgram->GetProcessPids()) {
			CProcessPtr pProcess = theCore->ProcessList()->GetProcess(Pid);
			if (pProcess) Processes.insert(Pid, pProcess);
		}
	}

	foreach(CWindowsServicePtr pService, m_CurrentItems.ServicesEx) {
		CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pService->GetProcessId());
		if (pProcess) Processes.insert(pService->GetProcessId(), pProcess);
	}

	return Processes;
}

QString CMajorPrivacy::FormatError(const STATUS& Error)
{
	const wchar_t* pMsg = Error.GetMessageText();
	if (pMsg && *pMsg)
		return QString::fromWCharArray(pMsg);
	
	ULONG status = Error.GetStatus();
	if (IS_MAJOR_STATUS(status))
	{
		switch (status)
		{
		case STATUS_ERR_PROG_NOT_FOUND:				return tr("Program not found.");
		case STATUS_ERR_DUPLICATE_PROG:				return tr("Program already exists.");
		case STATUS_ERR_PROG_HAS_RULES:				return tr("Program has rules.");
		case STATUS_ERR_PROG_PARENT_NOT_FOUND:		return tr("Parent program not found.");
		case STATUS_ERR_CANT_REMOVE_FROM_PATTERN:	return tr("Can't remove from pattern.");
		case STATUS_ERR_PROG_PARENT_NOT_VALID:		return tr("Parent program not valid.");
		case STATUS_ERR_CANT_REMOVE_DEFAULT_ITEM:	return tr("Can't remove default item.");
		case STATUS_ERR_NO_USER_KEY:				return tr("Before you can sign a Binary you need to create your User Key, use the 'Security->Setup User Key' menu comand to do that.");
		case STATUS_ERR_WRONG_PASSWORD:				return tr("Wrong Password!");
		}
	}
	else
	{
		wchar_t* messageBuffer = nullptr;
		DWORD bufferLength = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, RtlNtStatusToDosError(status), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, nullptr);

		if (bufferLength) {
			QString msg = QString::fromWCharArray(messageBuffer);
			LocalFree(messageBuffer);
			return msg;
		}
	}
	return tr("Error 0x%1").arg(QString::number(status, 16).right(8));
}

int CMajorPrivacy::CheckResults(QList<STATUS> Results, QWidget* pParent, bool bAsync)
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
		QMessageBox::warning(pParent ? pParent : this, tr("MajorPrivacy - Error"), Errors.first());
	else if (Errors.count() > 1) {
		CMultiErrorDialog Dialog("MajorPrivacy", tr("Operation failed for %1 item(s).").arg(Errors.size()), Errors, pParent ? pParent : this);
		Dialog.exec();
	}
	return Errors.count();
}

void CMajorPrivacy::ShowNotification(const QString& Title, const QString& Message, QSystemTrayIcon::MessageIcon Icon, int Timeout)
{
	m_pTrayIcon->showMessage(Title, Message, Icon, Timeout);
	statusBar()->showMessage(Message, Timeout);
}

void CMajorPrivacy::IgnoreEvent(ERuleType Type, const CProgramFilePtr& pProgram, const QString& Path)
{
	if (Path.isEmpty()) {
		ASSERT(!pProgram.isNull());
		m_IgnoreEvents[(int)Type - 1][pProgram.isNull() ? "" : pProgram->GetPath(EPathType::eWin32).toLower()].bAllPaths = true;
	} else
		m_IgnoreEvents[(int)Type - 1][pProgram.isNull() ? "" : pProgram->GetPath(EPathType::eWin32).toLower()].Paths.insert(Path);

	StoreIgnoreList(Type);
}

bool CMajorPrivacy::IsEventIgnored(ERuleType Type, const CProgramFilePtr& pProgram, const CLogEntryPtr& pLogEntry) const
{
	const QMap<QString, SIgnoreEvent>& Map = m_IgnoreEvents[(int)Type - 1];
	if (Map.isEmpty())
		return false;

	SIgnoreEvent Ignore = Map.value(pProgram->GetPath(EPathType::eWin32).toLower());

	if (Ignore.bAllPaths)
		return true;


	QString Path;
	if (Type == ERuleType::eProgram)
	{
		const CExecLogEntry* pEntry = dynamic_cast<const CExecLogEntry*>(pLogEntry.constData());
		if (pEntry->GetType() == EExecLogType::eImageLoad)
		{
			uint64 uLibRef = pEntry->GetMiscID();
			CProgramLibraryPtr pLibrary = theCore->ProgramManager()->GetLibraryByUID(uLibRef);
			if (pLibrary) {
				Path = pLibrary->GetPath(EPathType::eWin32);
			}
		}
		else if (pEntry->GetType() == EExecLogType::eProcessStarted)
		{
			uint64 uID = pEntry->GetMiscID();
			CProgramFilePtr pExecutable = theCore->ProgramManager()->GetProgramByUID(uID).objectCast<CProgramFile>();
			if (pExecutable) {
				Path = pExecutable->GetPath(EPathType::eWin32);
			}
		}
	}
	else if (Type == ERuleType::eAccess)
	{
		const CResLogEntry* pEntry = dynamic_cast<const CResLogEntry*>(pLogEntry.constData());
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		if (pEntry) {
			Path = pEntry->GetPath(EPathType::eWin32);
		}
	}


	// Test Current Program

	auto TestPath = [](const QString& Path, const QSet<QString>& Paths){ 

		foreach(auto TestPath, Paths) 
		{	
			if (TestPath.contains("*")) {
				QString PathExp = QRegularExpression::escape(TestPath).replace("\\*", ".*");
				QRegularExpression RegExp(PathExp, QRegularExpression::CaseInsensitiveOption);
				if (RegExp.match(Path).hasMatch() || RegExp.match(Path + "\\").hasMatch())
					return true;
			}
			else if(TestPath.compare(Path, Qt::CaseInsensitive) == 0)
				return true;
		}
		return false; 
	};

	if(TestPath(Path, Ignore.Paths))
		return true;


	// Test All Programs

	SIgnoreEvent IgnoreAll = Map.value(NULL);

	if(TestPath(Path, IgnoreAll.Paths))
		return true;


	return false;
}

void CMajorPrivacy::LoadIgnoreList()
{
	LoadIgnoreList(ERuleType::eProgram);
	LoadIgnoreList(ERuleType::eAccess);
	LoadIgnoreList(ERuleType::eFirewall);
}

void CMajorPrivacy::StoreIgnoreList()
{
	StoreIgnoreList(ERuleType::eProgram);
	StoreIgnoreList(ERuleType::eAccess);
	StoreIgnoreList(ERuleType::eFirewall);
}

//void CMajorPrivacy::ClearIgnoreLists()
//{
//	if(QMessageBox::question(this, tr("MajorPrivacy"), tr("Do you really want to clear the ignore list?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
//		return;
//
//	m_IgnoreEvents[(int)ERuleType::eProgram - 1].clear();
//	m_IgnoreEvents[(int)ERuleType::eAccess - 1].clear();
//	m_IgnoreEvents[(int)ERuleType::eFirewall - 1].clear();
//
//	QStringList Keys = theConf->ListKeys(IGNORE_LIST_GROUP);
//	foreach(const QString & Key, Keys)
//		theConf->DelValue(IGNORE_LIST_GROUP "/" + Key);
//}

void CMajorPrivacy::ClearLogs()
{
	theCore->ClearLogs();
}

QString GetIgnoreTypeName(ERuleType Type)
{
	switch (Type)
	{
	case ERuleType::eProgram: return "IgnorePrograms"; 
	case ERuleType::eAccess: return "IgnoreAccess"; 
	case ERuleType::eFirewall: return "IgnoreFirewall"; 
	}
	return "";
}

ERuleType GetIgnoreType(const QString& Key)
{
	QString Type = Split2(Key, "_").first;
	if (Type == "IgnorePrograms") return ERuleType::eProgram;
	if (Type == "IgnoreAccess") return ERuleType::eAccess;
	if (Type == "IgnoreFirewall") return ERuleType::eFirewall;
	return ERuleType::eUnknown;
}

void CMajorPrivacy::LoadIgnoreList(ERuleType Type)
{
	QMap<QString, SIgnoreEvent>& Map = m_IgnoreEvents[(int)Type - 1];
	Map.clear();

	QString Prefix = GetIgnoreTypeName(Type);

	QStringList Keys = theConf->ListKeys(IGNORE_LIST_GROUP);
	foreach(const QString & Key, Keys) {
		if(Key.startsWith(Prefix)) {
			QString Value = theConf->GetString(IGNORE_LIST_GROUP "/" + Key);
			QStringList Parts = Value.split("|");
			if(Parts.count() == 1)
				Map[Parts[0].toLower()].bAllPaths = true;
			else if (Parts.count() == 2) {
				if(Parts[0] == "*")
					Map[""].Paths.insert(Parts[1]);
				else
					Map[Parts[0].toLower()].Paths.insert(Parts[1]);
			}
		}
	}
}

void CMajorPrivacy::StoreIgnoreList(ERuleType Type)
{
	const QMap<QString, SIgnoreEvent>& Map = m_IgnoreEvents[(int)Type - 1];

	QString Prefix = GetIgnoreTypeName(Type);

	QStringList Keys = theConf->ListKeys(IGNORE_LIST_GROUP);
	foreach(const QString & Key, Keys) {
		if(Key.startsWith(Prefix))
			theConf->DelValue(IGNORE_LIST_GROUP "/" + Key);
	}

	int i = 0;
	for(auto I = Map.begin(); I != Map.end(); ++I) {
		if(I.value().bAllPaths)
			theConf->SetValue(IGNORE_LIST_GROUP "/" + Prefix + "_" + QString::number(++i), I.key());
		else {
			foreach(const QString & Path, I.value().Paths)
				theConf->SetValue(IGNORE_LIST_GROUP "/" + Prefix + "_" + QString::number(++i), (I.key().isEmpty() ? "*" : I.key()) + "|" + Path);
		}
	}
}

void CMajorPrivacy::OnExecutionEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pLogEntry)
{
	if (theConf->GetBool("ProcessProtection/ShowNotifications", true)) {
		if (!IsEventIgnored(ERuleType::eProgram, pProgram, pLogEntry))
			m_pPopUpWindow->PushExecEvent(pProgram, pLogEntry);
	}
}

void CMajorPrivacy::OnAccessEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pLogEntry)
{
	if (theConf->GetBool("ResourceAccess/ShowNotifications", true)) {
		if (!IsEventIgnored(ERuleType::eAccess, pProgram, pLogEntry))
			m_pPopUpWindow->PushResEvent(pProgram, pLogEntry);
	}
}

void CMajorPrivacy::OnUnruledFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pLogEntry)
{
	if (theConf->GetBool("NetworkFirewall/ShowNotifications", false)) {
		if (!IsEventIgnored(ERuleType::eFirewall, pProgram, pLogEntry))
			m_pPopUpWindow->PushFwEvent(pProgram, pLogEntry);
	}
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

void CMajorPrivacy::UpdateSettings(bool bRebuildUI)
{
	if (bRebuildUI)
	{
		StoreState();

		LoadLanguage();

		BuildGUI();
	}
}

void CMajorPrivacy::OnMaintenance()
{
	if (sender() == m_pImportOptions) {
	
		QString FileName = QFileDialog::getOpenFileName(this, tr("Import Options"), "", tr("Options Files (*.xml)"));
		if (FileName.isEmpty())
			return;

		QString Xml = ReadFileAsString(FileName);

		XVariant Options;
		bool bOk = Options.ParseXml(Xml);
		if(!bOk) {
			QMessageBox::warning(this, tr("MajorPrivacy"), tr("Failed to parse XML data!"));
			return;
		}

		quint32 Selection = 0;
		if(Options.Has(API_S_GUI_CONFIG))
			Selection |= COptionsTransferWnd::eGUI;
		if(Options.Has(API_S_DRIVER_CONFIG))
			Selection |= COptionsTransferWnd::eDriver;
		 if(Options.Has(API_S_USER_KEY))
			 Selection |= COptionsTransferWnd::eUserKeys;
		 if(Options.Has(API_S_PROGRAM_RULES))
			Selection |= COptionsTransferWnd::eExecRules;
		 if(Options.Has(API_S_ACCESS_RULES))
			Selection |= COptionsTransferWnd::eResRules;
		if(Options.Has(API_S_SERVICE_CONFIG))
			Selection |= COptionsTransferWnd::eService;
		 /*if(Options.Has(API_S_FW_RULES)) // todo
			Selection |= COptionsTransferWnd::eFwRules;
		 if(Options.Has(API_S_PROGRAMS))
			Selection |= COptionsTransferWnd::ePrograms;*/
		//  if(Options.Has(API_S_TRACELOG))
		//	Selection |= COptionsTransferWnd::eTraceLog;

		COptionsTransferWnd wnd(COptionsTransferWnd::eImport, Selection, this);
		auto UserKey = theCore->Driver()->GetUserKey();
		if (Options.Has(API_S_USER_KEY)) {
			if (!UserKey.IsError()) // if a key is set it can not be changed
				wnd.Disable(COptionsTransferWnd::eUserKeys);
		}
		if(!wnd.exec())
			return;
		Selection = wnd.GetOptions();

		if (Selection & COptionsTransferWnd::eGUI) {
			XVariant Data = Options[API_S_GUI_CONFIG];
			for (size_t i = 0; i < Data.Count(); i++) {
				QString Section = Data.Key(i);
				XVariant Values = Data[Data.Key(i)];
				for (size_t j = 0; j < Values.Count(); j++) {
					QString Key = Values.Key(j);
					XVariant Value = Values[Values.Key(j)];
					theConf->SetValue(Section + "/" + Key, Value.ToQVariant());
				}
			}
		}

		if (Selection & COptionsTransferWnd::eDriver) {
			theCore->SetDrvConfig(Options[API_S_DRIVER_CONFIG]);
		}

		if (Selection & COptionsTransferWnd::eUserKeys) {
			XVariant Keys = Options[API_S_USER_KEY];
			theCore->Driver()->SetUserKey(Keys[API_S_PUB_KEY], Keys[API_S_KEY_BLOB]);
		}

		if (Selection & COptionsTransferWnd::eExecRules) {
			theCore->SetAllProgramRules(Options[API_S_PROGRAM_RULES]);
		}

		if (Selection & COptionsTransferWnd::eResRules) {
			theCore->SetAllAccessRules(Options[API_S_ACCESS_RULES]);
		}

		if (Selection & COptionsTransferWnd::eService) {
			theCore->SetSvcConfig(Options[API_S_SERVICE_CONFIG]);
		}

		if (Selection & COptionsTransferWnd::eFwRules) {
			// todo
		}

		if (Selection & COptionsTransferWnd::ePrograms) {
			// todo
		}

		//if (Selection & COptionsTransferWnd::eTraceLog) {
		//
		//}
	}
	else if (sender() == m_pExportOptions) {

		auto UserKey = theCore->Driver()->GetUserKey();

		COptionsTransferWnd wnd(COptionsTransferWnd::eExport, COptionsTransferWnd::eAllPermanent, this);
		if(UserKey.IsError())
			wnd.Disable(COptionsTransferWnd::eUserKeys);
		if(!wnd.exec())
			return;
		quint32 Selection = wnd.GetOptions();
		SVarWriteOpt Ops;
		Ops.Format = SVarWriteOpt::eMap; // todo: allow to select index format when exporting??

		XVariant Options;

		if (Selection & COptionsTransferWnd::eGUI) {

			XVariant Data;
			Data.BeginMap();
			for(auto Section: theConf->ListGroupes())
			{
				XVariant Values;
				Values.BeginMap();
				for(auto Key: theConf->ListKeys(Section))
					Values.WriteQStr(Key.toStdString().c_str(), theConf->GetString(Section, Key));
				Values.Finish();
				Data.WriteVariant(Section.toStdString().c_str(), Values);
			}
			Data.Finish();

			Options[API_S_GUI_CONFIG] = Data;
		}

		if (Selection & COptionsTransferWnd::eDriver) {
			auto ret = theCore->GetDrvConfig();
			if(ret) Options[API_S_DRIVER_CONFIG] = ret.GetValue();
		}

		if (Selection & COptionsTransferWnd::eUserKeys) {
			XVariant Keys;
			Keys[API_S_PUB_KEY] = CVariant(UserKey.GetValue()->PubKey);
			Keys[API_S_KEY_BLOB] = CVariant(UserKey.GetValue()->EncryptedBlob);
			Options[API_S_USER_KEY] = Keys;
		}

		if (Selection & COptionsTransferWnd::eExecRules) {
			XVariant ProgramRules;
			for (auto pRule : theCore->ProgramManager()->GetProgramRules()) {
				if (pRule->IsTemporary()) continue;
				ProgramRules.Append(pRule->ToVariant(Ops));
			}
			Options[API_S_PROGRAM_RULES] = ProgramRules;
		}

		if (Selection & COptionsTransferWnd::eResRules) {
			XVariant AccessRules;
			for (auto pRule : theCore->AccessManager()->GetAccessRules()) {
				if (pRule->IsTemporary()) continue;
				AccessRules.Append(pRule->ToVariant(Ops));
			}
			Options[API_S_ACCESS_RULES] = AccessRules;
		}

		if (Selection & COptionsTransferWnd::eService) {
			auto ret = theCore->GetSvcConfig();
			if(ret) Options[API_S_SERVICE_CONFIG] = ret.GetValue();
		}

		if (Selection & COptionsTransferWnd::eFwRules) {
			XVariant FwRules;
			for (auto pRule : theCore->NetworkManager()->GetFwRules())
				FwRules.Append(pRule->ToVariant(Ops));
			Options[API_S_FW_RULES] = FwRules;
		}

		if (Selection & COptionsTransferWnd::ePrograms) {
			XVariant Programs;
			for (auto pProgram : theCore->ProgramManager()->GetItems()) {
				if (pProgram == theCore->ProgramManager()->GetRoot() || pProgram == theCore->ProgramManager()->GetAll())
					continue; // skip built in items
				Programs.Append(pProgram->ToVariant(Ops));
			}
			Options[API_S_PROGRAMS] = Programs;
		}

		//if (Selection & COptionsTransferWnd::eTraceLog) {
		//	
		//}

		if(Options.Count() == 0)
			return;

		QString Xml = Options.SerializeXml();

		QString FileName = QFileDialog::getSaveFileName(this, tr("Export Options"), "", tr("Options Files (*.xml)"));
		if (FileName.isEmpty())
			return;

		WriteStringToFile(FileName, Xml);
	}
	else if (sender() == m_pOpenUserFolder)
		QDesktopServices::openUrl(QUrl::fromLocalFile(theConf->GetConfigDir()));
	else if (sender() == m_pOpenSystemFolder)
		QDesktopServices::openUrl(QUrl::fromLocalFile(theCore->GetConfigDir()));
}

void CMajorPrivacy::OnExit()
{
	bool bKeepEngine = false;
#ifndef _DEBUG
	if (theCore->IsEngineMode()){
		switch (QMessageBox::question(this, tr("MajorPrivacy"), tr("Do you want to stop the privacy service and unload the kernel isolator?\n"
			"CAUTION: This will disable protection."), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel)) {
		case QMessageBox::Yes:		break;
		case QMessageBox::No:		bKeepEngine = true; break;
		case QMessageBox::Cancel:	return;
		}
	}
#endif

	theCore->Disconnect(bKeepEngine);

	m_bExit = true;
	close();
}

QWidget* g_GUIParent = NULL;

int CMajorPrivacy::SafeExec(QDialog* pDialog)
{
	QWidget* pPrevParent = g_GUIParent;
	g_GUIParent = pDialog;
	SafeShow(pDialog);
	int ret = pDialog->exec();
	g_GUIParent = pPrevParent;
	return ret;
}

QString CMajorPrivacy::GetResourceStr(const QString& Name)
{
	if(Name.isEmpty())
		return tr("Unnamed Rule");

	//
	// Convert MajorPrivacy resource strings to full strings
	//

	auto StrAux = Split2(Name,",");
	if(StrAux.first == "&Protect")
		return tr("Volume Protection Rule for: %1").arg(StrAux.second);
	if(StrAux.first == "&Unmount")
		return tr("Volume Unmount Rule for: %1").arg(StrAux.second);

	//
	// Convert Windows resource strings to full strings
	//

	if(Name.startsWith("@"))
		return QString::fromStdWString(::GetResourceStr(Name.toStdWString()));

	return Name;
}

void CMajorPrivacy::LoadLanguage()
{
	m_Language = theConf->GetString("Options/UiLanguage");
	if(m_Language.isEmpty())
		m_Language = QLocale::system().name();

	if (m_Language.compare("native", Qt::CaseInsensitive) == 0)
#ifdef _DEBUG
		m_Language = "en";
#else
		m_Language.clear();
#endif

	/*m_LanguageId = LocaleNameToLCID(m_Language.toStdWString().c_str(), 0);
	if (!m_LanguageId)
		m_LanguageId = 1033; // default to English*/

	LoadLanguage(m_Language, "majorprivacy", 0);
	LoadLanguage(m_Language, "qt", 1);

	QTreeViewEx::m_ResetColumns = tr("Reset Columns");
	CPanelView::m_CopyCell = tr("Copy Cell");
	CPanelView::m_CopyRow = tr("Copy Row");
	CPanelView::m_CopyPanel = tr("Copy Panel");
	CFinder::m_CaseInsensitive = tr("Case Sensitive");
	CFinder::m_RegExpStr = tr("RegExp");
	CFinder::m_Highlight = tr("Highlight");
	CFinder::m_CloseStr = tr("Close");
	CFinder::m_FindStr = tr("&Find ...");
	CFinder::m_AllColumns = tr("All columns");
	CFinder::m_Placeholder = tr("Search ...");
	CFinder::m_ButtonTip = tr("Toggle Search Bar");
}

void CMajorPrivacy::LoadLanguage(const QString& Lang, const QString& Module, int Index)
{
	qApp->removeTranslator(&m_Translator[Index]);

	if (Lang.isEmpty())
		return;

	QString LangAux = Lang; // Short version as fallback
	LangAux.truncate(LangAux.lastIndexOf('_'));

	QString LangDir;
	/*C7zFileEngineHandler LangFS("lang", this);
	if (LangFS.Open(QApplication::applicationDirPath() + "/translations.7z"))
		LangDir = LangFS.Prefix() + "/";
	else*/
		LangDir = QApplication::applicationDirPath() + "/translations/";

	QString LangPath = LangDir + Module + "_";
	bool bAux = false;
	if (QFile::exists(LangPath + Lang + ".qm") || (bAux = QFile::exists(LangPath + LangAux + ".qm")))
	{
		if(m_Translator[Index].load(LangPath + (bAux ? LangAux : Lang) + ".qm", LangDir))
			qApp->installTranslator(&m_Translator[Index]);
	}
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

		QString AboutText = tr("MajorPrivacy is a program that helps you to protect your privacy and security.\n\n");

		if (!g_CertName.isEmpty()) {
			if(g_CertInfo.active)
				AboutText += tr("This version is licensed to %1.").arg(g_CertName);
			else
				AboutText += tr("This version was licensed to %1, but its no longer valid.").arg(g_CertName);
			AboutText += "\n\n";
		}

		//AboutText += tr("GUI Data Folder: \n%1\n\nCore Data Folder: \n%2\n\n")
		//	.arg(theConf->GetConfigDir().replace("/","\\"))
		//	.arg(theCore->GetConfigDir());

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