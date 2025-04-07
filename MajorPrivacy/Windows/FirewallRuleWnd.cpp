#include "pch.h"
#include "FirewallRuleWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Network/NetworkManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NetUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"
#include "../Windows/ProgramPicker.h"

CFirewallRuleWnd::CFirewallRuleWnd(const CFwRulePtr& pRule, QSet<CProgramItemPtr> Items, QWidget* parent)
	: QDialog(parent)
{
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
		m_pRule->m_Name = tr("&MP-Rule, New Firewall Rule");
	} else
		m_NameChanged = true;

	connect(ui.txtName, SIGNAL(textChanged(const QString&)), this, SLOT(OnNameChanged(const QString&)));

	connect(ui.btnProg, SIGNAL(clicked()), this, SLOT(OnPickProgram()));
	connect(ui.cmbProgram, SIGNAL(currentIndexChanged(int)), this, SLOT(OnProgramChanged()));

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

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

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

	ui.cmbProgram->setEditable(true);
	ui.cmbProgram->lineEdit()->setReadOnly(true);
	CProgramItemPtr pItem;
	if (m_pRule->m_ProgramID.GetType() == EProgramType::eAllPrograms)
		pItem = theCore->ProgramManager()->GetAll();
	else if(!bNew)
		pItem = theCore->ProgramManager()->GetProgramByID(m_pRule->m_ProgramID);
	int Index = m_Items.indexOf(pItem);
	ui.cmbProgram->setCurrentIndex(Index);

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

	OnProfilesChanged();
	OnInterfacesChanged();
	OnProtocolChanged();
	OnLocalIPChanged();
	OnRemoteIPChanged();

	m_NameHold = false;
}

CFirewallRuleWnd::~CFirewallRuleWnd()
{
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
		if(theCore->GetConfigBool("Service/UseFwRuleTemplates", false))
			break;
	default:
		return false;
	}

	m_Items.append(pItem);
	ui.cmbProgram->addItem(pItem->GetNameEx());
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

void CFirewallRuleWnd::OnSaveAndClose()
{
	if (!Save()) {
		QApplication::beep();
		return;
	}

	STATUS Status = theCore->NetworkManager()->SetFwRule(m_pRule);	
	if (theGUI->CheckResults(QList<STATUS>() << Status, this))
		return;
	accept();
}

bool CFirewallRuleWnd::Save()
{
	m_pRule->m_Name = ui.txtName->text();
	m_pRule->m_Grouping = GetComboBoxValue(ui.cmbGroup).toString();
	m_pRule->m_Description = ui.txtInfo->toPlainText();

	int Index = ui.cmbProgram->currentIndex();
	if (Index == -1) 
		return false;

	CProgramItemPtr pItem = m_Items[Index];
	if (m_pRule->m_ProgramID != pItem->GetID()) {
		m_pRule->m_BinaryPath = pItem->GetPath();
		CWindowsServicePtr pSvc = pItem.objectCast<CWindowsService>();
		m_pRule->m_ServiceTag = pSvc ? pSvc->GetServiceTag() : "";
		CAppPackagePtr pApp = pItem.objectCast<CAppPackage>();
		m_pRule->m_AppContainerSid = pApp ? pApp->GetAppSid() : "";
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
				QMessageBox::warning(this, "MajorPrivacy", tr("The sellected program type is not supported for this rule type"));
			else
				Index = m_Items.indexOf(pItem);
		}
		if (Index != -1) {
			ui.cmbProgram->setCurrentIndex(Index);
			OnProgramChanged();
		}
	}
}

void CFirewallRuleWnd::OnProgramChanged()
{
	int Index = ui.cmbProgram->currentIndex();
	if (Index == -1) return;

	CProgramItemPtr pItem = m_Items[Index];
	//if (pItem) ui.cmbProgram->setCurrentText(pItem->GetName());
	CProgramID ID = pItem->GetID();
	ui.txtPath->setText(pItem->GetPath());
	CWindowsServicePtr pSvc = pItem.objectCast<CWindowsService>();
	ui.txtService->setText(pSvc ? pSvc->GetServiceTag() : "");
	//ui.txtService->setToolTip(pSvc ? pSvc->GetName() : "");
	CAppPackagePtr pApp = pItem.objectCast<CAppPackage>();
	ui.txtApp->setText(pApp ? pApp->GetAppSid() : "");
	//ui.txtApp->setToolTip(pApp ? pApp->GetContainerName() : "");

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
	ui.txtName->setText(tr("%4, %1 %2 %3").arg(Action).arg(Program).arg(Protocol).arg(Path.contains(QRegExp("[*?<>|]")) ? "&MP-Template" : "&MP-Rule"));
	m_NameHold = false;
}