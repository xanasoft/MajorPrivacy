#include "pch.h"
#include "PresetProperties.h"
#include "../Models/PresetItemsModel.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Presets/PresetManager.h"
#include "../Core/Network/NetworkManager.h"
#include "../Core/Access/AccessManager.h"
#include "../Core/Tweaks/TweakManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../MajorPrivacy.h"
#include "../Windows/ItemPicker.h"
#include "../Windows/ProgramRuleWnd.h"
#include "../Windows/AccessRuleWnd.h"
#include "../Windows/FirewallRuleWnd.h"
#include "../MiscHelpers/Common/OtherFunctions.h"

CPresetProperties::CPresetProperties(QWidget *parent)
	: QWidget(parent)
{
	m_pMainLayout = new QGridLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pToolBar = new QToolBar();
	m_pMainLayout->addWidget(m_pToolBar, 0, 0);

	m_pBtnAddExec = new QToolButton();
	m_pBtnAddExec->setIcon(IconAddOverlay(QIcon(":/Icons/Process.png"), ":/Icons/Add.png", 16));
	m_pBtnAddExec->setToolTip(tr("Add Program Rule"));
	m_pBtnAddExec->setMaximumHeight(22);
	connect(m_pBtnAddExec, SIGNAL(clicked()), this, SLOT(AddExecRule()));
	m_pToolBar->addWidget(m_pBtnAddExec);

	m_pBtnAddRes = new QToolButton();
	m_pBtnAddRes->setIcon(IconAddOverlay(QIcon(":/Icons/Ampel.png"), ":/Icons/Add.png", 16));
	m_pBtnAddRes->setToolTip(tr("Add Resource Rule"));
	m_pBtnAddRes->setMaximumHeight(22);
	connect(m_pBtnAddRes, SIGNAL(clicked()), this, SLOT(AddResRule()));
	m_pToolBar->addWidget(m_pBtnAddRes);

	m_pBtnAddFw = new QToolButton();
	m_pBtnAddFw->setIcon(IconAddOverlay(QIcon(":/Icons/Wall3.png"), ":/Icons/Add.png", 16));
	m_pBtnAddFw->setToolTip(tr("Add Firewall Rule"));
	m_pBtnAddFw->setMaximumHeight(22);
	connect(m_pBtnAddFw, SIGNAL(clicked()), this, SLOT(AddFwRule()));
	m_pToolBar->addWidget(m_pBtnAddFw);

	m_pBtnAddDns = new QToolButton();
	m_pBtnAddDns->setIcon(IconAddOverlay(QIcon(":/Icons/Network2.png"), ":/Icons/Add.png", 16));
	m_pBtnAddDns->setToolTip(tr("Add DNS Rule"));
	m_pBtnAddDns->setMaximumHeight(22);
	connect(m_pBtnAddDns, SIGNAL(clicked()), this, SLOT(AddDnsRule()));
	m_pToolBar->addWidget(m_pBtnAddDns);

	m_pBtnAddCfg = new QToolButton();
	m_pBtnAddCfg->setIcon(IconAddOverlay(QIcon(":/Icons/Tweaks.png"), ":/Icons/Add.png", 16));
	m_pBtnAddCfg->setToolTip(tr("Add Tweak"));
	m_pBtnAddCfg->setMaximumHeight(22);
	connect(m_pBtnAddCfg, SIGNAL(clicked()), this, SLOT(AddTweak()));
	m_pToolBar->addWidget(m_pBtnAddCfg);

	//m_pToolBar->addSeparator();

	//m_pBtnRemove = new QToolButton();
	//m_pBtnRemove->setIcon(QIcon(":/Icons/Remove.png"));
	//m_pBtnRemove->setToolTip(tr("Remove Item"));
	//m_pBtnRemove->setMaximumHeight(22);
	//connect(m_pBtnRemove, SIGNAL(clicked()), this, SLOT(RemoveSelectedItems()));
	//m_pToolBar->addWidget(m_pBtnRemove);

	//m_pBtnEnable = new QToolButton();
	//m_pBtnEnable->setIcon(QIcon(":/Icons/Enable.png"));
	//m_pBtnEnable->setToolTip(tr("Enable"));
	//m_pBtnEnable->setMaximumHeight(22);
	//connect(m_pBtnEnable, SIGNAL(clicked()), this, SLOT(EnableSelectedItems()));
	//m_pToolBar->addWidget(m_pBtnEnable);

	//m_pBtnDisable = new QToolButton();
	//m_pBtnDisable->setIcon(QIcon(":/Icons/Disable.png"));
	//m_pBtnDisable->setToolTip(tr("Disable"));
	//m_pBtnDisable->setMaximumHeight(22);
	//connect(m_pBtnDisable, SIGNAL(clicked()), this, SLOT(DisableSelectedItems()));
	//m_pToolBar->addWidget(m_pBtnDisable);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_pToolBar->addWidget(pSpacer);

	m_pGroup = new QGroupBox(tr("Rules and Tweaks"));
	m_pMainLayout->addWidget(m_pGroup, 1, 0);

	QGridLayout* pGroupLayout = new QGridLayout(m_pGroup);

	m_pItemsModel = new CPresetItemsModel(this);

	m_pSortProxy = new CSortFilterProxyModel(this);
	m_pSortProxy->setSortRole(Qt::EditRole);
	m_pSortProxy->setSourceModel(m_pItemsModel);
	m_pSortProxy->setDynamicSortFilter(true);

	m_pTreeItems = new QTreeViewEx();
	m_pTreeItems->setModel(m_pSortProxy);
	m_pTreeItems->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeItems->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeItems->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	m_pTreeItems->setRootIsDecorated(false);
	pGroupLayout->addWidget(m_pTreeItems, 0, 0);

	m_pFinder = new CFinder(m_pSortProxy, m_pTreeItems, CFinder::eRegExp | CFinder::eCaseSens);
	pGroupLayout->addWidget(m_pFinder, 1, 0);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	connect(m_pTreeItems->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
		this, SLOT(OnSelectionChanged()));
	connect(m_pTreeItems, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(OnMenu(const QPoint&)));
	connect(m_pTreeItems, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));

	// Setup context menu
	m_pMenu = new QMenu(this);

	m_pAddMenu = m_pMenu->addMenu(QIcon(":/Icons/Add.png"), tr("Add"));
	m_pAddExecAction = m_pAddMenu->addAction(QIcon(":/Icons/Process.png"), tr("Program Rule"), this, SLOT(AddExecRule()));
	m_pAddResAction = m_pAddMenu->addAction(QIcon(":/Icons/Ampel.png"), tr("Resource Rule"), this, SLOT(AddResRule()));
	m_pAddFwAction = m_pAddMenu->addAction(QIcon(":/Icons/Wall3.png"), tr("Firewall Rule"), this, SLOT(AddFwRule()));
	m_pAddDnsAction = m_pAddMenu->addAction(QIcon(":/Icons/Network2.png"), tr("DNS Rule"), this, SLOT(AddDnsRule()));
	m_pAddTweakAction = m_pAddMenu->addAction(QIcon(":/Icons/Tweaks.png"), tr("Tweak"), this, SLOT(AddTweak()));

	m_pMenu->addSeparator();

	m_pRemoveAction = m_pMenu->addAction(QIcon(":/Icons/Remove.png"), tr("Remove"), this, SLOT(RemoveSelectedItems()));

	m_pMenu->addSeparator();

	m_pSetEnableAction = m_pMenu->addAction(QIcon(":/Icons/Enable.png"), tr("Set Enabling"), this, SLOT(EnableSelectedItems()));
	m_pSetDisableAction = m_pMenu->addAction(QIcon(":/Icons/Disable.png"), tr("Set Disabling"), this, SLOT(DisableSelectedItems()));

	m_pMenu->addSeparator();

	m_pActivateAction = m_pMenu->addAction(QIcon(":/Icons/Check.png"), tr("Activate"), this, SLOT(ActivateSelectedItems()));
	m_pDeactivateAction = m_pMenu->addAction(QIcon(":/Icons/Error.png"), tr("Deactivate"), this, SLOT(DeactivateSelectedItems()));

	m_pMenu->addSeparator();

	m_pRuleMenu = m_pMenu->addMenu(QIcon(":/Icons/EditIni.png"), tr("Item"));
	m_pEditRuleAction = m_pRuleMenu->addAction(QIcon(":/Icons/EditIni.png"), tr("Edit"), this, SLOT(OnEditRule()));
	m_pRuleMenu->addSeparator();
	m_pRuleEnableAction = m_pRuleMenu->addAction(QIcon(":/Icons/Enable.png"), tr("Enable"), this, SLOT(OnRuleEnable()));
	m_pRuleDisableAction = m_pRuleMenu->addAction(QIcon(":/Icons/Disable.png"), tr("Disable"), this, SLOT(OnRuleDisable()));

	QByteArray Columns = theConf->GetBlob("MainWindow/PresetProperties_Columns");
	if (Columns.isEmpty()) {
		m_pTreeItems->setColumnWidth(CPresetItemsModel::eName, 200);
		m_pTreeItems->setColumnWidth(CPresetItemsModel::eType, 80);
		m_pTreeItems->setColumnWidth(CPresetItemsModel::eAction, 80);
		m_pTreeItems->setColumnWidth(CPresetItemsModel::eStatus, 120);
		m_pTreeItems->setColumnWidth(CPresetItemsModel::eOwner, 120);
	} else
		m_pTreeItems->header()->restoreState(Columns);

	UpdateButtons();
}

