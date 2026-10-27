#include "pch.h"
#include "StatusView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Access/AccessManager.h"
#include "../Core/Network/NetworkManager.h"
#include "../Core/Enclaves/EnclaveManager.h"
#include "../Core/Volumes/VolumeManager.h"
#include "../Core/Tweaks/TweakManager.h"
#include "../Core/IssueManager.h"
#include "../Windows/SettingsWindow.h"
#include "../../MiscHelpers/Common/Common.h"
#include "../../MiscHelpers/Common/OtherFunctions.h"
#include "../Library/Crypto/Encryption.h"
#include "../../Library/Boot/EfiVars.h"
#include "../Library/Crypto/HashFunction.h"

CStatusView::CStatusView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QFlowLayout(this);
	setLayout(m_pMainLayout);

	int height = 240;

	m_pSecBox = new QGroupBox(tr("System Status"), this);
	m_pSecLayout = new QGridLayout(m_pSecBox);
	m_pSecLayout->setContentsMargins(3, 3, 3, 3);
	m_pSecIcon = new QLabel(this);
	//m_pSecIcon->setText("ICON");
	m_pSecLayout->addWidget(m_pSecIcon, 0, 0, 3, 1);
		m_pSecService = new QLabel(this);
		m_pSecLayout->addWidget(m_pSecService, 0, 1);
		m_pSecDriver = new QLabel(this);
		m_pSecLayout->addWidget(m_pSecDriver, 1, 1);
	m_pSecKey = new QLabel(this);
	m_pSecLayout->addWidget(m_pSecKey, 3, 0, 1, 2);
	m_pSecConfig = new QLabel(this);
	m_pSecLayout->addWidget(m_pSecConfig, 4, 0, 1, 2);
	m_pSecCore = new QLabel(this);
	m_pSecLayout->addWidget(m_pSecCore, 5, 0, 1, 2);
	m_pSecFirmware = new QLabel(this);
	m_pSecLayout->addWidget(m_pSecFirmware, 6, 0, 1, 2);
	m_pSecProvision = new QLabel(this);
	m_pSecLayout->addWidget(m_pSecProvision, 7, 0, 1, 2);
	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pSecLayout->addWidget(pSpacer, 8, 0);
	m_pMainLayout->addWidget(m_pSecBox);
	m_pSecBox->setFixedHeight(height);

	m_pExecBox = new QGroupBox(tr("Process Protection"), this);
	m_pExecLayout = new QGridLayout(m_pExecBox);
	m_pExecLayout->setContentsMargins(3, 3, 3, 3);
	m_pExecIcon = new QLabel(this);
	//m_pExecIcon->setText("ICON");
	m_pExecLayout->addWidget(m_pExecIcon, 0, 0, 3, 1);
		m_pExecStatus = new QLabel(this);
		m_pExecLayout->addWidget(m_pExecStatus, 0, 1);
	m_pExecRules = new QLabel(this);
	m_pExecLayout->addWidget(m_pExecRules, 3, 0, 1, 2);
	m_pExecEnclaves = new QLabel(this);
	m_pExecLayout->addWidget(m_pExecEnclaves, 4, 0, 1, 2);
	pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pExecLayout->addWidget(pSpacer, 5, 0);
	m_pMainLayout->addWidget(m_pExecBox);
	m_pExecBox->setFixedHeight(height);

	m_pResBox = new QGroupBox(tr("Resource Protection"), this);
	m_pResLayout = new QGridLayout(m_pResBox);
	m_pResLayout->setContentsMargins(3, 3, 3, 3);
	m_pResIcon = new QLabel(this);
	//m_pResIcon->setText("ICON");
	m_pResLayout->addWidget(m_pResIcon, 0, 0, 3, 1);
		m_pResStatus = new QLabel(this);
		m_pResLayout->addWidget(m_pResStatus, 0, 1);
	m_pResRules = new QLabel(this);
	m_pResLayout->addWidget(m_pResRules, 3, 0, 1, 2);
	m_pResFolders = new QLabel(this);
	m_pResLayout->addWidget(m_pResFolders, 4, 0, 1, 2);
	m_pResVolumes = new QLabel(this);
	m_pResLayout->addWidget(m_pResVolumes, 5, 0, 1, 2);
	pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pResLayout->addWidget(pSpacer, 6, 0);
	m_pMainLayout->addWidget(m_pResBox);
	m_pResBox->setFixedHeight(height);

	m_pFirewallBox = new QGroupBox(tr("Networking Protection"), this);
	m_pFirewallLayout = new QGridLayout(m_pFirewallBox);
	m_pFirewallLayout->setContentsMargins(3, 3, 3, 3);
	m_pFirewallIcon = new QLabel(this);
	//m_pFirewallIcon->setText("ICON");
	m_pFirewallLayout->addWidget(m_pFirewallIcon, 0, 0, 3, 1);
		m_pFirewallStatus = new QLabel(this);
		m_pFirewallLayout->addWidget(m_pFirewallStatus, 0, 1);
		m_pDnsStatus = new QLabel(this);
		m_pFirewallLayout->addWidget(m_pDnsStatus, 1, 1);
	m_pFirewallRules = new QLabel(this);
	m_pFirewallLayout->addWidget(m_pFirewallRules, 3, 0, 1, 2);
	m_pFirewallPending = new QLabel(this);
	m_pFirewallLayout->addWidget(m_pFirewallPending, 4, 0, 1, 2);
	m_pFirewallAltered = new QLabel(this);
	m_pFirewallLayout->addWidget(m_pFirewallAltered, 5, 0, 1, 2);
	m_pFirewallMissing = new QLabel(this);
	m_pFirewallLayout->addWidget(m_pFirewallMissing, 6, 0, 1, 2);
	pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pFirewallLayout->addWidget(pSpacer, 7, 0);
	m_pMainLayout->addWidget(m_pFirewallBox);
	m_pFirewallBox->setFixedHeight(height);

	m_pTweakBox = new QGroupBox(tr("Privacy Tweaks"), this);
	m_pTweakLayout = new QGridLayout(m_pTweakBox);
	m_pTweakLayout->setContentsMargins(3, 3, 3, 3);
	m_pTweakIcon = new QLabel(this);
	//m_pTweakIcon->setText("ICON");
	m_pTweakLayout->addWidget(m_pTweakIcon, 0, 0, 3, 1);
		m_pTweakStatus = new QLabel(this);
		m_pTweakLayout->addWidget(m_pTweakStatus, 0, 1);
	m_pTweakApplied = new QLabel(this);
	m_pTweakLayout->addWidget(m_pTweakApplied, 3, 0, 1, 2);
	m_pTweakFailed = new QLabel(this);
	m_pTweakLayout->addWidget(m_pTweakFailed, 4, 0, 1, 2);
	pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pTweakLayout->addWidget(pSpacer, 5, 0);
	m_pMainLayout->addWidget(m_pTweakBox);
	m_pTweakBox->setFixedHeight(height);


	// set black background
	/*QPalette pal = palette();
	pal.setColor(QPalette::Window, Qt::yellow);
	setAutoFillBackground(true);
	setPalette(pal);*/

	Update();

}

