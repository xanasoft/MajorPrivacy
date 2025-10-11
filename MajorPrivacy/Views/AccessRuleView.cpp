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
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	QByteArray Columns = theConf->GetBlob("MainWindow/AccessRuleView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pEnableRule = m_pMenu->addAction(QIcon(":/Icons/Enable.png"), tr("Enable Rule"), this, SLOT(OnRuleAction()));
	m_pDisableRule = m_pMenu->addAction(QIcon(":/Icons/Disable.png"), tr("Disable Rule"), this, SLOT(OnRuleAction()));
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
	m_pCmbAction->addItem(QIcon(":/Icons/NoAccess.png"), tr("All Actions"), (qint32)EAccessRuleType::eNone);
	m_pCmbAction->addItem(QIcon(":/Icons/Go.png"), tr("Allow"), (qint32)EAccessRuleType::eAllow);
	m_pCmbAction->addItem(QIcon(":/Icons/Go2.png"), tr("Allow Read-Only"), (qint32)EAccessRuleType::eAllowRO);
	m_pCmbAction->addItem(QIcon(":/Icons/Go3.png"), tr("Allow Listing"), (qint32)EAccessRuleType::eEnum);
	m_pCmbAction->addItem(QIcon(":/Icons/Shield16.png"), tr("Protect"), (qint32)EAccessRuleType::eProtect);
	m_pCmbAction->addItem(QIcon(":/Icons/Disable.png"), tr("Block"), (qint32)EAccessRuleType::eBlock);
	m_pCmbAction->addItem(QIcon(":/Icons/Invisible.png"), tr("Don't Log"), (qint32)EAccessRuleType::eIgnore);
	m_pToolBar->addWidget(m_pCmbAction);

	m_pBtnEnabled = new QToolButton();
	m_pBtnEnabled->setIcon(QIcon(":/Icons/Disable.png"));
	m_pBtnEnabled->setCheckable(true);
	m_pBtnEnabled->setToolTip(tr("Hide Disabled Rules"));
	m_pBtnEnabled->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnEnabled);

	m_pBtnTemp = new QToolButton();
	m_pBtnTemp->setIcon(QIcon(":/Icons/Invisible.png"));
	m_pBtnTemp->setCheckable(true);
	m_pBtnTemp->setToolTip(tr("Show Hidden Rules"));
	m_pBtnTemp->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnTemp);

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

CAccessRuleView::~CAccessRuleView()
{
	theConf->SetBlob("MainWindow/AccessRuleView_Columns", m_pTreeView->saveState());
}

void CAccessRuleView::Sync(QList<CAccessRulePtr> RuleList, const QString& VolumeRoot, const QString& VolumeImage)
{
	m_RuleList = RuleList;
	m_VolumeRoot = VolumeRoot;
	m_VolumeImage = VolumeImage;

	bool bHideDisabled = m_pBtnEnabled->isChecked();
	bool bHideTemp = !m_pBtnTemp->isChecked();
	EAccessRuleType Type = (EAccessRuleType)m_pCmbAction->currentData().toInt();

	if (bHideDisabled || bHideTemp || Type != EAccessRuleType::eNone || !VolumeImage.isNull()) 
	{
		if(/*!VolumeRoot.isNull() && VolumeRoot.isEmpty() &&*/ !VolumeImage.isNull() && VolumeImage.isEmpty())
			RuleList.clear();
		else

		for (auto I = RuleList.begin(); I != RuleList.end();)
		{
			if (bHideDisabled && !(*I)->IsEnabled())
				I = RuleList.erase(I);
			else if (bHideTemp && (*I)->IsTemporary() && (*I)->IsHidden())
				I = RuleList.erase(I);
			else if (Type != EAccessRuleType::eNone && (*I)->GetType() != Type)
				I = RuleList.erase(I);
			else if ((/*!VolumeRoot.isEmpty() ||*/ !VolumeImage.isEmpty()) && !PathStartsWith((*I)->GetPath(), VolumeRoot) && !PathStartsWith((*I)->GetPath(), VolumeImage))
				I = RuleList.erase(I);
			else
				++I;
		}
	}

	m_pItemModel->Sync(RuleList);
}

void CAccessRuleView::Clear()
{
	m_RuleList.clear();
	m_VolumeRoot.clear();
	m_VolumeImage.clear();
	m_pItemModel->Clear();
}

void CAccessRuleView::OnDoubleClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CAccessRulePtr pRule = m_pItemModel->GetItem(ModelIndex);
	if (pRule.isNull() || pRule->IsTemporary())
		return;

	OpenRullDialog(pRule);
}

void CAccessRuleView::OpenRullDialog(const CAccessRulePtr& pRule)
{
	auto Current = theGUI->GetCurrentItems();
	CAccessRuleWnd* pAccessRuleWnd = new CAccessRuleWnd(pRule, Current.Items, m_VolumeRoot, m_VolumeImage);
	pAccessRuleWnd->show();
}

void CAccessRuleView::OnMenu(const QPoint& Point)
{
	int iEnabledCount = 0;
	int iDisabledCount = 0;
	int iTempCount = 0;
	int iSelectedCount = 0;
	foreach(auto pItem, m_SelectedItems) {
		if (pItem->IsEnabled())
			iEnabledCount++;
		else
			iDisabledCount++;
		if (pItem->IsTemporary())
			iTempCount++;
		iSelectedCount++;
	}

	m_pEnableRule->setEnabled(iDisabledCount > 0 && iTempCount == 0);
	m_pDisableRule->setEnabled(iEnabledCount > 0 && iTempCount == 0);
	m_pRemoveRule->setEnabled(iSelectedCount > 0 && iTempCount == 0);
	m_pEditRule->setEnabled(iSelectedCount == 1 && iTempCount == 0);
	m_pCloneRule->setEnabled(iSelectedCount > 0 && iTempCount == 0);

	CPanelView::OnMenu(Point);
}

void CAccessRuleView::OnAddRule()
{
	CAccessRulePtr pRule = CAccessRulePtr(new CAccessRule());
	OpenRullDialog(pRule);
}

void CAccessRuleView::OnRuleAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());

	QList<STATUS> Results;

	if (pAction == m_pEnableRule)
	{
		foreach(const CAccessRulePtr & pRule, m_SelectedItems) {
			if (!pRule->IsEnabled())
			{
				pRule->SetEnabled(true);
				STATUS Status = theCore->AccessManager()->SetAccessRule(pRule);
				if(Status.IsError())
					pRule->SetEnabled(false);
				Results << Status;
			}
		}
	}
	else if (pAction == m_pDisableRule)
	{
		foreach(const CAccessRulePtr & pRule, m_SelectedItems) {
			if (pRule->IsEnabled())
			{
				pRule->SetEnabled(false);
				STATUS Status = theCore->AccessManager()->SetAccessRule(pRule);
				if(Status.IsError())
					pRule->SetEnabled(true);
				Results << Status;
			}
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

void CAccessRuleView::Refresh()
{
	theCore->AccessManager()->UpdateAllAccessRules();
}

void CAccessRuleView::CleanTemp()
{
	if (QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete all Temporary Rules?"), QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes)
		return;

	QList<STATUS> Results;
	foreach(const CAccessRulePtr & pRule, m_RuleList) {
		if (pRule->IsTemporary() && !pRule->IsHidden())
			Results << theCore->AccessManager()->DelAccessRule(pRule);
	}

	theGUI->CheckResults(Results, this);
}