CPresetProperties::~CPresetProperties()
{
	theConf->SetBlob("MainWindow/PresetProperties_Columns", m_pTreeItems->header()->saveState());
}

void CPresetProperties::SetPreset(const CPresetPtr& pPreset)
{
	m_pPreset = pPreset;
	m_Items.clear();

	if (pPreset) {
		m_Items = pPreset->m_Items;
	}

	Update();
	UpdateButtons();
}

QString CPresetProperties::GetCurrentGuid() const
{
	return m_pPreset ? m_pPreset->GetGuid().ToQS() : QString();
}

void CPresetProperties::Update()
{
	m_pItemsModel->SetItems(m_Items);
	m_pItemsModel->SetOwnership(theCore->PresetManager()->GetItemOwnership());
	m_pItemsModel->Sync();
}

void CPresetProperties::UpdateButtons()
{
	bool bHasPreset = !m_pPreset.isNull();
	m_pBtnAddExec->setEnabled(bHasPreset);
	m_pBtnAddRes->setEnabled(bHasPreset);
	m_pBtnAddFw->setEnabled(bHasPreset);
	m_pBtnAddDns->setEnabled(bHasPreset);
	m_pBtnAddCfg->setEnabled(bHasPreset);
}

void CPresetProperties::ApplyChanges()
{
	if (!m_pPreset)
		return;

	m_pPreset->m_Items = m_Items;

	auto Ret = theCore->PresetManager()->SetPreset(m_pPreset);
	theGUI->CheckResults(QList<STATUS>() << Ret, this);
}

