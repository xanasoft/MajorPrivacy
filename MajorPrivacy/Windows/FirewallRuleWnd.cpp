#include "pch.h"
#include "FirewallRuleWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Network/NetworkManager.h"
#include "../Core/GenericRule.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NetUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"
#include "../Windows/ProgramPicker.h"
#include "RuleLifetime.h"
#include "../Helpers/SidResolver.h"
#include <sddl.h>
#include <Lm.h>

static QString ResolveNameToSid(const QString& Name, QString* pFullName = nullptr)
{
	BYTE sidBuffer[SECURITY_MAX_SID_SIZE];
	DWORD sidSize = sizeof(sidBuffer);
	SID_NAME_USE sidType;
	WCHAR domain[256];
	DWORD domainSize = _countof(domain);

	if (LookupAccountNameW(nullptr, (LPCWSTR)Name.utf16(), sidBuffer, &sidSize, domain, &domainSize, &sidType))
	{
		if (pFullName) {
			QString domainStr = QString::fromWCharArray(domain);
			if (!domainStr.isEmpty())
				*pFullName = domainStr + "\\" + Name;
			else
				*pFullName = Name;
		}

		LPWSTR sidStr = nullptr;
		if (ConvertSidToStringSidW((PSID)sidBuffer, &sidStr))
		{
			QString sid = QString::fromWCharArray(sidStr);
			LocalFree(sidStr);
			return sid;
		}
	}
	return QString();
}

static void AddAccountsToUserComboBox(QComboBox* pComboBox)
{
	{
		LPUSER_INFO_0 pBuf = nullptr;
		DWORD entriesRead = 0, totalEntries = 0, resumeHandle = 0;
		if (NERR_Success == NetUserEnum(nullptr, 0, FILTER_NORMAL_ACCOUNT, (LPBYTE*)&pBuf, MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, &resumeHandle))
		{
			for (DWORD i = 0; i < entriesRead; ++i) {
				QString Name = QString::fromWCharArray(pBuf[i].usri0_name);
				QString FullName;
				QString Sid = ResolveNameToSid(Name, &FullName);
				pComboBox->addItem(FullName.isEmpty() ? Name : FullName, Sid.isEmpty() ? Name : Sid);
			}
		}
		if (pBuf) NetApiBufferFree(pBuf);
	}

	{
		LPLOCALGROUP_INFO_0 pBuf = nullptr;
		DWORD entriesRead = 0, totalEntries = 0;
		DWORD_PTR resumeHandle = NULL;
		if (NERR_Success == NetLocalGroupEnum(nullptr, 0, (LPBYTE*)&pBuf, MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, &resumeHandle))
		{
			for (DWORD i = 0; i < entriesRead; ++i) {
				QString Name = QString::fromWCharArray(pBuf[i].lgrpi0_name);
				QString FullName;
				QString Sid = ResolveNameToSid(Name, &FullName);
				pComboBox->addItem(QObject::tr("[%1]").arg(FullName.isEmpty() ? Name : FullName), Sid.isEmpty() ? Name : Sid);
			}
		}
		if (pBuf) NetApiBufferFree(pBuf);
	}
}

