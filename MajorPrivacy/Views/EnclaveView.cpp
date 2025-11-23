#include "pch.h"
#include "EnclaveView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Enclaves/EnclaveManager.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "MajorPrivacy.h"
#include "../Windows/EnclaveWnd.h"
#include "../MiscHelpers/Common/CheckableMessageBox.h"
#include "../Core/Processes/ProcessList.h"
#include "Helpers/WinHelper.h"
#include "../MiscHelpers/Common/OtherFunctions.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Access/AccessManager.h"
#include "../MiscHelpers/Common/CheckListDialog.h"

QString ShowRunDialog(const QString& EnclaveName);

CEnclaveView::CEnclaveView(QWidget *parent)
	:CPanelViewEx(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	QByteArray Columns = theConf->GetBlob("MainWindow/EnclaveView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);
	
	m_pRunInEnclave = m_pMenu->addAction(QIcon(":/Icons/Run.png"), tr("Run in Enclave"), this, SLOT(OnEnclaveAction()));
	m_pMenu->addSeparator();
	m_pTerminateAll = m_pMenu->addAction(QIcon(":/Icons/EmptyAll.png"), tr("Terminate All Processes"), this, SLOT(OnEnclaveAction()));
	m_pMenu->addSeparator();
	m_pRemoveEnclave = m_pMenu->addAction(QIcon(":/Icons/Remove.png"), tr("Delete Enclave"), this, SLOT(OnEnclaveAction()));
	m_pRemoveEnclave->setShortcut(QKeySequence::Delete);
	m_pRemoveEnclave->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pRemoveEnclave);
	m_pEditEnclave = m_pMenu->addAction(QIcon(":/Icons/EditIni.png"), tr("Edit Enclave"), this, SLOT(OnEnclaveAction()));
	m_pCloneEnclave = m_pMenu->addAction(QIcon(":/Icons/Duplicate.png"), tr("Duplicate Enclave"), this, SLOT(OnEnclaveAction()));
	m_pMenu->addSeparator();
	m_pExportEnclave = m_pMenu->addAction(QIcon(":/Icons/Exit.png"), tr("Export Enclave"), this, SLOT(OnEnclaveAction()));
	m_pImportEnclave = m_pMenu->addAction(QIcon(":/Icons/Entry.png"), tr("Import Enclave"), this, SLOT(OnEnclaveAction()));
	m_pMenu->addSeparator();
	m_pCreateEnclave = m_pMenu->addAction(QIcon(":/Icons/Add.png"), tr("Create Enclave"), this, SLOT(OnAddEnclave()));

	m_pMenuProcess = new QMenu();
	m_pExplore = m_pMenuProcess->addAction(QIcon(":/Icons/Folder.png"), tr("Open Parent Folder"), this, SLOT(OnProcessAction()));
	m_pProperties = m_pMenuProcess->addAction(QIcon(":/Icons/Presets.png"), tr("File Properties"), this, SLOT(OnProcessAction()));
	m_pMenuProcess->addSeparator();
	m_pTerminate = m_pMenuProcess->addAction(QIcon(":/Icons/Remove.png"), tr("Terminate Process"), this, SLOT(OnProcessAction()));

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pBtnAdd = new QToolButton();
	m_pBtnAdd->setIcon(QIcon(":/Icons/Add.png"));
	m_pBtnAdd->setToolTip(tr("Create Enclave"));
	m_pBtnAdd->setMaximumHeight(22);
	connect(m_pBtnAdd, SIGNAL(clicked()), this, SLOT(OnAddEnclave()));
	m_pToolBar->addWidget(m_pBtnAdd);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	AddPanelItemsToMenu();

	AddCopyMenu(m_pMenuProcess);
}

CEnclaveView::~CEnclaveView()
{
	theConf->SetBlob("MainWindow/EnclaveView_Columns", m_pTreeView->saveState());
}

void CEnclaveView::Sync()
{
	const QMap<QFlexGuid, CEnclavePtr>& EnclaveManager = theCore->EnclaveManager()->GetAllEnclaves();

	QList<QVariant> Added = m_pItemModel->Sync(EnclaveManager);

	if (m_pItemModel->IsTree()) {
		QTimer::singleShot(10, this, [this, Added]() {
			foreach(const QVariant ID, Added) {
				QModelIndex ModelIndex = m_pItemModel->FindIndex(ID);	
				m_pTreeView->expand(m_pSortProxy->mapFromSource(ModelIndex));
			}
		});
	}
}

void CEnclaveView::Clear()
{
	m_pItemModel->Clear();
}