void CPresetProperties::AddExecRule()
{
	auto Rules = theCore->ProgramManager()->GetProgramRules();

	QMap<QVariant, CItemPicker::SItem> Items;
	foreach(const CProgramRulePtr& pRule, Rules) {
		CItemPicker::SItem Item;
		Item.Name = CMajorPrivacy::GetResourceStr(pRule->GetName());
		Item.Description = CMajorPrivacy::GetResourceStr(pRule->GetDescription());
		CProgramItemPtr pProgItem = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID());
		if (pProgItem)
			Item.Icon = pProgItem->GetIcon();
		else
			Item.Icon = QIcon(":/Icons/Process.png");
		Items.insert(pRule->GetGuid().ToQV(), Item);
	}

	CItemPicker Picker(tr("Select a Program Rule"), Items, this);
	if (theGUI->SafeExec(&Picker)) {
		for (const QVariant& Item : Picker.GetItems()) {
			QFlexGuid Guid(Item);
			if (m_Items.contains(Guid))
				continue;

			m_Items[Guid].Type = EItemType::eExecRule;
			m_Items[Guid].Activate = SItemPreset::EActivate::eUndefined;
		}
		Update();
		ApplyChanges();
	}
}

void CPresetProperties::AddResRule()
{
	auto Rules = theCore->AccessManager()->GetAccessRules();

	QMap<QVariant, CItemPicker::SItem> Items;
	foreach(const CAccessRulePtr& pRule, Rules) {
		CItemPicker::SItem Item;
		Item.Name = CMajorPrivacy::GetResourceStr(pRule->GetName());
		Item.Description = CMajorPrivacy::GetResourceStr(pRule->GetDescription());
		CProgramItemPtr pProgItem = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID());
		if (pProgItem)
			Item.Icon = pProgItem->GetIcon();
		else
			Item.Icon = QIcon(":/Icons/Ampel.png");
		Items.insert(pRule->GetGuid().ToQV(), Item);
	}

	CItemPicker Picker(tr("Select a Resource Access Rule"), Items, this);
	if (theGUI->SafeExec(&Picker)) {
		for (const QVariant& Item : Picker.GetItems()) {
			QFlexGuid Guid(Item);
			if (m_Items.contains(Guid))
				continue;

			m_Items[Guid].Type = EItemType::eResRule;
			m_Items[Guid].Activate = SItemPreset::EActivate::eUndefined;
		}
		Update();
		ApplyChanges();
	}
}

