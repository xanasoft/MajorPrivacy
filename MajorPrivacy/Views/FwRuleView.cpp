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
#include "../../MiscHelpers/Common/CheckListDialog.h"
#include "../../MiscHelpers/Common/CheckableMessageBox.h"
#include "../Windows/FwTemplateWnd.h"
#include "../Windows/RuleLifetime.h"
#include "../Helpers/PathPattern.h"

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
	m_pMakeTemplate = m_pMenu->addAction(QIcon(":/Icons/Template.png"), tr("Create Template from Rule..."), this, SLOT(OnRuleAction()));
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

	m_pBtnHideDerived = new QToolButton();
	m_pBtnHideDerived->setIcon(QIcon(QPixmap::fromImage(ImageAddOverlay(QImage(":/Icons/Template.png"), ":/Icons/Disable.png", 40))));
	m_pBtnHideDerived->setCheckable(true);
	m_pBtnHideDerived->setToolTip(tr("Hide rules made from a rule template, the template stands for them"));
	m_pBtnHideDerived->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnHideDerived);

	m_pBtnTree = new QToolButton();
	m_pBtnTree->setIcon(QIcon(":/Icons/Tree.png"));
	m_pBtnTree->setCheckable(true);
	m_pBtnTree->setToolTip(tr("Show the rules made from a rule template under it"));
	m_pBtnTree->setMaximumHeight(22);
	m_pBtnTree->setChecked(theConf->GetBool("MainWindow/FWRuleView_TemplateTree", false));
	m_pItemModel->SetTemplateTree(m_pBtnTree->isChecked());
	connect(m_pBtnTree, SIGNAL(toggled(bool)), this, SLOT(OnTemplateTree(bool)));
	m_pToolBar->addWidget(m_pBtnTree);

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
	bool bHideDerived = m_pBtnHideDerived->isChecked();
	EFwDirections Dir = (EFwDirections)m_pCmbDir->currentData().toInt();
	EFwActions Type = (EFwActions)m_pCmbAction->currentData().toInt();

	// only a template that is still there backs a rule, one left behind by a deleted template is a rule of its own;
	// looked for among all rules, a program's own list holds the rules made for it but not the template they come from
	QSet<QString> Templates;
	if (bHideDerived) {
		foreach(const CFwRulePtr& pRule, theCore->NetworkManager()->GetFwRules()) {
			if (pRule->IsTemplate())
				Templates.insert(pRule->GetGuid().ToQS());
		}
	}

	if (bHideDisabled || bHideWin || bHideDerived || Type != EFwActions::Undefined || Dir != EFwDirections::Bidirectional)
	{
		for (auto I = RuleList.begin(); I != RuleList.end();)
		{
			if (bHideWin && (*I)->GetSource() == EFwRuleSource::eWindowsDefault)
				I = RuleList.erase(I);
			else if (bHideDerived && !(*I)->IsTemplate() && Templates.contains((*I)->GetTemplateGuid()))
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

	//
	// A program's own list holds the rules made for it but not the template they come from, that
	// belongs to the pattern. In tree mode the template is what they hang under, so it is brought
	// in, after the filters, as it is shown for what it holds rather than for what it is.
	//
	if (m_pItemModel->IsTemplateTree())
	{
		QSet<QString> Listed;
		foreach(const CFwRulePtr& pRule, RuleList)
			Listed.insert(pRule->GetGuid().ToQS().toLower());

		QMap<QFlexGuid, CFwRulePtr> AllRules = theCore->NetworkManager()->GetFwRules();
		QList<CFwRulePtr> Parents;
		foreach(const CFwRulePtr& pRule, RuleList)
		{
			if (pRule->IsTemplate() || pRule->GetTemplateGuid().isEmpty())
				continue;
			QFlexGuid Guid(pRule->GetTemplateGuid());
			if (Listed.contains(Guid.ToQS().toLower()))
				continue;
			CFwRulePtr pTemplate = AllRules.value(Guid);
			if (pTemplate.isNull() || !pTemplate->IsTemplate())
				continue;
			Listed.insert(Guid.ToQS().toLower());
			Parents.append(pTemplate);
		}
		RuleList.append(Parents);
	}

	m_pItemModel->Sync(RuleList);
}

void CFwRuleView::OnTemplateTree(bool bTree)
{
	theConf->SetValue("MainWindow/FWRuleView_TemplateTree", bTree);
	m_pItemModel->SetTemplateTree(bTree);
	Sync(m_RuleList); // rearranged at once rather than on the next update
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
		else if (pItem->IsBackup()) {
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
	m_pMakeTemplate->setEnabled(iSelectedCount == 1 && m_SelectedItems.count() == 1 && CanMakeTemplateFrom(m_SelectedItems.first()));
	m_pCloneRule->setEnabled(iSelectedCount > 0 || iBackups > 0);
	m_pExportRules->setEnabled(iSelectedCount > 0);

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

	if (pAction == m_pEnableRule || pAction == m_pDisableRule)
		NoteTemplateSwitch(pAction == m_pEnableRule);

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
		bool bWithDerived = false;
		if (!AskDeleteRules(bWithDerived))
			return;

		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			Results << theCore->NetworkManager()->DelFwRule(pRule, bWithDerived && pRule->IsTemplate());
		}
	}
	else if (pAction == m_pEditRule)
	{
		OpenRullDialog(m_CurrentItem);
	}
	else if (pAction == m_pMakeTemplate)
	{
		if (m_SelectedItems.count() == 1)
			MakeTemplateFrom(m_SelectedItems.first());
		return;
	}
	else if (pAction == m_pCloneRule)
	{
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			CFwRulePtr pClone = CFwRulePtr(pRule->Clone());
			Results << theCore->NetworkManager()->SetFwRule(pClone);
		}
	}
	else if (pAction == m_pImportRules)
	{
		QString FileName = QFileDialog::getOpenFileName(this, tr("Import Firewall Rules"), "", tr("Rule Files (*.xml)"));
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
		QList<CFwRulePtr> RulesToImport;
		for (uint32 i = 0; i < RulesData.Count(); i++) {
			CFwRulePtr pRule = CFwRulePtr(new CFwRule(CProgramID()));
			pRule->FromVariant(RulesData[i]);
			RulesToImport.append(pRule);
		}

		// Show selection dialog if there are multiple rules
		QList<int> selectedIndices;
		if (RulesToImport.count() > 1) {
			CCheckListDialog dialog(this);
			dialog.setWindowTitle(tr("Select Rules to Import"));
			dialog.setText(tr("Select which firewall rules you want to import:"));

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
			Results << theCore->NetworkManager()->SetFwRule(RulesToImport[idx]);
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
		foreach(const CFwRulePtr & pRule, m_SelectedItems) {
			RulesData.Append(pRule->ToVariant(Ops));
		}

		QString Xml = RulesData.SerializeXml();

		QString FileName = QFileDialog::getSaveFileName(this, tr("Export Firewall Rules"), "", tr("Rule Files (*.xml)"));
		if (FileName.isEmpty())
			return;

		WriteStringToFile(FileName, Xml);
		QMessageBox::information(this, "MajorPrivacy", tr("Successfully exported %1 rule(s).").arg(m_SelectedItems.count()));
	}

	theGUI->CheckResults(Results, this);
}