CFirewallRuleWnd::CFirewallRuleWnd(const CFwRulePtr& pRule, QSet<CProgramItemPtr> Items, QWidget* parent)
	: QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);

	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	//flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	//flags &= ~Qt::WindowMinMaxButtonsHint;
	//flags |= Qt::WindowMinimizeButtonHint;
	//flags &= ~Qt::WindowCloseButtonHint;
	flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	setWindowFlags(flags);

	ui.setupUi(this);


	m_pRule = pRule;
	bool bNew = m_pRule->m_Guid.IsNull();

	setWindowTitle(bNew ? tr("Create Firewall Rule") : tr("Edit Firewall Rule"));

	foreach(auto pItem, Items) 
		AddProgramItem(pItem);

	if (bNew && m_pRule->m_Name.isEmpty()) {
		m_pRule->m_Name = tr("MajorPrivacy-Rule, New Firewall Rule");
	} else
		m_NameChanged = true;

	connect(ui.txtName, SIGNAL(textChanged(const QString&)), this, SLOT(OnNameChanged(const QString&)));
	connect(ui.btnMkName, &QToolButton::clicked, this, [&]() {
		m_NameChanged = false;
		TryMakeName();
	});

	connect(ui.btnProg, SIGNAL(clicked()), this, SLOT(OnPickProgram()));
	connect(ui.cmbProgram, SIGNAL(currentIndexChanged(int)), this, SLOT(OnProgramChanged()));
	//connect(ui.chkAllPrograms, SIGNAL(toggled(bool)), this, SLOT(OnAllPrograms()));

	connect(ui.cmbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(OnActionChanged()));

	connect(ui.radProfiles, SIGNAL(idToggled(int, bool)), this, SLOT(OnProfilesChanged()));
	connect(ui.radNICs, SIGNAL(idToggled(int, bool)), this, SLOT(OnInterfacesChanged()));

	connect(ui.cmbDirection, SIGNAL(currentIndexChanged(int)), this, SLOT(OnDirectionChanged()));

	connect(ui.cmbProtocol, SIGNAL(currentIndexChanged(int)), this, SLOT(OnProtocolChanged()));
	connect(ui.cmbProtocol, SIGNAL(currentTextChanged(const QString&)), this, SLOT(OnProtocolChanged()));

	connect(ui.radLocalIP, SIGNAL(idToggled(int, bool)), this, SLOT(OnLocalIPChanged()));
	//connect(ui.btnLocalIPAdd, SIGNAL(clicked(bool)), this, SLOT(OnLocalIPAdd()));
	connect(ui.lstLocalIP, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnLocalIPEdit()));
	//connect(ui.lstLocalIP, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(OnLocalIPEdit()));
	connect(ui.btnLocalIPSet, SIGNAL(clicked(bool)), this, SLOT(OnLocalIPSet()));

	connect(ui.radRemoteIP, SIGNAL(idToggled(int, bool)), this, SLOT(OnRemoteIPChanged()));
	//connect(ui.btnRemoteIPAdd, SIGNAL(clicked(bool)), this, SLOT(OnRemoteIPAdd()));
	connect(ui.lstRemoteIP, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnRemoteIPEdit()));
	//connect(ui.lstRemoteIP, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(OnRemoteIPEdit()));
	connect(ui.btnRemoteIPSet, SIGNAL(clicked(bool)), this, SLOT(OnRemoteIPSet()));

	connect(ui.chkAllowUsers, SIGNAL(toggled(bool)), this, SLOT(OnAllowUsersChanged()));
	connect(ui.lstApplyUsers, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnApplyUsersEdit()));
	connect(ui.btnApplyUsersSet, SIGNAL(clicked(bool)), this, SLOT(OnApplyUsersSet()));

	connect(ui.chkExcemptUsers, SIGNAL(toggled(bool)), this, SLOT(OnExemptUsersChanged()));
	connect(ui.lstExemptUsers, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnExemptUsersEdit()));
	connect(ui.btnExemptUsersSet, SIGNAL(clicked(bool)), this, SLOT(OnExemptUsersSet()));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(OnSave()));

	AddColoredComboBoxEntry(ui.cmbAction, tr("Allow"), GetActionColor(EFwActions::Allow), (uint32)EFwActions::Allow);
	AddColoredComboBoxEntry(ui.cmbAction, tr("Block"), GetActionColor(EFwActions::Block), (uint32)EFwActions::Block);
	ColorComboBox(ui.cmbAction);

	if(bNew || pRule->IsTemplate())
		AddColoredComboBoxEntry(ui.cmbDirection, tr("Bidirectional"), GetDirectionColor(EFwDirections::Bidirectional), (uint32)EFwDirections::Bidirectional);
	AddColoredComboBoxEntry(ui.cmbDirection, tr("Inbound"), GetDirectionColor(EFwDirections::Inbound), (uint32)EFwDirections::Inbound);
	AddColoredComboBoxEntry(ui.cmbDirection, tr("Outbound"), GetDirectionColor(EFwDirections::Outbound), (uint32)EFwDirections::Outbound);
	ColorComboBox(ui.cmbDirection);

	ui.cmbProtocol->addItem(tr("Any Protocol"), (quint32)EFwKnownProtocols::Any);
	for(auto I: g_KnownProtocols)
		ui.cmbProtocol->addItem(QString::fromStdWString(I.second), I.first);

	FixComboBoxEditing(ui.cmbGroup);
	FixComboBoxEditing(ui.cmbProtocol);
	FixComboBoxEditing(ui.cmbICMP);
	FixComboBoxEditing(ui.cmbLocalPorts);
	FixComboBoxEditing(ui.cmbRemotePorts);

	ui.cmbLocalIPSet->setVisible(false);
	ui.btnLocalIPSet->setVisible(false);

	ui.cmbRemoteIPSet->setVisible(false);
	ui.btnRemoteIPSet->setVisible(false);

	ui.cmbRemoteIPSet->addItem("LocalSubnet");
	ui.cmbRemoteIPSet->addItem("DNS");
	ui.cmbRemoteIPSet->addItem("DHCP");
	ui.cmbRemoteIPSet->addItem("WINS");
	ui.cmbRemoteIPSet->addItem("DefaultGateway");
	// win 8+
	ui.cmbRemoteIPSet->addItem("LocalIntranet");
	//ui.cmbRemoteIPSet->addItem("RemoteIntranet");
	ui.cmbRemoteIPSet->addItem("Internet");
	//ui.cmbRemoteIPSet->addItem("Ply2Renders");


	ui.txtName->setText(m_pRule->m_Name);
	// todo: load groups
	SetComboBoxValue(ui.cmbGroup, m_pRule->m_Grouping);
	ui.txtInfo->setPlainText(m_pRule->m_Description);

	// how long the rule lasts; the form has no row for it, so it goes in below the description
	QLabel* pLifetimeLabel = new QLabel(tr("Lifetime"));
	pLifetimeLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
	m_pCmbLifetime = new QComboBox();
	PopulateRuleLifetime(m_pCmbLifetime, m_pRule.data());
	if (m_pRule->IsTemplate())
		m_pCmbLifetime->setToolTip(tr("The rules made from this template go when it does."));
	ui.gridLayout_2->addWidget(pLifetimeLabel, 3, 0);
	ui.gridLayout_2->addWidget(m_pCmbLifetime, 3, 1, 1, 2);

	ui.cmbProgram->setEditable(true);
	ui.cmbProgram->lineEdit()->setReadOnly(true);
	CProgramItemPtr pItem;
	if (m_pRule->m_ProgramID.GetType() == EProgramType::eAllPrograms)
		pItem = theCore->ProgramManager()->GetAll();
	else if(!bNew)
		pItem = theCore->ProgramManager()->GetProgramByID(m_pRule->m_ProgramID);
	else {
		// a new rule may already know which program it is meant for, preselect that one
		foreach(const CProgramItemPtr& pCurrent, m_Items) {
			if (pCurrent->GetID() == m_pRule->m_ProgramID) {
				pItem = pCurrent;
				break;
			}
		}
	}
	int Index = (int)m_Items.indexOf(pItem);
	ui.cmbProgram->setCurrentIndex(Index);

	ui.txtPath->setText(m_pRule->GetBinaryPath());
	ui.txtService->setText(m_pRule->GetServiceTag());
	QString AppSID = m_pRule->GetAppPkgId();
	QString AppPFN = m_pRule->GetAppPFN();
	ui.txtApp->setText(AppPFN.isEmpty() ? AppSID : AppPFN);

	//ui.chkAllPrograms->blockSignals(true); // just reflecting what the rule already is
	//ui.chkAllPrograms->setChecked(m_pRule->m_ProgramID.GetType() == EProgramType::eAllPrograms);
	//ui.chkAllPrograms->blockSignals(false);
	//UpdateAllPrograms();

	SetComboBoxValue(ui.cmbAction, (uint32)m_pRule->m_Action);

	if (m_pRule->m_Profile == (int)EFwProfiles::All || m_pRule->m_Profile == (int)EFwProfiles::Invalid)
		ui.radProfileAll->setChecked(true);
	else
	{
		ui.radProfileCustom->setChecked(true);
	
		ui.chkPublic->setChecked(m_pRule->m_Profile & (int)EFwProfiles::Public);
		ui.chkDomain->setChecked(m_pRule->m_Profile & (int)EFwProfiles::Domain);
		ui.chkPrivate->setChecked(m_pRule->m_Profile & (int)EFwProfiles::Private);
	}

	if(m_pRule->m_Interface == (int)EFwInterfaces::All)
		ui.radNicAll->setChecked(true);
	else
	{
		ui.radNicCustom->setChecked(true);

		ui.chkLAN->setChecked(m_pRule->m_Interface & (int)EFwInterfaces::Lan);
		ui.chkVPN->setChecked(m_pRule->m_Interface & (int)EFwInterfaces::RemoteAccess);
		ui.chkWiFi->setChecked(m_pRule->m_Interface & (int)EFwInterfaces::Wireless);
	}

	SetComboBoxValue(ui.cmbDirection, (uint32)m_pRule->m_Direction);

	SetComboBoxValue(ui.cmbProtocol, (uint32)m_pRule->m_Protocol);

	// ICMP set by OnProtocolChanged
		
	// Ports set by OnDirectionChanged and/or OnProtocolChanged via UpdatePorts

	auto SetAddresses = [&](const QStringList& Addresses, QListWidget* pList, QRadioButton* pAny, QRadioButton* pCustom) {
		if (Addresses.isEmpty() || (Addresses.size() == 1 && Addresses[0] == ""))
			pAny->setChecked(true);
		else
		{
			pCustom->setChecked(true);
			foreach(const QString& IP, Addresses)
			{
				QListWidgetItem* pIP = new QListWidgetItem(IP);
				pIP->setData(Qt::UserRole, IP);
				pList->addItem(pIP);
			}
		}
		QListWidgetItem* pNewIP = new QListWidgetItem(tr("[Add IP]"));
		pNewIP->setData(Qt::UserRole, "");
		pList->addItem(pNewIP);
	};

	SetAddresses(m_pRule->m_LocalAddresses, ui.lstLocalIP, ui.radLocalIPAny, ui.radLocalIPCustom);
	SetAddresses(m_pRule->m_RemoteAddresses, ui.lstRemoteIP, ui.radRemoteIPAny, ui.radRemoteIPCustom);

	// Initialize User Authorization Lists
	ui.cmbApplyUsersSet->setVisible(false);
	ui.btnApplyUsersSet->setVisible(false);
	ui.cmbExemptUsersSet->setVisible(false);
	ui.btnExemptUsersSet->setVisible(false);

	AddAccountsToUserComboBox(ui.cmbApplyUsersSet);
	AddAccountsToUserComboBox(ui.cmbExemptUsersSet);

	//ParseLocalUserAuthorizationList(m_pRule->m_LocalUserAuthorizationList);
	ParseLocalUserAuthorizationList(m_pRule->m_PrincipalSddl);

	ui.cmbEdgeTraversal->addItem(tr("No"), 0);
	ui.cmbEdgeTraversal->addItem(tr("Allow"), 1);
	ui.cmbEdgeTraversal->addItem(tr("Defer to App"), 2);
	ui.cmbEdgeTraversal->addItem(tr("Defer to User"), 3);
	SetComboBoxValue(ui.cmbEdgeTraversal, (uint32)m_pRule->m_EdgeTraversal);

	OnProfilesChanged();
	OnInterfacesChanged();
	OnProtocolChanged();
	OnLocalIPChanged();
	OnRemoteIPChanged();
	OnAllowUsersChanged();
	OnExemptUsersChanged();

	m_NameHold = false;

	restoreGeometry(theConf->GetBlob("FirewallRuleWindow/Window_Geometry"));
}