void CPresetProperties::AddFwRule()
{
	auto Rules = theCore->NetworkManager()->GetFwRules();

	QMap<QVariant, CItemPicker::SItem> Items;
	foreach(const CFwRulePtr& pRule, Rules) {
		CItemPicker::SItem Item;
		Item.Name = CMajorPrivacy::GetResourceStr(pRule->GetName());
		Item.Description = CMajorPrivacy::GetResourceStr(pRule->GetDescription());
		CProgramItemPtr pProgItem = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID());
		if (pProgItem)
			Item.Icon = pProgItem->GetIcon();
		else
			Item.Icon = QIcon(":/Icons/Wall3.png");
		Items.insert(pRule->GetGuid().ToQV(), Item);
	}

	CItemPicker Picker(tr("Select a Firewall Rule"), Items, this);
	if (theGUI->SafeExec(&Picker)) {
		for (const QVariant& Item : Picker.GetItems()) {
			QFlexGuid Guid(Item);
			if (m_Items.contains(Guid))
				continue;

			m_Items[Guid].Type = EItemType::eFwRule;
			m_Items[Guid].Activate = SItemPreset::EActivate::eUndefined;
		}
		Update();
		ApplyChanges();
	}
}

void CPresetProperties::AddDnsRule()
{
	auto Rules = theCore->NetworkManager()->GetDnsRules();

	QMap<QVariant, CItemPicker::SItem> Items;
	foreach(const CDnsRulePtr& pRule, Rules) {
		CItemPicker::SItem Item;
		Item.Name = pRule->GetHostName();
		Item.Icon = QIcon(":/Icons/Network2.png");
		Items.insert(pRule->GetGuid().ToQV(), Item);
	}

	CItemPicker Picker(tr("Select a DNS Rule"), Items, this);
	if (theGUI->SafeExec(&Picker)) {
		for (const QVariant& Item : Picker.GetItems()) {
			QFlexGuid Guid(Item);
			if (m_Items.contains(Guid))
				continue;

			m_Items[Guid].Type = EItemType::eDnsRule;
			m_Items[Guid].Activate = SItemPreset::EActivate::eUndefined;
		}
		Update();
		ApplyChanges();
	}
}

static void CPresetProperties__AddTweakList(QMap<QVariant, CItemPicker::SItem>& AllItems, QMap<QVariant, CItemPicker::SItem>& Items, const QSharedPointer<CTweakList>& pList)
{
	for (auto pTweak : pList->GetList())
	{
		CItemPicker::SItem Item;
		Item.Name = pTweak->GetName();
		auto pGroup = pTweak.objectCast<CTweakGroup>();
		if (pGroup)
			Item.Icon = pGroup->GetIcon();
		else
			Item.Icon = QIcon(":/Icons/Tweaks.png");

		auto pSubList = pTweak.objectCast<CTweakList>();
		if (pSubList)
			CPresetProperties__AddTweakList(AllItems, Item.SubItems, pSubList);

		Items.insert(pTweak->GetId(), Item);
		AllItems.insert(pTweak->GetId(), Item);
	}
}

void CPresetProperties::AddTweak()
{
	auto TweakRoot = theCore->TweakManager()->GetRoot();

	QMap<QVariant, CItemPicker::SItem> AllItems;
	QMap<QVariant, CItemPicker::SItem> Items;
	CPresetProperties__AddTweakList(AllItems, Items, TweakRoot);

	CItemPicker Picker(tr("Select a Tweak"), Items, this);
	if (theGUI->SafeExec(&Picker)) {
		for (const QVariant& Item : Picker.GetItems()) {
			QFlexGuid Guid(Item);
			if (m_Items.contains(Guid))
				continue;

			m_Items[Guid].Type = EItemType::eTweak;
			m_Items[Guid].Activate = SItemPreset::EActivate::eUndefined;
		}
		Update();
		ApplyChanges();
	}
}

