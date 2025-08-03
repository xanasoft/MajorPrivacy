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
#include "../Library/Crypto/HashFunction.h"

CStatusView::CStatusView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QFlowLayout(this);
	setLayout(m_pMainLayout);

	int height = 200;

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
	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pSecLayout->addWidget(pSpacer, 6, 0);
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
			CEncryption::GetKeyFromPW(pInfo->PubKey, FP, 1048576); // 2^20 iterations
			m_pSecKey->setText(tr("User Key: %1").arg(QByteArray((char*)FP.GetBuffer(), (int)FP.GetSize()).toHex().toUpper()));

			CBuffer Hash(64);
			CHashFunction::Hash(pInfo->PubKey, Hash);
			m_pSecKey->setToolTip(tr("SHA256-FP: %1").arg(QByteArray((char*)Hash.GetBuffer(), (int)Hash.GetSize()).toHex().toUpper()));

			m_bHasKey = true;
		}

		uint32 uConfigStatus = theCore->Driver()->GetConfigStatus();
		//uConfigStatus |= theCore->Service()->GetConfigStatus();
		bool bProtected = (uConfigStatus & CONFIG_STATUS_PROTECTED) != 0;
		if (theCore->Driver()->GetConfigBool("UnloadProtection", false))
			m_pSecConfig->setText(tr("Config Protection Active"));
		else
			m_pSecConfig->setText(tr("Config Protection NOT Active"));

		if (theCore->Driver()->GetConfigBool("UnloadProtection", false))
			m_pSecCore->setText(tr("Unload Protection Active"));
		else
			m_pSecCore->setText(tr("Unload Protection NOT Active"));
	}

	if (g_CertInfo.active)
	{
		m_pExecIcon->setPixmap(QPixmap(":/Icons/Process.png"));

		m_pExecRules->setText(tr("Program Rules: %1").arg(theCore->ProgramManager()->GetProgramRules().count()));
		m_pExecEnclaves->setText(tr("Enclaves: %1").arg(theCore->EnclaveManager()->GetAllEnclaves().count()));
		m_pExecStatus->setText(tr("Protection active"));
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
		m_pResStatus->setText(tr("Protection active"));
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
	else if (!theCore->GetConfigBool("Service/DnsInstallFilter", false))
		m_pDnsStatus->setText(tr("DNS Filter NOT installed"));
	else
		m_pDnsStatus->setText(tr("DNS Filter installed"));


	m_pTweakIcon->setPixmap(QPixmap(":/Icons/Tweaks.png"));
	//m_pTweakStatus->setText(tr("Status"));
	m_pTweakApplied->setText(tr("Applied Tweaks: %1").arg(theCore->TweakManager()->GetAppliedCount()));
	m_pTweakFailed->setText(tr("Failed Tweaks: %1").arg(theCore->TweakManager()->GetFailedCount()));
}