CFirewallRuleWnd::~CFirewallRuleWnd()
{
	theConf->SetBlob("FirewallRuleWindow/Window_Geometry", saveGeometry());
}

void CFirewallRuleWnd::SetReadOnly(bool bReadOnly)
{
	ui.buttonBox->setEnabled(!bReadOnly);
}

bool CFirewallRuleWnd::AddProgramItem(const CProgramItemPtr& pItem)
{
	switch (pItem->GetID().GetType())
	{
	case EProgramType::eProgramFile:
	case EProgramType::eWindowsService:
	case EProgramType::eAppPackage:
	case EProgramType::eAllPrograms:
		break;
	case EProgramType::eFilePattern:
		if(theCore->GetConfigBool("Service/UseFwRuleTemplates", true))
			break;
	default:
		return false;
	}

	//
	// All programs first, the rest by name. The combo does no sorting of its own and
	// the item list runs parallel to it, so the entry goes into both at the same spot.
	// Searched rather than scanned as the caller can hand us the entire program tree.
	//
	QString Name = pItem->GetNameEx();
	int Pos = 0;
	if (pItem->GetID().GetType() != EProgramType::eAllPrograms)
	{
		int Begin = (!m_Items.isEmpty() && m_Items[0]->GetID().GetType() == EProgramType::eAllPrograms) ? 1 : 0;
		int End = m_Items.count();
		while (Begin < End)
		{
			int Mid = (Begin + End) / 2;
			if (QString::compare(ui.cmbProgram->itemText(Mid), Name, Qt::CaseInsensitive) < 0)
				Begin = Mid + 1;
			else
				End = Mid;
		}
		Pos = Begin;
	}

	m_Items.insert(Pos, pItem);
	ui.cmbProgram->insertItem(Pos, Name);
	return true;
}

QColor CFirewallRuleWnd::GetActionColor(EFwActions Action)
{
	switch (Action) {
	case EFwActions::Allow:				return QColor(144, 238, 144); 
	case EFwActions::Block:				return QColor(255, 182, 193); 
	default: return QColor();
	}
}

QColor CFirewallRuleWnd::GetDirectionColor(EFwDirections Direction)
{
	switch (Direction) {
	case EFwDirections::Inbound:		return QColor(255, 255, 128); 
	case EFwDirections::Outbound:		return QColor(173, 216, 230); 
	case EFwDirections::Bidirectional:	return QColor(255, 200, 100); 
	default: return QColor();
	}
}

void CFirewallRuleWnd::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