void CPresetProperties::RemoveSelectedItems()
{
	QModelIndexList Selected = m_pTreeItems->selectionModel()->selectedRows();
	if (Selected.isEmpty())
		return;

	if (QMessageBox::question(this, tr("Remove Items"), tr("Are you sure you want to remove the selected items?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	for (const QModelIndex& Index : Selected) {
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		QFlexGuid Guid = m_pItemsModel->GetItemGuid(ModelIndex);
		m_Items.remove(Guid);
	}
	Update();
	ApplyChanges();
}

void CPresetProperties::EnableSelectedItems()
{
	QModelIndexList Selected = m_pTreeItems->selectionModel()->selectedRows();
	for (const QModelIndex& Index : Selected) {
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		QFlexGuid Guid = m_pItemsModel->GetItemGuid(ModelIndex);
		if (m_Items.contains(Guid))
			m_Items[Guid].Activate = SItemPreset::EActivate::eEnable;
	}
	Update();
	ApplyChanges();
}

void CPresetProperties::DisableSelectedItems()
{
	QModelIndexList Selected = m_pTreeItems->selectionModel()->selectedRows();
	for (const QModelIndex& Index : Selected) {
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		QFlexGuid Guid = m_pItemsModel->GetItemGuid(ModelIndex);
		if (m_Items.contains(Guid))
			m_Items[Guid].Activate = SItemPreset::EActivate::eDisable;
	}
	Update();
	ApplyChanges();
}

void CPresetProperties::ActivateSelectedItems()
{
	QModelIndexList Selected = m_pTreeItems->selectionModel()->selectedRows();
	for (const QModelIndex& Index : Selected) {
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		QFlexGuid Guid = m_pItemsModel->GetItemGuid(ModelIndex);
		if (m_Items.contains(Guid))
			m_Items[Guid].Enabled = true;
	}
	Update();
	ApplyChanges();
}

void CPresetProperties::DeactivateSelectedItems()
{
	QModelIndexList Selected = m_pTreeItems->selectionModel()->selectedRows();
	for (const QModelIndex& Index : Selected) {
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		QFlexGuid Guid = m_pItemsModel->GetItemGuid(ModelIndex);
		if (m_Items.contains(Guid))
			m_Items[Guid].Enabled = false;
	}
	Update();
	ApplyChanges();
}

void CPresetProperties::OnSelectionChanged()
{
	UpdateButtons();
}

void CPresetProperties::OnDoubleClicked(const QModelIndex& index)
{
	if (!index.isValid())
		return;

	QModelIndex ModelIndex = m_pSortProxy->mapToSource(index);
	SItemPreset Item = m_pItemsModel->GetItem(ModelIndex);

	// Only open edit dialog for rules, not tweaks
	if (Item.Type != EItemType::eTweak && Item.Type != EItemType::eDnsRule)
		OnEditRule();
}

void CPresetProperties::OnMenu(const QPoint& Point)
{
	QModelIndexList Selected = m_pTreeItems->selectionModel()->selectedRows();
	bool bHasSelection = !Selected.isEmpty();
	bool bSingleSelection = Selected.size() == 1;

	m_pAddMenu->setEnabled(!m_pPreset.isNull());
	m_pRemoveAction->setEnabled(bHasSelection);
	m_pRuleMenu->setEnabled(bHasSelection);

	// Determine preset action states (Set Enable/Disable)
	bool bAllSetEnable = true;
	bool bAllSetDisable = true;
	// Determine item activated/deactivated states
	bool bAllItemsActivated = true;
	bool bAllItemsDeactivated = true;
	// Determine live rule states (Rule Enable/Disable)
	bool bAllRulesEnabled = true;
	bool bAllRulesDisabled = true;
	bool bHasEditableRule = false;

	for (const QModelIndex& Index : Selected) {
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		QFlexGuid Guid = m_pItemsModel->GetItemGuid(ModelIndex);
		SItemPreset Item = m_pItemsModel->GetItem(ModelIndex);

		// Check preset action
		if (Item.Activate != SItemPreset::EActivate::eEnable)
			bAllSetEnable = false;
		if (Item.Activate != SItemPreset::EActivate::eDisable)
			bAllSetDisable = false;

		// Check item activated/deactivated state
		if (!Item.Enabled)
			bAllItemsActivated = false;
		if (Item.Enabled)
			bAllItemsDeactivated = false;

		// Check live rule status
		bool bRuleEnabled = false;
		switch (Item.Type)
		{
		case EItemType::eExecRule:
			if (CProgramRulePtr pRule = theCore->ProgramManager()->GetProgramRuleByGuid(Guid))
				bRuleEnabled = pRule->IsEnabled();
			bHasEditableRule = true;
			break;
		case EItemType::eResRule:
			if (CAccessRulePtr pRule = theCore->AccessManager()->GetAccessRuleByGuid(Guid))
				bRuleEnabled = pRule->IsEnabled();
			bHasEditableRule = true;
			break;
		case EItemType::eFwRule:
			if (CFwRulePtr pRule = theCore->NetworkManager()->GetFwRuleByGuid(Guid))
				bRuleEnabled = pRule->IsEnabled();
			bHasEditableRule = true;
			break;
		case EItemType::eDnsRule:
			if (CDnsRulePtr pRule = theCore->NetworkManager()->GetDnsRuleByGuid(Guid))
				bRuleEnabled = pRule->IsEnabled();
			break;
		case EItemType::eTweak:
			if (CTweakPtr pTweak = theCore->TweakManager()->GetTweakById(Guid.ToQS())) {
				ETweakStatus status = pTweak->GetStatus();
				bRuleEnabled = (status == ETweakStatus::eApplied || status == ETweakStatus::eSet);
			}
			break;
		}

		if (!bRuleEnabled)
			bAllRulesEnabled = false;
		if (bRuleEnabled)
			bAllRulesDisabled = false;
	}

	// Set Enable action: disable if all selected items already have Enable action
	m_pSetEnableAction->setEnabled(bHasSelection && !bAllSetEnable);
	// Set Disable action: disable if all selected items already have Disable action
	m_pSetDisableAction->setEnabled(bHasSelection && !bAllSetDisable);

	// Activate item: disable if all selected items are already activated
	m_pActivateAction->setEnabled(bHasSelection && !bAllItemsActivated);
	// Deactivate item: disable if all selected items are already deactivated
	m_pDeactivateAction->setEnabled(bHasSelection && !bAllItemsDeactivated);

	// Rule Enable action: disable if all selected rules are already enabled
	m_pRuleEnableAction->setEnabled(bHasSelection && !bAllRulesEnabled);
	// Rule Disable action: disable if all selected rules are already disabled
	m_pRuleDisableAction->setEnabled(bHasSelection && !bAllRulesDisabled);

	// Edit rule only available for single selection of non-tweak items
	m_pEditRuleAction->setEnabled(bSingleSelection && bHasEditableRule);

	m_pMenu->popup(m_pTreeItems->viewport()->mapToGlobal(Point));
}

void CPresetProperties::OnEditRule()
{
	QModelIndexList Selected = m_pTreeItems->selectionModel()->selectedRows();
	if (Selected.size() != 1)
		return;

	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Selected.first());
	QFlexGuid Guid = m_pItemsModel->GetItemGuid(ModelIndex);
	SItemPreset Item = m_pItemsModel->GetItem(ModelIndex);

	switch (Item.Type)
	{
	case EItemType::eExecRule:
		if (CProgramRulePtr pRule = theCore->ProgramManager()->GetProgramRuleByGuid(Guid)) {
			QSet<CProgramItemPtr> Items;
			if (CProgramItemPtr pProgItem = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID()))
				Items.insert(pProgItem);
			CProgramRuleWnd* pWnd = new CProgramRuleWnd(pRule, Items);
			pWnd->show();
		}
		break;

	case EItemType::eResRule:
		if (CAccessRulePtr pRule = theCore->AccessManager()->GetAccessRuleByGuid(Guid)) {
			QSet<CProgramItemPtr> Items;
			if (CProgramItemPtr pProgItem = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID()))
				Items.insert(pProgItem);
			CAccessRuleWnd* pWnd = new CAccessRuleWnd(pRule, Items);
			pWnd->show();
		}
		break;

	case EItemType::eFwRule:
		if (CFwRulePtr pRule = theCore->NetworkManager()->GetFwRuleByGuid(Guid)) {
			QSet<CProgramItemPtr> Items;
			if (CProgramItemPtr pProgItem = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID()))
				Items.insert(pProgItem);
			CFirewallRuleWnd* pWnd = new CFirewallRuleWnd(pRule, Items);
			pWnd->show();
		}
		break;

	case EItemType::eDnsRule:
		// DNS rules don't have a separate edit dialog typically
		break;

	case EItemType::eTweak:
		// Tweaks are not edited this way
		break;
	}
}

