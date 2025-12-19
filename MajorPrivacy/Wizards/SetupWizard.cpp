#include "pch.h"

#include "SetupWizard.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Windows/SettingsWindow.h"
#include "../MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Network/NetworkManager.h"
#include "Helpers/WinAdmin.h"
#include <QButtonGroup>
#include "../OnlineUpdater.h"
#include "../Library/Crypto/Encryption.h"
#include "../Library/Crypto/PrivateKey.h"
#include "../Library/Crypto/PublicKey.h"
#include "../Library/Crypto/HashFunction.h"

CSetupWizard::CSetupWizard(int iOldLevel, QWidget *parent)
    : QWizard(parent)
{
    if (iOldLevel < SETUP_LVL_1) {
        setPage(Page_Intro, new CIntroPage);
        setPage(Page_Certificate, new CCertificatePage(iOldLevel));
        setPage(Page_Warning, new CWarningPage);
        setPage(Page_Exec, new CExecPage);
        setPage(Page_Res, new CResPage);
        setPage(Page_Net, new CNetPage);
        setPage(Page_Config, new CConfigPage);
        setPage(Page_System, new CSystemPage);
        setPage(Page_Update, new CUpdatePage);
    }
    setPage(Page_Finish, new CFinishPage);

    this->setMinimumWidth(600);

    setWizardStyle(ModernStyle);
    //setOption(HaveHelpButton, true);
    setPixmap(QWizard::LogoPixmap, QPixmap(":/SandMan.png").scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    connect(this, &QWizard::helpRequested, this, &CSetupWizard::showHelp);

    setWindowTitle(tr("Setup Wizard"));
}

CSetupWizard::~CSetupWizard()
{
    if(m_pPrivateKey)
        delete m_pPrivateKey;
}

void CSetupWizard::showHelp()
{
    static QString lastHelpMessage;

    QString message;

    switch (currentId()) {
    case Page_Intro:
        message = tr("The decision you make here will affect which page you get to see next.");
        break;
    default:
        message = tr("This help is likely not to be of any help.");
    }

    if (lastHelpMessage == message)
        message = tr("Sorry, I already gave all the help I could.");

    QMessageBox::information(this, tr("Setup Wizard Help"), message);

    lastHelpMessage = message;
}

bool CSetupWizard::ShowWizard(int iOldLevel)
{
    CSetupWizard wizard(iOldLevel, theGUI);
    if (!theGUI->SafeExec(&wizard))
        return false;
    
    if (iOldLevel < SETUP_LVL_1) 
    {
        // Cert
        //bool useBusiness = wizard.field("useBusiness").toBool();
        //QString Certificate = wizard.field("useCertificate").toString();
        //bool isEvaluate = wizard.field("isEvaluate").toBool();
        //

        // Exec
		theCore->SetConfig("Service/SaveIngressRecord", wizard.field("recordExec").toBool());
        theCore->SetConfig("Service/ExecLog", wizard.field("logExec").toBool());
        //

        // Res
        theCore->SetConfig("Service/ResTrace", wizard.field("collectRes").toBool());
        theCore->SetConfig("Service/SaveAccessRecord", wizard.field("recordRes").toBool());
        theCore->SetConfig("Service/ResLog", wizard.field("logRes").toBool());
        theCore->SetConfig("Service/EnumAllOpenFiles", wizard.field("enumRes").toBool());
        //

        // Net
        if (wizard.field("enableFw").toBool())
        {
            theConf->SetValue("NetworkFirewall/ShowNotifications", true);
            theConf->SetValue("NetworkFirewall/ShowChangeAlerts", true);
        }

        if(wizard.field("restrictFw").toBool())
            theCore->SetFwProfile(FwFilteringModes::AllowList);

        theCore->SetConfig("Service/GuardFwRules", wizard.field("protectFw").toBool());

        if(wizard.field("createFwRules").toBool())
			theCore->NetworkManager()->CreateRecommendedFwRules();

        theCore->SetConfig("Service/NetTrace", wizard.field("collectNet").toBool());
        theCore->SetConfig("Service/SaveTrafficRecord", wizard.field("recordNet").toBool());
        theCore->SetConfig("Service/NetLog", wizard.field("logNet").toBool());

        if (wizard.field("enableFw").toBool() && wizard.field("collectNet").toBool())
			theCore->SetAuditPolicy(FwAuditPolicy::All);
        //

        // Config
        if (wizard.field("unloadLock").toBool())
            theCore->Driver()->SetConfig("UnloadProtection", true);

        if (wizard.field("configLock").toBool())
        {
#ifdef _DEBUG
            bool bHardLock = false;
#else
            bool bHardLock = true;
#endif
            if (wizard.m_pPrivateKey)
            {
                CBuffer ConfigHash;
                STATUS Status = theCore->Driver()->GetConfigHash(ConfigHash);
                if (!Status.IsError())
                {
                    CBuffer ConfigSignature;
                    wizard.m_pPrivateKey->Sign(ConfigHash, ConfigSignature);

                    Status = theCore->Driver()->ProtectConfig(ConfigSignature, bHardLock);
                }
                //theGUI->UpdateLockStatus();
                theGUI->CheckResults(QList<STATUS>() << Status, &wizard);
            }
        }
        //

        // System
        theCore->SetConfig("Service/EncryptPageFile", wizard.field("encryptSwap").toBool());
        theCore->SetConfig("Service/GuardHibernation", wizard.field("guardSuspend").toBool());
        //

        // UI
        //if (wizard.field("useBrightMode").toInt())
        //    theConf->SetValue("Options/UseDarkTheme", 0);
        //else if (wizard.field("useDarkMode").toInt())
        //    theConf->SetValue("Options/UseDarkTheme", 1);
        //
        
        // Update
        if (wizard.field("updateAll").toBool())
        {
            theConf->SetValue("Options/CheckForUpdates", 1);
            //theConf->SetValue("Options/OnNewUpdate", "install");
            theConf->SetValue("Options/UpdateTweaks", 1);
        }
        else
        {
            if(wizard.field("updateApp").toBool())
                theConf->SetValue("Options/CheckForUpdates", 1);
            //if(wizard.field("applyHotfixes").toBool())
            //    theConf->SetValue("Options/OnNewUpdate", "install");
            if(wizard.field("updateTweaks").toBool())
                theConf->SetValue("Options/UpdateTweaks", 1);
        }

        if (wizard.field("updateAll").toBool() || wizard.field("updateApp").toBool()) {
            QString ReleaseChannel;
            if (wizard.field("channelStable").toBool())
                ReleaseChannel = "stable";
            else if (wizard.field("channelPreview").toBool())
                ReleaseChannel = "preview";
            //else if (wizard.field("channelInsider").toBool())
            //    ReleaseChannel = "insider";
            if (!ReleaseChannel.isEmpty()) theConf->SetValue("Options/ReleaseChannel", ReleaseChannel);
        }
        //
    }

    //if (wizard.field("isUpdate").toBool())
    //    theConf->SetValue("Options/CheckForUpdates", 1);


    theConf->SetValue("Options/WizardLevel", SETUP_LVL_CURRENT);

    theGUI->UpdateSettings(true);
    
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CIntroPage
// 

CIntroPage::CIntroPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Introduction"));
    QPixmap Logo = QPixmap(CCustomTheme::IsDarkTheme() ? ":/PrivLogoDM.png" : ":/PrivLogo.png");
    int Scaling = theConf->GetInt("Options/FontScaling", 100);
    if(Scaling !=  100) Logo = Logo.scaled(Logo.width() * Scaling / 100, Logo.height() * Scaling / 100);
    setPixmap(QWizard::WatermarkPixmap, Logo);

    QVBoxLayout *layout = new QVBoxLayout;
    QLabel* pTopLabel = new QLabel(tr("Welcome to the Setup Wizard. This wizard will help you to configure your copy of <b>MajorPrivacy</b>. "
        "It is strongly recommended to go through this wizard in order to learn about some of the avilable featues and mechanisms. "
        "You can start this wizard at any time from the Privacy->Maintenance menu if you do not wish to complete it now."));
    pTopLabel->setWordWrap(true);
    layout->addWidget(pTopLabel);

    QWidget* pSpace = new QWidget();
    pSpace->setMinimumHeight(16);
    layout->addWidget(pSpace);

    //m_pLabel = new QLabel(tr("Select how you would like to use MajorPrivacy"));
    //layout->addWidget(m_pLabel);
    //
    //m_pPersonal = new QRadioButton(tr("&Personally, for private non-commercial use"));
    //layout->addWidget(m_pPersonal);
    //connect(m_pPersonal, SIGNAL(toggled(bool)), this, SIGNAL(completeChanged()));
    //registerField("usePersonal", m_pPersonal);
    //
    //m_pBusiness = new QRadioButton(tr("&Commercially, for business or enterprise use"));
    //layout->addWidget(m_pBusiness);
    //connect(m_pBusiness, SIGNAL(toggled(bool)), this, SIGNAL(completeChanged()));
    //registerField("useBusiness", m_pBusiness);
    //
    //QLabel* pNote = new QLabel(tr("Note: this option is persistent"));
    //layout->addWidget(pNote);
    //
    //uchar BusinessUse = 2;
    //if (!g_Certificate.isEmpty())
    //    BusinessUse = CERT_IS_TYPE(g_CertInfo, eCertBusiness) ? 1 : 0;
    //else {
    //    uchar UsageFlags = 0;
    //    if (theCore->GetSecureParam("UsageFlags", &UsageFlags, sizeof(UsageFlags)))
    //        BusinessUse = (UsageFlags & 1) != 0 ? 1 : 0;
    //}
    //if (BusinessUse != 2) {
    //    m_pPersonal->setChecked(BusinessUse == 0);
    //    m_pBusiness->setChecked(BusinessUse == 1);
    //    if ((QApplication::keyboardModifiers() & Qt::ControlModifier) == 0) {
    //        m_pLabel->setEnabled(false);
    //        m_pPersonal->setEnabled(false);
    //        m_pBusiness->setEnabled(false);
    //    }
    //    pNote->setEnabled(false);
    //}

    setLayout(layout);

    if (CCustomTheme::IsDarkTheme()) {
        QPalette palette = this->palette();
        palette.setColor(QPalette::Base, QColor(53, 53, 53));
        this->setPalette(palette);
    }
}

int CIntroPage::nextId() const
{
    return CSetupWizard::Page_Warning;
}

bool CIntroPage::isComplete() const 
{
    //if (m_pLabel->isEnabled() && !m_pPersonal->isChecked() && !m_pBusiness->isChecked())
    //    return false;
    return QWizardPage::isComplete();
}

//////////////////////////////////////////////////////////////////////////////////////////
// CWarningPage
// 

CWarningPage::CWarningPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Safety and Recovery instructions. <b>Please read carefully</b>"));
    setSubTitle(tr("Major Privacy is a powerfull tool, improper operation may break the system."));

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(3,3,3,3);
    layout->setSpacing(3);

    QLabel* pTopLabel = new QLabel(tr("MajorPrivacy allows you to control file access, process execution, and enforce custom code-integrity rules. Incorrect or overly restrictive rules may prevent Windows from booting into the desktop.<br /><br />"
        "To mitigate this risk, MajorPrivacy automatically disables rule enforcement when Windows starts in recovery mode. Additionally, if repeated boot failures are detected, MajorPrivacy will take corrective action: it will revert to the last known good rule set after the third failed boot, and completely disable all rules after the fifth.<br /><br />"
        "If recovery fails, you can manually disable MajorPrivacy from the Windows Recovery Environment (WinRE). Open the command prompt, navigate to your installation directory (typically \"C:\\Program Files\\MajorPrivacy\"), and rename the driver file - for example, by appending .disabled to its name.<br /><br />"
        "<b>Disclaimer:</b> Use of these features is at your own risk. We assume no responsibility for system instability or data loss resulting from user-defined rules."));
    pTopLabel->setWordWrap(true);
	//pTopLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    layout->addWidget(pTopLabel);

    layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Fixed, QSizePolicy::Expanding));

    m_pAcknowledge = new QCheckBox(tr("I acknowledge the risks and take full responsibility for teh rules I create."));
    connect(m_pAcknowledge, &QCheckBox::toggled, this, &QWizardPage::completeChanged);

    if(theConf->GetBool("Options/WarningAcknowledged", false)) {
        m_pAcknowledge->setChecked(true);
        m_pAcknowledge->setEnabled(false);
		m_pAcknowledge->setText(m_pAcknowledge->text() + tr(" (already acknowledged)"));
	}

	layout->addWidget(m_pAcknowledge);

    setLayout(layout);
}