QFlexGuid CEnclaveView::GetSelectedEnclave()
{
	auto Items = GetSelectedItems();
	if (Items.count() != 1) return QFlexGuid(NULL_GUID);
	return Items.at(0).pEnclave->GetGuid();
}

void CEnclaveView::OnDoubleClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	auto Item = m_pItemModel->GetItem(ModelIndex);
	if (Item.pEnclave.isNull())
		return;

	OpenEnclaveDialog(Item.pEnclave);
}

void CEnclaveView::OpenEnclaveDialog(const CEnclavePtr& pEnclave)
{
	if (pEnclave->IsSystem()) {
		QMessageBox::warning(this, "MajorPrivacy", tr("%1 can't be edited!").arg(pEnclave->GetName()));
		return;
	}
	CEnclaveWnd* pEnclaveWnd = new CEnclaveWnd(pEnclave);
	pEnclaveWnd->show();
}

void CEnclaveView::OnMenu(const QPoint& Point)
{
	QSet<CEnclavePtr> Enclaves;
	QSet<CProcessPtr> Processes;
	bool bSystem = false;
	for (auto& Item : m_SelectedItems) {
		if (!Enclaves.contains(Item.pEnclave)) {
			Enclaves.insert(Item.pEnclave);
			if (Item.pEnclave->IsSystem())
				bSystem = true;
		}
		if (!Item.pProcess.isNull())
			Processes.insert(Item.pProcess);
	}

	QModelIndex ModelIndex = m_pSortProxy->mapToSource(m_pTreeView->currentIndex());
	auto pItem = m_pItemModel->GetItem(ModelIndex);

	if (!pItem.pProcess.isNull())
	{
		m_pExplore->setEnabled(Processes.count() == 1);
		m_pProperties->setEnabled(Processes.count() == 1);
		m_pTerminate->setEnabled(!bSystem);

		m_pMenuProcess->popup(QCursor::pos());
	}
	else
	{
		m_pRunInEnclave->setEnabled(!bSystem && Enclaves.count() == 1);
		m_pTerminateAll->setEnabled(!bSystem);
		m_pRemoveEnclave->setEnabled(!bSystem);
		m_pEditEnclave->setEnabled(!bSystem && Enclaves.count() == 1);
		m_pCloneEnclave->setEnabled(!bSystem);
		m_pExportEnclave->setEnabled(!bSystem && Enclaves.count() == 1);

		CPanelView::OnMenu(Point);
	}
}

void CEnclaveView::OnAddEnclave()
{
	//if(!theGUI->CheckAcknowledgement(tr("Secure Enclave"))) // enclaves shouldn't break anything without a protection rule
	//	return;

	CEnclavePtr pEnclave = CEnclavePtr(new CEnclave());
	OpenEnclaveDialog(pEnclave);
}

