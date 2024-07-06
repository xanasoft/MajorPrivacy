#include "pch.h"
#include "FwRuleView.h"
#include "../Core/PrivacyCore.h"
#include "../Windows/FirewallRuleWnd.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Library/API/PrivacyAPI.h" // todo
#include "../MiscHelpers/Common/CustomStyles.h"

CFwRuleView::CFwRuleView(QWidget *parent)
	:CPanelViewEx<CFwRuleModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	QByteArray Columns = theConf->GetBlob("MainWindow/FWRuleView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pCreateRule = m_pMenu->addAction(QIcon(":/Icons/Plus.png"), tr("Create Rule"), this, SLOT(OnRuleAction()));
	m_pMenu->addSeparator();
	m_pEnableRule = m_pMenu->addAction(QIcon(":/Icons/Enable.png"), tr("Enable Rule"), this, SLOT(OnRuleAction()));
	m_pDisableRule = m_pMenu->addAction(QIcon(":/Icons/Disable.png"), tr("Disable Rule"), this, SLOT(OnRuleAction()));
	m_pMenu->addSeparator();
	m_pRuleBlock = m_pMenu->addAction(QIcon(":/Icons/Stop.png"), tr("Set Blocking"), this, SLOT(OnRuleAction()));
	m_pRuleAllow = m_pMenu->addAction(QIcon(":/Icons/Go.png"), tr("Set Allowing"), this, SLOT(OnRuleAction()));
	m_pMenu->addSeparator();
	m_pRemoveRule = m_pMenu->addAction(QIcon(":/Icons/Remove.png"), tr("Delete Rule"), this, SLOT(OnRuleAction()));
	m_pEditRule = m_pMenu->addAction(QIcon(":/Icons/EditIni.png"), tr("Edit Rule"), this, SLOT(OnRuleAction()));
	m_pCloneRule = m_pMenu->addAction(QIcon(":/Icons/Duplicate.png"), tr("Duplicate Rule"), this, SLOT(OnRuleAction()));


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

	OpenRullDialog(pRule);
}

void CFwRuleView::OpenRullDialog(const CFwRulePtr& pRule)
{
	auto Current = theGUI->GetCurrentItems();

	QSet<CProgramItemPtr> Items;
	if(Current.bAllPrograms)
		Items = theCore->ProgramManager()->GetItems();
	else
		Items = Current.Items;
	CFirewallRuleWnd* pFirewallRuleWnd = new CFirewallRuleWnd(pRule, Items);
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
		OpenRullDialog(pRule);
	}
	else if (pAction == m_pEnableRule)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			pRule->SetEnabled(true);
			Results << theCore->NetworkManager()->SetFwRule(pRule);
		}
	}
	else if (pAction == m_pDisableRule)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			pRule->SetEnabled(false);
			Results << theCore->NetworkManager()->SetFwRule(pRule);
		}
	}
	else if (pAction == m_pRuleBlock)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			pRule->SetAction(EFwActions::Block);
			Results << theCore->NetworkManager()->SetFwRule(pRule);
		}
	}
	else if (pAction == m_pRuleAllow)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			pRule->SetAction(EFwActions::Allow);
			Results << theCore->NetworkManager()->SetFwRule(pRule);
		}
	}
	else if (pAction == m_pRemoveRule)
	{
		if(QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete selected Rules Items?") , QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes)
			return;

		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			Results << theCore->NetworkManager()->DelFwRule(pRule);
		}
	}
	else if (pAction == m_pEditRule)
	{
		OpenRullDialog(m_CurrentItem);
	}
	else if (pAction == m_pCloneRule)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			CFwRulePtr pClone = CFwRulePtr(pRule->Clone());
			Results << theCore->NetworkManager()->SetFwRule(pClone);
		}
	}

	theGUI->CheckResults(Results, this);
}