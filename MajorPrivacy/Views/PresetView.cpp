#include "pch.h"
#include "PresetView.h"
#include "../Models/PresetsModel.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Presets/PresetManager.h"
#include "../../Library/Helpers/NtUtil.h"
#include "TweakView.h"
#include "../MajorPrivacy.h"
#include "../Windows/PresetWindow.h"

CPresetView::CPresetView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QGridLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pToolBar = new QToolBar();
	m_pMainLayout->addWidget(m_pToolBar, 0, 0);

	m_pBtnAdd = new QToolButton();
	m_pBtnAdd->setIcon(QIcon(":/Icons/Add.png"));
	m_pBtnAdd->setToolTip(tr("Add Preset"));
	m_pBtnAdd->setMaximumHeight(22);
	connect(m_pBtnAdd, SIGNAL(clicked()), this, SLOT(OnAction()));
	m_pToolBar->addWidget(m_pBtnAdd);

	m_pToolBar->addSeparator();

	m_pBtnActivate = new QToolButton();
	m_pBtnActivate->setIcon(QIcon(":/Icons/Enable.png"));
	m_pBtnActivate->setToolTip(tr("Activate Preset"));
	m_pBtnActivate->setMaximumHeight(22);
	connect(m_pBtnActivate, SIGNAL(clicked()), this, SLOT(OnAction()));
	m_pToolBar->addWidget(m_pBtnActivate);

	m_pBtnDeactivate = new QToolButton();
	m_pBtnDeactivate->setIcon(QIcon(":/Icons/Disable.png"));
	m_pBtnDeactivate->setToolTip(tr("Deactivate Preset"));
	m_pBtnDeactivate->setMaximumHeight(22);
	connect(m_pBtnDeactivate, SIGNAL(clicked()), this, SLOT(OnAction()));
	m_pToolBar->addWidget(m_pBtnDeactivate);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	m_pModel = new CPresetsModel(this);

	m_pTreeView = new QTreeViewEx();
	m_pTreeView->setModel(m_pModel);
	m_pTreeView->setRootIsDecorated(false);
	m_pTreeView->setSortingEnabled(true);
	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	m_pMainLayout->addWidget(m_pTreeView, 1, 0);

	m_pFinder = new CFinder(nullptr, m_pTreeView, CFinder::eRegExp | CFinder::eCaseSens);
	m_pMainLayout->addWidget(m_pFinder, 2, 0);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	// Setup context menu
	m_pMenu = new QMenu(this);

	m_pAddPreset = m_pMenu->addAction(QIcon(":/Icons/Add.png"), tr("Add Preset"), this, SLOT(OnAction()));

	m_pMenu->addSeparator();

	m_pActivate = m_pMenu->addAction(QIcon(":/Icons/Enable.png"), tr("Activate Preset"), this, SLOT(OnAction()));
	m_pDeactivate = m_pMenu->addAction(QIcon(":/Icons/Disable.png"), tr("Deactivate Preset"), this, SLOT(OnAction()));

	m_pMenu->addSeparator();

	m_pEditPreset = m_pMenu->addAction(QIcon(":/Icons/EditIni.png"), tr("Edit Preset"), this, SLOT(OnAction()));
	m_pDuplicatePreset = m_pMenu->addAction(QIcon(":/Icons/Duplicate.png"), tr("Duplicate Preset"), this, SLOT(OnAction()));
	m_pRemovePreset = m_pMenu->addAction(QIcon(":/Icons/Remove.png"), tr("Remove Preset"), this, SLOT(OnAction()));

	connect(m_pTreeView, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(OnMenu(const QPoint&)));
	connect(m_pTreeView->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
		this, SLOT(OnSelectionChanged(const QItemSelection&, const QItemSelection&)));
	connect(m_pTreeView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));

	connect(theCore->PresetManager(), SIGNAL(PresetsChanged()), this, SLOT(Update()));

	QByteArray Columns = theConf->GetBlob("MainWindow/PresetView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(CPresetsModel::eName, 300);
	} else
		m_pTreeView->header()->restoreState(Columns);
}

CPresetView::~CPresetView()
{
	theConf->SetBlob("MainWindow/PresetView_Columns", m_pTreeView->header()->saveState());
}


