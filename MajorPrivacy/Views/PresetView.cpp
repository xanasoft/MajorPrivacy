#include "pch.h"
#include "PresetView.h"
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

	m_pLabel = new QLabel(tr("Rule Presets:"));
	m_pMainLayout->addWidget(m_pLabel, 0, 0);

	//m_pPresetToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pPresetToolBar, 1, 0);

	
	m_pPresets = new CPanelWidgetEx();
	m_pPresets->GetTree()->setHeaderLabels(tr("Name|Description").split("|"));
	m_pPresets->GetTree()->setIndentation(0);
	m_pPresets->GetTree()->setSortingEnabled(true);
	m_pPresets->GetTree()->setSelectionBehavior(QAbstractItemView::SelectItems);
	m_pPresets->GetTree()->setItemDelegate(new CTreeItemDelegate());
	m_pPresets->GetTree()->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	//m_pPresets->GetTree()->setItemDelegate(new CTreeItemDelegate2());
	//m_pPresets->GetTree()->setIconSize(QSize(32, 32));
	m_pMainLayout->addWidget(m_pPresets, 2, 0);


	QAction* pFirst = m_pPresets->GetMenu()->actions().first();

	m_pAddPreset = new QAction(QIcon(":/Icons/Add.png"), tr("Add Preset"), this);
	connect(m_pAddPreset, SIGNAL(triggered()), this, SLOT(OnAction()));
	m_pPresets->GetMenu()->insertAction(pFirst, m_pAddPreset);

	m_pPresets->GetMenu()->insertSeparator(pFirst);

	m_pActivate = new QAction(QIcon(":/Icons/Enable.png"), tr("Activate Preset"), this);
	connect(m_pActivate, SIGNAL(triggered()), this, SLOT(OnAction()));
	m_pPresets->GetMenu()->insertAction(pFirst, m_pActivate);

	m_pDeactivate = new QAction(QIcon(":/Icons/Disable.png"), tr("Deactivate Preset"), this);
	connect(m_pDeactivate, SIGNAL(triggered()), this, SLOT(OnAction()));
	m_pPresets->GetMenu()->insertAction(pFirst, m_pDeactivate);

	m_pPresets->GetMenu()->insertSeparator(pFirst);

	m_pEditPreset = new QAction(QIcon(":/Icons/EditIni.png"), tr("Edit Preset"), this);
	connect(m_pEditPreset, SIGNAL(triggered()), this, SLOT(OnAction()));
	m_pPresets->GetMenu()->insertAction(pFirst, m_pEditPreset);

	m_pRemovePreset = new QAction(QIcon(":/Icons/Remove.png"), tr("Remove Preset"), this);
	connect(m_pRemovePreset, SIGNAL(triggered()), this, SLOT(OnAction()));
	m_pPresets->GetMenu()->insertAction(pFirst, m_pRemovePreset);

	disconnect(m_pPresets->GetTree(), SIGNAL(customContextMenuRequested(const QPoint&)), m_pPresets, SLOT(OnMenu(const QPoint &)));
	connect(m_pPresets->GetTree(), SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	//connect(m_pPresets->GetTree(), SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnPresetClicked(QTreeWidgetItem*)));
	connect(m_pPresets->GetTree(), SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)), this, SLOT(OnPresetClicked(QTreeWidgetItem*)));
	connect(m_pPresets->GetTree(), SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnPresetDoubleClicked(QTreeWidgetItem*)));

	connect(theCore->PresetManager(), SIGNAL(PresetsChanged()), this, SLOT(Update()));

	QByteArray Columns = theConf->GetBlob("MainWindow/PresetView_Columns");
	if (Columns.isEmpty()) {
		m_pPresets->GetTree()->setColumnWidth(0, 300);
	} else
		m_pPresets->GetTree()->header()->restoreState(Columns);
}

CPresetView::~CPresetView()
{
	theConf->SetBlob("MainWindow/PresetView_Columns", m_pPresets->GetTree()->header()->saveState());
}