CStatusView::~CStatusView()
{

}

void CStatusView::Refresh()
{
}

void CStatusView::Update()
{
	static QImage Major;
	if(Major.isNull())
		Major = QPixmap(":/MajorPrivacy.png").scaled(50, 50, Qt::KeepAspectRatio, Qt::SmoothTransformation).toImage();

	bool bSvc = theCore->Service()->IsConnected();
	bool bDrv = theCore->Driver()->IsConnected();
	bool bSuspended = false;
	bool bKeyMismatch = false;

	auto Icon = Major;
	if(!bSvc)
		GrayScale(Icon);
	else if(!bDrv)
		Icon = ImageAddOverlay(Icon, ":/Icons/Error.png", 25);
	//else
	//	Icon = ImageAddOverlay(Icon, ":/Icons/Warning.png", 25);
	m_pSecIcon->setPixmap(QPixmap::fromImage(Icon));
	m_pSecService->setText(tr("Privacy Agent: %1").arg(bSvc ? tr("OK") : tr("Inactive")));
	m_pSecDriver->setText(tr("Kernel Isolator: %1").arg(bDrv ? tr("OK") : tr("Inactive")));

	if (!bDrv)
	{
		m_pSecKey->setText(tr("User Key NOT available"));
		m_pSecConfig->setText(tr("Config Protection NOT available"));
		m_pSecCore->setText(tr("Unload Protection NOT available"));
		//
		// Both of these are read through the driver, so without it there is
		// nothing to report either way - and the tooltips are cleared so that
		// a reading from when it was connected cannot linger on them.
		//
		m_pSecFirmware->setText(tr("Bootloader: n/a"));
		m_pSecFirmware->setToolTip(tr("The state of the pre-boot loader is read through the "
			"kernel isolator, which is not connected."));
		m_pSecProvision->setText("");
		m_pSecProvision->setToolTip(QString());
		m_bHasKey = false;
	}
	else
	{
		auto Ret = theCore->Driver()->GetUserKey();
		if (Ret.IsError()) {
			m_pSecKey->setText(tr("User Key is NOT configured"));
			m_bHasKey = false;
		}
		else if(!m_bHasKey)
		{
			auto pInfo = Ret.GetValue();

			CBuffer FP(8); // 64 bits
			CEncryption::GetKeyFromPWOld(pInfo->PubKey, CBuffer(), FP, 1048576); // 2^20 iterations
			m_pSecKey->setText(tr("User Key: %1").arg(QString::fromLatin1(QByteArray((char*)FP.GetBuffer(), (int)FP.GetSize()).toHex().toUpper())));

			CBuffer Hash(64);
			CHashFunction::Hash(pInfo->PubKey, Hash);
			m_pSecKey->setToolTip(tr("SHA256-FP: %1").arg(QString::fromLatin1(QByteArray((char*)Hash.GetBuffer(), (int)Hash.GetSize()).toHex().toUpper())));

			m_bHasKey = true;
		}

		uint32 uConfigStatus = theCore->Driver()->GetConfigStatus();
		//uConfigStatus |= theCore->Service()->GetConfigStatus();
		bool bProtected = (uConfigStatus & CONFIG_STATUS_PROTECTED) != 0;
		bSuspended = (uConfigStatus & CONFIG_STATUS_SUSPENDED) != 0;
		bKeyMismatch = (uConfigStatus & CONFIG_STATUS_KEYMISMATCH) != 0;

		if (bKeyMismatch) {
			m_pSecKey->setText(tr("User Key MISMATCH (EFI vs registry)!"));
			m_pSecKey->setToolTip(tr("The pre-boot protected UserPubKey EFI variable does not match the user key in the protected registry."));
		}

		if (bSuspended)
			m_pSecConfig->setText(tr("Enforcement SUSPENDED (pre-boot recovery)"));
		else if (theCore->Driver()->GetConfigBool("UnloadProtection", false))
			m_pSecConfig->setText(tr("Config Protection Active"));
		else
			m_pSecConfig->setText(tr("Config Protection NOT Active"));

		if (theCore->Driver()->GetConfigBool("UnloadProtection", false))
			m_pSecCore->setText(tr("Unload Protection Active"));
		else
			m_pSecCore->setText(tr("Unload Protection NOT Active"));

		//
		// Pre-boot firmware protection (DiskCryptor DcsOwner) + user-key provisioning state.
		//
		bool bFwAvail = (uConfigStatus & CONFIG_STATUS_FWAVAIL) != 0;
		bool bProvisioned = (uConfigStatus & CONFIG_STATUS_KEYPROVISIONED) != 0; // UserPubKey_RT mirror set
		bool bCommitted = (uConfigStatus & CONFIG_STATUS_KEYCOMMITTED) != 0;     // committed/enforced this boot

		//
		// Secure Boot is the other half of this line. The loader being there
		// says a user key can be committed before Windows starts; Secure Boot
		// says whether the firmware checks that the loader is still ours.
		//
		// Tri-state, and the third state matters: 1 on, 0 off, -1 could not be
		// determined. Only 1 may be reported as secure.
		//
		int iSecureBoot = CEfiVars::GetSecureBootStatus();

		if (!bFwAvail) {
			m_pSecFirmware->setText(tr("Bootloader: NOT installed"));
			m_pSecFirmware->setToolTip(tr(
				"The DCS pre-boot loader is not on this machine's EFI partition. Without it a user "
				"key cannot be committed before Windows starts, and pre-boot recovery through the "
				"suspend switch is not available.\n\n"
				"Install it under Tools > Advanced Tools > EFI Boot Loader."));
		}
		else if (iSecureBoot != 1) {
			m_pSecFirmware->setText(tr("Bootloader: installed"));
			m_pSecFirmware->setToolTip(iSecureBoot == 0
				? tr("The pre-boot loader is installed, so DcsOwner can hold the user key and the "
					 "suspend switch in firmware variables that Windows cannot write to.\n\n"
					 "Secure Boot is off, so the firmware does not verify the loader before "
					 "running it - anything able to write to the EFI partition could replace it.")
				: tr("The pre-boot loader is installed, so DcsOwner can hold the user key and the "
					 "suspend switch in firmware variables that Windows cannot write to.\n\n"
					 "Whether Secure Boot is on could not be determined on this machine."));
		}
		else {
			m_pSecFirmware->setText(tr("Secure Bootloader: installed"));
			m_pSecFirmware->setToolTip(tr(
				"The pre-boot loader is installed and Secure Boot is on, so the firmware verifies "
				"the loader before running it - a replaced loader will not start.\n\n"
				"DcsOwner holds the user key and the suspend switch in firmware variables that "
				"Windows cannot write to."));
		}

		//
		// Short enough to fit the panel; what a pending state means goes in the
		// tooltip rather than onto the line.
		//
		QString Provision, Tip;
		if (!bFwAvail) {
			Provision = tr("unavailable");
			Tip = tr("Without the pre-boot loader there is nothing to protect a firmware "
				"variable, so the user key cannot be committed before Windows starts.");
		}
		else if (bKeyMismatch) {
			Provision = tr("MISMATCH");
			Tip = tr("The user key committed before boot is not the one in the protected "
				"registry. One of the two has been changed behind the other's back, and the "
				"isolator is treating the key as untrusted until they agree again.");
		}
		else if (bProvisioned && bCommitted) {
			Provision = tr("provisioned");
			Tip = tr("The user key was committed before Windows started and DcsOwner is "
				"enforcing it this boot - the operating system cannot change it.");
		}
		else if (bProvisioned && !bCommitted) {
			Provision = tr("provision pending");
			Tip = tr("The key is written to the pre-boot mirror but not yet committed.\n\n"
				"Restart and confirm it at the boot screen; until then it is not enforced.");
		}
		else if (!bProvisioned && bCommitted) {
			Provision = tr("removal pending");
			Tip = tr("A committed key is still being enforced this boot, but the mirror has "
				"been cleared.\n\nRestart and confirm the removal at the boot screen.");
		}
		else {
			Provision = tr("not provisioned");
			Tip = tr("The pre-boot loader is installed but no user key has been committed to "
				"firmware, so the isolator relies on the protected registry alone.");
		}

		m_pSecProvision->setText(tr("EFI Key: %1").arg(Provision));
		m_pSecProvision->setToolTip(Tip);
	}

	if (g_CertInfo.active)
	{
		m_pExecIcon->setPixmap(QPixmap(":/Icons/Process.png"));

		m_pExecRules->setText(tr("Program Rules: %1").arg(theCore->ProgramManager()->GetProgramRules().count()));
		m_pExecEnclaves->setText(tr("Enclaves: %1").arg(theCore->EnclaveManager()->GetAllEnclaves().count()));
		m_pExecStatus->setText(bSuspended ? tr("Enforcement SUSPENDED") : tr("Protection active"));
	}
	else
	{
		QImage Image = QPixmap(":/Icons/Process.png").toImage();
		GrayScale(Image);
		m_pExecIcon->setPixmap(QPixmap::fromImage(Image));

		m_pExecRules->setText(tr("Program Rules: DISABLED"));
		m_pExecEnclaves->setText(tr("Enclaves: DISABLED"));
		m_pExecStatus->setText(tr("Protection NOT active"));
	}


	if (g_CertInfo.active)
	{
		m_pResIcon->setPixmap(QPixmap(":/Icons/Ampel.png"));

		m_pResRules->setText(tr("Access Rules: %1").arg(theCore->AccessManager()->GetAccessRules().count()));
		int Folders = 0;
		int Volumes = 0;
		foreach(const CVolumePtr& pVolume, theCore->VolumeManager()->List()) {
			QString Path = pVolume->GetImagePath();
			if(pVolume->IsFolder()) Folders++;
			else Volumes++;
		}
		m_pResFolders->setText(tr("Protected Folders: %1").arg(Folders));
		m_pResVolumes->setText(tr("Secure Volumes: %1").arg(Volumes));
		m_pResStatus->setText(bSuspended ? tr("Enforcement SUSPENDED") : tr("Protection active"));
	}
	else
	{
		QImage Image = QPixmap(":/Icons/Ampel.png").toImage();
		GrayScale(Image);
		m_pResIcon->setPixmap(QPixmap::fromImage(Image));

		m_pResRules->setText(tr("Access Rules: DISABLED"));
		m_pResFolders->setText(tr("Protected Folders: DISABLED"));
		m_pResVolumes->setText(tr("Secure Volumes: DISABLED"));
		m_pResStatus->setText(tr("Protection NOT active"));
	}


	QImage FwIcon = QPixmap(":/Icons/Wall3.png").toImage();
	auto FwState = theCore->GetFwProfile().GetValue();
	switch (FwState)
	{
	case FwFilteringModes::AllowList:	FwIcon = ImageAddOverlay(FwIcon, ":/Icons/Shield11.png", 25); break;
	case FwFilteringModes::BlockList:	FwIcon = ImageAddOverlay(FwIcon, ":/Icons/Shield7.png", 25); break;
	case FwFilteringModes::NoFiltering: FwIcon = ImageAddOverlay(FwIcon, ":/Icons/Shield10.png", 25); break;
	}
	m_pFirewallIcon->setPixmap(QPixmap::fromImage(FwIcon));

	if (FwState == FwFilteringModes::NoFiltering)
		m_pFirewallStatus->setText(tr("Windows Firewall DISABLED"));
	else
		m_pFirewallStatus->setText(tr("Windows Firewall Active"));
	m_pFirewallRules->setText(tr("Firewall Rules: %1").arg(theCore->NetworkManager()->GetFwRules().count()));

	int Unapproved = theCore->IssueManager()->GetUnapprovedFwRuleCount();
	if(Unapproved)
		m_pFirewallPending->setText(tr("Unapproved Rules: %1").arg(Unapproved));
	else
		m_pFirewallPending->setText(tr("No Unapproved Rules"));

	int Altered = theCore->IssueManager()->GetAlteredFwRuleCount();
	if(Altered)
		m_pFirewallAltered->setText(tr("Altered Rules: %1").arg(Altered));
	else
		m_pFirewallAltered->setText(tr("No Altered Rules"));

	int Missing = theCore->IssueManager()->GetMissingFwRuleCount();
	if(Missing)
		m_pFirewallMissing->setText(tr("Missing Rules: %1").arg(Missing));
	else
		m_pFirewallMissing->setText(tr("No Missing Rules"));

	if (!theCore->GetConfigBool("Service/DnsEnableFilter", false))
		m_pDnsStatus->setText(tr("DNS Filter NOT enabled"));
	else if (!theCore->GetConfigBool("Service/DnsInstallFilter", true))
		m_pDnsStatus->setText(tr("DNS Filter NOT installed"));
	else
		m_pDnsStatus->setText(tr("DNS Filter installed"));


	m_pTweakIcon->setPixmap(QPixmap(":/Icons/Tweaks.png"));
	//m_pTweakStatus->setText(tr("Status"));
	m_pTweakApplied->setText(tr("Applied Tweaks: %1").arg(theCore->TweakManager()->GetAppliedCount()));
	m_pTweakFailed->setText(tr("Failed Tweaks: %1").arg(theCore->TweakManager()->GetFailedCount()));
}