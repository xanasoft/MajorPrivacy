#include "pch.h"
#include "PresetWindow.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Presets/PresetManager.h"
#include "../Core/Network/NetworkManager.h"
#include "../Core/Access/AccessManager.h"
#include "../Core/Tweaks/TweakManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NetUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"
#include "../Helpers/WinHelper.h"
#include "../Core/HashDB/HashDB.h"
#include "../MiscHelpers/Common/WinboxMultiCombo.h"
#include "../Windows/ScriptWindow.h"
#include "../Windows/ItemPicker.h"

CPresetWindow::CPresetWindow(const CPresetPtr& pPreset, QWidget* parent)
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

	ui.btnAddExec->setIcon(QIcon(":/Icons/Process.png"));
	ui.btnAddExec->setToolTip(tr("Add Execution Rule"));
	ui.btnAddRes->setIcon(QIcon(":/Icons/Ampel.png"));
	ui.btnAddRes->setToolTip(tr("Add Resource Access Rule"));
	ui.btnAddFw->setIcon(QIcon(":/Icons/Wall3.png"));
	ui.btnAddFw->setToolTip(tr("Add Firewall Rule"));
	ui.btnAddDns->setIcon(QIcon(":/Icons/Network2.png"));
	ui.btnAddDns->setToolTip(tr("Add DNS Rule"));
	ui.btnAddCfg->setIcon(QIcon(":/Icons/Tweaks.png"));
	ui.btnAddCfg->setToolTip(tr("Add Tweak"));
	ui.btnRemove->setIcon(QIcon(":/Icons/Remove.png"));
	ui.btnRemove->setToolTip(tr("Remove Selected Items"));

	connect(ui.btnAddExec, SIGNAL(clicked(bool)), this, SLOT(AddExecRule()));
	connect(ui.btnAddRes, SIGNAL(clicked(bool)), this, SLOT(AddResRule()));
	connect(ui.btnAddFw, SIGNAL(clicked(bool)), this, SLOT(AddFwRule()));
	connect(ui.btnAddDns, SIGNAL(clicked(bool)), this, SLOT(AddDnsRule()));
	connect(ui.btnAddCfg, SIGNAL(clicked(bool)), this, SLOT(AddTweak()));
	connect(ui.btnRemove, SIGNAL(clicked(bool)), this, SLOT(RemoveSelectedItems()));


	connect(ui.btnIcon, SIGNAL(clicked(bool)), this, SLOT(PickIcon()));
	ui.btnIcon->setToolTip(tr("Change Icon"));
	QMenu* pIconMenu = new QMenu(ui.btnIcon);
	pIconMenu->addAction(tr("Browse for Image"), this, SLOT(BrowseImage()));
	//QAction* pResetIcon = pIconMenu->addAction(tr("Reset Icon"), this, SLOT(ResetIcon()));
	//pResetIcon->setEnabled(!pProgram.isNull());
	ui.btnIcon->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnIcon->setMenu(pIconMenu);

	m_pPreset = pPreset;
	bool bNew = m_pPreset->m_Guid.IsNull();

	setWindowTitle(bNew ? tr("Create Rule Preset") : tr("Edit Rule Preset"));

	connect(ui.txtName, SIGNAL(textChanged(const QString&)), this, SLOT(OnNameChanged(const QString&)));

	connect(ui.chkScript, SIGNAL(stateChanged(int)), this, SLOT(OnActionChanged()));
	connect(ui.btnScript, SIGNAL(clicked()), this, SLOT(EditScript()));

	connect(ui.treeItems, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemDoubleClicked(QTreeWidgetItem*, int)));
	connect(ui.treeItems, SIGNAL(itemSelectionChanged()), this, SLOT(OnSelectionChanged()));

	ui.treeItems->viewport()->installEventFilter(this);
	this->installEventFilter(this); // prevent enter from closing the dialog

	//connect(ui.cmbDllMode, SIGNAL(currentIndexChanged(int)), this, SLOT(OnActionChanged()));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(OnSave()));

	//FixComboBoxEditing(ui.cmbGroup);


	ui.txtName->setText(m_pPreset->m_Name);
	ui.btnIcon->setIcon(m_pPreset->GetIcon());
	m_IconFile = m_pPreset->GetIconFile();
	// todo: load groups
	//SetComboBoxValue(ui.cmbGroup, m_pPreset->m_Grouping); // todo
	ui.txtInfo->setPlainText(m_pPreset->m_Description);

	//ui.txtScript->setPlainText(m_pPreset->m_Script);
	m_Script = m_pPreset->m_Script;

	ui.chkScript->setChecked(m_pPreset->m_bUseScript);
	if(m_pPreset->m_Script.isEmpty())
		ui.btnScript->setIcon(QIcon(":/Icons/Script-Add.png"));
	else
		ui.btnScript->setIcon(QIcon(":/Icons/Script-Edit.png"));


	for(auto I = m_pPreset->m_Items.begin(); I != m_pPreset->m_Items.end(); ++I)
	{
		const QFlexGuid& Guid = I.key();
		const SItemPreset& Item = I.value();

		QString Name;
		QIcon Icon;
		switch (Item.Type)
		{
		case EItemType::eExecRule:
			if (CProgramRulePtr pRule = theCore->ProgramManager()->GetProgramRuleByGuid(Guid)) {
				Name = CMajorPrivacy::GetResourceStr(pRule->GetName());
				CProgramItemPtr pProgItem = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID());
				if (pProgItem)
					Icon = pProgItem->GetIcon();
				else
					Icon = QIcon(":/Icons/Process.png");
			} else {
				Name = tr("Unknown Program Rule (%1)").arg(Guid.ToQS());
				Icon = QIcon(":/Icons/Process.png");
			}
			break;
		
		case EItemType::eResRule:
			if (CAccessRulePtr pRule = theCore->AccessManager()->GetAccessRuleByGuid(Guid)) {
				Name = CMajorPrivacy::GetResourceStr(pRule->GetName());
				CProgramItemPtr pProgItem = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID());
				if (pProgItem)
					Icon = pProgItem->GetIcon();
				else
					Icon = QIcon(":/Icons/Ampel.png");
			} else {
				Name = tr("Unknown Resource Rule (%1)").arg(Guid.ToQS());
				Icon = QIcon(":/Icons/Ampel.png");
			}
			Name = tr("Resource Rule (%1)").arg(Guid.ToQS());
			Icon = QIcon(":/Icons/Ampel.png");
			break;
		
		case EItemType::eFwRule:
			if (CFwRulePtr pRule = theCore->NetworkManager()->GetFwRuleByGuid(Guid)) {
				Name = CMajorPrivacy::GetResourceStr(pRule->GetName());
				CProgramItemPtr pProgItem = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID());
				if (pProgItem)
					Icon = pProgItem->GetIcon();
				else
					Icon = QIcon(":/Icons/Wall3.png");
			} else {
				Name = tr("Unknown Firewall Rule (%1)").arg(Guid.ToQS());
				Icon = QIcon(":/Icons/Wall3.png");
			}
			Name = tr("Firewall Rule (%1)").arg(Guid.ToQS());
			Icon = QIcon(":/Icons/Wall3.png");
			break;
		
		case EItemType::eDnsRule:
			if (CDnsRulePtr pRule = theCore->NetworkManager()->GetDnsRuleByGuid(Guid)) {
				Name = CMajorPrivacy::GetResourceStr(pRule->GetHostName());
				Icon = QIcon(":/Icons/Network2.png");
			} else {
				Name = tr("Unknown DNS Rule (%1)").arg(Guid.ToQS());
				Icon = QIcon(":/Icons/Network2.png");
			}
			break;
		
		case EItemType::eTweak:
			if (CTweakPtr pTweak = theCore->TweakManager()->GetTweakById(Guid.ToQS())) {
				Name = CMajorPrivacy::GetResourceStr(pTweak->GetName());
				auto pGroup = pTweak.objectCast<CTweakGroup>();
				if (pGroup)
					Icon = pGroup->GetIcon();
				else
					Icon = QIcon(":/Icons/Tweaks.png");
			} else {
				Name = tr("Unknown Tweak (%1)").arg(Guid.ToQS());
				Icon = QIcon(":/Icons/Tweaks.png");
			}
			break;
		
		}

		QTreeWidgetItem* pItem = new QTreeWidgetItem(ui.treeItems);

		pItem->setText(eName, Name);
		pItem->setIcon(eName, Icon);
		QVariantMap Data;
		Data["GUID"] = Guid.ToQV();
		Data["Type"] = (int)Item.Type;
		pItem->setData(eName, Qt::UserRole, Data);

		switch (Item.Activate) {
		case SItemPreset::EActivate::eUndefined:	pItem->setText(eAction, tr("Do Nothing")); break;
		case SItemPreset::EActivate::eEnable:		pItem->setText(eAction, tr("Enable")); break;
		case SItemPreset::EActivate::eDisable:		pItem->setText(eAction, tr("Disable")); break;
		}
		pItem->setData(eAction, Qt::UserRole, (int)Item.Activate);
	}


	restoreGeometry(theConf->GetBlob("PresetWindow/Window_Geometry"));
	QByteArray Columns = theConf->GetBlob("PresetWindow/PresetWindow_Columns");
	if (Columns.isEmpty()) {
		ui.treeItems->setColumnWidth(0, 300);
	} else
		ui.treeItems->header()->restoreState(Columns);
}