bool CFirewallRuleWnd::OnSave()
{
	if (!Save()) {
		QApplication::beep();
		return false;
	}

	auto Ret = theCore->NetworkManager()->SetFwRule(m_pRule);	
	if (theGUI->CheckResults(QList<STATUS>() << Ret, this))
		return false;

	if(m_pRule->m_Guid.IsNull())
		m_pRule->m_Guid = Ret.GetValue();
	return true;
}

void CFirewallRuleWnd::OnSaveAndClose()
{
	if(OnSave())
		accept();
}

bool CFirewallRuleWnd::Save()
{
	m_pRule->m_Name = ui.txtName->text();
	m_pRule->m_Grouping = GetComboBoxValue(ui.cmbGroup).toString();
	m_pRule->m_Description = ui.txtInfo->toPlainText();
	ApplyRuleLifetime(m_pRule.data(), m_pCmbLifetime->currentData().toInt()); // after the group, a temporary rule is marked in it

	int Index = ui.cmbProgram->currentIndex();
	if (Index == -1) 
		return false;

	CProgramItemPtr pItem = m_Items[Index];
	if (m_pRule->m_ProgramID != pItem->GetID()) 
	{
		CWindowsServicePtr pSvc = pItem.objectCast<CWindowsService>();
		CAppPackagePtr pApp = pItem.objectCast<CAppPackage>();
		m_pRule->m_BinaryPath = pApp ? "" : pItem->GetPath();
		m_pRule->m_ServiceTag = pSvc ? pSvc->GetServiceTag() : "";
		m_pRule->m_AppContainerSid = pApp ? pApp->GetAppSid() : "";
		m_pRule->m_PackageFamilyName = pApp ? pApp->GetPackageName() : "";
	}

	m_pRule->m_Action = (EFwActions)GetComboBoxValue(ui.cmbAction).toUInt();

	if (!ui.radProfileCustom->isChecked())
		m_pRule->m_Profile = (int)EFwProfiles::All;
	else
	{
		m_pRule->m_Profile = 0;
		if(ui.chkPublic->isChecked())
			m_pRule->m_Profile |= (int)EFwProfiles::Public;
		if(ui.chkDomain->isChecked())
			m_pRule->m_Profile |= (int)EFwProfiles::Domain;
		if(ui.chkPrivate->isChecked())
			m_pRule->m_Profile |= (int)EFwProfiles::Private;
	}

	if (!ui.radNicCustom->isChecked())
		m_pRule->m_Interface = (int)EFwInterfaces::All;
	else
	{
		m_pRule->m_Interface = 0;
		if(ui.chkLAN->isChecked())
			m_pRule->m_Interface |= (int)EFwInterfaces::Lan;
		if(ui.chkVPN->isChecked())
			m_pRule->m_Interface |= (int)EFwInterfaces::RemoteAccess;
		if(ui.chkWiFi->isChecked())
			m_pRule->m_Interface |= (int)EFwInterfaces::Wireless;
	}

	m_pRule->m_Direction = (EFwDirections)GetComboBoxValue(ui.cmbDirection).toUInt();

	m_pRule->m_Protocol = (EFwKnownProtocols)GetComboBoxValue(ui.cmbProtocol).toUInt();

	bool bICMP = (m_pRule->m_Protocol == EFwKnownProtocols::ICMP || m_pRule->m_Protocol == EFwKnownProtocols::ICMPv6);
	if (!bICMP)
		m_pRule->m_IcmpTypesAndCodes.clear();
	else
	{
		if (ui.cmbICMP->currentIndex() != -1)
			m_pRule->m_IcmpTypesAndCodes = QStringList() << ui.cmbICMP->currentData().toString();
		else
			m_pRule->m_IcmpTypesAndCodes = SplitStr(ui.cmbICMP->currentText(), ",");
	}

	bool bPorts = (m_pRule->m_Protocol == EFwKnownProtocols::TCP || m_pRule->m_Protocol == EFwKnownProtocols::UDP);

	auto GetPorts = [&](QComboBox* pBox)
	{
		if (pBox->currentIndex() != -1) {
			QStringList List;
			if (!pBox->currentData().isNull())
				List.append(pBox->currentData().toString());
			return List;
		}
		return SplitStr(pBox->currentText(), ",");
	};

	m_pRule->m_LocalPorts = GetPorts(ui.cmbLocalPorts);
	m_pRule->m_RemotePorts = GetPorts(ui.cmbRemotePorts);

	auto GetAddresses = [&](QListWidget* pList, QRadioButton* pAny, QRadioButton* pCustom) {
		if (pAny->isChecked())
			return QStringList();
		QStringList Addresses;
		for (int i = 0; i < pList->count(); i++) {
			QListWidgetItem* pItem = pList->item(i);
			QString Address = pItem->data(Qt::UserRole).toString();
			if (!Address.isEmpty())
				Addresses.append(Address);
		}
		return Addresses;
	};

	m_pRule->m_LocalAddresses = GetAddresses(ui.lstLocalIP, ui.radLocalIPAny, ui.radLocalIPCustom);
	m_pRule->m_RemoteAddresses = GetAddresses(ui.lstRemoteIP, ui.radRemoteIPAny, ui.radRemoteIPCustom);

	//m_pRule->m_LocalUserAuthorizationList = BuildLocalUserAuthorizationList();
	m_pRule->m_PrincipalSddl = BuildLocalUserAuthorizationList();

	m_pRule->m_EdgeTraversal = ui.cmbEdgeTraversal->currentData().toInt();

	return true;
}

void CFirewallRuleWnd::OnNameChanged(const QString& Text)
{
	if (m_NameHold) return;
	m_NameChanged = true;
}

void CFirewallRuleWnd::OnPickProgram()
{
	int Index = ui.cmbProgram->currentIndex();
	CProgramItemPtr pItem = Index != -1 ? m_Items[Index] : nullptr;
	CProgramPicker Picker(pItem, m_Items, this);
	if (theGUI->SafeExec(&Picker)) {
		pItem = Picker.GetProgram();
		Index = m_Items.indexOf(pItem);
		if (Index == -1) {
			if(!AddProgramItem(pItem))
				QMessageBox::warning(this, "MajorPrivacy", tr("The selected program type is not supported for this rule type"));
			else
				Index = m_Items.indexOf(pItem);
		}
		if (Index != -1) {
			ui.cmbProgram->setCurrentIndex(Index);
			OnProgramChanged();
		}
	}
}