void CWarningPage::initializePage()
{
}

int CWarningPage::nextId() const
{
#ifndef _DEBUG
    //if(!g_Certificate.isEmpty())
    if(g_CertInfo.active)
        return CSetupWizard::Page_Exec;
#endif
    return CSetupWizard::Page_Certificate;
}

bool CWarningPage::isComplete() const
{
    if (!m_pAcknowledge->isChecked())
        return false;

    return QWizardPage::isComplete();
}

bool CWarningPage::validatePage()
{
    if (m_pAcknowledge->isEnabled() && m_pAcknowledge->isChecked())
		theConf->SetValue("Options/WarningAcknowledged", true);
    
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CCertificatePage
// 

CCertificatePage::CCertificatePage(int iOldLevel, QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Install your <b>MajorPrivacy</b> License Certificate"));
    setSubTitle(tr("To use advanced protection features a License Certificate must be installed."));
   
    QGridLayout *layout = new QGridLayout;
    layout->setContentsMargins(3,3,3,3);
    layout->setSpacing(3);

    m_pTopLabel = new QLabel(tr("<b><span style=\"color:#ff0000;\">Important:</span></b> <b>MajorPrivacy</b> runs in <u>monitor-only mode</u> without a license. "
        "HIPS rules will be shown, but <b><span style=\"color:#ff0000;\">they will NOT be enforced</span></b>. "
        "To enable <b><span style=\"color:#009000;\">full protection and rule enforcement</span></b>, please enter valid license data. "
        "If you don't have a license yet, you can <a href=\"https://xanasoft.com/go.php?to=priv-get-cert\"><b>get one here</b></a>."));
    m_pTopLabel->setWordWrap(true);
    connect(m_pTopLabel, SIGNAL(linkActivated(const QString&)), theGUI, SLOT(OpenUrl(const QString&)));
    layout->addWidget(m_pTopLabel);

    m_pCertificate = new QPlainTextEdit();
    m_pCertificate->setMaximumSize(QSize(16777215, 73));
    m_pCertificate->setPlaceholderText(
		"NAME: User Name\n"
		"TYPE: ULTIMATE\n"
		"DATE: dd.mm.yyyy\n"
		"UPDATEKEY: 00000000000000000000000000000000\n"
		"SIGNATURE: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="
	);
    layout->addWidget(m_pCertificate);
    connect(m_pCertificate, SIGNAL(textChanged()), this, SIGNAL(completeChanged()));
    registerField("useCertificate", m_pCertificate, "plainText");
    
    layout->addWidget(new QLabel(tr("Retrieve License Certificate using a Serial Number:")));

    m_pSerial = new QLineEdit();
    m_pSerial->setPlaceholderText("PRIV_-_____-_____-_____-_____");
    layout->addWidget(m_pSerial);

    m_pEvaluate = new QCheckBox(tr("Start evaluation without a certificate for a limited period of time."));
    if (CERT_IS_TYPE(g_CertInfo, eCertEvaluation)) {
        m_pEvaluate->setEnabled(false);
        m_pEvaluate->setChecked(true);
    }
    layout->addWidget(m_pEvaluate);
    connect(m_pEvaluate, SIGNAL(toggled(bool)), this, SIGNAL(completeChanged()));
    registerField("isEvaluate", m_pEvaluate);

    QLabel* pGetEvalCert = new QLabel(tr("<b><a href=\"_\"><font color='red'>Get a free evaluation certificate</font></a> and enjoy all features for %1 days.</b>").arg(EVAL_DAYS));
    pGetEvalCert->setToolTip(tr("You can request a free %1-day evaluation certificate up to %2 times per hardware ID.").arg(EVAL_DAYS).arg(EVAL_MAX));
    layout->addWidget(pGetEvalCert);
    connect(pGetEvalCert, &QLabel::linkActivated, this, [=]() {
        CSettingsWindow::StartEval(this, this, SLOT(OnCertData(const QByteArray&, const QVariantMap&)));
	});

    layout->addWidget(new QWidget());

    setLayout(layout);
}

void CCertificatePage::initializePage()
{
    m_pCertificate->setPlainText(g_Certificate);

    uchar UsageFlags = 0;
    theCore->GetSecureParam("UsageFlags", &UsageFlags, sizeof(UsageFlags));

    m_pEvaluate->setVisible(false);

    m_pSerial->setVisible(true);
    m_pSerial->clear();
}

int CCertificatePage::nextId() const
{
    return CSetupWizard::Page_Exec;
}

bool CCertificatePage::isComplete() const 
{
    //if (field("useBusiness").toBool())
    //{
    //    m_pCertificate->setEnabled(!(m_pEvaluate->isChecked() && m_pEvaluate->isEnabled()));
    //    if (m_pCertificate->toPlainText().isEmpty() && !(m_pEvaluate->isChecked() && m_pEvaluate->isEnabled()))
    //        return false;
    //}

    return QWizardPage::isComplete();
}

void CCertificatePage::OnCertData(const QByteArray& Certificate, const QVariantMap& Params)
{
    if (!Certificate.isEmpty()) {
        m_pSerial->clear();
        m_pCertificate->setPlainText(Certificate);
        wizard()->next();
    }
    else {
        QString Message = tr("Failed to retrieve the certificate.");
        Message += tr("\nError: %1").arg(Params["error"].toString());
        QMessageBox::critical(this, "MajorPrivacy", Message);
    }
}

bool CCertificatePage::validatePage()
{
    if (m_pEvaluate->isChecked())
        return true;

    QByteArray Certificate = m_pCertificate->toPlainText().toUtf8();
    QString Serial = m_pSerial->text();

    if (!Serial.isEmpty()) 
    {
        QVariantMap Params;
	    if(!Certificate.isEmpty())
		    Params["key"] = GetArguments(Certificate, L'\n', L':').value("UPDATEKEY");

		CProgressDialogPtr pProgress = CProgressDialogPtr(new CProgressDialog(tr("Retrieving certificate..."), this));
        theGUI->m_pUpdater->GetSupportCert(Serial, this, SLOT(OnCertData(const QByteArray&, const QVariantMap&)), Params, pProgress);
        pProgress->exec();

        return false;
    }

    if (!Certificate.isEmpty()) {
        if (Certificate != g_Certificate) {
            if (CSettingsWindow::ApplyCertificate(Certificate, this))
                return false;
        }
        if (CSettingsWindow::CertRefreshRequired()) {
            if (!CSettingsWindow::TryRefreshCert(this, this, SLOT(OnCertData(const QByteArray&, const QVariantMap&)))) {
                m_pCertificate->clear();
                CSettingsWindow::SetCertificate("");
            }
            return false;
        }
    }

    //if (g_Certificate.isEmpty()) 
    /*if(!g_CertInfo.active)
    {
        if (QMessageBox::warning((CCertificatePage*)this, "MajorPrivacy", tr("Without an active License Certificate, MajorPrivacy runs in demo mode and does not enforce any rules. "
           "You can get a free %1-day evaluation certificate to unlock all features. Are you sure you want to continue without one?").arg(EVAL_DAYS), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::No)
        return false;
    }*/

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CExecPage
// 

CExecPage::CExecPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Configure <b>MajorPrivacy</b> Process Protection"));
    setSubTitle(tr("MajorPrivacy can protect the integrity of processes and enforce custom Code Integrity rules."));

    QGridLayout* layout = new QGridLayout;
    layout->setContentsMargins(3,3,3,3);
    layout->setSpacing(1);
    int row = 0;

    QLabel* pLabel = new QLabel;
    pLabel->setWordWrap(true);
    pLabel->setText(tr("MajorPrivacy puts you in charge of your system by letting you decide which programs are allowed to start others, which libraries they may load, and how processes can interact with each other (read/write memory, etc.). "
        "To give you clear insight, Execution and inter-process access (Ingress) events are aggregated into records that clearly show interactions between programs. "
        "These records are stored in memory (RAM) and can be saved to disk, giving you long-term insight into how your programs interact. For privacy, you can configure the storage options individually per program. "
        "If needed, the raw events can also be logged to RAM, though this will consume a significant amount of memory."));
    layout->addWidget(pLabel, row++, 0, 1, 2);

    QWidget* pSpacer = new QWidget();
    pSpacer->setMinimumHeight(16);
    pSpacer->setMaximumWidth(16);
    layout->addWidget(pSpacer, row, 0, 1, 1);

    m_pRecord = new QCheckBox(tr("Save Process Execution and Ingress Records to Disk (IngressRecord.dat)"));
    m_pRecord->setChecked(theCore->GetConfigBool("Service/SaveIngressRecord", false));
    layout->addWidget(m_pRecord, row++, 1);
    registerField("recordExec", m_pRecord);

    m_pLog = new QCheckBox(tr("Log Process Execution and Ingress events to Memory"));
    m_pLog->setChecked(theCore->GetConfigBool("Service/ExecLog", false));
    layout->addWidget(m_pLog, row++, 1);
    registerField("logExec", m_pLog);

    setLayout(layout);
}

int CExecPage::nextId() const
{
    return CSetupWizard::Page_Res;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CResPage
// 

CResPage::CResPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Configure <b>MajorPrivacy</b> Resource Protection"));
    setSubTitle(tr("MajorPrivacy can pritect files and folders on your system from access by rogue applications."));

    QGridLayout* layout = new QGridLayout;
    layout->setContentsMargins(3,3,3,3);
    layout->setSpacing(1);
    int row = 0;

    QLabel* pLabel = new QLabel;
    pLabel->setWordWrap(true);
    pLabel->setText(tr("MajorPrivacy lets you control how programs interact with your files and folders, you can decide which applications can read, write, modify, or delete protected data. "
        "These access events can be aggregated into clear records that show which programs touched which files, when and how often. "
        "These records are kept in memory (RAM) and can be saved to disk for long-term insight into data usage. For privacy, you can configure the storage options individually per program. "
        "If needed, raw file access events can also be logged in detail, though this will consume a significant amount of memory."));
    layout->addWidget(pLabel, row++, 0, 1, 2);

    m_pCollect = new QCheckBox(tr("Collect File/Folder Accesses Events"));
    m_pCollect->setChecked(theCore->GetConfigBool("Service/ResTrace", true));
    layout->addWidget(m_pCollect, row++, 0, 1, 2);
    registerField("collectRes", m_pCollect);

    QWidget* pSpacer = new QWidget();
    pSpacer->setMinimumHeight(16);
    pSpacer->setMaximumWidth(16);
    layout->addWidget(pSpacer, row, 0, 1, 1);

        m_pRecord = new QCheckBox(tr("Save File/Folder Accesses Records to Disk (AccessRecord.dat)"));
        m_pRecord->setChecked(theCore->GetConfigBool("Service/SaveAccessRecord", false));
        layout->addWidget(m_pRecord, row++, 1);
        registerField("recordRes", m_pRecord);

        m_pLog = new QCheckBox(tr("Log all File/Folder Accesses events to Memory"));
        m_pLog->setChecked(theCore->GetConfigBool("Service/ResLog", false));
        layout->addWidget(m_pLog, row++, 1);
        registerField("logRes", m_pLog);

    m_pEnum = new QCheckBox(tr("Enable list of all Open Files (increases CPU load)"));
    m_pEnum->setChecked(theCore->GetConfigBool("Service/EnumAllOpenFiles", false));
    layout->addWidget(m_pEnum, row++, 0, 1, 2);
    registerField("enumRes", m_pEnum);

    setLayout(layout);
}

int CResPage::nextId() const
{
    return CSetupWizard::Page_Net;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CNetPage
// 

CNetPage::CNetPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Configure <b>MajorPrivacy</b> Network Protection"));
    setSubTitle(tr("MajorPrivacy enhance and protect the Windows Firewall and Filter DNS requests."));

    QGridLayout* layout = new QGridLayout;
    layout->setContentsMargins(3,3,3,3);
    layout->setSpacing(1);
    int row = 0;

    QLabel* pLabel = new QLabel;
    pLabel->setWordWrap(true);
    pLabel->setText(tr("MajorPrivacy can take full control of the Windows Firewall, giving you the power to manage both outbound and inbound network connections. "
        "It enhances the firewall with flexible, file-pattern based rule templates and safeguards your configuration against unauthorized changes made by other programs. "
        "Every connection attempt can be tracked and aggregated (per domain) into records that clearly show which programs communicated where, and when."));
    layout->addWidget(pLabel, row++, 0, 1, 2);


    m_pEnable = new QCheckBox(tr("Take control of the Windows Firewall"));
    m_pEnable->setChecked(true);
    layout->addWidget(m_pEnable, row++, 0, 1, 2);
    registerField("enableFw", m_pEnable);

    QWidget* pSpacer = new QWidget();
    pSpacer->setMinimumHeight(16);
    pSpacer->setMaximumWidth(16);
    layout->addWidget(pSpacer, row, 0, 1, 1);

        m_pRestrict = new QCheckBox(tr("Block all outbound connection attempts by default"));
        m_pRestrict->setChecked(theCore->GetFwProfile().GetValue() == FwFilteringModes::AllowList);
        layout->addWidget(m_pRestrict, row++, 1);
        registerField("restrictFw", m_pRestrict);

		m_pCreate = new QCheckBox(tr("Create recommended windows firewall rules"));
		m_pCreate->setChecked(true);
		layout->addWidget(m_pCreate, row++, 1);
		registerField("createFwRules", m_pCreate);

        m_pProtect = new QCheckBox(tr("Automatically revert all unauthorized rule changes"));
        m_pProtect->setChecked(theCore->GetConfigBool("Service/GuardFwRules", false));
        layout->addWidget(m_pProtect, row++, 1);
        registerField("protectFw", m_pProtect);

    m_pCollect = new QCheckBox(tr("Collect File/Folder Accesses Events"));
    m_pCollect->setChecked(theCore->GetConfigBool("Service/NetTrace", true));
    layout->addWidget(m_pCollect, row++, 0, 1, 2);
    registerField("collectNet", m_pCollect);

        m_pRecord = new QCheckBox(tr("Save Network Traffic Record to Disk (TrafficRecord.dat)"));
        m_pRecord->setChecked(theCore->GetConfigBool("Service/SaveTrafficRecord", false));
        layout->addWidget(m_pRecord, row++, 1);
        registerField("recordNet", m_pRecord);

        m_pLog = new QCheckBox(tr("Log all Network Access events to Memory"));
        m_pLog->setChecked(theCore->GetConfigBool("Service/NetLog", false));
        layout->addWidget(m_pLog, row++, 1);
        registerField("logNet", m_pLog);

    connect(m_pEnable, &QCheckBox::toggled, this, [&] {
        m_pRestrict->setEnabled(m_pEnable->isChecked());
        m_pCreate->setEnabled(m_pEnable->isChecked());
        m_pProtect->setEnabled(m_pEnable->isChecked());
		m_pCollect->setEnabled(m_pEnable->isChecked());
		m_pRecord->setEnabled(m_pEnable->isChecked());
		m_pLog->setEnabled(m_pEnable->isChecked());
    });

    setLayout(layout);
}

int CNetPage::nextId() const
{
#ifndef _DEBUG
    if(theCore->IsEngineMode())
		return CSetupWizard::Page_System;
#endif
    return CSetupWizard::Page_Config;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CConfigPage
// 

CConfigPage::CConfigPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Configure <b>MajorPrivacy</b> Config Protection"));
    setSubTitle(tr("To make MajorPrivacy truly secure its configuration must be protected."));

    QGridLayout* layout = new QGridLayout;
    layout->setContentsMargins(3,3,3,3);
    layout->setSpacing(1);
    int row = 0;

    QLabel* pLabel = new QLabel;
    pLabel->setWordWrap(true);
    pLabel->setText(tr("The security-relevant configuration of MajorPrivacy (Process Protection rules, Secure Enclave settings, the Allowed Image Hash Database, and File/Folder access rules) is stored in the Windows registry. "
        "While MajorPrivacy is running, this data is protected against tampering, and the software itself can prevent unauthorized termination. However, when MajorPrivacy is not active, the registry remains vulnerable. "
        "To address this, the configuration can be protected with a digital signature, with the last valid revision locked in a read-only state that is only cleared on system shutdown. "
        "It will be re-locked automatically as soon as the KernelIsolator driver loads early in the boot process. "
        "The trade-off is that while this protection is engaged, the software cannot be uninstalled. To uninstall, the protection must first be disabled and the system rebooted."));
    layout->addWidget(pLabel, row++, 0, 1, 2);

    m_pUnloadLock = new QCheckBox(tr("Enable Unload Protection"));
    layout->addWidget(m_pUnloadLock, row++, 0, 1, 2);
    registerField("unloadLock", m_pUnloadLock);

    m_pConfigLock = new QCheckBox(tr("Enable Config Protection and set up User Key"));
    layout->addWidget(m_pConfigLock, row++, 0, 1, 2);
    registerField("configLock", m_pConfigLock);

    setLayout(layout);
}

int CConfigPage::nextId() const
{
    return CSetupWizard::Page_System;
}

bool CConfigPage::validatePage()
{
    if (m_pConfigLock->isChecked() && !((CSetupWizard*)wizard())->m_pPrivateKey)
    {
        ((CSetupWizard*)wizard())->m_pPrivateKey = new CPrivateKey;
        STATUS Status = theCore->Driver()->GetUserKey();
        if (Status.IsError())
            Status = theGUI->MakeKeyPair(((CSetupWizard*)wizard())->m_pPrivateKey);
        else
            Status = theGUI->InitSigner(CMajorPrivacy::ESignerPurpose::eEnableProtection, *((CSetupWizard*)wizard())->m_pPrivateKey);
        theGUI->CheckResults(QList<STATUS>() << Status, wizard());
        
        if (!((CSetupWizard*)wizard())->m_pPrivateKey->IsPrivateKeySet()) {
            delete ((CSetupWizard*)wizard())->m_pPrivateKey;
            ((CSetupWizard*)wizard())->m_pPrivateKey = nullptr;
            return false;
        }
    }
    return true;
}


//////////////////////////////////////////////////////////////////////////////////////////
// CSystemPage
// 

CSystemPage::CSystemPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Windows System Configuration"));
    setSubTitle(tr("To allow MajorPrivacy provide the up upmost security some system configurations should be changed."));

    QGridLayout* layout = new QGridLayout;
    layout->setContentsMargins(3,3,3,3);
    layout->setSpacing(1);
    int row = 0;

    QLabel* pLabel = new QLabel;
    pLabel->setWordWrap(true);
    pLabel->setText(tr("When sensitive data is stored in RAM, it is critical to prevent that information from being written to disk in an unprotected form. "
        "Windows may write memory contents into the page file during normal operation or into the hibernation file when the system is suspended, leaving sensitive data exposed to offline analysis or theft. "
        "Enabling NTFS page file encryption ensures that any memory pages written to disk are securely encrypted, reducing the risk of leakage. "
        "At the same time, disabling hibernation prevents entire memory snapshots from being saved to disk, closing off another major avenue through which sensitive data could persist beyond system uptime."));
    layout->addWidget(pLabel, row++, 0, 1, 2);

    m_pEncryptPageFile = new QCheckBox(tr("Enable Virtual Memory Pagefile Encryption"));
    layout->addWidget(m_pEncryptPageFile, row++, 0, 1, 2);
    registerField("encryptSwap", m_pEncryptPageFile);

    m_pNoHibernation = new QCheckBox(tr("Prevent Hibernation when Secure Protected Volumes are mounted"));
    layout->addWidget(m_pNoHibernation, row++, 0, 1, 2);
    registerField("guardSuspend", m_pNoHibernation);

    setLayout(layout);
}