void CPresetProperties::OnRuleEnable()
{
	QModelIndexList Selected = m_pTreeItems->selectionModel()->selectedRows();
	if (Selected.isEmpty())
		return;

	QList<STATUS> Results;

	for (const QModelIndex& Index : Selected)
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		QFlexGuid Guid = m_pItemsModel->GetItemGuid(ModelIndex);
		SItemPreset Item = m_pItemsModel->GetItem(ModelIndex);

		switch (Item.Type)
		{
		case EItemType::eExecRule:
			if (CProgramRulePtr pRule = theCore->ProgramManager()->GetProgramRuleByGuid(Guid)) {
				if (!pRule->IsEnabled()) {
					pRule->SetEnabled(true);
					Results << theCore->ProgramManager()->SetProgramRule(pRule);
				}
			}
			break;

		case EItemType::eResRule:
			if (CAccessRulePtr pRule = theCore->AccessManager()->GetAccessRuleByGuid(Guid)) {
				if (!pRule->IsEnabled()) {
					pRule->SetEnabled(true);
					Results << theCore->AccessManager()->SetAccessRule(pRule);
				}
			}
			break;

		case EItemType::eFwRule:
			if (CFwRulePtr pRule = theCore->NetworkManager()->GetFwRuleByGuid(Guid)) {
				if (!pRule->IsEnabled()) {
					pRule->SetEnabled(true);
					Results << theCore->NetworkManager()->SetFwRule(pRule);
				}
			}
			break;

		case EItemType::eDnsRule:
			if (CDnsRulePtr pRule = theCore->NetworkManager()->GetDnsRuleByGuid(Guid)) {
				if (!pRule->IsEnabled()) {
					pRule->SetEnabled(true);
					Results << theCore->NetworkManager()->SetDnsRule(pRule);
				}
			}
			break;

		case EItemType::eTweak:
			if (CTweakPtr pTweak = theCore->TweakManager()->GetTweakById(Guid.ToQS())) {
				theCore->TweakManager()->ApplyTweak(pTweak);
			}
			break;
		}
	}

	theGUI->CheckResults(Results, this);

	// Refresh the display to show updated status
	Update();
}