//
// The all programs item sits in the combo like any other, the box is only a shortcut
// to it. With it picked there is nothing left to choose - the rule matches every
// program - so the picker and the fields it fills go dead.
//
/*void CFirewallRuleWnd::UpdateAllPrograms()
{
	bool bAll = ui.chkAllPrograms->isChecked();

	ui.cmbProgram->setEnabled(!bAll);
	ui.btnProg->setEnabled(!bAll);
	ui.txtPath->setEnabled(!bAll);
	ui.txtService->setEnabled(!bAll);
	ui.txtApp->setEnabled(!bAll);
}

void CFirewallRuleWnd::OnAllPrograms()
{
	if (ui.chkAllPrograms->isChecked())
	{
		CProgramItemPtr pAll = theCore->ProgramManager()->GetAll();
		int Index = m_Items.indexOf(pAll);
		if (Index == -1 && AddProgramItem(pAll))
			Index = m_Items.indexOf(pAll);

		m_PrevProgram = ui.cmbProgram->currentIndex(); // after the insert, which shifts the indices
		if (Index != -1) {
			ui.cmbProgram->setCurrentIndex(Index);
			OnProgramChanged();
		}
	}
	else
	{
		//
		// Back to whatever was picked before, which for a new rule is nothing.
		//
		ui.cmbProgram->setCurrentIndex(m_PrevProgram);
		if (m_PrevProgram != -1)
			OnProgramChanged();
		else {
			ui.txtPath->clear();
			ui.txtService->clear();
			ui.txtApp->clear();
		}
		m_PrevProgram = -1;
	}

	UpdateAllPrograms();
}*/

void CFirewallRuleWnd::OnProgramChanged()
{
	int Index = ui.cmbProgram->currentIndex();
	if (Index == -1) return;

	CProgramItemPtr pItem = m_Items[Index];
	//if (pItem) ui.cmbProgram->setCurrentText(pItem->GetName());
	//CProgramID ID = pItem->GetID();

	//ui.chkAllPrograms->blockSignals(true); // the box follows the picked item
	//ui.chkAllPrograms->setChecked(pItem->GetID().GetType() == EProgramType::eAllPrograms);
	//ui.chkAllPrograms->blockSignals(false);
	//UpdateAllPrograms();

	CWindowsServicePtr pSvc = pItem.objectCast<CWindowsService>();
	CAppPackagePtr pApp = pItem.objectCast<CAppPackage>();
	ui.txtPath->setText(pApp ? "" : pItem->GetPath());
	ui.txtService->setText(pSvc ? pSvc->GetServiceTag() : "");
	//ui.txtService->setToolTip(pSvc ? pSvc->GetName() : "");
	if (pApp) {
		QString AppSID = pApp->GetAppSid() ;
		QString AppPFN = pApp->GetPackageName();
		ui.txtApp->setText(AppPFN.isEmpty() ? AppSID : AppPFN);
	} else
		ui.txtApp->clear();

	TryMakeName();
}

void CFirewallRuleWnd::OnProfilesChanged()
{
	ui.chkPublic->setEnabled(ui.radProfileCustom->isChecked());
	ui.chkDomain->setEnabled(ui.radProfileCustom->isChecked());
	ui.chkPrivate->setEnabled(ui.radProfileCustom->isChecked());
}

void CFirewallRuleWnd::OnInterfacesChanged()
{
	ui.chkLAN->setEnabled(ui.radNicCustom->isChecked());
	ui.chkVPN->setEnabled(ui.radNicCustom->isChecked());
	ui.chkWiFi->setEnabled(ui.radNicCustom->isChecked());
}

void CFirewallRuleWnd::OnDirectionChanged()
{
	EFwKnownProtocols Protocol = (EFwKnownProtocols)GetComboBoxValue(ui.cmbProtocol).toUInt();
	bool bPorts = (Protocol == EFwKnownProtocols::TCP || Protocol == EFwKnownProtocols::UDP);

	if(bPorts)
		UpdatePorts();

	TryMakeName();
}

void CFirewallRuleWnd::OnProtocolChanged()
{
	quint32 Protocol = GetComboBoxValue(ui.cmbProtocol).toUInt();

	bool bICMP = (Protocol == (quint32)EFwKnownProtocols::ICMP || Protocol == (quint32)EFwKnownProtocols::ICMPv6);
	bool bPorts = (Protocol == (quint32)EFwKnownProtocols::TCP || Protocol == (quint32)EFwKnownProtocols::UDP);

	ui.lblICMP->setVisible(bICMP);
	ui.cmbICMP->setVisible(bICMP);
	ui.lblLocalPorts->setVisible(bPorts);
	ui.cmbLocalPorts->setVisible(bPorts);
	ui.cmbRemotePorts->setVisible(bPorts);
	ui.lblRemotePorts->setVisible(bPorts);

	if (bICMP)
	{
		ui.cmbICMP->clear();
		ui.cmbICMP->addItem(tr("All Types"), "*");
		for(auto I: (Protocol == (quint32)EFwKnownProtocols::ICMPv6) ? g_KnownIcmp6Types : g_KnownIcmp4Types)
			ui.cmbICMP->addItem(QString("%1:* (%2)").arg(I.first).arg(QString::fromStdWString(I.second)), QString("%1:*").arg(I.first));
		ui.cmbICMP->addItem("3:4 (Type 3, Code 4)", "3:4");

		int Pos = m_pRule->m_IcmpTypesAndCodes.count() == 1 ? ui.cmbICMP->findData(m_pRule->m_IcmpTypesAndCodes[0]) : -1;
		ui.cmbICMP->setCurrentIndex(Pos);
		if (Pos == -1)
			ui.cmbICMP->setCurrentText(m_pRule->m_IcmpTypesAndCodes.join(", "));
	}

	if(bPorts)
		UpdatePorts();

	TryMakeName();
}