static QString CFwRuleView__GetRulePath(const CFwRulePtr& pRule)
{
	QString Path = pRule->GetProgramID().GetFilePath();
	if (Path.isEmpty())
		Path = pRule->GetBinaryPath();
	return Path;
}

// a template stands for binaries by path, so only a rule for one binary by path can be turned into one
bool CFwRuleView::CanMakeTemplateFrom(const CFwRulePtr& pRule)
{
	if (pRule->IsTemplate() || pRule->IsBackup())
		return false;
	if (!pRule->GetServiceTag().isEmpty() || !pRule->GetAppPkgId().isEmpty() || !pRule->GetAppPFN().isEmpty())
		return false;
	if (pRule->GetAction() != EFwActions::Allow && pRule->GetAction() != EFwActions::Block)
		return false;
	QString Path = CFwRuleView__GetRulePath(pRule);
	return !Path.isEmpty() && Path != "*" && !Path.contains('*') && !Path.contains('?');
}

//
// The same thing the prompt does for a program asking, done for a rule already standing: the
// pattern is deduced from the other rules where one of them is a sibling of this one, and read off
// the path where none is. The template keeps everything the rule says, and the rules already there
// for the programs it covers are put under it, so they follow it from then on as if it had made them.
//
void CFwRuleView::MakeTemplateFrom(const CFwRulePtr& pRule)
{
	QString Path = CFwRuleView__GetRulePath(pRule);

	QStringList Known;
	QSet<QString> Seen;
	QList<CFwRulePtr> AllRules = theCore->NetworkManager()->GetFwRules().values();
	foreach(const CFwRulePtr& pOther, AllRules) {
		if (pOther->IsTemplate())
			continue; // a pattern tells us nothing about what varies
		QString Other = CFwRuleView__GetRulePath(pOther);
		if (Other.isEmpty() || Other == "*" || Other.compare(Path, Qt::CaseInsensitive) == 0 || Seen.contains(Other.toLower()))
			continue;
		Seen.insert(Other.toLower());
		Known.append(Other);
	}

	SPathPattern Pattern = MakePathPattern(Path, Known);
	if (!Pattern.IsValid())
		Pattern = GuessPathPattern(Path);

	// the rule's own direction is proposed; setting it to both in the window takes the rules going the other way over too
	CFwTemplateWnd Wnd(Pattern.Pattern, pRule->GetAction() == EFwActions::Allow, pRule->GetDirection(), Pattern.Similar, this);
	Wnd.SetTakeOver(AllRules);
	if (theGUI->SafeExec(&Wnd) != QDialog::Accepted)
		return;

	CProgramItemPtr pPattern = CFwTemplateWnd::GetPatternItem(Wnd.GetPattern(), this);
	if (pPattern.isNull())
		return;

	CProgramItemPtr pItem = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID());
	QString Name = pItem.isNull() ? QFileInfo(Path).fileName() : pItem->GetNameEx();

	// a new guid of its own, made here so the template can be named when its rules are put under it
	CProgramID ID = pPattern->GetID();
	CFwRulePtr pTemplate = CFwRulePtr(pRule->CloneAsTemplate(ID));
	pTemplate->SetName(QString("MajorPrivacy-Template, %1").arg(Name));
	pTemplate->SetEnabled(true);
	pTemplate->SetAction(Wnd.IsAllow() ? EFwActions::Allow : EFwActions::Block);
	pTemplate->SetDirection(Wnd.GetDirection());
	ApplyRuleLifetime(pTemplate.data(), Wnd.GetLifetime());

	if (Wnd.WantsFullEditor())
	{
		QSet<CProgramItemPtr> Items;
		Items.insert(pPattern);
		if (pItem) Items.insert(pItem);
		CFirewallRuleWnd* pFirewallRuleWnd = new CFirewallRuleWnd(pTemplate, Items);
		if (!pFirewallRuleWnd->exec())
			return;
	}

	// what is taken over is decided by the template as it was saved, the editor may have changed it
	QStringList TakeOver;
	QString Saved = CFwRuleView__GetRulePath(pTemplate);
	foreach(const CFwRulePtr& pOther, AllRules) {
		if (CFwTemplateWnd::TakesOver(pOther, Saved, pTemplate->GetAction(), pTemplate->GetDirection()))
			TakeOver.append(pOther->GetGuid().ToQS());
	}

	// saved again after the editor, as the editor saves without them
	STATUS Status = theCore->NetworkManager()->SetFwRule(pTemplate, TakeOver);
	theGUI->CheckResults(QList<STATUS>() << Status, this);
}