CPresetWindow::~CPresetWindow()
{

	theConf->SetBlob("PresetWindow/Window_Geometry", saveGeometry());
	theConf->SetBlob("PresetWindow/PresetWindow_Columns", ui.treeItems->header()->saveState());
}

void CPresetWindow::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

bool CPresetWindow::OnSave()
{
	if (!Save()) {
		QApplication::beep();
		return false;
	}

	STATUS Status = theCore->PresetManager()->SetPreset(m_pPreset);	
	if (theGUI->CheckResults(QList<STATUS>() << Status, this))
		return false;
	return true;
}

void CPresetWindow::OnSaveAndClose()
{
	if(OnSave())
		accept();
}

bool CPresetWindow::Save()
{
	m_pPreset->m_Name = ui.txtName->text();
	m_pPreset->SetIconFile(m_IconFile);
	//m_pPreset->m_Grouping = GetComboBoxValue(ui.cmbGroup).toString(); // todo
	m_pPreset->m_Description = ui.txtInfo->toPlainText();

	//m_pPreset->m_Script = ui.txtScript->toPlainText();
	m_pPreset->m_Script = m_Script;

	m_pPreset->m_bUseScript = ui.chkScript->isChecked();

	m_pPreset->m_Items.clear();

	for (int i = 0; i < ui.treeItems->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* pItem = ui.treeItems->topLevelItem(i);

		QVariantMap Data = pItem->data(eName, Qt::UserRole).toMap();
		QFlexGuid Guid(Data["GUID"]);
		m_pPreset->m_Items[Guid].Type = (EItemType)Data["Type"].toInt();
		m_pPreset->m_Items[Guid].Activate = (SItemPreset::EActivate)pItem->data(eAction, Qt::UserRole).toInt();
	}

	return true;
}