void CPresetView::Update()
{
	theCore->PresetManager()->Update();

	auto Presets = theCore->PresetManager()->GetPresets();

	m_pModel->Sync(Presets.values());
}

void CPresetView::OnMenu(const QPoint& Point)
{
	QModelIndexList Selected = m_pTreeView->selectionModel()->selectedRows();

	bool bHasSelection = !Selected.isEmpty();
	bool bSingleSelection = Selected.size() == 1;
	bool bHasInactive = false;
	bool bHasActive = false;
	bool bAllActive = true;
	bool bAllInactive = true;

	// Check if we have any active or inactive Presets in selection
	for (const QModelIndex& Index : Selected)
	{
		CPresetPtr pPreset = m_pModel->GetItem(Index);
		if (pPreset)
		{
			if (pPreset->IsActive()) {
				bHasActive = true;
				bAllInactive = false;
			} else {
				bHasInactive = true;
				bAllActive = false;
			}
		}
	}

	// Activate: enable if any are inactive (disable if all active)
	m_pActivate->setEnabled(bHasSelection && !bAllActive);
	// Deactivate: enable if any are active (disable if all inactive)
	m_pDeactivate->setEnabled(bHasSelection && !bAllInactive);
	m_pEditPreset->setEnabled(bSingleSelection);
	m_pDuplicatePreset->setEnabled(bHasSelection);
	m_pRemovePreset->setEnabled(bHasSelection);

	m_pMenu->popup(m_pTreeView->viewport()->mapToGlobal(Point));
}