void CEnclaveView::OnEnclaveAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());

	QList<CEnclavePtr> Enclaves;
	for (auto& Item : m_SelectedItems) {
		if (Item.pProcess.isNull())
			Enclaves << Item.pEnclave;
	}

	QList<STATUS> Results;

	if (pAction == m_pRunInEnclave)
	{
		QString Command = ShowRunDialog(m_CurrentItem.pEnclave->GetName());
		if (!Command.isEmpty()) {
			STATUS Status = theCore->StartProcessInEnclave(Command, m_CurrentItem.pEnclave->GetGuid());
			if (Status.IsError())
			{
				switch (Status.GetStatus())
				{
				case STATUS_UNSUCCESSFUL:		Status.SetMessageText(tr("Failed to start process!").toStdWString()); break;
				}
			}
			Results << Status;
		}
	}
	else if (pAction == m_pTerminateAll)
	{
		if (theConf->GetInt("Options/WarnTerminate", -1) == -1)
		{
			bool State = false;
			if(CCheckableMessageBox::question(this, "MajorPrivacy", tr("Do you want to terminate all processes in the selected enclaves?")
				, tr("Terminate without asking"), &State, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes) != QDialogButtonBox::Yes)
				return;

			if (State)
				theConf->SetValue("Options/WarnTerminate", 1);
		}

		foreach(auto & pEnclave, Enclaves) {
			foreach(auto & pProcess, pEnclave->GetProcesses()) {
				Results << theCore->ProcessList()->TerminateProcess(pProcess);
			}
		}
	}
	else if (pAction == m_pRemoveEnclave)
	{
		if(QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete selected Enclave?") , QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes)
			return;

		foreach(auto& pEnclave, Enclaves) {
			Results << theCore->EnclaveManager()->DelEnclave(pEnclave);
		}
	}
	else if (pAction == m_pEditEnclave)
	{
		if(m_CurrentItem.pProcess.isNull()) 
			OpenEnclaveDialog(m_CurrentItem.pEnclave);
	}
	else if (pAction == m_pCloneEnclave)
	{
		foreach(auto& pEnclave, Enclaves) {
			CEnclavePtr pClone = CEnclavePtr(pEnclave->Clone());
			Results << theCore->EnclaveManager()->SetEnclave(pClone);
		}
	}
	else if (pAction == m_pImportEnclave)
	{
		QString FileName = QFileDialog::getOpenFileName(this, tr("Import Enclave"), "", tr("Enclave Files (*.xml)"));
		if (FileName.isEmpty())
			return;

		QString Xml = ReadFileAsString(FileName);

		QtVariant EnclaveData;
		bool bOk = EnclaveData.ParseXml(Xml);
		if (!bOk) {
			QMessageBox::warning(this, "MajorPrivacy", tr("Failed to parse XML data!"));
			return;
		}

		SVarWriteOpt Ops;
		Ops.Format = SVarWriteOpt::eMap;
		Ops.Flags = SVarWriteOpt::eSaveToFile | SVarWriteOpt::eTextGuids;

		// Structure to hold enclave data for import
		struct EnclaveImportData {
			CEnclavePtr pEnclave;
			QtVariant ProgramRules;
			QtVariant AccessRules;
			QString name;
		};
		QList<EnclaveImportData> EnclavesToImport;

		// Check if it's a list of enclaves or a single enclave
		if (EnclaveData.IsList()) {
			// Multiple enclaves in a list
			for (uint32 i = 0; i < EnclaveData.Count(); i++) {
				QtVariant EnclaveItem = EnclaveData[i];
				if (EnclaveItem.Has(API_S_ENCLAVE)) {
					EnclaveImportData data;
					data.pEnclave = CEnclavePtr(new CEnclave());
					data.pEnclave->FromVariant(EnclaveItem[API_S_ENCLAVE]);
					data.name = data.pEnclave->GetName();
					if (EnclaveItem.Has(API_S_PROGRAM_RULES))
						data.ProgramRules = EnclaveItem[API_S_PROGRAM_RULES];
					if (EnclaveItem.Has(API_S_ACCESS_RULES))
						data.AccessRules = EnclaveItem[API_S_ACCESS_RULES];
					EnclavesToImport.append(data);
				}
			}
		} else if (EnclaveData.Has(API_S_ENCLAVE)) {
			// Single enclave
			EnclaveImportData data;
			data.pEnclave = CEnclavePtr(new CEnclave());
			data.pEnclave->FromVariant(EnclaveData[API_S_ENCLAVE]);
			data.name = data.pEnclave->GetName();
			if (EnclaveData.Has(API_S_PROGRAM_RULES))
				data.ProgramRules = EnclaveData[API_S_PROGRAM_RULES];
			if (EnclaveData.Has(API_S_ACCESS_RULES))
				data.AccessRules = EnclaveData[API_S_ACCESS_RULES];
			EnclavesToImport.append(data);
		} else {
			QMessageBox::warning(this, "MajorPrivacy", tr("Invalid enclave file format!"));
			return;
		}

		// Show selection dialog if there are multiple enclaves
		QList<int> selectedIndices;
		if (EnclavesToImport.count() > 1) {
			CCheckListDialog dialog(this);
			dialog.setWindowTitle(tr("Select Enclaves to Import"));
			dialog.setText(tr("Select which enclaves you want to import (including all their associated rules):"));

			for (int i = 0; i < EnclavesToImport.count(); i++) {
				QString enclaveName = EnclavesToImport[i].name;
				if (enclaveName.isEmpty())
					enclaveName = tr("Enclave %1").arg(i + 1);

				int programRuleCount = EnclavesToImport[i].ProgramRules.IsValid() ? EnclavesToImport[i].ProgramRules.Count() : 0;
				int accessRuleCount = EnclavesToImport[i].AccessRules.IsValid() ? EnclavesToImport[i].AccessRules.Count() : 0;
				int totalRules = programRuleCount + accessRuleCount;

				QString displayText = enclaveName;
				if (totalRules > 0)
					displayText += tr(" (%1 rule(s))").arg(totalRules);

				dialog.addItem(displayText, i, true);
			}

			if (dialog.exec() != QDialog::Accepted)
				return;

			selectedIndices = dialog.getCheckedIndices();
			if (selectedIndices.isEmpty()) {
				QMessageBox::information(this, "MajorPrivacy", tr("No enclaves selected for import."));
				return;
			}
		} else {
			// Import all if only one enclave
			selectedIndices.append(0);
		}

		// Import selected enclaves
		int totalImported = 0;
		for (int idx : selectedIndices) {
			EnclaveImportData& data = EnclavesToImport[idx];

			// Import the enclave
			Results << theCore->EnclaveManager()->SetEnclave(data.pEnclave);

			// Import program rules for this enclave
			if (data.ProgramRules.IsValid()) {
				for (uint32 i = 0; i < data.ProgramRules.Count(); i++) {
					CProgramRulePtr pRule = CProgramRulePtr(new CProgramRule());
					pRule->FromVariant(data.ProgramRules[i]);
					Results << theCore->ProgramManager()->SetProgramRule(pRule);
				}
			}

			// Import access rules for this enclave
			if (data.AccessRules.IsValid()) {
				for (uint32 i = 0; i < data.AccessRules.Count(); i++) {
					CAccessRulePtr pRule = CAccessRulePtr(new CAccessRule());
					pRule->FromVariant(data.AccessRules[i]);
					Results << theCore->AccessManager()->SetAccessRule(pRule);
				}
			}

			totalImported++;
		}

		if (Results.isEmpty() || !Results.last().IsError()) {
			if (totalImported == 1)
				QMessageBox::information(this, "MajorPrivacy", tr("Successfully imported enclave with all associated rules."));
			else
				QMessageBox::information(this, "MajorPrivacy", tr("Successfully imported %1 enclave(s) with all associated rules.").arg(totalImported));
		}
	}
	else if (pAction == m_pExportEnclave)
	{
		if (Enclaves.count() != 1)
			return;

		CEnclavePtr pEnclave = Enclaves.first();
		QFlexGuid EnclaveGuid = pEnclave->GetGuid();

		SVarWriteOpt Ops;
		Ops.Format = SVarWriteOpt::eMap;
		Ops.Flags = SVarWriteOpt::eSaveToFile | SVarWriteOpt::eTextGuids;

		QtVariant EnclaveData;

		// Export the enclave itself
		EnclaveData[API_S_ENCLAVE] = pEnclave->ToVariant(Ops);

		// Export all program rules for this enclave
		QtVariant ProgramRules;
		for (auto pRule : theCore->ProgramManager()->GetProgramRules()) {
			if (pRule->GetEnclave() == EnclaveGuid && !pRule->IsTemporary()) {
				ProgramRules.Append(pRule->ToVariant(Ops));
			}
		}
		if (ProgramRules.Count() > 0)
			EnclaveData[API_S_PROGRAM_RULES] = ProgramRules;

		// Export all access rules for this enclave
		QtVariant AccessRules;
		for (auto pRule : theCore->AccessManager()->GetAccessRules()) {
			if (pRule->GetEnclave() == EnclaveGuid && !pRule->IsTemporary()) {
				AccessRules.Append(pRule->ToVariant(Ops));
			}
		}
		if (AccessRules.Count() > 0)
			EnclaveData[API_S_ACCESS_RULES] = AccessRules;

		QString Xml = EnclaveData.SerializeXml();

		QString FileName = QFileDialog::getSaveFileName(this, tr("Export Enclave"), pEnclave->GetName() + ".xml", tr("Enclave Files (*.xml)"));
		if (FileName.isEmpty())
			return;

		WriteStringToFile(FileName, Xml);

		int ruleCount = ProgramRules.Count() + AccessRules.Count();
		QMessageBox::information(this, "MajorPrivacy", tr("Successfully exported enclave with %1 associated rule(s).").arg(ruleCount));
	}

	theGUI->CheckResults(Results, this);
}