void CFirewallRuleWnd::UpdatePorts()
{
	ui.cmbLocalPorts->clear();
	ui.cmbRemotePorts->clear();

	EFwDirections Direction = (EFwDirections)ui.cmbDirection->currentData().toUInt();
	EFwKnownProtocols Protocol = (EFwKnownProtocols)GetComboBoxValue(ui.cmbProtocol).toUInt();

	ui.cmbLocalPorts->addItem("Any Port");
	ui.cmbRemotePorts->addItem("Any Port");

	if (Direction == EFwDirections::Outbound)
	{
		if (Protocol == EFwKnownProtocols::TCP)
		{
			ui.cmbRemotePorts->addItem("IPHTTPSOut", "IPHTTPSOut");
		}
	}

	if (Direction == EFwDirections::Inbound)
	{
		if (Protocol == EFwKnownProtocols::TCP)
		{
			ui.cmbLocalPorts->addItem("IPHTTPSIn", "IPHTTPSIn");
			ui.cmbLocalPorts->addItem("RPC-EPMap", "RPC-EPMap");
			ui.cmbLocalPorts->addItem("RPC", "RPC");
		}
		else if (Protocol == EFwKnownProtocols::UDP)
		{
			ui.cmbLocalPorts->addItem("Teredo", "Teredo");
			ui.cmbLocalPorts->addItem("Ply2Disc", "Ply2Disc");
			ui.cmbLocalPorts->addItem("mDNS", "mDNS");
			ui.cmbLocalPorts->addItem("DHCP", "DHCP");
		}
	}

	auto SetPorts = [&](const QStringList& Ports, QComboBox* pBox) {
		int Pos;
		if (Ports.isEmpty() || (Ports.size() == 1 && Ports[0] == ""))
			Pos = 0; // Any Port
		else
			Pos = Ports.count() == 1 ? pBox->findData(Ports[0]) : -1;
		pBox->setCurrentIndex(Pos);
		if (Pos == -1)
			pBox->setCurrentText(Ports.join(", "));
	};

	SetPorts(m_pRule->m_LocalPorts, ui.cmbLocalPorts);
	SetPorts(m_pRule->m_RemotePorts, ui.cmbRemotePorts);
}

void CFirewallRuleWnd::OnLocalIPChanged()
{
	ui.btnLocalIPSet->setEnabled(ui.radLocalIPCustom->isChecked());
	ui.lstLocalIP->setEnabled(ui.radLocalIPCustom->isChecked());
}

void CFirewallRuleWnd::OnLocalIPEdit()
{
	ui.lstLocalIP->setEnabled(false);
	QListWidgetItem* pItem = ui.lstLocalIP->currentItem();
	ui.cmbLocalIPSet->setCurrentText(pItem->data(Qt::UserRole).toString());
	ui.cmbLocalIPSet->setVisible(true);
	ui.btnLocalIPSet->setVisible(true);
}

void CFirewallRuleWnd::OnLocalIPSet()
{
	QString Address = ui.cmbLocalIPSet->currentText();
	if (!Address.isEmpty() && !ValidateAddress(Address, false))
		return;

	QListWidgetItem* pItem = ui.lstLocalIP->currentItem();
	bool bAdd = pItem->data(Qt::UserRole).toString() == "";
	if (Address.isEmpty())
	{
		if(!bAdd) delete pItem;
		pItem = NULL;
	}
	else if (bAdd) {
		pItem = new QListWidgetItem();
		ui.lstLocalIP->insertItem(ui.lstLocalIP->count() - 1, pItem);
	}
	if (pItem) {
		pItem->setText(Address);
		pItem->setData(Qt::UserRole, Address);
	}

	ui.lstLocalIP->setEnabled(true);
	ui.cmbLocalIPSet->setVisible(false);
	ui.btnLocalIPSet->setVisible(false);
}

void CFirewallRuleWnd::OnRemoteIPChanged()
{
	ui.btnRemoteIPSet->setEnabled(ui.radRemoteIPCustom->isChecked());
	ui.lstRemoteIP->setEnabled(ui.radRemoteIPCustom->isChecked());
}

void CFirewallRuleWnd::OnRemoteIPEdit()
{
	ui.lstRemoteIP->setEnabled(false);
	QListWidgetItem* pItem = ui.lstRemoteIP->currentItem();
	ui.cmbRemoteIPSet->setCurrentText(pItem->data(Qt::UserRole).toString());
	ui.cmbRemoteIPSet->setVisible(true);
	ui.btnRemoteIPSet->setVisible(true);
}

void CFirewallRuleWnd::OnRemoteIPSet()
{
	QString Address = ui.cmbRemoteIPSet->currentText();
	if (!Address.isEmpty() && !ValidateAddress(Address, true))
		return;

	QListWidgetItem* pItem = ui.lstRemoteIP->currentItem();
	bool bAdd = pItem->data(Qt::UserRole).toString() == "";
	if (Address.isEmpty())
	{
		if(!bAdd) delete pItem;
		pItem = NULL;
	}
	else if (bAdd) {
		pItem = new QListWidgetItem();
		ui.lstRemoteIP->insertItem(ui.lstRemoteIP->count() - 1, pItem);
	}
	if (pItem) {
		pItem->setText(Address);
		pItem->setData(Qt::UserRole, Address);
	}

	ui.lstRemoteIP->setEnabled(true);
	ui.cmbRemoteIPSet->setVisible(false);
	ui.btnRemoteIPSet->setVisible(false);
}