// how many rules were made from the templates among these, those are what deleting a template has to ask about
static int CFwRuleView__CountDerived(const QList<CFwRulePtr>& Rules)
{
	QSet<QString> Templates;
	foreach(const CFwRulePtr& pRule, Rules) {
		if (pRule->IsTemplate())
			Templates.insert(pRule->GetGuid().ToQS());
	}
	if (Templates.isEmpty())
		return 0;

	int Count = 0;
	foreach(const CFwRulePtr& pRule, theCore->NetworkManager()->GetFwRules()) {
		if (!pRule->IsTemplate() && Templates.contains(pRule->GetTemplateGuid()))
			Count++;
	}
	return Count;
}

//
// A template takes effect through the rules it makes, so deleting one is two questions: whether
// to delete, and whether those rules go with it. Three answers want three buttons. The second
// question can be answered once for good; the first is still asked every time, as for any rule.
//
bool CFwRuleView::AskDeleteRules(bool& bWithDerived)
{
	bWithDerived = false;

	int Derived = CFwRuleView__CountDerived(m_SelectedItems);
	int Remembered = theConf->GetInt("Options/DeleteTemplateDerived", -1);
	if (Derived == 0 || Remembered != -1)
	{
		bWithDerived = (Remembered == 1);
		return QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete selected Rules Items?"), QMessageBox::Yes, QMessageBox::Cancel) == QMessageBox::Yes;
	}

	bool bRemember = false;
	QDialogButtonBox::StandardButton Ret = CCheckableMessageBox::question(this, "MajorPrivacy",
		tr("Do you want to delete selected Rules Items?<br /><br />"
			"The selection holds rule templates, and %1 rule(s) were made from them. Shall those be deleted as well?<br /><br />"
			"<b>Yes</b> - delete the templates and the rules made from them<br />"
			"<b>No</b> - delete the templates only, the rules made from them stay as rules of their own<br />"
			"<b>Cancel</b> - delete nothing").arg(Derived),
		tr("Remember this choice"), &bRemember,
		QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel, QDialogButtonBox::Yes);

	if (Ret != QDialogButtonBox::Yes && Ret != QDialogButtonBox::No)
		return false;

	bWithDerived = (Ret == QDialogButtonBox::Yes);
	if (bRemember)
		theConf->SetValue("Options/DeleteTemplateDerived", bWithDerived ? 1 : 0);
	return true;
}

//
// Switching a template on or off switches the rules made from it as well, as a wildcard rule
// would switch every program it covers. That is said once, where it happens.
//
void CFwRuleView::NoteTemplateSwitch(bool bEnable)
{
	bool bTemplate = std::any_of(m_SelectedItems.begin(), m_SelectedItems.end(), [](const CFwRulePtr& pRule) { return pRule->IsTemplate(); });
	if (!bTemplate || !theConf->GetBool("Options/InfoTemplateSwitch", true))
		return;

	bool bDontShow = false;
	CCheckableMessageBox::question(this, "MajorPrivacy",
		bEnable ? tr("Enabling a rule template enables the rules made from it as well.")
				: tr("Disabling a rule template disables the rules made from it as well."),
		tr("Don't show this message again"), &bDontShow,
		QDialogButtonBox::Ok, QDialogButtonBox::Ok, QMessageBox::Information);
	if (bDontShow)
		theConf->SetValue("Options/InfoTemplateSwitch", false);
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
			Results << theCore->NetworkManager()->DelFwRule(pRule, pRule->IsTemplate()); // a temporary template takes its rules along, as when it runs out
	}
	theGUI->CheckResults(Results, this);
}