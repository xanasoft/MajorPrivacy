#include "pch.h"
#include "ProgramPicker.h"
#include "../MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/Finder.h"
#include "ProgramWnd.h"

CProgramPicker::CProgramPicker(CProgramItemPtr pProgram, const QList<CProgramItemPtr>& Items, QWidget* parent)
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
	this->setWindowTitle(tr("MajorPrivacy - Select Program"));

	QWidget* pWidget = new QWidget(this);
	QVBoxLayout* pLayout = new QVBoxLayout(pWidget);
	pLayout->setContentsMargins(0, 0, 0, 0);
	CFinder* pFinder = new CFinder(this, pWidget, 0);
	pLayout->addWidget(pFinder);
	ui.treePrograms->parentWidget()->layout()->replaceWidget(ui.treePrograms, pWidget);
	pLayout->insertWidget(0, ui.treePrograms);
	connect(ui.treePrograms, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnPick()));

	QAbstractButton* pBtnSearch = pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	ui.btnFind->parentWidget()->layout()->replaceWidget(ui.btnFind, pBtnSearch);
	delete ui.btnFind;
	
	ui.btnAdd->setIcon(QIcon(":/Icons/Add.png"));

	connect(ui.btnAdd, SIGNAL(clicked()), SLOT(OnAdd()));
	connect(ui.chkAll, SIGNAL(toggled(bool)), SLOT(OnAll(bool)));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnPick()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

	m_pProgram = pProgram;
	m_Items = Items;

	auto pAll = theCore->ProgramManager()->GetAll();
	int pos = m_Items.indexOf(pAll);
	if (pos != -1) {
		m_Items.removeAt(pos);
		if (pAll == m_pProgram)
			ui.chkAll->setChecked(true);
	} else 
		ui.chkAll->setEnabled(false);

	LoadList();

	ui.treePrograms->setFocus();

	restoreGeometry(theConf->GetBlob("ProgramPicker/Window_Geometry"));
}

CProgramPicker::~CProgramPicker()
{
	theConf->SetBlob("ProgramPicker/Window_Geometry", saveGeometry());
}

void CProgramPicker::SetFilter(const QRegularExpression& Exp, int iOptions, int Column)
{
	m_FilterExp = Exp;
	LoadList();
}

void CProgramPicker::OnAll(bool bChecked)
{
	ui.treePrograms->setEnabled(!bChecked);
	ui.btnAdd->setEnabled(!bChecked);
}

void CProgramPicker::OnAdd()
{
	CProgramWnd* pProgramWnd = new CProgramWnd(NULL);
	if (!pProgramWnd->exec())
		return;

	m_pProgram = theCore->ProgramManager()->GetProgramByUID(pProgramWnd->GetProgram()->GetUID(), true);
	if (m_pProgram) {
		m_Items.append(m_pProgram);
		LoadList();
	}
}

void CProgramPicker::LoadList()
{
	ui.treePrograms->clear();

	for (auto pItem : m_Items) 
	{
		if (m_FilterExp.isValid() && !pItem->GetNameEx().contains(m_FilterExp))
			continue;

		QTreeWidgetItem* pTreeItem = new QTreeWidgetItem(ui.treePrograms);
		pTreeItem->setText(0, pItem->GetNameEx());
		pTreeItem->setIcon(0, pItem->GetIcon());
		pTreeItem->setToolTip(0, pItem->GetPath());
		pTreeItem->setData(0, Qt::UserRole, pItem->GetUID());
		//pTreeItem->setText(1, pItem->GetPath());

		if (pItem == m_pProgram)
			ui.treePrograms->setCurrentItem(pTreeItem);
	}
}

void CProgramPicker::OnPick()
{
	if (ui.chkAll->isChecked())
		m_pProgram = theCore->ProgramManager()->GetAll();
	else 
	{
		QList<QTreeWidgetItem*> Items = ui.treePrograms->selectedItems();
		if (Items.isEmpty()) {
			QMessageBox::warning(this, "MajorPrivacy", tr("Please select a program"));
			return;
		}

		m_pProgram = theCore->ProgramManager()->GetProgramByUID(Items.first()->data(0, Qt::UserRole).toULongLong());
	}
	
	accept();
}