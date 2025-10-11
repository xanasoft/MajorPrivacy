#include "pch.h"
#include "FwRuleView.h"
#include "../Core/PrivacyCore.h"
#include "../Windows/FirewallRuleWnd.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Library/API/PrivacyAPI.h" // todo
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../../MiscHelpers/Common/Common.h"
#include "../../MiscHelpers/Common/OtherFunctions.h"

CFwRuleView::CFwRuleView(QWidget *parent)
	:CPanelViewEx<CFwRuleModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	QByteArray Columns = theConf->GetBlob("MainWindow/FWRuleView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);
	
	m_pEnableRule = m_pMenu->addAction(QIcon(":/Icons/Enable.png"), tr("Enable Rule"), this, SLOT(OnRuleAction()));
	m_pDisableRule = m_pMenu->addAction(QIcon(":/Icons/Disable.png"), tr("Disable Rule"), this, SLOT(OnRuleAction()));
	m_pMenu->addSeparator();
	m_pApproveRule = m_pMenu->addAction(QIcon(":/Icons/Approve.png"), tr("Approve Rule"), this, SLOT(OnRuleAction()));
	m_pRestoreRule = m_pMenu->addAction(QIcon(":/Icons/Recover.png"), tr("Restore Rule"), this, SLOT(OnRuleAction()));
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

	m_pCmbDir = new QComboBox();
	m_pCmbDir->addItem(QIcon(":/Icons/ArrowUpDown.png"), tr("All Directions"), (qint32)EFwDirections::Bidirectional);
	m_pCmbDir->addItem(QIcon(":/Icons/ArrowDown.png"), tr("Inbound"), (qint32)EFwDirections::Inbound);
	m_pCmbDir->addItem(QIcon(":/Icons/ArrowUp.png"), tr("Outbound"), (qint32)EFwDirections::Outbound);
	m_pToolBar->addWidget(m_pCmbDir);
	
	m_pCmbAction = new QComboBox();
	m_pCmbAction->addItem(QIcon(":/Icons/NoAccess.png"), tr("All Actions"), (qint32)EFwActions::Undefined);
	m_pCmbAction->addItem(QIcon(":/Icons/Go.png"), tr("Allow"), (qint32)EFwActions::Allow);
	m_pCmbAction->addItem(QIcon(":/Icons/Disable.png"), tr("Block"), (qint32)EFwActions::Block);
	m_pToolBar->addWidget(m_pCmbAction);
	
	m_pBtnEnabled = new QToolButton();
	m_pBtnEnabled->setIcon(QIcon(":/Icons/Disable.png"));
	m_pBtnEnabled->setCheckable(true);
	m_pBtnEnabled->setToolTip(tr("Hide Disabled Rules"));
	m_pBtnEnabled->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnEnabled);

	m_pBtnHideWin = new QToolButton();
	m_pBtnHideWin->setIcon(QIcon(QPixmap::fromImage(ImageAddOverlay(QImage(":/Icons/Windows.png"), ":/Icons/Disable.png", 40))));
	m_pBtnHideWin->setCheckable(true);
	m_pBtnHideWin->setToolTip(tr("Hide default rules"));
	m_pBtnHideWin->setMaximumHeight(22);
	//connect(m_pBtnHideWin, SIGNAL(clicked()), this, SLOT(OnRefresh()));
	m_pToolBar->addWidget(m_pBtnHideWin);

	m_pToolBar->addSeparator();

	m_pBtnRefresh = new QToolButton();
	m_pBtnRefresh->setIcon(QIcon(":/Icons/Recover.png"));
	m_pBtnRefresh->setToolTip(tr("Refresh Rules"));
	m_pBtnRefresh->setFixedHeight(22);
	m_pBtnRefresh->setShortcut(QKeySequence::fromString("F5"));
	connect(m_pBtnRefresh, SIGNAL(clicked()), this, SLOT(Refresh()));
	m_pToolBar->addWidget(m_pBtnRefresh);

	m_pToolBar->addSeparator();

	m_pBtnCleanUp = new QToolButton();
	m_pBtnCleanUp->setIcon(QIcon(":/Icons/Clean.png"));
	m_pBtnCleanUp->setToolTip(tr("Remove Temporary Rules"));
	m_pBtnCleanUp->setFixedHeight(22);
	connect(m_pBtnCleanUp, SIGNAL(clicked()), this, SLOT(CleanTemp()));
	m_pToolBar->addWidget(m_pBtnCleanUp);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);


	AddPanelItemsToMenu();
}

CFwRuleView::~CFwRuleView()
{
	theConf->SetBlob("MainWindow/FWRuleView_Columns", m_pTreeView->saveState());
}

void CFwRuleView::Sync(QList<CFwRulePtr> RuleList)
{
	m_RuleList = RuleList;
	bool bHideDisabled = m_pBtnEnabled->isChecked();
	bool bHideWin = m_pBtnHideWin->isChecked();
	EFwDirections Dir = (EFwDirections)m_pCmbDir->currentData().toInt();
	EFwActions Type = (EFwActions)m_pCmbAction->currentData().toInt();

	if (bHideDisabled || bHideWin || Type != EFwActions::Undefined || Dir != EFwDirections::Bidirectional)
	{
		for (auto I = RuleList.begin(); I != RuleList.end();)
		{
			if (bHideWin && (*I)->GetSource() == EFwRuleSource::eWindowsDefault)
				I = RuleList.erase(I);
			else if (bHideDisabled && !(*I)->IsEnabled())
				I = RuleList.erase(I);
			else if (Dir != EFwDirections::Bidirectional && (*I)->GetDirection() != Dir)
				I = RuleList.erase(I);
			else if (Type != EFwActions::Undefined && (*I)->GetAction() != Type)
				I = RuleList.erase(I);
			else
				++I;
		}
	}

	m_pItemModel->Sync(RuleList);
}

