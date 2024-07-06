#include "pch.h"
#include "AccessRuleView.h"
#include "../Core/PrivacyCore.h"
#include "../Windows/AccessRuleWnd.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Library/API/PrivacyAPI.h" // todo
#include "../MiscHelpers/Common/CustomStyles.h"

CAccessRuleView::CAccessRuleView(QWidget *parent)
	:CPanelViewEx<CAccessRuleModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	QByteArray Columns = theConf->GetBlob("MainWindow/AccessRuleView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

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

CAccessRuleView::~CAccessRuleView()
{
	theConf->SetBlob("MainWindow/AccessRuleView_Columns", m_pTreeView->saveState());
}

void CAccessRuleView::Sync(const QList<CAccessRulePtr>& RuleList)
{
	m_pItemModel->Sync(RuleList);
}

void CAccessRuleView::OnDoubleClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CAccessRulePtr pRule = m_pItemModel->GetItem(ModelIndex);
	if (pRule.isNull())
		return;

	OpenRullDialog(pRule);
}

void CAccessRuleView::OpenRullDialog(const CAccessRulePtr& pRule)
{
	auto Current = theGUI->GetCurrentItems();

	QSet<CProgramItemPtr> Items;
	if(Current.bAllPrograms)
		Items = theCore->ProgramManager()->GetItems();
	else
		Items = Current.Items;
	CAccessRuleWnd* pAccessRuleWnd = new CAccessRuleWnd(pRule, Items);
	pAccessRuleWnd->show();
}

void CAccessRuleView::OnMenu(const QPoint& Point)
{
	int Count = m_SelectedItems.count();

	m_pEnableRule->setEnabled(Count > 0);
	m_pDisableRule->setEnabled(Count > 0);
	m_pRemoveRule->setEnabled(Count > 0);
	m_pEditRule->setEnabled(Count == 1);
	m_pCloneRule->setEnabled(Count > 0);

	CPanelView::OnMenu(Point);
}

void CAccessRuleView::OnRuleAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());

	QList<STATUS> Results;

	if (pAction == m_pCreateRule)
	{
		CAccessRulePtr pRule = CAccessRulePtr(new CAccessRule());
		OpenRullDialog(pRule);
	}
	else if (pAction == m_pEnableRule)
	{
		foreach(const CAccessRulePtr & pRule, m_SelectedItems) {
			pRule->SetEnabled(true);
			Results << theCore->AccessManager()->SetAccessRule(pRule);
		}
	}
	else if (pAction == m_pDisableRule)
	{
		foreach(const CAccessRulePtr & pRule, m_SelectedItems) {
			pRule->SetEnabled(false);
			Results << theCore->AccessManager()->SetAccessRule(pRule);
		}
	}
	else if (pAction == m_pRemoveRule)
	{
		if(QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete selected Rules Items?") , QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes)
			return;

		foreach(const CAccessRulePtr & pRule, m_SelectedItems) {
			Results << theCore->AccessManager()->DelAccessRule(pRule);
		}
	}
	else if (pAction == m_pEditRule)
	{
		OpenRullDialog(m_CurrentItem);
	}
	else if (pAction == m_pCloneRule)
	{
		foreach(const CAccessRulePtr & pRule, m_SelectedItems) {
			CAccessRulePtr pClone = CAccessRulePtr(pRule->Clone());
			Results << theCore->AccessManager()->SetAccessRule(pClone);
		}
	}

	theGUI->CheckResults(Results, this);
}