int CSystemPage::nextId() const
{
    return CSetupWizard::Page_Update;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CUpdatePage
// 

CUpdatePage::CUpdatePage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Configure <b>MajorPrivacy</b> updater"));
    setSubTitle(tr("Like with any other security product, it's important to keep your MajorPrivacy up to date."));

    QGridLayout *layout = new QGridLayout;
    layout->setContentsMargins(3,3,3,3);
    layout->setSpacing(1);

    int row = 0;
    int rows = 4;

    m_pUpdate = new QCheckBox(tr("Regularly check for all updates to MajorPrivacy and optional components"));
    m_pUpdate->setToolTip(tr("Let MajorPrivacy regularly check for latest updates."));
    layout->addWidget(m_pUpdate, row++, 0, 1, rows);
    connect(m_pUpdate, &QCheckBox::toggled, this, &CUpdatePage::UpdateOptions);
    registerField("updateAll", m_pUpdate);

    QWidget* pSpacer = new QWidget();
    pSpacer->setMinimumHeight(16);
    pSpacer->setMaximumWidth(16);
    layout->addWidget(pSpacer, row+5, 0, 1, 1);

    m_pVersion = new QCheckBox(tr("Check for new MajorPrivacy versions:"));
    m_pVersion->setToolTip(tr("Check for new MajorPrivacy builds."));
    layout->addWidget(m_pVersion, row++, 1, 1, rows-2);
    //layout->addWidget(new QComboBox(), row-1, 3, 1, 1);
    connect(m_pVersion, &QCheckBox::toggled, this, &CUpdatePage::UpdateOptions);
    registerField("updateApp", m_pVersion);

    m_pChanelInfo = new QLabel(tr("Select in which update channel to look for new MajorPrivacy builds:"));
    m_pChanelInfo->setMinimumHeight(20);
    layout->addWidget(m_pChanelInfo, row++, 1, 1, rows-1);

    pSpacer = new QWidget();
    pSpacer->setMinimumHeight(16);
    pSpacer->setMaximumWidth(16);
    layout->addWidget(pSpacer, row, 1, 1, 1);

    m_pStable = new QRadioButton(tr("In the Stable Channel"));
    m_pStable->setToolTip(tr("The stable channel contains the latest stable GitHub releases."));
    layout->addWidget(m_pStable, row++, 2, 1, rows-2);
    registerField("channelStable", m_pStable);

    m_pPreview = new QRadioButton(tr("In the Preview Channel - with newest experimental changes"));
    m_pPreview->setToolTip(tr("The preview channel contains the latest GitHub pre-releases."));
    layout->addWidget(m_pPreview, row++, 2, 1, rows-2);
    registerField("channelPreview", m_pPreview);

    //m_pInsider = new QRadioButton(tr("In the Insider Channel - exclusive features"));
    //m_pInsider->setToolTip(tr("The Insider channel offers early access to new features and bugfixes that will eventually be released to the public, "
    //    "as well as all relevant improvements from the stable channel. \nUnlike the preview channel, it does not include untested, potentially breaking, "
    //    "or experimental changes that may not be ready for wider use."));
    //layout->addWidget(m_pInsider, row, 2, 1, 1);
    //registerField("channelInsider", m_pInsider);
    //QLabel* pInsiderInfo = new QLabel(tr("More about the <a href=\"https://xanasoft.com/go.php?to=priv-insider\">Insider Channel</a>"));
    //connect(pInsiderInfo, SIGNAL(linkActivated(const QString&)), theGUI, SLOT(OpenUrl(const QString&)));
    //layout->addWidget(pInsiderInfo, row++, 3, 1, 1);

    //m_pHotfixes = new QCheckBox(tr("Keep Compatibility Templates up to date and apply hotfixes"));
    //m_pHotfixes->setToolTip(tr("Check for latest compatibility templates and hotfixes."));
    //layout->addWidget(m_pHotfixes, row++, 1, 1, rows-1);
    //registerField("applyHotfixes", m_pHotfixes);

    m_pTweaks = new QCheckBox(tr("Keep tweak list up to date"));
    m_pTweaks->setToolTip(tr("Check for latest privacy tweaks."));
    layout->addWidget(m_pTweaks, row++, 1, 1, rows-1);
    registerField("updateTweaks", m_pTweaks);

    //m_pUpdateInfo = new QLabel();
    //m_pUpdateInfo->setWordWrap(true);
    //m_pUpdateInfo->setText(tr("MajorPrivacy applies strict application restrictions, which can lead to compatibility issues. "
    //    "Stay updated with MajorPrivacy, including compatibility templates and troubleshooting, to ensure smooth operation amid Windows updates and application changes."));
    //layout->addWidget(m_pUpdateInfo, row++, 0, 1, rows);

    //layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Fixed, QSizePolicy::Expanding), row++, 0);

    //m_pBottomLabel = new QLabel(tr("Access to the latest compatibility templates and the online troubleshooting database requires a valid <a href=\"https://xanasoft.com/go.php?to=priv-cert\">supporter certificate</a>."));
    //connect(m_pBottomLabel, SIGNAL(linkActivated(const QString&)), theGUI, SLOT(OpenUrl(const QString&)));
    //m_pBottomLabel->setWordWrap(true);
    //layout->addWidget(m_pBottomLabel, row++, 0, 1, rows);

    setLayout(layout);
}