void CPresetWindow::PickIcon()
{
	QString Path = m_IconFile;
	quint32 Index = 0;

	StrPair PathIndex = Split2(Path, ",", true);
	if (!PathIndex.second.isEmpty() && !PathIndex.second.contains(".")) {
		Path = PathIndex.first;
		Index = PathIndex.second.toInt();
	}

	if (!PickWindowsIcon(this, Path, Index))
		return;

	ui.btnIcon->setIcon(LoadWindowsIcon(Path, Index));
	m_IconFile = QString("%1,%2").arg(Path).arg(Index);
}

void CPresetWindow::BrowseImage()
{
	QString Value = QFileDialog::getOpenFileName(this, tr("Select Image File"), "", tr("Image Files (*.png)")).replace("/", "\\");
	if (Value.isEmpty())
		return;

	ui.btnIcon->setIcon(QIcon(Value));
	m_IconFile = Value;
}

void CPresetWindow::OnNameChanged(const QString& Text)
{
}

void CPresetWindow::EditScript()
{
	CScriptWindow* pScriptWindow = new CScriptWindow(m_pPreset->GetGuid(), EItemType::ePreset, this);
	pScriptWindow->SetScript(m_Script);
	pScriptWindow->SetSaver([&](const QString& Script, bool bApply){
		m_Script = Script;
		if (bApply) {
			m_pPreset->m_Script = Script;
			return theCore->PresetManager()->SetPreset(m_pPreset);
		}
		return OK;
		});
	SafeShow(pScriptWindow);
}

void CPresetWindow::AddExecRule()
{
	auto Rules = theCore->ProgramManager()->GetProgramRules();

	QMap<QVariant, CItemPicker::SItem> Items;
	foreach(const CProgramRulePtr& pRule, Rules) {
		CItemPicker::SItem Item;
		Item.Name = CMajorPrivacy::GetResourceStr(pRule->GetName());
		Item.Description = CMajorPrivacy::GetResourceStr(pRule->GetDescription());
		CProgramItemPtr pProgItem = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID());
		if(pProgItem)
			Item.Icon = pProgItem->GetIcon();
		else
			Item.Icon = QIcon(":/Icons/Process.png");
		Items.insert(pRule->GetGuid().ToQV(), Item);
	}

	CItemPicker Picker(tr("Sellect a Program Rule"), Items, this);
	if (theGUI->SafeExec(&Picker)) {
		QVariant Item = Picker.GetItem();

		QTreeWidgetItem* pItem = new QTreeWidgetItem(ui.treeItems);
		pItem->setText(eName, Items[Item].Name);
		pItem->setIcon(eName, Items[Item].Icon);
		QVariantMap Data;
		Data["GUID"] = Item;
		Data["Type"] = (int)EItemType::eExecRule;
		pItem->setData(eName, Qt::UserRole, Data);
	}
}

