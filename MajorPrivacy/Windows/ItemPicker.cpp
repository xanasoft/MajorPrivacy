#include "pch.h"
#include "ItemPicker.h"
#include "../MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/Finder.h"
#include "ProgramWnd.h"

CItemPicker::CItemPicker(const QString& Prompt, const QMap<QVariant, SItem>& Items, QWidget* parent)
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

	//setWindowState(Qt::WindowActive);
	//SetForegroundWindow((HWND)QWidget::winId());

	ui.setupUi(this);
	this->setWindowTitle(tr("MajorPrivacy - Select Item"));
	ui.label->setText(Prompt);

	QWidget* pWidget = new QWidget(this);
	QVBoxLayout* pLayout = new QVBoxLayout(pWidget);
	pLayout->setContentsMargins(0, 0, 0, 0);
	CFinder* pFinder = new CFinder(this, pWidget, 0);
	pLayout->addWidget(pFinder);
	ui.treeItems->parentWidget()->layout()->replaceWidget(ui.treeItems, pWidget);
	pLayout->insertWidget(0, ui.treeItems);
	connect(ui.treeItems, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnPick()));

	QAbstractButton* pBtnSearch = pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	ui.btnFind->parentWidget()->layout()->replaceWidget(ui.btnFind, pBtnSearch);
	delete ui.btnFind;
	
	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnPick()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

	m_Items = Items;

	LoadList();

	ui.treeItems->setFocus();

	restoreGeometry(theConf->GetBlob("ItemPicker/Window_Geometry"));
}

CItemPicker::~CItemPicker()
{
	theConf->SetBlob("ItemPicker/Window_Geometry", saveGeometry());
}

void CItemPicker::SetFilter(const QRegularExpression& Exp, int iOptions, int Column)
{
	m_FilterExp = Exp;
	LoadList();
}

void CItemPicker::LoadList()
{
	ui.treeItems->clear();

	for (auto I = m_Items.begin(); I != m_Items.end(); ++I) 
	{
		const SItem& Item = I.value();
		
		QTreeWidgetItem* pTreeItem = new QTreeWidgetItem(ui.treeItems);
		pTreeItem->setText(0, Item.Name);
		pTreeItem->setIcon(0, Item.Icon);
		pTreeItem->setToolTip(0, Item.Description);
		pTreeItem->setData(0, Qt::UserRole, I.key());

		int SubCount = LoadSubList(pTreeItem, Item.SubItems);

		if (m_FilterExp.isValid() && !Item.Name.contains(m_FilterExp) && SubCount == 0)
			delete pTreeItem;
	}
}

int CItemPicker::LoadSubList(QTreeWidgetItem* pParent, const QMap<QVariant, SItem>& SubItems)
{
	int count = 0;
	for (auto I = SubItems.begin(); I != SubItems.end(); ++I)
	{
		const SItem& Item = I.value();
		if (m_FilterExp.isValid() && !Item.Name.contains(m_FilterExp))
			continue;

		QTreeWidgetItem* pTreeItem = new QTreeWidgetItem(pParent);
		pTreeItem->setText(0, Item.Name);
		pTreeItem->setIcon(0, Item.Icon);
		pTreeItem->setToolTip(0, Item.Description);
		pTreeItem->setData(0, Qt::UserRole, I.key());
		
		int SubCount = LoadSubList(pTreeItem, Item.SubItems);

		if (m_FilterExp.isValid() && !Item.Name.contains(m_FilterExp) && SubCount == 0)
			delete pTreeItem;
		else
			count += 1 + SubCount;
	}
	return true;
}

void CItemPicker::OnPick()
{
	QList<QTreeWidgetItem*> Items = ui.treeItems->selectedItems();
	if (Items.isEmpty()) {
		QMessageBox::warning(this, "MajorPrivacy", tr("Please select an item"));
		return;
	}

	accept();
}

QVariant CItemPicker::GetItem() 
{
	auto pCurrent = ui.treeItems->currentItem();
	return pCurrent ? pCurrent->data(0, Qt::UserRole) : QVariant();
}