void CPresetView::OnAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());

	if(pAction == m_pAddPreset || sender() == m_pBtnAdd)
	{
		CPresetWindow* pPresetWnd = new CPresetWindow(CPresetPtr(new CPreset()));
		pPresetWnd->show();
		return;
	}

	QModelIndexList Selected = m_pTreeView->selectionModel()->selectedRows();

	if(pAction == m_pActivate || sender() == m_pBtnActivate)
	{
		if(Selected.isEmpty())
			return;

		// Collect inactive Presets to activate
		QList<QString> PresetsToActivate;
		for (const QModelIndex& Index : Selected)
		{
			CPresetPtr pPreset = m_pModel->GetItem(Index);
			if (pPreset && !pPreset->IsActive())
				PresetsToActivate.append(pPreset->GetGuid().ToQS());
		}

		if (PresetsToActivate.isEmpty())
			return;

		// Loop through Presets and activate them
		bool bForceAll = false;
		int nSucceeded = 0;
		int nSkipped = 0;
		int nFailed = 0;

		for (const QString& Guid : PresetsToActivate)
		{
			CPresetPtr pPreset = theCore->PresetManager()->GetPreset(Guid);
			if (!pPreset)
				continue;

			bool bForce = bForceAll;

retry_activation:
			STATUS Status = theCore->PresetManager()->ActivatePreset(Guid, bForce);

			if (Status)
			{
				nSucceeded++;
			}
			else if (Status.GetStatus() == STATUS_OBJECT_NAME_COLLISION)
			{
				// Conflict detected - show dialog
				QMessageBox msgBox(this);
				msgBox.setWindowTitle(tr("Preset Activation Conflict"));
				msgBox.setText(tr("Preset '%1' has conflicts with items managed by other active Presets.")
					.arg(pPreset->GetName()));
				msgBox.setInformativeText(tr("Do you want to force activate this Preset and override the conflicts?"));
				msgBox.setIcon(QMessageBox::Question);

				QCheckBox* pCheckBox = new QCheckBox(tr("Apply to all remaining Presets"));
				msgBox.setCheckBox(pCheckBox);

				QPushButton* pYesButton = msgBox.addButton(tr("Yes (Force)"), QMessageBox::YesRole);
				QPushButton* pNoButton = msgBox.addButton(tr("No (Skip)"), QMessageBox::NoRole);
				QPushButton* pCancelButton = msgBox.addButton(tr("Cancel All"), QMessageBox::RejectRole);
				Q_UNUSED(pCancelButton);

				msgBox.setDefaultButton(pNoButton);
				msgBox.exec();

				QAbstractButton* pClicked = msgBox.clickedButton();

				if (pClicked == pYesButton)
				{
					// Force this Preset
					bForce = true;
					if (pCheckBox->isChecked())
						bForceAll = true;
					goto retry_activation;
				}
				else if (pClicked == pNoButton)
				{
					// Skip this Preset
					nSkipped++;
					continue;
				}
				else // Cancel
				{
					// Abort the entire loop
					nSkipped += (PresetsToActivate.size() - PresetsToActivate.indexOf(Guid) - 1);
					break;
				}
			}
			else
			{
				// Other error
				nFailed++;
			}
		}

		// Show summary if multiple Presets were processed
		if (PresetsToActivate.size() > 1)
		{
			QString summary = tr("Preset activation complete:\n");
			if (nSucceeded > 0)
				summary += tr("  Activated: %1\n").arg(nSucceeded);
			if (nSkipped > 0)
				summary += tr("  Skipped: %1\n").arg(nSkipped);
			if (nFailed > 0)
				summary += tr("  Failed: %1\n").arg(nFailed);

			QMessageBox::information(this, tr("Preset Activation"), summary);
		}
	}
	else if(pAction == m_pDeactivate || sender() == m_pBtnDeactivate)
	{
		if(Selected.isEmpty())
			return;

		// Collect active Presets to deactivate
		QList<QString> PresetsToDeactivate;
		for (const QModelIndex& Index : Selected)
		{
			CPresetPtr pPreset = m_pModel->GetItem(Index);
			if (pPreset && pPreset->IsActive())
				PresetsToDeactivate.append(pPreset->GetGuid().ToQS());
		}

		if (PresetsToDeactivate.isEmpty())
			return;

		// Loop through Presets and deactivate them
		int nSucceeded = 0;
		int nFailed = 0;

		for (const QString& Guid : PresetsToDeactivate)
		{
			STATUS Status = theCore->PresetManager()->DeactivatePreset(Guid);
			if (Status)
				nSucceeded++;
			else
				nFailed++;
		}

		// Show summary if multiple Presets were processed
		if (PresetsToDeactivate.size() > 1)
		{
			QString summary = tr("Preset deactivation complete:\n");
			if (nSucceeded > 0)
				summary += tr("  Deactivated: %1\n").arg(nSucceeded);
			if (nFailed > 0)
				summary += tr("  Failed: %1\n").arg(nFailed);

			QMessageBox::information(this, tr("Preset Deactivation"), summary);
		}
		else if (nFailed > 0)
		{
			QMessageBox::warning(this, tr("Preset Deactivation"),
				tr("Failed to deactivate Preset."));
		}
	}
	else if(pAction == m_pEditPreset)
	{
		if(Selected.size() != 1)
			return;
		OnDoubleClicked(Selected.first());
	}
	else if(pAction == m_pDuplicatePreset)
	{
		if(Selected.isEmpty())
			return;

		QList<STATUS> Results;
		for (const QModelIndex& Index : Selected)
		{
			CPresetPtr pPreset = m_pModel->GetItem(Index);
			if (pPreset) {
				CPresetPtr pClone = CPresetPtr(pPreset->Clone());
				pClone->SetName(pPreset->GetName() + tr(" (Copy)"));
				Results << theCore->PresetManager()->SetPreset(pClone);
			}
		}
		theGUI->CheckResults(Results, this);
	}
	else if(pAction == m_pRemovePreset)
	{
		if(Selected.isEmpty())
			return;

		if (QMessageBox::question(this, tr("Remove Presets"),
			tr("Are you sure you want to remove the selected presets?"),
			QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		for (const QModelIndex& Index : Selected)
		{
			QString Guid = m_pModel->GetGuid(Index);
			theCore->PresetManager()->DelPreset(Guid);
		}
	}
}

void CPresetView::OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
	Q_UNUSED(deselected);

	QModelIndexList Selected = m_pTreeView->selectionModel()->selectedRows();
	if (Selected.size() == 1) {
		QString Guid = m_pModel->GetGuid(Selected.first());
		emit CurrentChanged(Guid);
	}
}

void CPresetView::OnDoubleClicked(const QModelIndex& index)
{
	CPresetPtr pPreset = m_pModel->GetItem(index);
	if (pPreset) {
		CPresetWindow* pPresetWnd = new CPresetWindow(pPreset);
		pPresetWnd->show();
	}
}
