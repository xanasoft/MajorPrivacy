#include "pch.h"
#include "MajorPrivacy.h"
#include "Core/PrivacyCore.h"
#include "Core/Processes/ProcessList.h"
#include "Core/Enclaves/EnclaveManager.h"
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
#include "Windows/VariantEditorWnd.h"
#include "Core/Network/NetLogEntry.h"
#include "../MiscHelpers/Common/CheckableMessageBox.h"
#include "Windows/MountMgrWnd.h"
#include "Windows/SignatureDbWnd.h"
#include "../QtSingleApp/src/qtsingleapplication.h"
#include "../MiscHelpers/Archive/ArchiveFS.h"
#include "OnlineUpdater.h"
#include "Wizards/SetupWizard.h"

#include <Windows.h>
#include <ShellApi.h>


CMajorPrivacy* theGUI = NULL;

BOOLEAN OnWM_Notify(NMHDR *Header, LRESULT *Result);

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
				LRESULT ret;
				if (OnWM_Notify((NMHDR*)msg->lParam, &ret))
					*result = ret;
				return true;
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

	QDesktopServices::setUrlHandler("http", this, "OpenUrl");
	QDesktopServices::setUrlHandler("https", this, "OpenUrl");
	QDesktopServices::setUrlHandler("app", this, "OpenUrl");

	connect(theCore, SIGNAL(ProgramsAdded()), this, SLOT(OnProgramsAdded()));
	connect(theCore, SIGNAL(UnruledFwEvent(const CProgramFilePtr&, const CLogEntryPtr&)), this, SLOT(OnUnruledFwEvent(const CProgramFilePtr&, const CLogEntryPtr&)));
	connect(theCore, SIGNAL(ExecutionEvent(const CProgramFilePtr&, const CLogEntryPtr&)), this, SLOT(OnExecutionEvent(const CProgramFilePtr&, const CLogEntryPtr&)));
	connect(theCore, SIGNAL(AccessEvent(const CProgramFilePtr&, const CLogEntryPtr&, uint32)), this, SLOT(OnAccessEvent(const CProgramFilePtr&, const CLogEntryPtr&, uint32)));
	connect(theCore, SIGNAL(CleanUpDone()), this, SLOT(OnCleanUpDone()));
	connect(theCore, SIGNAL(CleanUpProgress(quint64, quint64)), this, SLOT(OnCleanUpProgress(quint64, quint64)));

	//ui.setupUi(this);

	extern QString g_IconCachePath;
	g_IconCachePath = theConf->GetConfigDir() + "/IconCache/";

	LoadLanguage();

	m_pUpdater = new COnlineUpdater(this);

	statusBar()->showMessage(tr("Starting ..."), 10000);

	m_pStatusLabel = new QLabel(this);
	statusBar()->addPermanentWidget(m_pStatusLabel);

	CFinder::m_CaseInsensitiveIcon = QIcon(":/Icons/CaseSensitive.png");
	CFinder::m_RegExpStrIcon = QIcon(":/Icons/RegExp.png");
	CFinder::m_HighlightIcon = QIcon(":/Icons/Highlight.png");

	BuildMenu();

	BuildGUI();

	m_pPopUpWindow = new CPopUpWindow();

	m_pProgressDialog = new CProgressDialog("");
	m_pProgressDialog->setWindowTitle("MajorPrivacy");
	m_pProgressDialog->setWindowModality(Qt::ApplicationModal);

	bool bAlwaysOnTop = theConf->GetBool("Options/AlwaysOnTop", false);
	this->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);
	//m_pPopUpWindow->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);
	m_pProgressDialog->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);

	LoadIgnoreList();

	SafeShow(this);

	STATUS Status = Connect();
	if (Status.IsError())
		CheckResults(QList<STATUS>() << Status, this);

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

void CMajorPrivacy::UpdateTitle()
{
	QString Title = "Major Privacy";
	QString Version = GetVersion();
	Title += " v" + Version;

	if (!theCore->Service()->IsConnected())
		Title += "   -   NOT CONNECTED";
	
	if (theCore->Driver()->IsConnected())
	{
		if (!g_CertInfo.active)
			Title += "   -   !!! NOT ACTIVATED !!!";
		else
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
	}
	else
	{
		m_pSignFile->setEnabled(false);
		m_pClearKeys->setEnabled(false);
		m_pMakeKeyPair->setEnabled(false);
	}

	setWindowTitle(Title);
}

STATUS CMajorPrivacy::ReloadCert(QWidget* pWidget)
{
	auto Res = theCore->GetSupportInfo(true);
	if (!Res)
		return Res.GetStatus();

	QtVariant Info = Res.GetValue();
	g_CertName = Info[API_V_SUPPORT_NAME].AsQStr();
	g_SystemHwid = Info[API_V_SUPPORT_HWID].AsQStr();
	g_CertInfo.State = Info[API_V_SUPPORT_STATE].To<uint64>();
	NTSTATUS status = Info[API_V_SUPPORT_STATUS].To<sint32>();
	if(NT_SUCCESS(status))
		CSettingsWindow::LoadCertificate();
	else if(status != 0xc0000225 /*STATUS_NOT_FOUND*/)
		CSettingsWindow::SetCertificate(""); // always delete invalid certificates

	if (NT_SUCCESS(status))
	{
		BYTE CertBlocked = 0;
		theCore->GetSecureParam("CertBlocked", &CertBlocked, sizeof(CertBlocked));
		if (CertBlocked) {
			if (g_CertInfo.type == eCertEvaluation)
				g_CertInfo.active = 0; // no eval when cert blocked
			else {
				CertBlocked = 0;
				theCore->SetSecureParam("CertBlocked", &CertBlocked, sizeof(CertBlocked));
			}
		}
	}
	else if (status == 0xC0000804L /*STATUS_CONTENT_BLOCKED*/)
	{
		QMessageBox::critical(pWidget ? pWidget : this, "MajorPrivacy",
			tr("The certificate you are attempting to use has been blocked, meaning it has been invalidated for cause. Any attempt to use it constitutes a breach of its terms of use!"));

		BYTE CertBlocked = 1;
		theCore->SetSecureParam("CertBlocked", &CertBlocked, sizeof(CertBlocked));
	}
	else if (status != 0xC0000225L /*STATUS_NOT_FOUND*/)
	{
		QString Info;
		switch (status)
		{
		case 0xC000000DL: /*STATUS_INVALID_PARAMETER*/
		case 0xC0000079L: /*STATUS_INVALID_SECURITY_DESCR:*/
		case 0xC000A000L: /*STATUS_INVALID_SIGNATURE:*/			Info = tr("The Certificate Signature is invalid!"); break;
		case 0xC0000024L: /*STATUS_OBJECT_TYPE_MISMATCH:*/		Info = tr("The Certificate is not suitable for this product."); break;
		case 0xC0000485L: /*STATUS_FIRMWARE_IMAGE_INVALID:*/	Info = tr("The Certificate is node locked."); break;
		default:												Info = QString("0x%1").arg((qint32)status, 8, 16, QChar('0'));
		}

		QMessageBox::critical(pWidget ? pWidget : this, "MajorPrivacy", tr("The support certificate is not valid.\nError: %1").arg(Info));
	}

#ifdef _DEBUG
	qDebug() << "g_CertInfo" << g_CertInfo.State;
	qDebug() << "g_CertInfo.active" << g_CertInfo.active;
	qDebug() << "g_CertInfo.expired" << g_CertInfo.expired;
	qDebug() << "g_CertInfo.outdated" << g_CertInfo.outdated;
	qDebug() << "g_CertInfo.grace_period" << g_CertInfo.grace_period;
	qDebug() << "g_CertInfo.type" << CSettingsWindow::GetCertType();
	qDebug() << "g_CertInfo.level" << CSettingsWindow::GetCertLevel();
#endif

	if (g_CertInfo.active)
	{
		// behave as if there would be no certificate at all
		if (theConf->GetBool("Debug/IgnoreCertificate", false))
			g_CertInfo.State = 0;
		else
		{
			// simulate certificate being about to expire in 3 days from now
			if (theConf->GetBool("Debug/CertFakeAboutToExpire", false))
				g_CertInfo.expirers_in_sec = 3 * 24 * 3600;

			// simulate certificate having expired but being in the grace period
			if (theConf->GetBool("Debug/CertFakeGracePeriode", false))
				g_CertInfo.grace_period = 1;

			// simulate a subscription type certificate having expired
			if (theConf->GetBool("Debug/CertFakeOld", false)) {
				g_CertInfo.active = 0;
				g_CertInfo.expired = 1;
			}

			// simulate a perpetual use certificate being outside the update window
			if (theConf->GetBool("Debug/CertFakeExpired", false)) {
				// still valid
				g_CertInfo.expired = 1;
			}

			// simulate a perpetual use certificate being outside the update window
			// and having been applied to a version built after the update window has ended
			if (theConf->GetBool("Debug/CertFakeOutdated", false)) {
				g_CertInfo.active = 0;
				g_CertInfo.expired = 1;
				g_CertInfo.outdated = 1;
			}

			int Type = theConf->GetInt("Debug/CertFakeType", -1);
			if (Type != -1)
				g_CertInfo.type = Type << 2;

			int Level = theConf->GetInt("Debug/CertFakeLevel", -1);
			if (Level != -1)
				g_CertInfo.level = Level;
		}
	}

	/*if (CERT_IS_TYPE(g_CertInfo, eCertBusiness))
		InitCertSlot();

	if (CERT_IS_TYPE(g_CertInfo, eCertEvaluation))
	{
		if (g_CertInfo.expired)
			OnLogMessage(tr("The evaluation period has expired!!!"));
	}
	else
	{
		if (g_CertInfo.outdated)
			OnLogMessage(tr("The supporter certificate is not valid for this build, please get an updated certificate"));
		// outdated always implicates it is no longer valid
		else if (g_CertInfo.expired) // may be still valid for the current and older builds
			OnLogMessage(tr("The supporter certificate has expired%1, please get an updated certificate")
				.arg(!g_CertInfo.outdated ? tr(", but it remains valid for the current build") : ""));
		else if (g_CertInfo.expirers_in_sec > 0 && g_CertInfo.expirers_in_sec < (60 * 60 * 24 * 30))
			OnLogMessage(tr("The supporter certificate will expire in %1 days, please get an updated certificate").arg(g_CertInfo.expirers_in_sec / (60 * 60 * 24)));
	}*/

	emit CertUpdated();

	if(!NT_SUCCESS(status))
		return ERR(status);
	return OK;
}