void CPresetWindow::AddResRule()
{
	auto Rules = theCore->AccessManager()->GetAccessRules();

	QMap<QVariant, CItemPicker::SItem> Items;
	foreach(const CAccessRulePtr& pRule, Rules) {
		CItemPicker::SItem Item;
		Item.Name = CMajorPrivacy::GetResourceStr(pRule->GetName());
		Item.Description = CMajorPrivacy::GetResourceStr(pRule->GetDescription());
		CProgramItemPtr pProgItem = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID());
		if(pProgItem)
			Item.Icon = pProgItem->GetIcon();
		else
			Item.Icon = QIcon(":/Icons/Process.png");
		Items.insert(pRule->GetGuid().ToQV(), Item);
	}

	CItemPicker Picker(tr("Sellect a Resource Access Rule"), Items, this);
	if (theGUI->SafeExec(&Picker)) {
		QVariant Item = Picker.GetItem();

		QTreeWidgetItem* pItem = new QTreeWidgetItem(ui.treeItems);
		pItem->setText(eName, Items[Item].Name);
		pItem->setIcon(eName, Items[Item].Icon);
		pItem->setData(eName, Qt::UserRole, Item);
		QVariantMap Data;
		Data["GUID"] = Item;
		Data["Type"] = (int)EItemType::eResRule;
		pItem->setData(eName, Qt::UserRole, Data);
	}
}

void CPresetWindow::AddFwRule()
{
	auto Rules = theCore->NetworkManager()->GetFwRules();

	QMap<QVariant, CItemPicker::SItem> Items;
	foreach(const CFwRulePtr& pRule, Rules) {
		CItemPicker::SItem Item;
		Item.Name = CMajorPrivacy::GetResourceStr(pRule->GetName());
		Item.Description = CMajorPrivacy::GetResourceStr(pRule->GetDescription());
		CProgramItemPtr pProgItem = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID());
		if(pProgItem)
			Item.Icon = pProgItem->GetIcon();
		else
			Item.Icon = QIcon(":/Icons/Process.png");
		Items.insert(pRule->GetGuid().ToQV(), Item);
	}

	CItemPicker Picker(tr("Sellect a Firewall Rule"), Items, this);
	if (theGUI->SafeExec(&Picker)) {
		QVariant Item = Picker.GetItem();

		QTreeWidgetItem* pItem = new QTreeWidgetItem(ui.treeItems);
		pItem->setText(eName, Items[Item].Name);
		pItem->setIcon(eName, Items[Item].Icon);
		pItem->setData(eName, Qt::UserRole, Item);
		QVariantMap Data;
		Data["GUID"] = Item;
		Data["Type"] = (int)EItemType::eFwRule;
		pItem->setData(eName, Qt::UserRole, Data);
	}
}

void CPresetWindow::AddDnsRule()
{
	auto Rules = theCore->NetworkManager()->GetDnsRules();

	QMap<QVariant, CItemPicker::SItem> Items;
	foreach(const CDnsRulePtr& pRule, Rules) {
		CItemPicker::SItem Item;
		Item.Name = pRule->GetHostName();
		Item.Icon = QIcon(":/Icons/Network2.png");
		Items.insert(pRule->GetGuid().ToQV(), Item);
	}

	CItemPicker Picker(tr("Sellect a DNS Rule"), Items, this);
	if (theGUI->SafeExec(&Picker)) {
		QVariant Item = Picker.GetItem();

		QTreeWidgetItem* pItem = new QTreeWidgetItem(ui.treeItems);
		pItem->setText(eName, Items[Item].Name);
		pItem->setIcon(eName, Items[Item].Icon);
		pItem->setData(eName, Qt::UserRole, Item);
		QVariantMap Data;
		Data["GUID"] = Item;
		Data["Type"] = (int)EItemType::eDnsRule;
		pItem->setData(eName, Qt::UserRole, Data);
	}
}