bool CFirewallRuleWnd::ValidateAddress(const QString& Address, bool bRemote)
{
	QStringList BeginEnd = SplitStr(Address, "-");
	if (BeginEnd.size() == 1) // singel IP
	{
		if (bRemote)
		{
			if (Address == "LocalSubnet" ||
				Address == "DNS" ||
				Address == "DHCP" ||
				Address == "WINS" ||
				Address == "DefaultGateway" ||
				// win 8+
				Address == "LocalIntranet" ||
				//Address == "RemoteIntranet" ||
				Address == "Internet" // ||
				//Address == "Ply2Renders" ||
			  )
				return true;
		}

		QHostAddress IP;

		if (Address.contains("/")) // ip/net
		{
			QStringList IpNet = SplitStr(Address, "/");
			if(IpNet.size() != 2) {
				QMessageBox::critical(this, "MajorPrivacy", tr("Invalid Sub Net"));
				return false;
			}

			// todo text subnet
		}
		else
			IP = QHostAddress(Address);

		if (IP.isNull()) {
			QMessageBox::critical(this, "MajorPrivacy", tr("Invalid IP Address"));
			return false;
		}
	}
	else if (BeginEnd.size() == 2) // range
	{
		QHostAddress BeginIP(BeginEnd[0]);
		QHostAddress EndIP(BeginEnd[1]);
		if (BeginIP.isNull() || EndIP.isNull()) {
			QMessageBox::critical(this, "MajorPrivacy", tr("Invalid IP Address"));
			return false;
		}
		if(BeginIP < EndIP || BeginIP.protocol() != EndIP.protocol()) {
			QMessageBox::critical(this, "MajorPrivacy", tr("Invalid IP Range"));
			return false;
		}
	}
	else {
		QMessageBox::critical(this, "MajorPrivacy", tr("Invalid IP Range"));
		return false;
	}
	return true;
}

void CFirewallRuleWnd::OnActionChanged()
{
	TryMakeName();
}

QString CFirewallRuleWnd::SidToUserName(const QString& Sid)
{
	QByteArray sidBinary = CGenericRule::SidStringToBinary(Sid);
	if (sidBinary.isEmpty())
		return Sid;

	QString userName = theCore->GetSidResolver()->GetSidFullName(sidBinary, this, SLOT(OnSidResolved(const QByteArray&, const QString&)));
	return userName.isEmpty() ? Sid : userName;
}

void CFirewallRuleWnd::ParseLocalUserAuthorizationList(const QString& SDDL)
{
	ui.lstApplyUsers->clear();
	ui.lstExemptUsers->clear();

	if (SDDL.isEmpty()) {
		ui.chkAllowUsers->setChecked(false);
		ui.chkExcemptUsers->setChecked(false);
	}
	else {
		// Parse SDDL format: O:LSD:(D;;CC;;;SID1)(A;;CC;;;SID2)...
		// Find the start of ACEs (after "D:")
		int dPos = SDDL.indexOf("D:");
		if (dPos == -1) {
			ui.chkAllowUsers->setChecked(false);
			ui.chkExcemptUsers->setChecked(false);
		}
		else {
			QString acesPart = SDDL.mid(dPos + 2);

			bool hasApplyUsers = false;
			bool hasExemptUsers = false;

			// Parse each ACE: (type;;rights;;;SID)
			QRegularExpression aceRx("\\(([AD]);;[^;]*;;;([^)]+)\\)");
			QRegularExpressionMatchIterator i = aceRx.globalMatch(acesPart);
			while (i.hasNext()) {
				QRegularExpressionMatch match = i.next();
				QString aceType = match.captured(1);
				QString sid = match.captured(2);
				QString userName = SidToUserName(sid);

				if (aceType == "A") {
					// Allow - rule applies to this user
					hasApplyUsers = true;
					QListWidgetItem* pItem = new QListWidgetItem(userName);
					pItem->setData(Qt::UserRole, sid);
					ui.lstApplyUsers->addItem(pItem);
				}
				else if (aceType == "D") {
					// Deny - user is exempt
					hasExemptUsers = true;
					QListWidgetItem* pItem = new QListWidgetItem(userName);
					pItem->setData(Qt::UserRole, sid);
					ui.lstExemptUsers->addItem(pItem);
				}
			}

			ui.chkAllowUsers->setChecked(hasApplyUsers);
			ui.chkExcemptUsers->setChecked(hasExemptUsers);
		}
	}

	// Add "Add User" placeholder items
	QListWidgetItem* pNewApply = new QListWidgetItem(tr("[Add User]"));
	pNewApply->setData(Qt::UserRole, "");
	ui.lstApplyUsers->addItem(pNewApply);

	QListWidgetItem* pNewExempt = new QListWidgetItem(tr("[Add User]"));
	pNewExempt->setData(Qt::UserRole, "");
	ui.lstExemptUsers->addItem(pNewExempt);
}

QString CFirewallRuleWnd::BuildLocalUserAuthorizationList()
{
	if (!ui.chkAllowUsers->isChecked() && !ui.chkExcemptUsers->isChecked())
		return QString();

	QString sddl = "O:LSD:";

	// Add Deny (exempt) entries first
	if (ui.chkExcemptUsers->isChecked()) {
		for (int i = 0; i < ui.lstExemptUsers->count(); i++) {
			QListWidgetItem* pItem = ui.lstExemptUsers->item(i);
			QString sid = pItem->data(Qt::UserRole).toString();
			if (!sid.isEmpty())
				sddl += QString("(D;;CC;;;%1)").arg(sid);
		}
	}

	// Add Allow (apply) entries
	if (ui.chkAllowUsers->isChecked()) {
		for (int i = 0; i < ui.lstApplyUsers->count(); i++) {
			QListWidgetItem* pItem = ui.lstApplyUsers->item(i);
			QString sid = pItem->data(Qt::UserRole).toString();
			if (!sid.isEmpty())
				sddl += QString("(A;;CC;;;%1)").arg(sid);
		}
	}

	// If no actual entries were added, return empty
	if (sddl == "O:LSD:")
		return QString();

	return sddl;
}

void CFirewallRuleWnd::OnAllowUsersChanged()
{
	ui.lstApplyUsers->setEnabled(ui.chkAllowUsers->isChecked());
}

void CFirewallRuleWnd::OnApplyUsersEdit()
{
	ui.lstApplyUsers->setEnabled(false);
	QListWidgetItem* pItem = ui.lstApplyUsers->currentItem();
	if (pItem->data(Qt::UserRole).toString().isEmpty())
		ui.cmbApplyUsersSet->setCurrentText("");
	else {
		// Try to find matching item in combo by SID
		QString sid = pItem->data(Qt::UserRole).toString();
		int index = ui.cmbApplyUsersSet->findData(sid);
		if (index != -1)
			ui.cmbApplyUsersSet->setCurrentIndex(index);
		else
			ui.cmbApplyUsersSet->setCurrentText(pItem->text());
	}
	ui.cmbApplyUsersSet->setVisible(true);
	ui.btnApplyUsersSet->setVisible(true);
}