void CMajorPrivacy::UpdateLockStatus(bool bOnConnect)
{
	bool bConnected = theCore->Driver()->IsConnected();

	m_pUnloadProtection->setEnabled(bConnected);
	m_pUnloadProtection->setChecked(theCore->Driver()->GetConfigBool("UnloadProtection", false));

	uint32 uConfigStatus = theCore->Driver()->GetConfigStatus();
	uConfigStatus |= theCore->Service()->GetConfigStatus();

	if (uConfigStatus & CONFIG_STATUS_KEYLOCKED)
		m_pClearKeys->setEnabled(false);

	bool bProtected = (uConfigStatus & CONFIG_STATUS_PROTECTED) != 0;
	m_pProtectConfig->setEnabled(bConnected && m_pSignFile->isEnabled() && !bProtected);
	m_pUnprotectConfig->setEnabled(bConnected && m_pSignFile->isEnabled() && bProtected);
	bool bLocked = (uConfigStatus & CONFIG_STATUS_LOCKED) != 0;
	m_pUnlockConfig->setEnabled(bConnected && m_pSignFile->isEnabled() && bProtected && bLocked);
	m_DrvConfigLocked = bLocked;

	bool bDirty = (uConfigStatus & CONFIG_STATUS_DIRTY) != 0;
	m_pCommitConfig->setEnabled(bConnected && bDirty);
	m_pDiscardConfig->setEnabled(bConnected && (bDirty || (bProtected && !bLocked)));

	if (bOnConnect) 
	{
		if ((uConfigStatus & CONFIG_STATUS_CORRUPT) != 0)
			QMessageBox::warning(this, "MajorPrivacy", tr("The driver configuration is corrupted, and could not be loaded, plrease restore from a backup."));
		else if ((uConfigStatus & CONFIG_STATUS_BAD) != 0)
		{
			if (QMessageBox::question(this, "MajorPrivacy", tr("The driver configuration is Untrusted (INVALID Signature, or Sequence). Do you want to load ir anyways?"), QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
			{
				// Loads, ignoreing the signature and unlocks the config
				OnUnlockConfig();
			}
		}
	}
}

STATUS CMajorPrivacy::Connect()
{
	STATUS Status;
	
	bool bEngineMode = false;
	if (!theCore->IsInstalled())
	{
		int iEngineMode = theConf->GetInt("Options/AskAgentMode", -1);
		if (iEngineMode == -1) {
			bool State = false;
			int ret = CCheckableMessageBox::question(this, "MajorPrivacy",
				tr("The Privacy Agent is not currently installed as a service. Would you like to install it now (Yes), run it without installation (No), or abort the connection attempt (Cancel)?")
				, tr("Remember this choice."), &State, QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel, QDialogButtonBox::No, QMessageBox::Question);

			if (ret == QMessageBox::Cancel)
				return ERR(STATUS_OK_CNCELED);

			bEngineMode = ret == QMessageBox::No;

			if (State)
				theConf->SetValue("Options/AskAgentMode", bEngineMode ? 1 : 0);
		}
		else
			bEngineMode = iEngineMode == 1;
	}

	if(!theCore->SvcIsRunning() && !IsRunningElevated())
		QMessageBox::information(this, "MajorPrivacy", tr("Major Privacy will now attempt to start the Privacy Agent, please confirm the UAC prompt."));
	
	m_iReConnected = 1;
	Status = theCore->Connect(bEngineMode);

	if (Status) 
	{
		ReloadCert();
		UpdateLabel();

		statusBar()->showMessage(tr("Privacy Agent Ready %1/%2")
			.arg(CProcess::GetSecStateStr(theCore->GetSvcSecState()))
			.arg(CProcess::GetSecStateStr(theCore->GetGuiSecState())), 10000);

#ifndef _DEBUG
		if (theCore->IsSvcMaxSecurity() && (!theCore->IsGuiMaxSecurity() && theCore->IsGuiHighSecurity()))
		{
			if (theCore->StartProcessBySvc(QCoreApplication::applicationFilePath().replace("/", "\\"))) {
				theCore->Disconnect(true);
				((QtSingleApplication*)QApplication::instance())->disableSingleApp();
				PostQuitMessage(0);
			}
		}
#endif

		// Log all user config dirs in the global config file for cleanup during uninstall
		theCore->SetConfig("Users/" + QString::fromLocal8Bit(qgetenv("USERNAME")), theConf->GetConfigDir());
		
		// todo uncomment
		/*int WizardLevel = abs(theConf->GetInt("Options/WizardLevel", 0));
		if (WizardLevel < (!g_CertInfo.active ? SETUP_LVL_3 : (theConf->GetInt("Options/CheckForUpdates", 2) != 1 ? SETUP_LVL_2 : SETUP_LVL_1))) {
			if (!CSetupWizard::ShowWizard(WizardLevel)) { // if user canceled, mark that and do not show again, until there is something new
				if(QMessageBox::question(NULL, "MajorPrivacy", tr("Do you want the setup wizard to be omitted?"), QMessageBox::Yes, QMessageBox::No | QMessageBox::Default) == QMessageBox::Yes)
					theConf->SetValue("Options/WizardLevel", -SETUP_LVL_CURRENT);
			}
		}*/

		UpdateLockStatus(true);

		OnProgramsAdded();
	}
	else 
	{
		statusBar()->showMessage(tr("Privacy Agent Failed: %1").arg(FormatError(Status)));
	}

	UpdateTitle();
	UpdateLabel();

	return Status;
}

void CMajorPrivacy::Disconnect()
{
	m_iReConnected = 0;
	theCore->Disconnect(true);

	UpdateTitle();

	UpdateLockStatus();
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

	bool bConnected = theCore->Service()->IsConnected();

	// Try reconnect if we lost connection
	if (m_iReConnected == 1 && !bConnected) {
		STATUS Status = theCore->Connect(theCore->IsEngineMode());
		if (Status) 
			bConnected = true;
		else // on failure go back to disconnected state
			m_iReConnected = 2; 
	}

	// handle connection state change
	if (m_bWasConnected != bConnected) {
		m_bWasConnected = bConnected;

		UpdateTitle();

		m_pConnect->setEnabled(!bConnected);
		m_pDisconnect->setEnabled(bConnected);

		bool bInstalled = theCore->IsInstalled();
		m_pInstallService->setEnabled(!bConnected && !bInstalled);
		m_pRemoveService->setEnabled(!bConnected && bInstalled);

		if (!bConnected) {
			theCore->Clear();
			m_CurrentItems = SCurrentItems();
			emit Clear();

			if (m_iReConnected != 0) {
				CExitDialog ReconnectDialog(tr("Lost connection to Service, do you want to reconnect?"));
				if (ReconnectDialog.exec())
					Connect();
			}
		}
	}

	bool bVisible = isVisible() && !isMinimized();

	if (bConnected)
	{
		if (!bVisible) {
			if (m_bWasVisible)
				theCore->SetWatchedPrograms(QSet<CProgramItemPtr>());
		}
		else {
			if (!m_bWasVisible)
				m_CurrentItems = MakeCurrentItems();
		}
		m_bWasVisible = bVisible;

		if (bVisible) {
			STATUS Status = theCore->Update();
			//if (!Status)
			//	Connect();

			m_pStatusLabel->setText(tr("Mem Usage: %1 + %2 (Logs)").arg(FormatSize(theCore->GetTotalMemUsage() - theCore->GetLogMemUsage())).arg(FormatSize(theCore->GetTotalMemUsage())));
		}

		theCore->ProcessEvents();
	}
	else
		m_pStatusLabel->setText(tr("Disconnected from privacy agent"));


	quint64 CurTime = QDateTime::currentSecsSinceEpoch();

	if (m_AutoCommitConf && m_AutoCommitConf < CurTime){
		if (bConnected) {
			STATUS Status = CommitDrvConfig();
			CheckResults(QList<STATUS>() << Status, this);
		}
		m_AutoCommitConf = 0;
	}

	if (m_ForgetSignerPW && m_ForgetSignerPW < CurTime)
		m_ForgetSignerPW = 0;

	if(!m_CachedPassword.isEmpty() && !(m_AutoCommitConf || m_ForgetSignerPW))
		m_CachedPassword.clear();


	if (bVisible)
	{
		UpdateLockStatus();

		m_EnclavePage->Update();
		m_pProgramView->Update();
		m_ProcessPage->Update();
		m_AccessPage->Update();
		m_NetworkPage->Update();
		m_DnsPage->Update();
		m_VolumePage->Update();
		m_TweakPage->Update();
	}
}

void CMajorPrivacy::OnMessage(const QString& MsgData)
{
}

void CMajorPrivacy::ShowMessageBox(QWidget* Widget, QMessageBox::Icon Icon, const QString& Message)
{
	QMessageBox msgBox(Widget);
	msgBox.setTextFormat(Qt::RichText);
	msgBox.setIcon(Icon);
	msgBox.setWindowTitle("MajorPrivacy");
	msgBox.setText(Message);
	msgBox.setStandardButtons(QMessageBox::Ok);
	msgBox.exec();
}

void CMajorPrivacy::OpenUrl(QUrl url)
{
	QString scheme = url.scheme();
	QString host = url.host();
	QString path = url.path();
	QString query = url.query();

	if (host == "xanasoft.com" && path == "/go.php") {
		query += "&language=" + QLocale::system().name();
		url.setQuery(query);
	}

	if (scheme == "app") {
		if (path == "/check")
			m_pUpdater->CheckForUpdates(true);
		else if (path == "/installer")
			m_pUpdater->RunInstaller(false);
		else if (path == "/apply")
			m_pUpdater->ApplyUpdate(COnlineUpdater::eFull, false);
		return;
	}

	ShellExecuteW(MainWndHandle, NULL, url.toString().toStdWString().c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void CMajorPrivacy::LoadState(bool bFull)
{
	if (bFull) {
		setWindowState(Qt::WindowNoState);
		restoreGeometry(theConf->GetBlob("MainWindow/Window_Geometry"));
		restoreState(theConf->GetBlob("MainWindow/Window_State"));
	}

	m_pProgramSplitter->restoreState(theConf->GetBlob("MainWindow/ProgramSplitter"));

	if(m_pTabBar) m_pTabBar->setCurrentIndex(theConf->GetInt("MainWindow/MainTab", 0));
	m_ProcessPage->LoadState();
	m_EnclavePage->LoadState();
	m_AccessPage->LoadState();
	m_VolumePage->LoadState();
	m_NetworkPage->LoadState();
	m_DnsPage->LoadState();
}

void CMajorPrivacy::StoreState()
{
	theConf->SetBlob("MainWindow/Window_Geometry", saveGeometry());
	theConf->SetBlob("MainWindow/Window_State", saveState());

	theConf->SetBlob("MainWindow/ProgramSplitter", m_pProgramSplitter->saveState());

	if(m_pTabBar) theConf->SetValue("MainWindow/MainTab", m_pTabBar->currentIndex());
}

void CMajorPrivacy::BuildMenu()
{
	m_pMain = menuBar()->addMenu(tr("Privacy"));
	//m_pMain->addSeparator();
	m_pMaintenance = m_pMain->addMenu(QIcon(":/Icons/Maintenance.png"), tr("&Maintenance"));
		m_pImportOptions = m_pMaintenance->addAction(QIcon(":/Icons/Import.png"), tr("Import Options"), this, SLOT(OnMaintenance()));
		m_pExportOptions = m_pMaintenance->addAction(QIcon(":/Icons/Export.png"), tr("Export Options"), this, SLOT(OnMaintenance()));
		m_pMaintenance->addSeparator();
		m_pMaintenanceItems = m_pMaintenance->addMenu(QIcon(":/Icons/ManMaintenance.png"), tr("&Advanced"));
			m_pConnect = m_pMaintenanceItems->addAction(QIcon(":/Icons/Connect.png"), tr("Connect"), this, SLOT(OnMaintenance()));
			m_pDisconnect = m_pMaintenanceItems->addAction(QIcon(":/Icons/Disconnect.png"), tr("Disconnect"), this, SLOT(OnMaintenance()));
			m_pMaintenance->addSeparator();
			m_pInstallService = m_pMaintenanceItems->addAction(QIcon(":/Icons/Install.png"), tr("Install Services"), this, SLOT(OnMaintenance()));
			m_pRemoveService = m_pMaintenanceItems->addAction(QIcon(":/Icons/Stop.png"), tr("Remove Services"), this, SLOT(OnMaintenance()));
		
		m_pMaintenance->addSeparator();
		m_pOpenUserFolder = m_pMaintenance->addAction(QIcon(":/Icons/Folder.png"), tr("Open User Data Folder"), this, SLOT(OnMaintenance()));
		m_pOpenSystemFolder = m_pMaintenance->addAction(QIcon(":/Icons/Folder.png"), tr("Open System Data Folder"), this, SLOT(OnMaintenance()));
		m_pVariantEditor = m_pMaintenance->addAction(QIcon(":/Icons/EditIni.png"), tr("Open *.dat Editor"), this, SLOT(OnMaintenance()));
		m_pMaintenance->addSeparator();
		m_pSetupWizard = m_pMaintenance->addAction(QIcon(":/Icons/Wizard.png"), tr("Setup Wizard"), this, SLOT(OnMaintenance()));
	m_pMain->addSeparator();
	m_pExit = m_pMain->addAction(QIcon(":/Icons/Exit.png"), tr("Exit"), this, SLOT(OnExit()));


	m_pView = menuBar()->addMenu(tr("View"));
	m_pProgTree = m_pView->addAction(QIcon(":/Icons/Tree.png"), tr("Toggle Program Tree"), this, SLOT(OnToggleTree()));
	m_pProgTree->setCheckable(true);
	m_pProgTree->setChecked(theConf->GetBool("Options/ShowProgramTree", true));
	m_pStackPanels = m_pView->addAction(QIcon(":/Icons/Stack.png"), tr("Stack Panels"), this, SLOT(OnStackPanels()));
	m_pStackPanels->setCheckable(true);
	m_pStackPanels->setChecked(theConf->GetBool("Options/StackPanels", false));
	m_pMergePanels = m_pView->addAction(QIcon(":/Icons/all.png"), tr("Merge Panels"), this, SLOT(OnMergePanels()));
	m_pMergePanels->setCheckable(true);
	m_pMergePanels->setChecked(theConf->GetBool("Options/MergePanels", false));
	m_pView->addSeparator();

	//m_pShowMenu = m_pView->addAction(tr("Show Program Tool Bar"), this, SLOT(RebuildGUI()));
	//m_pShowMenu->setCheckable(true);
	//m_pShowMenu->setChecked(theConf->GetBool("Options/ShowProgramToolbar", true));

	m_pSplitColumns = m_pView->addAction(tr("Separate Column Sets"), this, SLOT(OnSplitColumns()));
	m_pSplitColumns->setCheckable(true);
	m_pSplitColumns->setChecked(theConf->GetBool("Options/SplitColumns", true));

	m_pTabLabels = m_pView->addAction(tr("Show Tab Labels"), this, SLOT(RebuildGUI()));
	m_pTabLabels->setCheckable(true);
	m_pTabLabels->setChecked(theConf->GetBool("Options/ShowTabLabels", false));

	m_pWndTopMost = m_pView->addAction(tr("Always on Top"), this, SLOT(OnAlwaysTop()));
	m_pWndTopMost->setCheckable(true);
	m_pWndTopMost->setChecked(theConf->GetBool("Options/AlwaysOnTop", false));


	m_pVolumes = menuBar()->addMenu(tr("Volumes"));
	m_pMountVolume = m_pVolumes->addAction(QIcon(":/Icons/AddVolume.png"), tr("Add and Mount Volume Image"), this, SIGNAL(OnMountVolume()));
	m_pUnmountAllVolumes = m_pVolumes->addAction(QIcon(":/Icons/UnmountVolume.png"), tr("Unmount All Volumes"), this, SIGNAL(OnUnmountAllVolumes()));
	m_pVolumes->addSeparator();
	m_pCreateVolume = m_pVolumes->addAction(QIcon(":/Icons/MountVolume.png"), tr("Create Volume"), this, SIGNAL(OnCreateVolume()));

	m_pSecurity = menuBar()->addMenu(tr("Security"));
	m_pSignFile = m_pSecurity->addAction(QIcon(":/Icons/Cert.png"), tr("Sign File"), this, SLOT(OnSignFile()));
	m_pSignDb  = m_pSecurity->addAction(QIcon(":/Icons/CertDB.png"), tr("Signature Database"), this, [this]() {
		CSignatureDbWnd* pWnd = new CSignatureDbWnd();
		pWnd->show();
	});
	m_pSecurity->addSeparator();
	m_pUnloadProtection = m_pSecurity->addAction(QIcon(":/Icons/Shield15.png"), tr("Unload Protection"), this, SLOT(OnUnloadProtection()));
	m_pUnloadProtection->setCheckable(true);
	m_pSecurity->addSeparator();
	m_pProtectConfig = m_pSecurity->addAction(QIcon(":/Icons/LockClosed.png"), tr("Enable Rule Protection"), this, SLOT(OnProtectConfig()));
	m_pUnprotectConfig = m_pSecurity->addAction(QIcon(":/Icons/LockOpen2.png"), tr("Disable Rules Protection"), this, SLOT(OnUnprotectConfig()));
	m_pSecurity->addSeparator();
	m_pMakeKeyPair = m_pSecurity->addAction(QIcon(":/Icons/AddKey.png"), tr("Setup User Key"), this, SLOT(OnMakeKeyPair()));
	m_pClearKeys = m_pSecurity->addAction(QIcon(":/Icons/RemoveKey.png"), tr("Remove User Key"), this, SLOT(OnClearKeys()));
	


	m_pTools = menuBar()->addMenu(tr("Tools"));
	m_pCleanUpProgs = m_pTools->addAction(QIcon(":/Icons/Clean.png"), tr("CleanUp Program List"), this, SLOT(CleanUpPrograms()));
	m_pReGroupProgs = m_pTools->addAction(QIcon(":/Icons/ReGroup.png"), tr("Re-Group all Programs"), this, SLOT(ReGroupPrograms()));

	m_pTools->addSeparator();
	m_pExpTools = m_pTools->addMenu(QIcon(":/Icons/Maintenance.png"), tr("&Advanced Tools"));
		m_pMountMgr = m_pExpTools->addAction(QIcon(":/Icons/Disk.png"), tr("Mount Manager"), this, [this]() {
			CMountMgrWnd* pWnd = new CMountMgrWnd();
			pWnd->show();
		});

		QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
		QString ImDiskCpl = env.value("SystemRoot") + "\\system32\\imdisk.cpl";
		if (QFile::exists(ImDiskCpl)) {
			//m_pExpTools->addSeparator();
			m_pImDiskCpl = m_pExpTools->addAction(LoadWindowsIcon(ImDiskCpl, 0), tr("Virtual Disks"), this, [ImDiskCpl]() {
				std::wstring imDiskCpl = ImDiskCpl.toStdWString();
				SHELLEXECUTEINFOW si = { 0 };
				si.cbSize = sizeof(si);
				si.lpVerb = L"runas";
				si.lpFile = imDiskCpl.c_str();
				si.nShow = SW_SHOW;
				ShellExecuteExW(&si);
			});
			//QMessageBox::critical(this, "MajorPrivacy", tr("ImDisk Control Panel not found!"));
		}

	m_pTools->addSeparator();
	m_pClearLogs = m_pTools->addAction(QIcon(":/Icons/Trash.png"), tr("Clear All Trace Logs"), this, SLOT(ClearTraceLogs()));

	m_pOptions = menuBar()->addMenu(tr("Options"));
	m_pSettings = m_pOptions->addAction(QIcon(":/Icons/Settings.png"), tr("Settings"), this, SLOT(OpenSettings()));
	m_pOptions->addSeparator();
	m_pUnlockConfig = m_pOptions->addAction(QIcon(":/Icons/LockOpen.png"), tr("Unlock Config"), this, SLOT(OnUnlockConfig()));
	m_pCommitConfig = m_pOptions->addAction(QIcon(":/Icons/Approve.png"), tr("Commit Changes"), this, SLOT(OnCommitConfig()));
	m_pDiscardConfig = m_pOptions->addAction(QIcon(":/Icons/Uninstall.png"), tr("Discard Changes"), this, SLOT(OnDiscardConfig()));
	m_pOptions->addSeparator();
	m_pClearIgnore = m_pOptions->addAction(QIcon(":/Icons/ClearList.png"), tr("Clear Ignore List"), this, SLOT(ClearIgnoreLists()));
	m_pResetPrompts = m_pOptions->addAction(QIcon(":/Icons/Refresh.png"), tr("Reset All Prompts"), this, SLOT(ResetPrompts()));

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

void CMajorPrivacy::RebuildGUI()
{
	//theConf->SetValue("Options/ShowProgramToolbar", m_pShowMenu->isChecked());
	theConf->SetValue("Options/ShowTabLabels", m_pTabLabels->isChecked());

	BuildGUI();
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
	m_ProcessPage = new CProcessPage(false, this);
	m_AccessPage = new CAccessPage(false, this);
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
	m_pTabBar->addTab(QIcon(":/Icons/SecureDisk.png"), tr("Secure Volumes"));
	m_pTabBar->addTab(QIcon(":/Icons/Wall3.png"), tr("Network Firewall"));
	m_pTabBar->addTab(QIcon(":/Icons/Network2.png"), tr("DNS Filtering"));
	m_pTabBar->addTab(QIcon(":/Icons/Tweaks.png"), tr("Privacy Tweaks"));
	m_pMainLayout->addWidget(m_pTabBar, 0, 0, 2, 1);

	connect(m_pTabBar, SIGNAL(currentChanged(int)), this, SLOT(OnPageChanged(int)));

	m_pToolBar = new QToolBar();
	m_pMainLayout->addWidget(m_pToolBar, 0, 1, 1, 1);

	m_pPageStack = new QStackedLayout();
	m_pMainLayout->addLayout(m_pPageStack, 1, 1, 2, 1);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
	m_pMainLayout->addWidget(pSpacer, 2, 0, 1, 1);

	m_pPageStack->addWidget(m_HomePage);

	m_pPageStack->addWidget(m_EnclavePage);

	m_pProgramWidget = new QWidget();
	m_pProgramLayout = new QVBoxLayout(m_pProgramWidget);
	m_pProgramLayout->setContentsMargins(0, 0, 0, 0);
	m_pProgramLayout->setSpacing(0);


	//m_pProgramToolBar = new QToolBar();

	m_pToolBar->addAction(m_pSettings);

	QWidget* pSpacer1 = new QWidget();
	pSpacer1->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	pSpacer1->setMaximumWidth(50);
	m_pToolBar->addWidget(pSpacer1);

	m_pToolBar->addAction(m_pUnlockConfig);
	m_pToolBar->addAction(m_pCommitConfig);
	m_pToolBar->addAction(m_pDiscardConfig);

	QWidget* pSpacer2 = new QWidget();
	pSpacer2->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer2);

	CreateLabel();
	m_pToolBar->addWidget(m_pSupportLabel);

	QWidget* pSpacer3 = new QWidget();
	pSpacer3->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer3);

	m_pBtnTime = new QToolButton();
	m_pBtnTime->setIcon(QIcon(":/Icons/Time.png"));
	m_pBtnTime->setToolTip(tr("Set Time Filter"));
	m_pBtnTime->setCheckable(true);
	connect(m_pBtnTime, &QToolButton::toggled, this, [&](bool checked) { m_pCmbRecent->setEnabled(checked); });
	m_pToolBar->addWidget(m_pBtnTime);

	m_pCmbRecent = new QComboBox();
	m_pCmbRecent->addItem(tr("Any time"),		0ull);
	m_pCmbRecent->addItem(tr("Last 5 Seconds"),	5*1000ull);
	m_pCmbRecent->addItem(tr("Last 10 Seconds"),10*1000ull);
	m_pCmbRecent->addItem(tr("Last 30 Seconds"),30*1000ull);
	m_pCmbRecent->addItem(tr("Last Minute"),	60*1000ull);
	m_pCmbRecent->addItem(tr("Last 3 Minutes"), 3*60*1000ull);
	m_pCmbRecent->addItem(tr("Last 5 Minutes"), 5*60*1000ull);
	m_pCmbRecent->addItem(tr("Last 15 Minutes"),15*60*1000ull);
	m_pCmbRecent->addItem(tr("Last 30 Minutes"),30*60*1000ull);
	m_pCmbRecent->addItem(tr("Last Hour"),		60*60*1000ull);
	m_pCmbRecent->addItem(tr("Last 3 Hours"),	3*60*60*1000ull);
	m_pCmbRecent->addItem(tr("Last 12 Hours"),	12*60*60*1000ull);
	m_pCmbRecent->addItem(tr("Last 24 Hours"),	24*60*60*1000ull);
	m_pCmbRecent->addItem(tr("Last 48 Hours"),	48*60*60*1000ull);
	m_pCmbRecent->addItem(tr("Last Week"),		7*24*60*60*1000ull);
	m_pCmbRecent->addItem(tr("Last 2 Weeks"),	2*7*24*60*60*1000ull);
	m_pCmbRecent->addItem(tr("Last Month"),		30*24*60*60*1000ull);
	m_pCmbRecent->setEnabled(false);
	m_pToolBar->addWidget(m_pCmbRecent);
	QFont font = m_pCmbRecent->font();
	font.setPointSizeF(font.pointSizeF() * 1.5);
	m_pCmbRecent->setFont(font);

	QWidget* pSpacer4 = new QWidget();
	pSpacer4->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	pSpacer4->setMaximumWidth(50);
	m_pToolBar->addWidget(pSpacer4);

	m_pToolBar->addAction(m_pProgTree);
	m_pToolBar->addAction(m_pStackPanels);
	m_pToolBar->addAction(m_pMergePanels);

	//m_pProgramLayout->addWidget(m_pProgramToolBar);

	
	m_pProgramSplitter = new QSplitter();
	m_pProgramSplitter->setOrientation(Qt::Horizontal);
	m_pProgramLayout->addWidget(m_pProgramSplitter);
	m_pPageStack->addWidget(m_pProgramWidget);

	m_pProgramSplitter->addWidget(m_pProgramView);

	m_pStackPanels->setEnabled(m_pProgTree->isChecked());
	m_pProgramView->setVisible(m_pProgTree->isChecked());

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

	m_pProgramSplitter->setOrientation(m_pStackPanels->isChecked() ? Qt::Vertical : Qt::Horizontal);
	
	m_ProcessPage->SetMergePanels(m_pMergePanels->isChecked());
	m_EnclavePage->SetMergePanels(m_pMergePanels->isChecked());
	m_AccessPage->SetMergePanels(m_pMergePanels->isChecked());
	m_VolumePage->SetMergePanels(m_pMergePanels->isChecked());
	m_NetworkPage->SetMergePanels(m_pMergePanels->isChecked());
	m_DnsPage->SetMergePanels(m_pMergePanels->isChecked());

	CreateTrayIcon();

	LoadState(bFull);
}

void CMajorPrivacy::CreateLabel()
{
	m_pSupportLabel = new QLabel(m_pMainWidget);
	m_pSupportLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
	connect(m_pSupportLabel, SIGNAL(linkActivated(const QString&)), this, SLOT(OpenUrl(const QString&)));

	m_pSupportLabel->setAlignment(Qt::AlignCenter);
	m_pSupportLabel->setContentsMargins(24, 0, 24, 0);

	QFont fnt = m_pSupportLabel->font();
	fnt.setBold(true);
	//fnt.setWeight(QFont::DemiBold);
	m_pSupportLabel->setFont(fnt);
}

void CMajorPrivacy::UpdateLabel()
{
	QString LabelText;
	QString LabelTip;

	if (!theConf->GetString("Updater/PendingUpdate").isEmpty())
	{
		QString FilePath = theConf->GetString("Updater/InstallerPath");
		if (!FilePath.isEmpty() && QFile::exists(FilePath)) {
			LabelText = tr("<a href=\"sbie://update/installer\" style=\"color: red;\">There is a new MajorPrivacy release %1 ready</a>").arg(theConf->GetString("Updater/InstallerVersion"));
			LabelTip = tr("Click to run installer");
		}
		else if (!theConf->GetString("Updater/UpdateVersion").isEmpty()){
			LabelText = tr("<a href=\"sbie://update/apply\" style=\"color: red;\">There is a new MajorPrivacy update %1 ready</a>").arg(theConf->GetString("Updater/UpdateVersion"));
			LabelTip = tr("Click to apply update");
		}
		else {
			LabelText = tr("<a href=\"sbie://update/check\" style=\"color: red;\">There is a new MajorPrivacy update v%1 available</a>").arg(theConf->GetString("Updater/PendingUpdate"));
			LabelTip = tr("Click to download update");
		}

		//auto neon = new CNeonEffect(10, 4, 180); // 140
		//m_pSupportLabel->setGraphicsEffect(NULL);
	}
	else if (g_Certificate.isEmpty())
	{
		LabelText = theConf->GetString("Updater/LabelMessage");
		if(LabelText.isEmpty())
			LabelText = tr("<a href=\"https://xanasoft.com/go.php?to=patreon\">Support MajorPrivacy on Patreon</a>");
		LabelTip = tr("Click to open web browser");

		/*
		//auto neon = new CNeonEffect(10, 4, 240);
		auto neon = new CNeonEffect(10, 4);
		//neon->setGlowColor(Qt::green);
		neon->setHue(240);
		/if(m_DarkTheme)
		neon->setColor(QColor(218, 130, 42));
		else
		neon->setColor(Qt::blue);/
		m_pSupportLabel->setGraphicsEffect(neon);
		*/

		/*auto glowAni = new QVariantAnimation(neon);
		glowAni->setDuration(10000);
		glowAni->setLoopCount(-1);
		glowAni->setStartValue(0);
		glowAni->setEndValue(360);
		glowAni->setEasingCurve(QEasingCurve::InQuad);
		connect(glowAni, &QVariantAnimation::valueChanged, [neon](const QVariant &value) {
		neon->setHue(value.toInt());
		qDebug() << value.toInt();
		});
		glowAni->start();*/

		/*auto glowAni = new QVariantAnimation(neon);
		glowAni->setDuration(3000);
		glowAni->setLoopCount(-1);
		glowAni->setStartValue(5);
		glowAni->setEndValue(20);
		glowAni->setEasingCurve(QEasingCurve::InQuad);
		connect(glowAni, &QVariantAnimation::valueChanged, [neon](const QVariant &value) {
		neon->setBlurRadius(value.toInt());
		qDebug() << value.toInt();
		});
		glowAni->start();*/

		/*auto glowAni = new QVariantAnimation(neon);
		glowAni->setDuration(3000);
		glowAni->setLoopCount(-1);
		glowAni->setStartValue(1);
		glowAni->setEndValue(20);
		glowAni->setEasingCurve(QEasingCurve::InQuad);
		connect(glowAni, &QVariantAnimation::valueChanged, [neon](const QVariant &value) {
		neon->setGlow(value.toInt());
		qDebug() << value.toInt();
		});
		glowAni->start();*/

		/*auto glowAni = new QVariantAnimation(neon);
		glowAni->setDuration(3000);
		glowAni->setLoopCount(-1);
		glowAni->setStartValue(5);
		glowAni->setEndValue(25);
		glowAni->setEasingCurve(QEasingCurve::InQuad);
		connect(glowAni, &QVariantAnimation::valueChanged, [neon](const QVariant &value) {
		int iValue = value.toInt();
		if (iValue >= 15)
		iValue = 30 - iValue;
		neon->setGlow(iValue);
		neon->setBlurRadius(iValue);
		});
		glowAni->start();*/

	}

	m_pSupportLabel->setVisible(!LabelText.isEmpty());
	m_pSupportLabel->setText(LabelText);
	m_pSupportLabel->setToolTip(LabelTip);
}

void CMajorPrivacy::CreateTrayIcon()
{
	delete m_pTrayIcon;

	m_pTrayIcon = new QSystemTrayIcon(QIcon(":/MajorPrivacy.png"), this);
	m_pTrayIcon->setToolTip("MajorPrivacy");
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

	m_pExecShowPopUp = m_pTrayMenu->addAction(tr("Process Protection Popups"), this, SLOT(OnPopUpPreset()));
	m_pExecShowPopUp->setCheckable(true);

	m_pTrayMenu->addSeparator();
	m_pResShowPopUp = m_pTrayMenu->addAction(tr("Resource Access Popups"), this, SLOT(OnPopUpPreset()));
	m_pResShowPopUp->setCheckable(true);

	m_pTrayMenu->addSeparator();
	m_pFwShowPopUp = m_pTrayMenu->addAction(tr("Network Firewall Popups"), this, SLOT(OnPopUpPreset()));
	m_pFwShowPopUp->setCheckable(true);

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
	m_pDnsFilter = m_pTrayMenu->addAction(tr("DNS Filtering"), this, SLOT(OnDnsPreset()));
	m_pDnsFilter->setCheckable(true);

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

void CMajorPrivacy::OnSplitColumns()
{
	bool bSplitColumns = m_pSplitColumns->isChecked();
	theConf->SetValue("Options/SplitColumns", bSplitColumns);
	if(!bSplitColumns)
		m_pProgramView->SetColumnSet("");
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

	//m_pPopUpWindow->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);
	m_pProgressDialog->setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);
}

bool CMajorPrivacy::IsAlwaysOnTop() const
{
	return m_bOnTop || theConf->GetBool("Options/AlwaysOnTop", false);
}

STATUS CMajorPrivacy::InitSigner(ESignerPurpose Purpose, class CPrivateKey& PrivateKey)
{
	auto Ret = theCore->Driver()->GetUserKey();
	if(Ret.IsError())
		return Ret.GetStatus();
	auto pInfo = Ret.GetValue();
	
	QString Password;

	switch (Purpose) {
		case ESignerPurpose::eSignFile:
		case ESignerPurpose::eSignCert:
			if (m_ForgetSignerPW)
				Password = m_CachedPassword;
			break;
		case ESignerPurpose::eCommitConfig:
			if (m_AutoCommitConf)
				Password = m_CachedPassword;
			break;
	}

	int AutoLock = 0;
	if (Password.isEmpty())
	{
		QString Prompt;
		switch (Purpose) {
			case ESignerPurpose::eSignFile:		Prompt = tr("Enter Secure Configuration Password, to sign a file"); break;
			case ESignerPurpose::eSignCert:		Prompt = tr("Enter Secure Configuration Password, to sign a certificate"); break;
			case ESignerPurpose::eEnableProtection:	Prompt = tr("Enter Secure Configuration Password, to enable rule protection."
				"\nOnce that is done you can not change rules (except windows firewall) without using the user key."); break;
			case ESignerPurpose::eDisableProtection:	Prompt = tr("Enter Secure Configuration Password, to disable rule protection."); break;
			case ESignerPurpose::eUnlockConfig:	Prompt = tr("Enter Secure Configuration Password, to allow rule editing."); break;
			case ESignerPurpose::eCommitConfig: Prompt = tr("Enter Secure Configuration Password, to commit changes."); break;
			case ESignerPurpose::eClearUserKey: Prompt = tr("Enter Secure Configuration Password, to remove the user key."); break;
			default: return ERR(STATUS_INVALID_PARAMETER);
		}

		CVolumeWindow window(Prompt, CVolumeWindow::eGetPW, this);

		if(Purpose == ESignerPurpose::eSignFile || Purpose == ESignerPurpose::eSignCert)
			window.SetAutoLock(0, tr("Remember Password to sign more items for:"));
		else if (Purpose == ESignerPurpose::eUnlockConfig)
			window.SetAutoLock(0, tr("Remember Password and commit changes after:"));

		if (theGUI->SafeExec(&window) != 1)
			return STATUS_OK_CNCELED;
		Password = window.GetPassword();
		if (Password.isEmpty())
			return STATUS_OK_CNCELED;

		AutoLock = window.GetAutoLock();
	}

	STATUS Status;
	do {
		CEncryption Encryption;
		Status = Encryption.SetPassword(CBuffer(Password.utf16(), Password.length() * sizeof(ushort), true));
		if(Status.IsError()) break;

		CBuffer KeyBlob;
		Encryption.Decrypt(pInfo->EncryptedBlob, KeyBlob);
		if(Status.IsError()) break;

		QtVariant KeyData;
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

	if (!Status.IsError() && AutoLock > 0) 
	{
		if (Purpose == ESignerPurpose::eSignFile || Purpose == ESignerPurpose::eSignCert)
			m_ForgetSignerPW = QDateTime::currentSecsSinceEpoch() + AutoLock;
		else if (Purpose == ESignerPurpose::eUnlockConfig)
			m_AutoCommitConf = QDateTime::currentSecsSinceEpoch() + AutoLock;
		m_CachedPassword = Password;
	}

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
	STATUS Status = InitSigner(ESignerPurpose::eSignFile, PrivateKey);
	if (!PrivateKey.IsPrivateKeySet())
		return Status;

	Status = theCore->SignFiles(Paths, &PrivateKey);

	return Status;
}

STATUS CMajorPrivacy::SignCerts(const QMap<QByteArray, QString>& Certs)
{
	CPrivateKey PrivateKey;
	STATUS Status = InitSigner(ESignerPurpose::eSignCert, PrivateKey);
	if (!PrivateKey.IsPrivateKeySet())
		return Status;

	Status = theCore->SignCerts(Certs, &PrivateKey);

	return Status;

}

void CMajorPrivacy::OnUnloadProtection()
{
	theCore->Driver()->SetConfig("UnloadProtection", m_pUnloadProtection->isChecked());

	if (theConf->GetBool("Options/WarnProtection", true)) {
		bool State = false;
		CCheckableMessageBox::question(this, "MajorPrivacy",
			tr("This setting required the service to be restarted to tak effect")
			, tr("Don't show this message again."), &State, QDialogButtonBox::Ok, QDialogButtonBox::Ok, QMessageBox::Information);
		if (State)
			theConf->SetValue("Options/WarnProtection", false);
	}
}

void CMajorPrivacy::OnProtectConfig()
{
	bool bHardLock = false;
    int ret = QMessageBox::warning(this, "MajorPrivacy", tr("Would you like to lock down the user key (Yes), or only enable user key signature-based rule protection (No)?"
						"\n\nLocking down the user key will prevent it from being removed or changed, and rule protection cannot be disabled until the system is rebooted."
						"\n\nIf the driver is set to auto-start, it will automatically re-lock the key on reboot!"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);
	if (ret == QMessageBox::Cancel)
		return;
	bHardLock = (ret == QMessageBox::Yes);


	CPrivateKey PrivateKey;
	STATUS Status = InitSigner(ESignerPurpose::eEnableProtection, PrivateKey);
	if (!PrivateKey.IsPrivateKeySet()) { // eider error or user canceled
		CheckResults(QList<STATUS>() << Status, this);
		return;
	}

	CBuffer ConfigHash;
	Status = theCore->Driver()->GetConfigHash(ConfigHash);
	if (!Status.IsError())
	{
		CBuffer ConfigSignature;
		PrivateKey.Sign(ConfigHash, ConfigSignature);

		Status = theCore->Driver()->ProtectConfig(ConfigSignature, bHardLock);

	}
	UpdateLockStatus();
	CheckResults(QList<STATUS>() << Status, this);
}

void CMajorPrivacy::OnUnprotectConfig()
{
	CPrivateKey PrivateKey;
	STATUS Status = InitSigner(ESignerPurpose::eDisableProtection, PrivateKey);
	if (!PrivateKey.IsPrivateKeySet()) { // eider error or user canceled
		CheckResults(QList<STATUS>() << Status, this);
		return;
	}

	CBuffer ConfigHash;
	QtVariant Data;
	if (!m_pClearKeys->isEnabled()) {
        QMessageBox::warning(this, "MajorPrivacy", tr("The user key is locked. Please reboot the system to complete the removal of the config protection."));
		Data[API_S_UNLOCK] = true;
	}
	Status = theCore->Driver()->GetConfigHash(ConfigHash, Data);
	if (!Status.IsError())
	{
		CBuffer ConfigSignature;
		PrivateKey.Sign(ConfigHash, ConfigSignature);

		Status = theCore->Driver()->UnprotectConfig(ConfigSignature);

	}
	UpdateLockStatus();
	CheckResults(QList<STATUS>() << Status, this);
}

STATUS CMajorPrivacy::UnlockDrvConfig()
{
	CPrivateKey PrivateKey;
	STATUS Status = InitSigner(ESignerPurpose::eUnlockConfig, PrivateKey);
	if (!PrivateKey.IsPrivateKeySet()) // eider error or user canceled
		return Status;

	CBuffer Challenge;
	Status = theCore->Driver()->GetChallenge(Challenge);
	if (!Status.IsError())
	{
		CBuffer Hash;
		CHashFunction::Hash(Challenge, Hash);

		CBuffer ChallengeResponse;
		PrivateKey.Sign(Hash, ChallengeResponse);

		Status = theCore->Driver()->UnlockConfig(ChallengeResponse);

	}
	UpdateLockStatus();

	return Status;
}

STATUS CMajorPrivacy::CommitDrvConfig()
{
	uint32 uConfigStatus = theCore->Driver()->GetConfigStatus();
	if ((uConfigStatus & CONFIG_STATUS_DIRTY) == 0) // nothign changed
		return theCore->Driver()->DiscardConfigChanges(); // to relock the config
	
	if ((uConfigStatus & CONFIG_STATUS_PROTECTED) == 0)
		return theCore->Driver()->CommitConfigChanges();
	
	CPrivateKey PrivateKey;
	STATUS Status = InitSigner(ESignerPurpose::eCommitConfig, PrivateKey);
	if (!PrivateKey.IsPrivateKeySet()) // eider error or user canceled
		return Status;

	CBuffer ConfigHash;
	Status = theCore->Driver()->GetConfigHash(ConfigHash);
	if (!Status.IsError())
	{
		CBuffer ConfigSignature;
		PrivateKey.Sign(ConfigHash, ConfigSignature);

		Status = theCore->Driver()->CommitConfigChanges(ConfigSignature);

	}
	return Status;
}

STATUS CMajorPrivacy::DiscardDrvConfig()
{
	uint32 uConfigStatus = theCore->Driver()->GetConfigStatus();
	bool bProtected = (uConfigStatus & CONFIG_STATUS_PROTECTED) != 0;
	bool bLocked = (uConfigStatus & CONFIG_STATUS_LOCKED) != 0;
	bool bDirty = (uConfigStatus & CONFIG_STATUS_DIRTY) != 0;

	if (!(bDirty || (bProtected && !bLocked)))
		return OK;

	return theCore->Driver()->DiscardConfigChanges();
}

void CMajorPrivacy::OnUnlockConfig()
{
	STATUS Status = UnlockDrvConfig();
	CheckResults(QList<STATUS>() << Status, this);
}

void CMajorPrivacy::OnCommitConfig()
{
	QList<STATUS> Results;

	Results.append(CommitDrvConfig());

	if (m_AutoCommitConf){
		m_AutoCommitConf = 0;
		if(!m_ForgetSignerPW)
			m_CachedPassword.clear();
	}

	uint32 uConfigStatus = theCore->Service()->GetConfigStatus();
	if ((uConfigStatus & CONFIG_STATUS_DIRTY) != 0)
	{
		STATUS Status = theCore->Service()->CommitConfigChanges();
		Results.append(Status);
	}

	CheckResults(Results, this);
}

void CMajorPrivacy::OnDiscardConfig()
{
	if (m_pCommitConfig->isEnabled() && QMessageBox::question(this, "MajorPrivacy", tr("Do you really want to discard all changes?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	QList<STATUS> Results;

	if (m_AutoCommitConf){
		m_AutoCommitConf = 0;
		if(!m_ForgetSignerPW)
			m_CachedPassword.clear();
	}

	Results.append(DiscardDrvConfig());

	uint32 uConfigStatus = theCore->Service()->GetConfigStatus();
	if ((uConfigStatus & CONFIG_STATUS_DIRTY) != 0)
	{
		STATUS Status = theCore->Service()->DiscardConfigChanges();
		Results.append(Status);
	}

	CheckResults(Results, this);
}

void CMajorPrivacy::OnMakeKeyPair()
{
	CVolumeWindow window(tr("Set new Secure Configuration Password"), CVolumeWindow::eSetPW, this);
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

		QtVariant KeyData;
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

	UpdateTitle();
}

void CMajorPrivacy::OnClearKeys()
{
	CPrivateKey PrivateKey;
	STATUS Status = InitSigner(ESignerPurpose::eClearUserKey, PrivateKey);
	if (!PrivateKey.IsPrivateKeySet()) { // eider error or user canceled
		CheckResults(QList<STATUS>() << Status, this);
		return;
	}

	CBuffer Challenge;
	Status = theCore->Driver()->GetChallenge(Challenge);
	if (!Status.IsError()) 
	{
		CBuffer Hash;
		CHashFunction::Hash(Challenge, Hash);

		CBuffer Response;
		PrivateKey.Sign(Hash, Response);

		Status = theCore->Driver()->ClearUserKey(Response);
	}
	CheckResults(QList<STATUS>() << Status, this);

	UpdateTitle();
}

void CMajorPrivacy::OnPopUpPreset()
{
	QAction* pAction = (QAction*)sender();
	if (pAction == m_pExecShowPopUp)
		theConf->SetValue("ProcessProtection/ShowNotifications", m_pExecShowPopUp->isChecked());
	else if (pAction == m_pResShowPopUp)
		theConf->SetValue("ResourceAccess/ShowNotifications", m_pResShowPopUp->isChecked());
	else if (pAction == m_pFwShowPopUp)
		theConf->SetValue("NetworkFirewall/ShowNotifications", m_pFwShowPopUp->isChecked());
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

void CMajorPrivacy::OnDnsPreset()
{
	theCore->SetConfig("Service/DnsInstallFilter", m_pDnsFilter->isChecked());
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
		m_pExecShowPopUp->setChecked(theConf->GetBool("ProcessProtection/ShowNotifications", true));
		m_pResShowPopUp->setChecked(theConf->GetBool("ResourceAccess/ShowNotifications", true));
		m_pFwShowPopUp->setChecked(theConf->GetBool("NetworkFirewall/ShowNotifications", false));

		auto Result = theCore->GetFwProfile();
		if(!Result.IsError()){
			FwFilteringModes Profile = Result.GetValue();
			//m_pFwBlockAll->setChecked(Profile == FwFilteringModes::BlockAll);
			m_pFwAllowList->setChecked(Profile == FwFilteringModes::AllowList);
			m_pFwBlockList->setChecked(Profile == FwFilteringModes::BlockList);
			m_pFwDisabled->setChecked(Profile == FwFilteringModes::NoFiltering);
		}

		m_pDnsFilter->setEnabled(theCore->GetConfigBool("Service/DnsEnableFilter", false));
		m_pDnsFilter->setChecked(theCore->GetConfigBool("Service/DnsInstallFilter", false));

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
		case eProcesses: 
			if(m_pSplitColumns->isChecked()) m_pProgramView->SetColumnSet("Process");
			m_pPageSubStack->setCurrentWidget(m_ProcessPage); 
			break;
		case eResource: 
			if(m_pSplitColumns->isChecked()) m_pProgramView->SetColumnSet("Access");
			m_pPageSubStack->setCurrentWidget(m_AccessPage); 
			break;
		case eFirewall: 
			if(m_pSplitColumns->isChecked()) m_pProgramView->SetColumnSet("Network");
			m_pPageSubStack->setCurrentWidget(m_NetworkPage); 
			break;
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
	m_CurrentItems = MakeCurrentItems();
}

void CMajorPrivacy::OnProgSplitter(int pos, int index)
{
	//bool bShowAll = m_pProgramSplitter->sizes().at(0) < 10;
	//if(m_bShowAll != bShowAll) {
	//	m_bShowAll = bShowAll;
	//	m_CurrentItems = MakeCurrentItems();
	//}
}

CMajorPrivacy::SCurrentItems CMajorPrivacy::MakeCurrentItems() const
{
	QList<CProgramItemPtr> Progs = m_pProgramView->GetCurrentProgs();
	bool bShowAll = !m_pProgramView->isVisible();// || (m_pProgramSplitter->sizes().at(0) < 10);
	if (bShowAll || Progs.isEmpty())
	{
		Progs.clear();
		auto pAll = theCore->ProgramManager()->GetAll();
		if(pAll) Progs.append(pAll);
	}

	SCurrentItems Current = MakeCurrentItems(Progs);

	// special case where the all programs item is selected
	if (Current.bAllPrograms)
	{
		if (!bShowAll && m_pProgramView->GetFilter())
		{
			Current = MakeCurrentItems(QList<CProgramItemPtr>() << theCore->ProgramManager()->GetRoot(), [&](const CProgramItemPtr& pItem) {
				return m_pProgramView->TestFilter(pItem->GetType(), pItem->GetStats());
			});
		}
		else
		{
			Current = MakeCurrentItems(QList<CProgramItemPtr>() << theCore->ProgramManager()->GetRoot());

			/*SCurrentItems All = MakeCurrentItems(QList<CProgramItemPtr>() << theCore->ProgramManager()->GetRoot());

			//Current.Processes = All.Processes;
			Current.Programs = All.Programs;
			Current.ServicesEx.clear();
			Current.ServicesIm = All.ServicesIm;*/
		}
	}

	theCore->SetWatchedPrograms(Current.Items);

	return Current;
}

CMajorPrivacy::SCurrentItems CMajorPrivacy::MakeCurrentItems(const QList<CProgramItemPtr>& Progs, std::function<bool(const CProgramItemPtr&)> Filter) const
{
	SCurrentItems Current;

	std::function<void(const CProgramItemPtr&)> AddItem = [&](const CProgramItemPtr& pItem) 
	{
		CWindowsServicePtr pService = pItem.objectCast<CWindowsService>();
		if (pService) {
			if (!Filter || Filter(pService))
				Current.ServicesIm.insert(pService);
			return;
		}

		CProgramSetPtr pSet = pItem.objectCast<CProgramSet>();
		if (!pSet) return;
		foreach(const CProgramItemPtr& pSubItem, pSet->GetNodes())
		{
			if (!Filter || Filter(pSubItem))
				Current.Items.insert(pSubItem); // implicite

			if (!Filter || Filter(pSubItem))
				AddItem(pSubItem);
		}
	};

	foreach(const CProgramItemPtr& pItem, Progs) 
	{
		if (!Filter || Filter(pItem))
			Current.Items.insert(pItem); // explicite

		CWindowsServicePtr pService = pItem.objectCast<CWindowsService>();
		if (pService) {
			if (!Filter || Filter(pService))
				Current.ServicesEx.insert(pService);
			continue;
		}

		CProgramFilePtr pFile = pItem.objectCast<CProgramFile>();
		if (pFile) {
			if (!Filter || Filter(pFile))
				Current.ProgramsEx.insert(pFile);
		}

		if (!Filter || Filter(pItem))
			AddItem(pItem);

		if(!Current.bAllPrograms && pItem->inherits("CAllPrograms")) // must be on root level
			Current.bAllPrograms = true;
	}

	Current.ServicesIm.subtract(Current.ServicesEx);
	
	foreach(CProgramItemPtr pItem, Current.Items)
	{
		CProgramFilePtr pProgram = pItem.objectCast<CProgramFile>();
		if (!pProgram) continue;

		if (!Filter || Filter(pProgram))
			Current.ProgramsIm.insert(pProgram);

		/*foreach(quint64 Pid, pProgram->GetProcessPids()) {
			CProcessPtr pProcess = theCore->ProcessList()->GetProcess(Pid);
			if (pProcess) Current.Processes.insert(Pid, pProcess);
		}*/
	}

	Current.ProgramsIm.subtract(Current.ProgramsEx);

	return Current;
}

const CMajorPrivacy::SCurrentItems& CMajorPrivacy::GetCurrentItems() const 
{
	return m_CurrentItems;
}

QMap<quint64, CProcessPtr> CMajorPrivacy::GetCurrentProcesses() const
{
	QMap<quint64, CProcessPtr> Processes;

	foreach(CProgramFilePtr pProgram, m_CurrentItems.ProgramsEx | m_CurrentItems.ProgramsIm) {
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

STATUS CMajorPrivacy::AddAsyncOp(const CAsyncProgressPtr& pProgress, bool bWait, const QString& InitialMsg, QWidget* pParent)
{
	m_pAsyncProgress.insert(pProgress.data(), qMakePair(pProgress, pParent));
	connect(pProgress.data(), SIGNAL(Message(const QString&)), this, SLOT(OnAsyncMessage(const QString&)));
	connect(pProgress.data(), SIGNAL(Progress(int)), this, SLOT(OnAsyncProgress(int)));
	connect(pProgress.data(), SIGNAL(Finished()), this, SLOT(OnAsyncFinished()));

	m_pProgressDialog->ShowStatus(InitialMsg);
	if (bWait) {
		m_pProgressModal = true;
		m_pProgressDialog->exec(); // safe exec breaks the closing
		m_pProgressModal = false;
	}
	else
		SafeShow(m_pProgressDialog);

	if (pProgress->IsFinished()) // Note: since the operation runs asynchronously, it may have already finished, so we need to test for that
		OnAsyncFinished(pProgress.data());

	if (pProgress->IsCanceled())
		return CStatus(STATUS_CANCELLED);
	return OK;
}

void CMajorPrivacy::OnAsyncFinished()
{
	OnAsyncFinished(qobject_cast<CAsyncProgress*>(sender()));
}

void CMajorPrivacy::OnAsyncFinished(CAsyncProgress* pSender)
{
	auto Pair = m_pAsyncProgress.take(pSender);
	CAsyncProgressPtr pProgress = Pair.first;
	if (pProgress.isNull())
		return;
	disconnect(pProgress.data() , SIGNAL(Finished()), this, SLOT(OnAsyncFinished()));

	STATUS Status = pProgress->GetStatus();
	if(Status.IsError())
		CheckResults(QList<STATUS>() << Status, Pair.second.data());

	if (m_pAsyncProgress.isEmpty()) {
		if(m_pProgressModal)
			m_pProgressDialog->close();
		else
			m_pProgressDialog->hide();
	}
}

void CMajorPrivacy::OnAsyncMessage(const QString& Text)
{
	m_pProgressDialog->ShowStatus(Text);
}

void CMajorPrivacy::OnAsyncProgress(int Progress)
{
	m_pProgressDialog->ShowProgress("", Progress);
}

void CMajorPrivacy::OnCancelAsync()
{
	foreach(auto Pair, m_pAsyncProgress)
		Pair.first->Cancel();
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
		case STATUS_ERR_PROG_HAS_RULES:				return tr("This Operation can not be performed on a program which has rules.");
		case STATUS_ERR_PROG_PARENT_NOT_FOUND:		return tr("Parent program not found.");
		case STATUS_ERR_CANT_REMOVE_FROM_PATTERN:	return tr("Can't remove from pattern.");
		case STATUS_ERR_PROG_PARENT_NOT_VALID:		return tr("Parent program not valid.");
		case STATUS_ERR_CANT_REMOVE_AUTO_ITEM:		return tr("Removing program items which are auto generated and represent found components can not be removed.");
		case STATUS_ERR_NO_USER_KEY:				return tr("Before you can sign a Binary you need to create your User Key, use the 'Security->Setup User Key' menu command to do that.");
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
		if (I->IsError() && I->GetStatus() != STATUS_OK_CNCELED)
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
		m_IgnoreEvents[(int)Type - 1][pProgram.isNull() ? "" : pProgram->GetPath().toLower()].bAllPaths = true;
	} else
		m_IgnoreEvents[(int)Type - 1][pProgram.isNull() ? "" : pProgram->GetPath().toLower()].Paths.insert(Path);

	StoreIgnoreList(Type);
}

bool CMajorPrivacy::IsEventIgnored(ERuleType Type, const CProgramFilePtr& pProgram, const CLogEntryPtr& pLogEntry) const
{
	const QMap<QString, SIgnoreEvent>& Map = m_IgnoreEvents[(int)Type - 1];
	if (Map.isEmpty())
		return false;

	SIgnoreEvent Ignore = Map.value(pProgram->GetPath().toLower());

	if (Ignore.bAllPaths)
		return true;


	QString Path;
	if (Type == ERuleType::eProgram)
	{
		const CExecLogEntry* pEntry = dynamic_cast<const CExecLogEntry*>(pLogEntry.constData());
		if (pEntry->GetType() == EExecLogType::eImageLoad)
		{
			uint64 uLibRef = pEntry->GetMiscID();
			CProgramLibraryPtr pLibrary = theCore->ProgramManager()->GetLibraryByUID(uLibRef, true);
			if (pLibrary)
				Path = pLibrary->GetPath();
		}
		else if (pEntry->GetType() == EExecLogType::eProcessStarted)
		{
			uint64 uID = pEntry->GetMiscID();
			CProgramFilePtr pExecutable = theCore->ProgramManager()->GetProgramByUID(uID, true).objectCast<CProgramFile>();
			if (pExecutable)
				Path = pExecutable->GetPath();
		}
	}
	else if (Type == ERuleType::eAccess)
	{
		const CResLogEntry* pEntry = dynamic_cast<const CResLogEntry*>(pLogEntry.constData());
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		if (pEntry)
			Path = theCore->NormalizePath(pEntry->GetNtPath());
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

void CMajorPrivacy::ClearTraceLogs()
{
	if (QMessageBox::question(this, "MajorPrivacy", tr("Are you sure you want to clear the All trace logs for all program items?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	theCore->ClearTraceLog(ETraceLogs::eLogMax);
	emit theCore->CleanUpDone();
}

void CMajorPrivacy::OnCleanUpDone()
{
	// clear the log cache on this end to force reload of service date
	foreach(auto pItem, theCore->ProgramManager()->GetItems()) 
		theCore->OnClearTraceLog(pItem, ETraceLogs::eLogMax);

	statusBar()->showMessage(tr("Cleanup done."), 5000);
}

void CMajorPrivacy::OnCleanUpProgress(quint64 Done, quint64 Total)
{
	statusBar()->showMessage(tr("Cleanup %1/%2").arg(Done).arg(Total));
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

void CMajorPrivacy::OnProgramsAdded()
{
	m_CurrentItems = MakeCurrentItems();
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
	const CNetLogEntry* pEntry = dynamic_cast<const CNetLogEntry*>(pLogEntry.constData());
	if(pEntry->GetRemoteAddress().isLoopback() || pEntry->GetDirection() == EFwDirections::Inbound) // ignore loopback ignroe incomming
		return;

	if (theConf->GetBool("NetworkFirewall/ShowNotifications", false)) {
		if (!IsEventIgnored(ERuleType::eFirewall, pProgram, pLogEntry))
			m_pPopUpWindow->PushFwEvent(pProgram, pLogEntry);
	}
}

void CMajorPrivacy::CleanUpPrograms()
{
	int ret = QMessageBox::question(this, "MajorPrivacy", tr("Do you want to clean up the Program List? Choosing 'Yes' will remove all missing program entries, including those with rules. "
		"Choosing 'No' will remove only missing entries without rules. Select 'Cancel' to abort the operation."), QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
	if (ret != QMessageBox::Cancel) {
		QList<STATUS> Results = QList<STATUS>() << theCore->CleanUpPrograms(ret == QMessageBox::Yes);
		theGUI->CheckResults(Results, this);
	}
}

void CMajorPrivacy::ReGroupPrograms()
{
	if (QMessageBox::question(this, "MajorPrivacy", tr("Do you want to re-group all Program Items? This will remove all program items from all auto associated groups and re add it based on the default rules."), QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
		QList<STATUS> Results = QList<STATUS>() << theCore->ReGroupPrograms();
		theGUI->CheckResults(Results, this);
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

void CMajorPrivacy::ClearIgnoreLists()
{
	if(QMessageBox::question(this, "MajorPrivacy", tr("Do you really want to clear the ignore list?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	m_IgnoreEvents[(int)ERuleType::eProgram - 1].clear();
	m_IgnoreEvents[(int)ERuleType::eAccess - 1].clear();
	m_IgnoreEvents[(int)ERuleType::eFirewall - 1].clear();

	QStringList Keys = theConf->ListKeys(IGNORE_LIST_GROUP);
	foreach(const QString & Key, Keys)
		theConf->DelValue(IGNORE_LIST_GROUP "/" + Key);
}

void CMajorPrivacy::ResetPrompts()
{
	if(QMessageBox::question(this, "MajorPrivacy", tr("Do you also want to reset all hidden message boxes?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	theConf->DelValue("Options/AskAgentMode");
	theConf->DelValue("Options/WarnBoxCrypto");
	theConf->DelValue("Options/WarnFolderProtection");
	theConf->DelValue("Options/WarnTerminate");
	theConf->DelValue("Options/WarnProtection");
	theConf->DelValue("Options/WarnBreakingTweaks");
}

void CMajorPrivacy::OnMaintenance()
{
	STATUS Status;

	if (sender() == m_pImportOptions) {

		QString FileName = QFileDialog::getOpenFileName(this, tr("Import Options"), "", tr("Options Files (*.xml)"));
		if (FileName.isEmpty())
			return;

		QString Xml = ReadFileAsString(FileName);

		QtVariant Options;
		bool bOk = Options.ParseXml(Xml);
		if (!bOk) {
			QMessageBox::warning(this, "MajorPrivacy", tr("Failed to parse XML data!"));
			return;
		}

		quint32 Selection = 0;
		if (Options.Has(API_S_GUI_CONFIG))
			Selection |= COptionsTransferWnd::eGUI;
		if (Options.Has(API_S_DRIVER_CONFIG))
			Selection |= COptionsTransferWnd::eDriver;
		if (Options.Has(API_S_USER_KEY))
			Selection |= COptionsTransferWnd::eUserKeys;
		if (Options.Has(API_S_ENCLAVES))
			Selection |= COptionsTransferWnd::eEnclaves;
		if (Options.Has(API_S_PROGRAM_RULES))
			Selection |= COptionsTransferWnd::eExecRules;
		if (Options.Has(API_S_ACCESS_RULES))
			Selection |= COptionsTransferWnd::eResRules;
		if (Options.Has(API_S_SERVICE_CONFIG))
			Selection |= COptionsTransferWnd::eService;
		if (Options.Has(API_S_FW_RULES))
			Selection |= COptionsTransferWnd::eFwRules;
		if (Options.Has(API_S_PROGRAMS))
			Selection |= COptionsTransferWnd::ePrograms;

		COptionsTransferWnd wnd(COptionsTransferWnd::eImport, Selection, this);
		auto UserKey = theCore->Driver()->GetUserKey();
		if (Options.Has(API_S_USER_KEY)) {
			if (!UserKey.IsError()) // if a key is set it can not be changed
				wnd.Disable(COptionsTransferWnd::eUserKeys);
		}
		if (!wnd.exec())
			return;
		Selection = wnd.GetOptions();

		if (Selection & COptionsTransferWnd::eGUI) {
			QtVariant Data = Options[API_S_GUI_CONFIG];
			for (size_t i = 0; i < Data.Count(); i++) {
				QString Section = Data.Key(i);
				QtVariant Values = Data[Data.Key(i)];
				for (size_t j = 0; j < Values.Count(); j++) {
					QString Key = Values.Key(j);
					QtVariant Value = Values[Values.Key(j)];
					theConf->SetValue(Section + "/" + Key, Value.ToQVariant());
				}
			}
		}

		if (Selection & COptionsTransferWnd::eDriver) {
			theCore->SetDrvConfig(Options[API_S_DRIVER_CONFIG]);
		}

		if (Selection & COptionsTransferWnd::eUserKeys) {
			QtVariant Keys = Options[API_S_USER_KEY];
			theCore->Driver()->SetUserKey(Keys[API_S_PUB_KEY], Keys[API_S_KEY_BLOB]);
		}

		if (Selection & COptionsTransferWnd::eEnclaves) {
			theCore->SetAllEnclaves(Options[API_S_ENCLAVES]);
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

		/*if (Selection & COptionsTransferWnd::eFwRules) {
			// todo
		}

		if (Selection & COptionsTransferWnd::ePrograms) {
			// todo
		}*/
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
		Ops.Flags = SVarWriteOpt::eSaveToFile | SVarWriteOpt::eTextGuids;

		QtVariant Options;

		if (Selection & COptionsTransferWnd::eGUI) {

			QtVariantWriter Data;
			Data.BeginMap();
			for(auto Section: theConf->ListGroupes())
			{
				QtVariantWriter Values;
				Values.BeginMap();
				for(auto Key: theConf->ListKeys(Section))
					Values.WriteEx(Key.toStdString().c_str(), theConf->GetString(Section, Key));
				Data.WriteVariant(Section.toStdString().c_str(), Values.Finish());
			}
			Options[API_S_GUI_CONFIG] = Data.Finish();
		}

		if (Selection & COptionsTransferWnd::eDriver) {
			auto ret = theCore->GetDrvConfig();
			if(ret) Options[API_S_DRIVER_CONFIG] = ret.GetValue();
		}

		if (Selection & COptionsTransferWnd::eUserKeys) {
			QtVariant Keys;
			Keys[API_S_PUB_KEY] = UserKey.GetValue()->PubKey;
			Keys[API_S_KEY_BLOB] = UserKey.GetValue()->EncryptedBlob;
			Options[API_S_USER_KEY] = Keys;
		}


		if (Selection & COptionsTransferWnd::eEnclaves) {
			QtVariant Enclaves;
			for (auto pEnclave : theCore->EnclaveManager()->GetAllEnclaves()) {
				Enclaves.Append(pEnclave->ToVariant(Ops));
			}
			Options[API_S_ENCLAVES] = Enclaves;
		}

		if (Selection & COptionsTransferWnd::eExecRules) {
			QtVariant ProgramRules;
			for (auto pRule : theCore->ProgramManager()->GetProgramRules()) {
				if (pRule->IsTemporary()) continue;
				ProgramRules.Append(pRule->ToVariant(Ops));
			}
			Options[API_S_PROGRAM_RULES] = ProgramRules;
		}

		if (Selection & COptionsTransferWnd::eResRules) {
			QtVariant AccessRules;
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
			QtVariant FwRules;
			for (auto pRule : theCore->NetworkManager()->GetFwRules())
				FwRules.Append(pRule->ToVariant(Ops));
			Options[API_S_FW_RULES] = FwRules;
		}

		if (Selection & COptionsTransferWnd::ePrograms) {
			QtVariant Programs;
			for (auto pProgram : theCore->ProgramManager()->GetItems()) {
				if (pProgram == theCore->ProgramManager()->GetRoot() || pProgram == theCore->ProgramManager()->GetAll())
					continue; // skip built in items
				Programs.Append(pProgram->ToVariant(Ops));
			}
			Options[API_S_PROGRAMS] = Programs;
		}

		if(Options.Count() == 0)
			return;

		QString Xml = Options.SerializeXml();

		QString FileName = QFileDialog::getSaveFileName(this, tr("Export Options"), "", tr("Options Files (*.xml)"));
		if (FileName.isEmpty())
			return;

		WriteStringToFile(FileName, Xml);
	}
	else if (sender() == m_pVariantEditor)
	{
		CVariantEditorWnd* pWnd = new CVariantEditorWnd(this);
		//connect(this, SIGNAL(Closed()), pWnd, SLOT(close()));
		SafeShow(pWnd);
	}
	else if (sender() == m_pOpenUserFolder)
		QDesktopServices::openUrl(QUrl::fromLocalFile(theConf->GetConfigDir()));
	else if (sender() == m_pOpenSystemFolder)
		QDesktopServices::openUrl(QUrl::fromLocalFile(theCore->GetConfigDir()));
	else if (sender() == m_pConnect)
		Status = Connect();
	else if (sender() == m_pDisconnect) 
		Disconnect();
	else if (sender() == m_pInstallService)
		Status = theCore->Install();
	else if (sender() == m_pRemoveService)
		theCore->Uninstall();
	else if (sender() == m_pSetupWizard) {
		CSetupWizard::ShowWizard();
		return;
	}

	CheckResults(QList<STATUS>() << Status, this);
}

void CMajorPrivacy::OnExit()
{
	bool bKeepEngine = false;

	if (theCore->IsEngineMode()) {
		int iKeepEngine = theConf->GetInt("Options/KeepEngine", -1);
		if (iKeepEngine == -1) {
			bool State = false;
			int ret = CCheckableMessageBox::question(this, "MajorPrivacy",
				tr("Do you want to stop the Privacy Agent and unload the Kernel Isolator?\nCAUTION: This will disable protection.")
				, tr("Remember this choice."), &State, QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel, QDialogButtonBox::No, QMessageBox::Question);

			if (ret == QMessageBox::Cancel)
				return;

			bKeepEngine = ret == QMessageBox::No;

			if (State)
				theConf->SetValue("Options/KeepEngine", bKeepEngine ? 1 : 0);
		}
		else
			bKeepEngine = iKeepEngine == 1;
	}

	theCore->Disconnect(bKeepEngine);

	m_bExit = true;
	close();
}

void CMajorPrivacy::OnToggleTree()
{
	theConf->SetValue("Options/ShowProgramTree", m_pProgTree->isChecked());
	m_pStackPanels->setEnabled(m_pProgTree->isChecked());
	m_pProgramView->setVisible(m_pProgTree->isChecked());
	m_CurrentItems = MakeCurrentItems();
}

void CMajorPrivacy::OnStackPanels()
{
	theConf->SetValue("Options/StackPanels", m_pStackPanels->isChecked());

	m_pProgramSplitter->setOrientation(m_pStackPanels->isChecked() ? Qt::Vertical : Qt::Horizontal);
}

void CMajorPrivacy::OnMergePanels()
{
	theConf->SetValue("Options/MergePanels", m_pMergePanels->isChecked());

	m_ProcessPage->SetMergePanels(m_pMergePanels->isChecked());
	m_EnclavePage->SetMergePanels(m_pMergePanels->isChecked());
	m_AccessPage->SetMergePanels(m_pMergePanels->isChecked());
	m_VolumePage->SetMergePanels(m_pMergePanels->isChecked());
	m_NetworkPage->SetMergePanels(m_pMergePanels->isChecked());
	m_DnsPage->SetMergePanels(m_pMergePanels->isChecked());
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
		return "";

	//
	// Convert MajorPrivacy resource strings to full strings
	//

	auto StrAux = Split2(Name,",");
	if(StrAux.first == "&Protect")
		return tr("Volume Protection Rule for: %1").arg(StrAux.second);
	else if(StrAux.first == "&Unmount")
		return tr("Volume Unmount Rule for: %1").arg(StrAux.second);
	else if(StrAux.first == "&Temporary")
		return tr("Temporary rule");
	else if(StrAux.first == "&MP-Rule")
		return tr("%1 - MajorPrivacy Rule").arg(StrAux.second);
	else if(StrAux.first == "&MP-Template")
		return tr("%1 - MajorPrivacy Rule Template").arg(StrAux.second);

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

	m_LanguageId = LocaleNameToLCID(m_Language.toStdWString().c_str(), 0);
	if (!m_LanguageId)
		m_LanguageId = 1033; // default to English

	LoadLanguage(m_Language, "MajorPrivacy", 0);
	LoadLanguage(m_Language, "qt", 1);

	QTreeViewEx::m_ResetColumns = tr("Reset Columns");
	CPanelView::m_CopyCell = tr("Copy Cell");
	CPanelView::m_CopyRow = tr("Copy Row");
	CPanelView::m_CopyPanel = tr("Copy Panel");
	CFinder::m_CaseInsensitive = tr("Case Sensitive");
	CFinder::m_RegExpStr = tr("Use Regular Expressions");
	CFinder::m_Highlight = tr("Highlight Items");
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
	C7zFileEngineHandler LangFS("lang", this);
	if (LangFS.Open(QApplication::applicationDirPath() + "/translations.7z"))
		LangDir = LangFS.Prefix() + "/";
	else
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
			"<p>" MY_COPYRIGHT_STRING "</p>"
		).arg(theGUI->GetVersion());

		QString CertInfo;
		if (!g_CertName.isEmpty()) {
			if(g_CertInfo.active)
				CertInfo = tr("This version is licensed to %1.").arg(g_CertName);
			else
				CertInfo = tr("This version was licensed to %1, but its no longer valid.").arg(g_CertName);
		}

		QString AboutText = tr(
			"<p>MajorPrivacy is a program that helps you to protect your privacy and security.</p>"
			"<p></p>"
			"<p>Visit <a href=\"https://xanasoft.com\">xanasoft.com</a> for more information.</p>"
			"<p></p>"
			"<p>%1</p>"
			"<p></p>"
			"<p>UI Config Dir: <br />%2<br />"
			"Core Config Dir: <br />%3</p>"
			"<p></p>"
			"<p>Icons from <a href=\"https://icons8.com\">icons8.com</a></p>"
			"<p></p>"
		).arg(CertInfo).arg(theConf->GetConfigDir().replace("/","\\")).arg(theCore->GetConfigDir());


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




//////////////////////////////////////////////////////////////////////////////////////////
//

#include <windows.h>
#include <shellapi.h>

#define RFF_NOBROWSE 0x0001
#define RFF_NODEFAULT 0x0002
#define RFF_CALCDIRECTORY 0x0004
#define RFF_NOLABEL 0x0008
#define RFF_NOSEPARATEMEM 0x0020
#define RFF_OPTRUNAS 0x0040

#define RFN_VALIDATE (-510)
#define RFN_LIMITEDRUNAS (-511)

#define RF_OK 0x0000
#define RF_CANCEL 0x0001
#define RF_RETRY 0x0002

typedef struct _NMRUNFILEDLGW
{
	NMHDR hdr;
	PWSTR lpszFile;
	PWSTR lpszDirectory;
	UINT ShowCmd;
} NMRUNFILEDLGW, *LPNMRUNFILEDLGW, *PNMRUNFILEDLGW;

QString g_RunDialogCommand;

BOOLEAN OnWM_Notify(NMHDR *Header, LRESULT *Result)
{
	LPNMRUNFILEDLGW runFileDlg = (LPNMRUNFILEDLGW)Header;
	if (Header->code == RFN_VALIDATE)
	{
		g_RunDialogCommand = QString::fromWCharArray(runFileDlg->lpszFile);

		*Result = RF_CANCEL;
		return TRUE;
	}
	/*else if (Header->code == RFN_LIMITEDRUNAS)
	{

	}*/
	return FALSE;
}

extern "C"
{
	//NTSYSCALLAPI NTSTATUS NTAPI LdrGetProcedureAddress(IN PVOID DllHandle, IN VOID* /*PANSI_STRING*/ ProcedureName OPTIONAL, IN ULONG ProcedureNumber OPTIONAL, OUT PVOID *ProcedureAddress, IN BOOLEAN RunInitRoutines);
	//NTSTATUS(NTAPI *LdrGetProcedureAddress)(HMODULE ModuleHandle, PANSI_STRING FunctionName, WORD Oridinal, PVOID *FunctionAddress);
}

BOOLEAN NTAPI ShowRunFileDialog(HWND WindowHandle, HICON WindowIcon, LPCWSTR WorkingDirectory, LPCWSTR WindowTitle, LPCWSTR WindowDescription, ULONG Flags)
{
	typedef BOOL(WINAPI *RunFileDlg_I)(HWND hwndOwner, HICON hIcon, LPCWSTR lpszDirectory, LPCWSTR lpszTitle, LPCWSTR lpszDescription, ULONG uFlags);

	BOOLEAN result = FALSE;

	if (HMODULE shell32Handle = LoadLibraryW(L"shell32.dll"))
	{
		RunFileDlg_I dialog = NULL;
		if (LdrGetProcedureAddress(shell32Handle, NULL, 61, (void**)&dialog/*, TRUE*/) == 0 /*STATUS_SUCCESS*/)
			result = !!dialog(WindowHandle, WindowIcon, WorkingDirectory, WindowTitle, WindowDescription, Flags);

		FreeLibrary(shell32Handle);
	}

	return result;
}

QString ShowRunDialog(const QString& EnclaveName)
{
	g_RunDialogCommand.clear();
	std::wstring enclaveName = EnclaveName.toStdWString();
	ShowRunFileDialog((HWND)theGUI->winId(), NULL, NULL, enclaveName.c_str(), (wchar_t*)CMajorPrivacy::tr("Enter the path of a program that will be run.").utf16(), 0); // RFF_OPTRUNAS);
	return g_RunDialogCommand;
}
