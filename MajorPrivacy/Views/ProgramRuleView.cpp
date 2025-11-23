#include "pch.h"
#include "ProgramRuleView.h"
#include "../Core/PrivacyCore.h"
#include "../Windows/ProgramRuleWnd.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Library/API/PrivacyAPI.h" // todo
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../MiscHelpers/Common/OtherFunctions.h"
#include "../MiscHelpers/Common/CheckListDialog.h"

CProgramRuleView::CProgramRuleView(QWidget *parent)
	:CPanelViewEx<CProgramRuleModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	QByteArray Columns = theConf->GetBlob("MainWindow/ProgramRuleView_Columns");
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
	m_pExportRules = m_pMenu->addAction(QIcon(":/Icons/Exit.png"), tr("Export Rules"), this, SLOT(OnRuleAction()));
	m_pImportRules = m_pMenu->addAction(QIcon(":/Icons/Entry.png"), tr("Import Rules"), this, SLOT(OnRuleAction()));
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
	m_pCmbAction->addItem(QIcon(":/Icons/NoAccess.png"), tr("All Actions"), (qint32)EExecRuleType::eUnknown);
	m_pCmbAction->addItem(QIcon(":/Icons/Go.png"), tr("Allow"), (qint32)EExecRuleType::eAllow);
	m_pCmbAction->addItem(QIcon(":/Icons/Disable.png"), tr("Block"), (qint32)EExecRuleType::eBlock);
	m_pCmbAction->addItem(QIcon(":/Icons/Shield16.png"), tr("Protect"), (qint32)EExecRuleType::eProtect);
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

	/*m_pToolBar->addSeparator();

	m_pBtnCleanUp = new QToolButton();
	m_pBtnCleanUp->setIcon(QIcon(":/Icons/Clean.png"));
	m_pBtnCleanUp->setToolTip(tr("Remove Temporary Rules"));
	m_pBtnCleanUp->setFixedHeight(22);
	connect(m_pBtnCleanUp, SIGNAL(clicked()), this, SLOT(CleanTemp()));
	m_pToolBar->addWidget(m_pBtnCleanUp);*/

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	AddPanelItemsToMenu();
}

CProgramRuleView::~CProgramRuleView()
{
	theConf->SetBlob("MainWindow/ProgramRuleView_Columns", m_pTreeView->saveState());
}

void CProgramRuleView::Sync(QList<CProgramRulePtr> RuleList, const QFlexGuid& EnclaveGuid)
{
	m_RuleList = RuleList;;
	m_EnclaveGuid = EnclaveGuid;

	bool bHideDisabled = m_pBtnEnabled->isChecked();
	EExecRuleType Type = (EExecRuleType)m_pCmbAction->currentData().toInt();

	if (bHideDisabled || Type != EExecRuleType::eUnknown || !EnclaveGuid.IsNull())
	{
		for (auto I = RuleList.begin(); I != RuleList.end();)
		{
			if (bHideDisabled && !(*I)->IsEnabled())
				I = RuleList.erase(I);
			else if (Type != EExecRuleType::eUnknown && (*I)->GetType() != Type)
				I = RuleList.erase(I);
			else if (!EnclaveGuid.IsNull() && (*I)->GetEnclave() != EnclaveGuid)
				I = RuleList.erase(I);
			else
				++I;
		}
	}

	m_pItemModel->Sync(RuleList);
}

void CProgramRuleView::Clear()
{
	m_RuleList.clear();
	m_pItemModel->Clear();
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
	CProgramRuleWnd* pProgramRuleWnd = new CProgramRuleWnd(pRule, Current.Items);
	pProgramRuleWnd->show();
}

void CProgramRuleView::OnMenu(const QPoint& Point)
{
	int iEnabledCount = 0;
	int iDisabledCount = 0;
	int iSelectedCount = 0;
	foreach(auto pItem, m_SelectedItems) {
		if (pItem->IsEnabled())
			iEnabledCount++;
		else
			iDisabledCount++;
		iSelectedCount++;
	}

	m_pEnableRule->setEnabled(iDisabledCount > 0);
	m_pDisableRule->setEnabled(iEnabledCount > 0);
	m_pRemoveRule->setEnabled(iSelectedCount > 0);
	m_pEditRule->setEnabled(iSelectedCount == 1);
	m_pCloneRule->setEnabled(iSelectedCount > 0);
	m_pExportRules->setEnabled(iSelectedCount > 0);

	CPanelView::OnMenu(Point);
}