void CFwRuleView::Clear()
{
	m_RuleList.clear();
	m_pItemModel->Clear();
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
	CFwRulePtr pClone = CFwRulePtr(pRule->Clone(true)); // clone the rule as not to edit the once storred in case the set fails
	CFirewallRuleWnd* pFirewallRuleWnd = new CFirewallRuleWnd(pClone, Current.Items);
	pFirewallRuleWnd->show();
}

void CFwRuleView::OnMenu(const QPoint& Point)
{
	int iEnabledCount = 0;
	int iDisabledCount = 0;
	int iUnaproved = 0;
	int iBackups = 0;
	int iBlockCount = 0;
	int iAllowCount = 0;
	int iSelectedCount = 0;
	foreach(auto pItem, m_SelectedItems) 
	{
		if (pItem->GetState() == EFwRuleState::eUnapproved || pItem->GetState() == EFwRuleState::eUnapprovedDisabled)
			iUnaproved++;
		else if (pItem->GetState() == EFwRuleState::eBackup) {
			iBackups++;
			continue;
		}

		if (pItem->IsEnabled())
			iEnabledCount++;
		else
			iDisabledCount++;

		if (pItem->GetAction() == EFwActions::Block)
			iBlockCount++;
		else if (pItem->GetAction() == EFwActions::Allow)
			iAllowCount++;

		iSelectedCount++;
	}

	m_pEnableRule->setEnabled(iDisabledCount > 0);
	m_pDisableRule->setEnabled(iEnabledCount > 0);
	m_pApproveRule->setEnabled(iUnaproved > 0);
	m_pRestoreRule->setEnabled(iBackups > 0);
	m_pRuleBlock->setEnabled(iAllowCount > 0);
	m_pRuleAllow->setEnabled(iBlockCount > 0);
	m_pRemoveRule->setEnabled(iSelectedCount > 0 || iBackups > 0);
	m_pEditRule->setEnabled(iSelectedCount == 1);
	m_pCloneRule->setEnabled(iSelectedCount > 0 || iBackups > 0);

	CPanelView::OnMenu(Point);
}

void CFwRuleView::OnAddRule()
{
	CFwRulePtr pRule = CFwRulePtr(new CFwRule(CProgramID()));
	pRule->SetApproved();
	pRule->SetSource(EFwRuleSource::eMajorPrivacy);
	OpenRullDialog(pRule);
}

void CFwRuleView::OnRuleAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());

	QList<STATUS> Results;

	if (pAction == m_pEnableRule)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			if (!pRule->IsEnabled())
			{
				pRule->SetEnabled(true);
				STATUS Status = theCore->NetworkManager()->SetFwRule(pRule);
				if(Status.IsError())
					pRule->SetEnabled(false);
				Results << Status;
			}
		}
	}
	else if (pAction == m_pDisableRule)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			if (pRule->IsEnabled())
			{
				pRule->SetEnabled(false);
				STATUS Status = theCore->NetworkManager()->SetFwRule(pRule);
				if(Status.IsError())
					pRule->SetEnabled(true);
				Results << Status;
			}
		}
	}
	else if (pAction == m_pApproveRule)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			if (pRule->IsUnapproved())
			{
				if(pRule->GetState() == EFwRuleState::eUnapprovedDisabled)
					pRule->SetEnabled(true);
				pRule->SetApproved();
				STATUS Status = theCore->NetworkManager()->SetFwRule(pRule);
				Results << Status;
			}
		}
	}
	else if (pAction == m_pRestoreRule)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			if (pRule->IsBackup())
			{
				pRule->SetApproved();
				STATUS Status = theCore->NetworkManager()->SetFwRule(pRule);
				Results << Status;
			}
		}
	}
	else if (pAction == m_pRuleBlock)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			auto Action = pRule->GetAction();
			if (Action != EFwActions::Block)
			{
				pRule->SetAction(EFwActions::Block);
				STATUS Status = theCore->NetworkManager()->SetFwRule(pRule);
				if(Status.IsError())
					pRule->SetAction(Action);
				Results << Status;
			}
		}
	}
	else if (pAction == m_pRuleAllow)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			auto Action = pRule->GetAction();
			if (Action != EFwActions::Allow)
			{
				pRule->SetAction(EFwActions::Allow);
				STATUS Status = theCore->NetworkManager()->SetFwRule(pRule);
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

void CFwRuleView::Refresh()
{
	theCore->NetworkManager()->UpdateAllFwRules(true);
}

void CFwRuleView::CleanTemp()
{
	if (QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete all Temporary Rules?"), QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes)
		return;

	QList<STATUS> Results;
	foreach(const CFwRulePtr & pRule, m_RuleList) {
		if (pRule->IsTemporary())
			Results << theCore->NetworkManager()->DelFwRule(pRule);
	}
	theGUI->CheckResults(Results, this);
}