void CEnclaveView::OnProcessAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());

	QList<CProcessPtr> Processes;
	for (auto& Item : m_SelectedItems) {
		if (!Item.pProcess.isNull())
			Processes << Item.pProcess;
	}

	QList<STATUS> Results;

	if (pAction == m_pTerminate)
	{
		if (theConf->GetInt("Options/WarnTerminate", -1) == -1)
		{
			bool State = false;
			if(CCheckableMessageBox::question(this, "MajorPrivacy", tr("Do you want to terminate %1?").arg(Processes.count() == 1 ? Processes[0]->GetName() : tr("the selected processes"))
				, tr("Terminate without asking"), &State, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes) != QDialogButtonBox::Yes)
				return;

			if (State)
				theConf->SetValue("Options/WarnTerminate", 1);
		}

		foreach(auto & pProcess, Processes) {
			Results << theCore->ProcessList()->TerminateProcess(pProcess);
		}
	}
	else if (pAction == m_pExplore || pAction == m_pProperties)
	{
		if (Processes.isEmpty())
			return;

		QString Path = theCore->NormalizePath(Processes[0]->GetNtPath());
		if(pAction == m_pExplore)
			OpenFileFolder(Path);
		else
			OpenFileProperties(Path);
	}

	theGUI->CheckResults(Results, this);
}