void CPresetProperties::OnRuleDisable()
{
	QModelIndexList Selected = m_pTreeItems->selectionModel()->selectedRows();
	if (Selected.isEmpty())
		return;

	QList<STATUS> Results;

	for (const QModelIndex& Index : Selected)
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		QFlexGuid Guid = m_pItemsModel->GetItemGuid(ModelIndex);
		SItemPreset Item = m_pItemsModel->GetItem(ModelIndex);

		switch (Item.Type)
		{
		case EItemType::eExecRule:
			if (CProgramRulePtr pRule = theCore->ProgramManager()->GetProgramRuleByGuid(Guid)) {
				if (pRule->IsEnabled()) {
					pRule->SetEnabled(false);
					Results << theCore->ProgramManager()->SetProgramRule(pRule);
				}
			}
			break;

		case EItemType::eResRule:
			if (CAccessRulePtr pRule = theCore->AccessManager()->GetAccessRuleByGuid(Guid)) {
				if (pRule->IsEnabled()) {
					pRule->SetEnabled(false);
					Results << theCore->AccessManager()->SetAccessRule(pRule);
				}
			}
			break;

		case EItemType::eFwRule:
			if (CFwRulePtr pRule = theCore->NetworkManager()->GetFwRuleByGuid(Guid)) {
				if (pRule->IsEnabled()) {
					pRule->SetEnabled(false);
					Results << theCore->NetworkManager()->SetFwRule(pRule);
				}
			}
			break;

		case EItemType::eDnsRule:
			if (CDnsRulePtr pRule = theCore->NetworkManager()->GetDnsRuleByGuid(Guid)) {
				if (pRule->IsEnabled()) {
					pRule->SetEnabled(false);
					Results << theCore->NetworkManager()->SetDnsRule(pRule);
				}
			}
			break;

		case EItemType::eTweak:
			if (CTweakPtr pTweak = theCore->TweakManager()->GetTweakById(Guid.ToQS())) {
				theCore->TweakManager()->UndoTweak(pTweak);
			}
			break;
		}
	}

	theGUI->CheckResults(Results, this);

	// Refresh the display to show updated status
	Update();
}
