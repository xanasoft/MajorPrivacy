#include "pch.h"
#include "FwRuleView.h"
#include "../Core/PrivacyCore.h"
#include "../Windows/FirewallRuleWnd.h"
#include "../MajorPrivacy.h"
#include "../Service/ServiceAPI.h" // todo

CFwRuleView::CFwRuleView(QWidget *parent)
	:CPanelViewEx<CFwRuleModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	QByteArray Columns = theConf->GetBlob("MainWindow/FWRuleView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pCreateRule = m_pMenu->addAction(tr("Create Rule"), this, SLOT(OnRuleAction()));
	m_pMenu->addSeparator();
	m_pEnableRule = m_pMenu->addAction(tr("Enable Rule"), this, SLOT(OnRuleAction()));
	m_pDisableRule = m_pMenu->addAction(tr("Disable Rule"), this, SLOT(OnRuleAction()));
	m_pMenu->addSeparator();
	m_pRuleBlock = m_pMenu->addAction(tr("Set Blocking"), this, SLOT(OnRuleAction()));
	m_pRuleAllow = m_pMenu->addAction(tr("Set Allowing"), this, SLOT(OnRuleAction()));
	m_pMenu->addSeparator();
	m_pRemoveRule = m_pMenu->addAction(tr("Delete Rule"), this, SLOT(OnRuleAction()));
	m_pEditRule = m_pMenu->addAction(tr("Edit Rule"), this, SLOT(OnRuleAction()));
	m_pCloneRule = m_pMenu->addAction(tr("Duplicate Rule"), this, SLOT(OnRuleAction()));

	AddPanelItemsToMenu();
}

CFwRuleView::~CFwRuleView()
{
	theConf->SetBlob("MainWindow/FWRuleView_Columns", m_pTreeView->saveState());
}

void CFwRuleView::Sync(const QList<CFwRulePtr>& RuleList)
{
	m_pItemModel->Sync(RuleList);
}

void CFwRuleView::OnDoubleClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CFwRulePtr pRule = m_pItemModel->GetItem(ModelIndex);
	if (pRule.isNull())
		return;

	OpenFwRullDialog(pRule);
}

void CFwRuleView::OpenFwRullDialog(const CFwRulePtr& pRule)
{
	auto Current = theGUI->GetCurrentItems();

	CFirewallRuleWnd* pFirewallRuleWnd = new CFirewallRuleWnd(pRule, Current.Items);
	pFirewallRuleWnd->show();
}

void CFwRuleView::OnMenu(const QPoint& Point)
{
	int Count = m_SelectedItems.count();

	m_pEnableRule->setEnabled(Count > 0);
	m_pDisableRule->setEnabled(Count > 0);
	m_pRuleBlock->setEnabled(Count > 0);
	m_pRuleAllow->setEnabled(Count > 0);
	m_pRemoveRule->setEnabled(Count > 0);
	m_pEditRule->setEnabled(Count == 1);
	m_pCloneRule->setEnabled(Count > 0);

	CPanelView::OnMenu(Point);
}

void CFwRuleView::OnRuleAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());

	QList<STATUS> Results;

	if (pAction == m_pCreateRule)
	{
		CFwRulePtr pRule = CFwRulePtr(new CFwRule(CProgramID()));
		OpenFwRullDialog(pRule);
	}
	else if (pAction == m_pEnableRule)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			pRule->SetEnabled(true);
			Results << theCore->Network()->SetFwRule(pRule);
		}
	}
	else if (pAction == m_pDisableRule)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			pRule->SetEnabled(false);
			Results << theCore->Network()->SetFwRule(pRule);
		}
	}
	else if (pAction == m_pRuleBlock)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			pRule->SetAction(SVC_API_FW_BLOCK);
			Results << theCore->Network()->SetFwRule(pRule);
		}
	}
	else if (pAction == m_pRuleAllow)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			pRule->SetAction(SVC_API_FW_ALLOW);
			Results << theCore->Network()->SetFwRule(pRule);
		}
	}
	else if (pAction == m_pRemoveRule)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			Results << theCore->DelFwRule(pRule->GetGuid());
		}
	}
	else if (pAction == m_pEditRule)
	{
		OpenFwRullDialog(m_CurrentItem);
	}
	else if (pAction == m_pCloneRule)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			CFwRulePtr pClone = CFwRulePtr(pRule->Clone());
			Results << theCore->Network()->SetFwRule(pClone);
		}
	}

	theGUI->CheckResults(Results, this);
}