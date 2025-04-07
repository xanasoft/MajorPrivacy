#include "pch.h"
#include "DnsRuleView.h"
#include "../Core/PrivacyCore.h"
#include "../Windows/FirewallRuleWnd.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Library/API/PrivacyAPI.h" // todo
#include "../MiscHelpers/Common/CustomStyles.h"

CDnsRuleView::CDnsRuleView(QWidget *parent)
	:CPanelViewEx<CDnsRuleModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	QByteArray Columns = theConf->GetBlob("MainWindow/DnsRuleView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);
	
	m_pEnableRule = m_pMenu->addAction(QIcon(":/Icons/Enable.png"), tr("Enable Rule"), this, SLOT(OnRuleAction()));
	m_pDisableRule = m_pMenu->addAction(QIcon(":/Icons/Disable.png"), tr("Disable Rule"), this, SLOT(OnRuleAction()));
	m_pMenu->addSeparator();
	m_pRuleBlock = m_pMenu->addAction(QIcon(":/Icons/Stop.png"), tr("Set Blocking"), this, SLOT(OnRuleAction()));
	m_pRuleAllow = m_pMenu->addAction(QIcon(":/Icons/Go.png"), tr("Set Allowing"), this, SLOT(OnRuleAction()));
	m_pMenu->addSeparator();
	m_pRemoveRule = m_pMenu->addAction(QIcon(":/Icons/Remove.png"), tr("Delete Rule"), this, SLOT(OnRuleAction()));
	m_pRemoveRule->setShortcut(QKeySequence::Delete);
	m_pRemoveRule->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pRemoveRule);
	m_pEditRule = m_pMenu->addAction(QIcon(":/Icons/EditIni.png"), tr("Edit Rule"), this, SLOT(OnRuleAction()));
	m_pCloneRule = m_pMenu->addAction(QIcon(":/Icons/Duplicate.png"), tr("Duplicate Rule"), this, SLOT(OnRuleAction()));
	m_pMenu->addSeparator();
	m_pCreateRule = m_pMenu->addAction(QIcon(":/Icons/Add.png"), tr("Create Rule"), this, SLOT(OnAddRule()));

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);
	
	m_pBtnAdd = new QToolButton();
	m_pBtnAdd->setIcon(QIcon(":/Icons/Add.png"));
	m_pBtnAdd->setToolTip(tr("Create Rule"));
	m_pBtnAdd->setMaximumHeight(22);
	connect(m_pBtnAdd, SIGNAL(clicked()), this, SLOT(OnAddRule()));
	m_pToolBar->addWidget(m_pBtnAdd);
	m_pToolBar->addSeparator();
	
	m_pCmbAction = new QComboBox();
	m_pCmbAction->addItem(QIcon(":/Icons/NoAccess.png"), tr("All Actions"), (qint32)CDnsRule::eUndefined);
	m_pCmbAction->addItem(QIcon(":/Icons/Go.png"), tr("Allow"), (qint32)CDnsRule::eAllow);
	m_pCmbAction->addItem(QIcon(":/Icons/Disable.png"), tr("Block"), (qint32)CDnsRule::eBlock);
	m_pToolBar->addWidget(m_pCmbAction);
	
	m_pBtnEnabled = new QToolButton();
	m_pBtnEnabled->setIcon(QIcon(":/Icons/Disable.png"));
	m_pBtnEnabled->setCheckable(true);
	m_pBtnEnabled->setToolTip(tr("Hide Disabled Rules"));
	m_pBtnEnabled->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnEnabled);

	m_pToolBar->addSeparator();

	m_pBtnRefresh = new QToolButton();
	m_pBtnRefresh->setIcon(QIcon(":/Icons/Recover.png"));
	m_pBtnRefresh->setToolTip(tr("Refresh Rules"));
	m_pBtnRefresh->setFixedHeight(22);
	m_pBtnRefresh->setShortcut(QKeySequence::fromString("F5"));
	connect(m_pBtnRefresh, SIGNAL(clicked()), this, SLOT(Refresh()));
	m_pToolBar->addWidget(m_pBtnRefresh);

	//m_pToolBar->addSeparator();

	//m_pBtnCleanUp = new QToolButton();
	//m_pBtnCleanUp->setIcon(QIcon(":/Icons/Clean.png"));
	//m_pBtnCleanUp->setToolTip(tr("Remove Temporary Rules"));
	//m_pBtnCleanUp->setFixedHeight(22);
	//connect(m_pBtnCleanUp, SIGNAL(clicked()), this, SLOT(CleanTemp()));
	//m_pToolBar->addWidget(m_pBtnCleanUp)


	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);


	AddPanelItemsToMenu();
}

CDnsRuleView::~CDnsRuleView()
{
	theConf->SetBlob("MainWindow/DnsRuleView_Columns", m_pTreeView->saveState());
}

void CDnsRuleView::Sync(QList<CDnsRulePtr> RuleList)
{
	m_RuleList = RuleList;
	bool bHideDisabled = m_pBtnEnabled->isChecked();
	CDnsRule::EAction Type = (CDnsRule::EAction)m_pCmbAction->currentData().toInt();

	if (bHideDisabled || Type != CDnsRule::EAction::eUndefined)
	{
		for (auto I = RuleList.begin(); I != RuleList.end();)
		{
			if (bHideDisabled && !(*I)->IsEnabled())
				I = RuleList.erase(I);
			else if (Type != CDnsRule::EAction::eUndefined && (*I)->GetAction() != Type)
				I = RuleList.erase(I);
			else
				++I;
		}
	}

	m_pItemModel->Sync(RuleList);
}