void CPresetWindow__AddTweakList(QMap<QVariant, CItemPicker::SItem>& AllItems, QMap<QVariant, CItemPicker::SItem>& Items, const QSharedPointer<CTweakList>& pList)
{
	for (auto pTweak : pList->GetList())
	{
		CItemPicker::SItem Item;
		Item.Name = pTweak->GetName();
		auto pGroup = pTweak.objectCast<CTweakGroup>();
		if(pGroup)
			Item.Icon = pGroup->GetIcon();
		else
			Item.Icon = QIcon(":/Icons/Tweaks.png");

		auto pList = pTweak.objectCast<CTweakList>();
		if (pList)
			CPresetWindow__AddTweakList(AllItems, Item.SubItems, pList);

		Items.insert(pTweak->GetId(), Item);
		AllItems.insert(pTweak->GetId(), Item);
	}
}

void CPresetWindow::AddTweak()
{
	auto TweakRoot = theCore->TweakManager()->GetRoot();
	
	QMap<QVariant, CItemPicker::SItem> AllItems;
	QMap<QVariant, CItemPicker::SItem> Items;
	CPresetWindow__AddTweakList(AllItems, Items, TweakRoot);

	CItemPicker Picker(tr("Sellect a Tweak"), Items, this);
	if (theGUI->SafeExec(&Picker)) {
		QVariant Item = Picker.GetItem();

		QTreeWidgetItem* pItem = new QTreeWidgetItem(ui.treeItems);
		pItem->setText(eName, AllItems[Item].Name);
		pItem->setIcon(eName, AllItems[Item].Icon);
		pItem->setData(eName, Qt::UserRole, Item);
		QVariantMap Data;
		Data["GUID"] = Item;
		Data["Type"] = (int)EItemType::eTweak;
		pItem->setData(eName, Qt::UserRole, Data);
	}
}

void CPresetWindow::RemoveSelectedItems()
{
	if(QMessageBox::question(this, tr("Remove Items"), tr("Are you sure you want to remove the selected items?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	QTreeWidgetItem* pItem = ui.treeItems->currentItem();
	if (!pItem)
		return;

	delete pItem;
}


void CPresetWindow::OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column)
{
	int Action = pItem->data(eAction, Qt::UserRole).toInt();
	
	QComboBox* pAction = new QComboBox();
	pAction->addItem(tr("Do nothing"), (int)SItemPreset::eUndefined);
	pAction->addItem(tr("Enable"), (int)SItemPreset::eEnable);
	pAction->addItem(tr("Disable"), (int)SItemPreset::eDisable);
	pAction->setCurrentIndex(pAction->findData(Action));
	ui.treeItems->setItemWidget(pItem, eAction, pAction);
}

void CPresetWindow::CloseItemEdit(bool bSave)
{
	for (int i = 0; i < ui.treeItems->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* pItem = ui.treeItems->topLevelItem(i);
		CloseItemEdit(pItem, bSave);
	}
}

void CPresetWindow::CloseItemEdit(QTreeWidgetItem* pItem, bool bSave)
{
	QWidget* pProgram = ui.treeItems->itemWidget(pItem, 1);
	if (!pProgram)
		return;

	if (bSave)
	{
		QComboBox* pAction = (QComboBox*)ui.treeItems->itemWidget(pItem, eAction);

		pItem->setText(eAction, pAction->currentText());
		pItem->setData(eAction, Qt::UserRole, pAction->currentData());
	}

	for (int i = 0; i < ui.treeItems->columnCount(); i++)
		ui.treeItems->setItemWidget(pItem, i, NULL);
}

bool CPresetWindow::eventFilter(QObject *source, QEvent *event)
{
	/*if (event->type() == QEvent::KeyPress && ((QKeyEvent*)event)->key() == Qt::Key_Escape 
		&& ((QKeyEvent*)event)->modifiers() == Qt::NoModifier
		&& source == )
	{
		return true; // cancel event
	}*/

	if (event->type() == QEvent::KeyPress && ((QKeyEvent*)event)->key() == Qt::Key_Escape 
		&& ((QKeyEvent*)event)->modifiers() == Qt::NoModifier
		&& (source == ui.treeItems->viewport()))
	{
		CloseItemEdit(false);
		return true; // cancel event
	}

	if (event->type() == QEvent::KeyPress && (((QKeyEvent*)event)->key() == Qt::Key_Enter || ((QKeyEvent*)event)->key() == Qt::Key_Return) 
		&& (((QKeyEvent*)event)->modifiers() == Qt::NoModifier || ((QKeyEvent*)event)->modifiers() == Qt::KeypadModifier))
	{
		CloseItemEdit(true);
		return true; // cancel event
	}

	if (source == ui.treeItems->viewport() && event->type() == QEvent::MouseButtonPress)
	{
		CloseItemEdit();
	}

	return QDialog::eventFilter(source, event);
}