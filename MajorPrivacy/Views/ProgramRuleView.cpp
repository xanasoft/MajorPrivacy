#include "pch.h"
#include "ProgramRuleView.h"
#include "../Core/PrivacyCore.h"
#include "../Windows/ProgramRuleWnd.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Library/API/PrivacyAPI.h" // todo
#include "../MiscHelpers/Common/CustomStyles.h"

CProgramRuleView::CProgramRuleView(QWidget *parent)
	:CPanelViewEx<CProgramRuleModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	QByteArray Columns = theConf->GetBlob("MainWindow/ProgramRuleView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbAction = new QComboBox();
	m_pCmbAction->addItem(tr("All Actions"), (qint32)EExecRuleType::eUnknown);
	m_pCmbAction->addItem(tr("Allow"), (qint32)EExecRuleType::eAllow);
	m_pCmbAction->addItem(tr("Block"), (qint32)EExecRuleType::eBlock);
	m_pCmbAction->addItem(tr("Protect"), (qint32)EExecRuleType::eProtect);
	m_pToolBar->addWidget(m_pCmbAction);

	int comboBoxHeight = m_pCmbAction->sizeHint().height();

	m_pBtnEnabled = new QToolButton();
	m_pBtnEnabled->setIcon(QIcon(":/Icons/Disable.png"));
	m_pBtnEnabled->setCheckable(true);
	m_pBtnEnabled->setToolTip(tr("Hide Disabled Rules"));
	m_pBtnEnabled->setMaximumHeight(comboBoxHeight);
	m_pToolBar->addWidget(m_pBtnEnabled);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(comboBoxHeight);
	m_pToolBar->addWidget(pBtnSearch);

	m_pCreateRule = m_pMenu->addAction(QIcon(":/Icons/Plus.png"), tr("Create Rule"), this, SLOT(OnRuleAction()));
	m_pMenu->addSeparator();
	m_pEnableRule = m_pMenu->addAction(QIcon(":/Icons/Enable.png"), tr("Enable Rule"), this, SLOT(OnRuleAction()));
	m_pDisableRule = m_pMenu->addAction(QIcon(":/Icons/Disable.png"), tr("Disable Rule"), this, SLOT(OnRuleAction()));
	m_pMenu->addSeparator();
	m_pRemoveRule = m_pMenu->addAction(QIcon(":/Icons/Remove.png"), tr("Delete Rule"), this, SLOT(OnRuleAction()));
	m_pEditRule = m_pMenu->addAction(QIcon(":/Icons/EditIni.png"), tr("Edit Rule"), this, SLOT(OnRuleAction()));
	m_pCloneRule = m_pMenu->addAction(QIcon(":/Icons/Duplicate.png"), tr("Duplicate Rule"), this, SLOT(OnRuleAction()));

	AddPanelItemsToMenu();
}

CProgramRuleView::~CProgramRuleView()
{
	theConf->SetBlob("MainWindow/ProgramRuleView_Columns", m_pTreeView->saveState());
}

void CProgramRuleView::Sync(QList<CProgramRulePtr> RuleList)
{
	bool bHideDisabled = m_pBtnEnabled->isChecked();
	EExecRuleType Type = (EExecRuleType)m_pCmbAction->currentData().toInt();

	if (bHideDisabled || Type != EExecRuleType::eUnknown) 
	{
		for (auto I = RuleList.begin(); I != RuleList.end();)
		{
			if (bHideDisabled && !(*I)->IsEnabled())
				I = RuleList.erase(I);
			else if (Type != EExecRuleType::eUnknown && (*I)->GetType() != Type)
				I = RuleList.erase(I);
			else
				++I;
		}
	}

	m_pItemModel->Sync(RuleList);
}

void CProgramRuleView::OnDoubleClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CProgramRulePtr pRule = m_pItemModel->GetItem(ModelIndex);
	if (pRule.isNull())
		return;

	OpenRullDialog(pRule);
}

void CProgramRuleView::OpenRullDialog(const CProgramRulePtr& pRule)
{
	auto Current = theGUI->GetCurrentItems();

	QSet<CProgramItemPtr> Items;
	if(Current.bAllPrograms)
		Items = theCore->ProgramManager()->GetItems();
	else
		Items = Current.Items;
	CProgramRuleWnd* pProgramRuleWnd = new CProgramRuleWnd(pRule, Items);
	pProgramRuleWnd->show();
}

void CProgramRuleView::OnMenu(const QPoint& Point)
{
	int Count = m_SelectedItems.count();

	m_pEnableRule->setEnabled(Count > 0);
	m_pDisableRule->setEnabled(Count > 0);
	m_pRemoveRule->setEnabled(Count > 0);
	m_pEditRule->setEnabled(Count == 1);
	m_pCloneRule->setEnabled(Count > 0);

	CPanelView::OnMenu(Point);
}

void CProgramRuleView::OnRuleAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());

	QList<STATUS> Results;

	if (pAction == m_pCreateRule)
	{
		CProgramRulePtr pRule = CProgramRulePtr(new CProgramRule());
		OpenRullDialog(pRule);
	}
	else if (pAction == m_pEnableRule)
	{
		foreach(const CProgramRulePtr & pRule, m_SelectedItems) {
			pRule->SetEnabled(true);
			Results << theCore->ProgramManager()->SetProgramRule(pRule);
		}
	}
	else if (pAction == m_pDisableRule)
	{
		foreach(const CProgramRulePtr & pRule, m_SelectedItems) {
			pRule->SetEnabled(false);
			Results << theCore->ProgramManager()->SetProgramRule(pRule);
		}
	}
	else if (pAction == m_pRemoveRule)
	{
		if(QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete selected Rules Items?") , QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes)
			return;

		foreach(const CProgramRulePtr & pRule, m_SelectedItems) {
			Results << theCore->ProgramManager()->DelProgramRule(pRule);
		}
	}
	else if (pAction == m_pEditRule)
	{
		OpenRullDialog(m_CurrentItem);
	}
	else if (pAction == m_pCloneRule)
	{
		foreach(const CProgramRulePtr & pRule, m_SelectedItems) {
			CProgramRulePtr pClone = CProgramRulePtr(pRule->Clone());
			Results << theCore->ProgramManager()->SetProgramRule(pClone);
		}
	}

	theGUI->CheckResults(Results, this);
}