void CProgramRuleView::OnAddRule()
{
	if(!theGUI->CheckAcknowledgement("Program Execution Rule"))
		return;

	CProgramRulePtr pRule = CProgramRulePtr(new CProgramRule());
	pRule->SetEnclave(m_EnclaveGuid);
	OpenRullDialog(pRule);
}

void CProgramRuleView::OnRuleAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());

	QList<STATUS> Results;

	if (pAction == m_pEnableRule)
	{
		foreach(const CProgramRulePtr & pRule, m_SelectedItems) {
			if (!pRule->IsEnabled())
			{
				pRule->SetEnabled(true);
				STATUS Status = theCore->ProgramManager()->SetProgramRule(pRule);
				if(Status.IsError())
					pRule->SetEnabled(false);
				Results << Status;
			}
		}
	}
	else if (pAction == m_pDisableRule)
	{
		foreach(const CProgramRulePtr & pRule, m_SelectedItems) {
			if (pRule->IsEnabled())
			{
				pRule->SetEnabled(false);
				STATUS Status = theCore->ProgramManager()->SetProgramRule(pRule);
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
	else if (pAction == m_pImportRules)
	{
		QString FileName = QFileDialog::getOpenFileName(this, tr("Import Program Rules"), "", tr("Rule Files (*.xml)"));
		if (FileName.isEmpty())
			return;

		QString Xml = ReadFileAsString(FileName);

		QtVariant RulesData;
		bool bOk = RulesData.ParseXml(Xml);
		if (!bOk) {
			QMessageBox::warning(this, "MajorPrivacy", tr("Failed to parse XML data!"));
			return;
		}

		SVarWriteOpt Ops;
		Ops.Format = SVarWriteOpt::eMap;
		Ops.Flags = SVarWriteOpt::eSaveToFile | SVarWriteOpt::eTextGuids;

		// Parse rules to get names for selection dialog
		QList<CProgramRulePtr> RulesToImport;
		for (uint32 i = 0; i < RulesData.Count(); i++) {
			CProgramRulePtr pRule = CProgramRulePtr(new CProgramRule());
			pRule->FromVariant(RulesData[i]);
			RulesToImport.append(pRule);
		}

		// Show selection dialog if there are multiple rules
		QList<int> selectedIndices;
		if (RulesToImport.count() > 1) {
			CCheckListDialog dialog(this);
			dialog.setWindowTitle(tr("Select Rules to Import"));
			dialog.setText(tr("Select which program rules you want to import:"));

			for (int i = 0; i < RulesToImport.count(); i++) {
				QString ruleName = RulesToImport[i]->GetName();
				if (ruleName.isEmpty())
					ruleName = tr("Rule %1").arg(i + 1);
				dialog.addItem(ruleName, i, true);
			}

			if (dialog.exec() != QDialog::Accepted)
				return;

			selectedIndices = dialog.getCheckedIndices();
			if (selectedIndices.isEmpty()) {
				QMessageBox::information(this, "MajorPrivacy", tr("No rules selected for import."));
				return;
			}
		} else {
			// Import all if only one rule
			selectedIndices.append(0);
		}

		// Import selected rules
		for (int idx : selectedIndices) {
			CProgramRulePtr pRule = RulesToImport[idx];
			if (!m_EnclaveGuid.IsNull())
				pRule->SetEnclave(m_EnclaveGuid);
			Results << theCore->ProgramManager()->SetProgramRule(pRule);
		}

		if (Results.isEmpty() || !Results.last().IsError())
			QMessageBox::information(this, "MajorPrivacy", tr("Successfully imported %1 rule(s).").arg(selectedIndices.count()));
	}
	else if (pAction == m_pExportRules)
	{
		SVarWriteOpt Ops;
		Ops.Format = SVarWriteOpt::eMap;
		Ops.Flags = SVarWriteOpt::eSaveToFile | SVarWriteOpt::eTextGuids;

		QtVariant RulesData;
		foreach(const CProgramRulePtr & pRule, m_SelectedItems) {
			RulesData.Append(pRule->ToVariant(Ops));
		}

		QString Xml = RulesData.SerializeXml();

		QString FileName = QFileDialog::getSaveFileName(this, tr("Export Program Rules"), "", tr("Rule Files (*.xml)"));
		if (FileName.isEmpty())
			return;

		WriteStringToFile(FileName, Xml);
		QMessageBox::information(this, "MajorPrivacy", tr("Successfully exported %1 rule(s).").arg(m_SelectedItems.count()));
	}

	theGUI->CheckResults(Results, this);
}

void CProgramRuleView::Refresh()
{
	theCore->ProgramManager()->UpdateAllProgramRules();
}