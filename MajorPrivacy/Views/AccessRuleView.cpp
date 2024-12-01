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

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbAction = new QComboBox();
	m_pCmbAction->addItem(QIcon(":/Icons/NoAccess.png"), tr("All Actions"), (qint32)EAccessRuleType::eNone);
	m_pCmbAction->addItem(QIcon(":/Icons/Go.png"), tr("Allow"), (qint32)EAccessRuleType::eAllow);
	m_pCmbAction->addItem(QIcon(":/Icons/Go2.png"), tr("Allow Read-Only"), (qint32)EAccessRuleType::eAllowRO);
	m_pCmbAction->addItem(QIcon(":/Icons/Go3.png"), tr("Allow Listing"), (qint32)EAccessRuleType::eEnum);
	m_pCmbAction->addItem(QIcon(":/Icons/Shield16.png"), tr("Protect"), (qint32)EAccessRuleType::eProtect);
	m_pCmbAction->addItem(QIcon(":/Icons/Disable.png"), tr("Block"), (qint32)EAccessRuleType::eBlock);
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

	m_pCreateRule = m_pMenu->addAction(QIcon(":/Icons/Add.png"), tr("Create Rule"), this, SLOT(OnRuleAction()));
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

void CAccessRuleView::Sync(QList<CAccessRulePtr> RuleList, const QString& VolumeRoot, const QString& VolumeImage)
{
	m_VolumeRoot = VolumeRoot;
	m_VolumeImage = VolumeImage;

	bool bHideDisabled = m_pBtnEnabled->isChecked();
	EAccessRuleType Type = (EAccessRuleType)m_pCmbAction->currentData().toInt();

	if (bHideDisabled || Type != EAccessRuleType::eNone || !VolumeImage.isNull()) 
	{
		if(!VolumeImage.isNull() && VolumeImage.isEmpty())
			RuleList.clear();
		else

		for (auto I = RuleList.begin(); I != RuleList.end();)
		{
			if (bHideDisabled && !(*I)->IsEnabled())
				I = RuleList.erase(I);
			else if (Type != EAccessRuleType::eNone && (*I)->GetType() != Type)
				I = RuleList.erase(I);
			else if (!VolumeImage.isEmpty() && !(*I)->GetPath().startsWith(VolumeImage + "/", Qt::CaseInsensitive)
			  && (VolumeRoot.isEmpty() || !(*I)->GetPath().startsWith(VolumeRoot, Qt::CaseInsensitive)))
				I = RuleList.erase(I);
			else
				++I;
		}
	}

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
	CAccessRuleWnd* pAccessRuleWnd = new CAccessRuleWnd(pRule, Items, m_VolumeRoot, m_VolumeImage);
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