void CPresetView::Update()
{
	theCore->PresetManager()->Update();

	auto Presets = theCore->PresetManager()->GetPresets();

	QMap<QString, QTreeWidgetItem*> Old;
	for(int i = 0; i < m_pPresets->GetTree()->topLevelItemCount(); i++) {
		QTreeWidgetItem* pItem = m_pPresets->GetTree()->topLevelItem(i);
		QString Guid = pItem->data(eName, Qt::UserRole).toString();
		Q_ASSERT(!Old.contains(Guid));
		Old.insert(Guid, pItem);
	}	

	foreach(const CPresetPtr& pPreset, Presets)
	{
		QString Guid = pPreset->GetGuid().ToQS();
		QTreeWidgetItem* pItem = Old.take(Guid);

		if (!pItem) {
			pItem = new QTreeWidgetItem();
			pItem->setData(eName, Qt::UserRole, Guid);
			m_pPresets->GetTree()->insertTopLevelItem(0, pItem);
		}

		pItem->setIcon(eName, pPreset->GetIcon());
		pItem->setText(eName, CMajorPrivacy::GetResourceStr(pPreset->GetName()));
		pItem->setToolTip(eName, pPreset->GetGuid().ToQS());
		pItem->setText(eDescription, CMajorPrivacy::GetResourceStr(pPreset->GetDescription()));

		bool bActive = pPreset->IsActive();
		QFont fnt = pItem->font(eName);
		fnt.setBold(bActive);
		pItem->setFont(eName, fnt);
	}

	foreach(QTreeWidgetItem* pItem, Old)
		delete pItem;
}

void CPresetView::OnMenu(const QPoint& Point)
{
	auto Items = m_pPresets->GetTree()->selectedItems();

	bool bHasSelection = (Items.size() > 0);
	bool bSingleSelection = (Items.size() == 1);
	bool bHasInactive = false;
	bool bHasActive = false;

	// Check if we have any active or inactive Presets in selection
	for (auto pItem : Items)
	{
		QString Guid = pItem->data(eName, Qt::UserRole).toString();
		CPresetPtr pPreset = theCore->PresetManager()->GetPreset(Guid);
		if (pPreset)
		{
			if (pPreset->IsActive())
				bHasActive = true;
			else
				bHasInactive = true;
		}
	}

	m_pActivate->setEnabled(bHasInactive);
	m_pDeactivate->setEnabled(bHasActive);
	m_pEditPreset->setEnabled(bSingleSelection);
	m_pRemovePreset->setEnabled(bHasSelection);

	m_pPresets->OnMenu(Point);
}

void CPresetView::OnAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());

	if(pAction == m_pAddPreset)
	{
		CPresetWindow* pPresetWnd = new CPresetWindow(CPresetPtr(new CPreset()));
		pPresetWnd->show();
	}
	else if(pAction == m_pActivate)
	{
		auto Items = m_pPresets->GetTree()->selectedItems();
		if(Items.isEmpty())
			return;

		// Collect inactive Presets to activate
		QList<QString> PresetsToActivate;
		for (auto pItem : Items)
		{
			QString Guid = pItem->data(eName, Qt::UserRole).toString();
			CPresetPtr pPreset = theCore->PresetManager()->GetPreset(Guid);
			if (pPreset && !pPreset->IsActive())
				PresetsToActivate.append(Guid);
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
	else if(pAction == m_pDeactivate)
	{
		auto Items = m_pPresets->GetTree()->selectedItems();
		if(Items.isEmpty())
			return;

		// Collect active Presets to deactivate
		QList<QString> PresetsToDeactivate;
		for (auto pItem : Items)
		{
			QString Guid = pItem->data(eName, Qt::UserRole).toString();
			CPresetPtr pPreset = theCore->PresetManager()->GetPreset(Guid);
			if (pPreset && pPreset->IsActive())
				PresetsToDeactivate.append(Guid);
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
		auto Items = m_pPresets->GetTree()->selectedItems();
		if(Items.size() != 1)
			return;
		OnPresetDoubleClicked(Items.first());
	}
	else if(pAction == m_pRemovePreset)
	{
		auto Items = m_pPresets->GetTree()->selectedItems();
		if(Items.isEmpty())
			return;
		foreach(QTreeWidgetItem* pItem, Items)
		{
			QString Guid = Items.first()->data(eName, Qt::UserRole).toString();
			theCore->PresetManager()->DelPreset(Guid);
			delete pItem;
		}
	}
}

void CPresetView::OnPresetClicked(QTreeWidgetItem*)
{
}

void CPresetView::OnPresetDoubleClicked(QTreeWidgetItem* pItem)
{
	CPresetPtr pPreset = theCore->PresetManager()->GetPreset(pItem->data(eName, Qt::UserRole).toString());
	if (pPreset) {
		CPresetWindow* pPresetWnd = new CPresetWindow(pPreset);
		pPresetWnd->show();
	}
}