void CDnsRuleView::OnDoubleClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CDnsRulePtr pRule = m_pItemModel->GetItem(ModelIndex);
	if (pRule.isNull())
		return;

	OpenRullDialog(pRule);
}

void CDnsRuleView::OpenRullDialog(const CDnsRulePtr& pRule)
{
	//auto Current = theGUI->GetCurrentItems();
	//CDnsRuleWnd* pDnsRuleWnd = new CDnsRuleWnd(pRule, Current.Items);
	//pDnsRuleWnd->show();

	QString HostName = pRule->GetHostName();
	HostName = QInputDialog::getText(this, tr("Edit Dns Rule"), tr("Enter Host Name"), QLineEdit::Normal, HostName);
	if (HostName.isEmpty())
		return;
	pRule->SetHostName(HostName);
	STATUS Status = theCore->NetworkManager()->SetDnsRule(pRule);
	theGUI->CheckResults(QList<STATUS>() << Status, this);
}

void CDnsRuleView::OnMenu(const QPoint& Point)
{
	int iEnabledCount = 0;
	int iDisabledCount = 0;
	int iBlockCount = 0;
	int iAllowCount = 0;
	int iSelectedCount = 0;
	foreach(auto pItem, m_SelectedItems) {
		if (pItem->IsEnabled())
			iEnabledCount++;
		else
			iDisabledCount++;
		if (pItem->GetAction() == CDnsRule::eBlock)
			iBlockCount++;
		else if (pItem->GetAction() == CDnsRule::eAllow)
			iAllowCount++;
		iSelectedCount++;
	}

	m_pEnableRule->setEnabled(iDisabledCount > 0);
	m_pDisableRule->setEnabled(iEnabledCount > 0);
	m_pRuleBlock->setEnabled(iAllowCount > 0);
	m_pRuleAllow->setEnabled(iBlockCount > 0);
	m_pRemoveRule->setEnabled(iSelectedCount > 0);
	m_pEditRule->setEnabled(iSelectedCount == 1);
	m_pCloneRule->setEnabled(iSelectedCount > 0);

	CPanelView::OnMenu(Point);
}

void CDnsRuleView::OnAddRule()
{
	CDnsRulePtr pRule = CDnsRulePtr(new CDnsRule());
	pRule->SetAction(CDnsRule::EAction::eBlock);
	OpenRullDialog(pRule);
}

void CDnsRuleView::OnRuleAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());

	QList<STATUS> Results;

	if (pAction == m_pEnableRule)
	{
		foreach(const CDnsRulePtr & pRule, m_SelectedItems) {
			if (!pRule->IsEnabled())
			{
				pRule->SetEnabled(true);
				STATUS Status = theCore->NetworkManager()->SetDnsRule(pRule);
				if(Status.IsError())
					pRule->SetEnabled(false);
				Results << Status;
			}
		}
	}
	else if (pAction == m_pDisableRule)
	{
		foreach(const CDnsRulePtr & pRule, m_SelectedItems) {
			if (pRule->IsEnabled())
			{
				pRule->SetEnabled(false);
				STATUS Status = theCore->NetworkManager()->SetDnsRule(pRule);
				if(Status.IsError())
					pRule->SetEnabled(true);
				Results << Status;
			}
		}
	}
	else if (pAction == m_pRuleBlock)
	{
		foreach(const CDnsRulePtr & pRule, m_SelectedItems) {
			auto Action = pRule->GetAction();
			if (Action != CDnsRule::eBlock)
			{
				pRule->SetAction(CDnsRule::eBlock);
				STATUS Status = theCore->NetworkManager()->SetDnsRule(pRule);
				if(Status.IsError())
					pRule->SetAction(Action);
				Results << Status;
			}
		}
	}
	else if (pAction == m_pRuleAllow)
	{
		foreach(const CDnsRulePtr & pRule, m_SelectedItems) {
			auto Action = pRule->GetAction();
			if (Action != CDnsRule::eAllow)
			{
				pRule->SetAction(CDnsRule::eAllow);
				STATUS Status = theCore->NetworkManager()->SetDnsRule(pRule);
				if(Status.IsError())
					pRule->SetAction(Action);
				Results << Status;
			}
		}
	}
	else if (pAction == m_pRemoveRule)
	{
		if(QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete selected Rules Items?") , QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes)
			return;

		foreach(const CDnsRulePtr & pRule, m_SelectedItems) {
			Results << theCore->NetworkManager()->DelDnsRule(pRule);
		}
	}
	else if (pAction == m_pEditRule)
	{
		OpenRullDialog(m_CurrentItem);
	}
	else if (pAction == m_pCloneRule)
	{
		foreach(const CDnsRulePtr & pRule, m_SelectedItems) {
			CDnsRulePtr pClone = CDnsRulePtr(pRule->Clone());
			Results << theCore->NetworkManager()->SetDnsRule(pClone);
		}
	}

	theGUI->CheckResults(Results, this);
}

void CDnsRuleView::Refresh()
{
	theCore->NetworkManager()->UpdateAllDnsRules();
}

//void CDnsRuleView::CleanTemp()
//{
//	if (QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete all Temporary Rules?"), QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes)
//		return;
//
//	QList<STATUS> Results;
//	foreach(const CDnsRulePtr & pRule, m_RuleList) {
//		if (pRule->IsTemporary())
//			Results << theCore->NetworkManager()->DelFwRule(pRule);
//	}
//	theGUI->CheckResults(Results, this);
//}