void CUpdatePage::initializePage()
{
    m_pUpdate->setChecked(true);
    m_pStable->setChecked(true);

    //m_pBottomLabel->setVisible(!g_CertInfo.active || g_CertInfo.expired);

    UpdateOptions();
}

void CUpdatePage::UpdateOptions()
{
    m_pVersion->setVisible(!m_pUpdate->isChecked());
    //m_pHotfixes->setVisible(!m_pUpdate->isChecked());
    m_pTweaks->setVisible(!m_pUpdate->isChecked());
    m_pChanelInfo->setVisible(m_pUpdate->isChecked());
    //m_pUpdateInfo->setVisible(m_pUpdate->isChecked());

    if (m_pUpdate->isChecked()) {
        m_pVersion->setChecked(true);
        m_pTweaks->setChecked(true);
    }

    m_pStable->setEnabled(m_pVersion->isChecked());
    m_pPreview->setEnabled(m_pVersion->isChecked());
    //m_pInsider->setEnabled(CERT_IS_INSIDER(g_CertInfo) && m_pVersion->isChecked());

    //m_pHotfixes->setEnabled(m_pVersion->isChecked());
}

int CUpdatePage::nextId() const
{
    return CSetupWizard::Page_Finish;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CFinishPage
// 

CFinishPage::CFinishPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Complete your configuration"));
    QPixmap Logo = QPixmap(CCustomTheme::IsDarkTheme() ? ":/PrivLogoDM.png" : ":/PrivLogo.png");
    int Scaling = theConf->GetInt("Options/FontScaling", 100);
    if(Scaling !=  100) Logo = Logo.scaled(Logo.width() * Scaling / 100, Logo.height() * Scaling / 100);
    setPixmap(QWizard::WatermarkPixmap, Logo);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(3,3,3,3);
	layout->setSpacing(1);

    m_pLabel = new QLabel;
    m_pLabel->setWordWrap(true);
    m_pLabel->setText(tr("Almost complete, click Finish to apply all selected options and conclude the wizard."));
    layout->addWidget(m_pLabel);

    //QWidget* pSpacer = new QWidget();
    //pSpacer->setMinimumHeight(16);
    //layout->addWidget(pSpacer);
    
    //QLabel* pLabel = new QLabel;
    //pLabel->setWordWrap(true);
    //pLabel->setText(tr("Like with any other security product it's important to keep your MajorPrivacy up to date."));
    //layout->addWidget(pLabel);

    //m_pUpdate = new QCheckBox(tr("Keep MajorPrivacy up to date."));
    //m_pUpdate->setChecked(true);
    //layout->addWidget(m_pUpdate);
    //registerField("isUpdate", m_pUpdate);

    setLayout(layout);
}

int CFinishPage::nextId() const
{
    return -1;
}

void CFinishPage::initializePage()
{

}