void CFirewallRuleWnd::OnApplyUsersSet()
{
	QString userName = ui.cmbApplyUsersSet->currentText();

	QListWidgetItem* pItem = ui.lstApplyUsers->currentItem();
	bool bAdd = pItem->data(Qt::UserRole).toString() == "";

	if (userName.isEmpty())
	{
		if (!bAdd) delete pItem;
		pItem = NULL;
	}
	else
	{
		QString sid;

		// Check if an item is selected from the combo box (has pre-resolved SID)
		int comboIndex = ui.cmbApplyUsersSet->currentIndex();
		if (comboIndex != -1) {
			sid = ui.cmbApplyUsersSet->currentData().toString();
			// If data starts with S- it's already a SID, otherwise it's a name that needs resolution
			if (!sid.startsWith("S-"))
				sid.clear();
		}

		// If no pre-resolved SID, resolve username to SID
		if (sid.isEmpty()) {
			sid = ResolveNameToSid(userName);
		}

		if (sid.isEmpty()) {
			QMessageBox::critical(this, "MajorPrivacy", tr("Could not resolve user '%1' to SID").arg(userName));
			ui.lstApplyUsers->setEnabled(true);
			ui.cmbApplyUsersSet->setVisible(false);
			ui.btnApplyUsersSet->setVisible(false);
			return;
		}

		if (bAdd) {
			pItem = new QListWidgetItem();
			ui.lstApplyUsers->insertItem(ui.lstApplyUsers->count() - 1, pItem);
		}
		pItem->setText(userName);
		pItem->setData(Qt::UserRole, sid);
	}

	ui.lstApplyUsers->setEnabled(true);
	ui.cmbApplyUsersSet->setVisible(false);
	ui.btnApplyUsersSet->setVisible(false);
}

void CFirewallRuleWnd::OnExemptUsersChanged()
{
	ui.lstExemptUsers->setEnabled(ui.chkExcemptUsers->isChecked());
}

void CFirewallRuleWnd::OnExemptUsersEdit()
{
	ui.lstExemptUsers->setEnabled(false);
	QListWidgetItem* pItem = ui.lstExemptUsers->currentItem();
	if (pItem->data(Qt::UserRole).toString().isEmpty())
		ui.cmbExemptUsersSet->setCurrentText("");
	else {
		// Try to find matching item in combo by SID
		QString sid = pItem->data(Qt::UserRole).toString();
		int index = ui.cmbExemptUsersSet->findData(sid);
		if (index != -1)
			ui.cmbExemptUsersSet->setCurrentIndex(index);
		else
			ui.cmbExemptUsersSet->setCurrentText(pItem->text());
	}
	ui.cmbExemptUsersSet->setVisible(true);
	ui.btnExemptUsersSet->setVisible(true);
}

void CFirewallRuleWnd::OnExemptUsersSet()
{
	QString userName = ui.cmbExemptUsersSet->currentText();

	QListWidgetItem* pItem = ui.lstExemptUsers->currentItem();
	bool bAdd = pItem->data(Qt::UserRole).toString() == "";

	if (userName.isEmpty())
	{
		if (!bAdd) delete pItem;
		pItem = NULL;
	}
	else
	{
		QString sid;

		// Check if an item is selected from the combo box (has pre-resolved SID)
		int comboIndex = ui.cmbExemptUsersSet->currentIndex();
		if (comboIndex != -1) {
			sid = ui.cmbExemptUsersSet->currentData().toString();
			// If data starts with S- it's already a SID, otherwise it's a name that needs resolution
			if (!sid.startsWith("S-"))
				sid.clear();
		}

		// If no pre-resolved SID, resolve username to SID
		if (sid.isEmpty()) {
			sid = ResolveNameToSid(userName);
		}

		if (sid.isEmpty()) {
			QMessageBox::critical(this, "MajorPrivacy", tr("Could not resolve user '%1' to SID").arg(userName));
			ui.lstExemptUsers->setEnabled(true);
			ui.cmbExemptUsersSet->setVisible(false);
			ui.btnExemptUsersSet->setVisible(false);
			return;
		}

		if (bAdd) {
			pItem = new QListWidgetItem();
			ui.lstExemptUsers->insertItem(ui.lstExemptUsers->count() - 1, pItem);
		}
		pItem->setText(userName);
		pItem->setData(Qt::UserRole, sid);
	}

	ui.lstExemptUsers->setEnabled(true);
	ui.cmbExemptUsersSet->setVisible(false);
	ui.btnExemptUsersSet->setVisible(false);
}

void CFirewallRuleWnd::OnSidResolved(const QByteArray& Sid, const QString& FullName)
{
	// Convert binary SID back to string for comparison
	LPWSTR sidStr = nullptr;
	if (!ConvertSidToStringSidW((PSID)Sid.data(), &sidStr))
		return;
	QString sidString = QString::fromWCharArray(sidStr);
	LocalFree(sidStr);

	// Update any list items that have this SID
	auto UpdateList = [&](QListWidget* pList) {
		for (int i = 0; i < pList->count(); i++) {
			QListWidgetItem* pItem = pList->item(i);
			if (pItem->data(Qt::UserRole).toString() == sidString) {
				pItem->setText(FullName);
			}
		}
	};

	UpdateList(ui.lstApplyUsers);
	UpdateList(ui.lstExemptUsers);
}

void CFirewallRuleWnd::TryMakeName()
{
	if (ui.txtName->text().isEmpty())
		m_NameChanged = false;
	if (m_NameHold || m_NameChanged)
		return;
	
	QString Path = ui.txtPath->text();
	QString Action = ui.cmbAction->currentText();
	QString Program = ui.cmbProgram->currentText();
	//QString Direction = ui.cmbDirection->currentText();
	QString Protocol = ui.cmbProtocol->currentText();
	if (Action.isEmpty() && Program.isEmpty())
		return;

	m_NameHold = true;
	ui.txtName->setText(tr("%4, %1 %2 %3").arg(Action).arg(Program).arg(Protocol).arg(Path.contains(QRegularExpression("[*?<>|]")) ? "MajorPrivacy-Template" : "MajorPrivacy-Rule"));
	m_NameHold = false;
}