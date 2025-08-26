#include "pch.h"
#include "HashDBView.h"
#include "../Core/PrivacyCore.h"
#include "../Windows/HashEntryWnd.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CHashDBView::CHashDBView(QWidget *parent)
	:CPanelViewEx<CHashDBModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	QByteArray Columns = theConf->GetBlob("MainWindow/HashDBView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pEnableHash = m_pMenu->addAction(QIcon(":/Icons/Enable.png"), tr("Enable Entry"), this, SLOT(OnHashAction()));
	m_pDisableHash = m_pMenu->addAction(QIcon(":/Icons/Disable.png"), tr("Disable Entry"), this, SLOT(OnHashAction()));
	m_pMenu->addSeparator();

	m_pRemoveHash = m_pMenu->addAction(QIcon(":/Icons/Remove.png"), tr("Delete Entry"), this, SLOT(OnHashAction()));
	m_pRemoveHash->setShortcut(QKeySequence::Delete);
	m_pRemoveHash->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pRemoveHash);
	m_pEditHash = m_pMenu->addAction(QIcon(":/Icons/EditIni.png"), tr("Edit Entry"), this, SLOT(OnHashAction()));
	m_pMenu->addSeparator();
	m_pAddHash = m_pMenu->addAction(QIcon(":/Icons/Add.png"), tr("Add Entry"), this, SLOT(OnAddHash()));

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pBtnAdd = new QToolButton();
	m_pBtnAdd->setIcon(QIcon(":/Icons/Add.png"));
	m_pBtnAdd->setToolTip(tr("Add Entry"));
	m_pBtnAdd->setMaximumHeight(22);
	connect(m_pBtnAdd, SIGNAL(clicked()), this, SLOT(OnAddRule()));
	m_pToolBar->addWidget(m_pBtnAdd);
	m_pToolBar->addSeparator();

	m_pBtnRefresh = new QToolButton();
	m_pBtnRefresh->setIcon(QIcon(":/Icons/Recover.png"));
	m_pBtnRefresh->setToolTip(tr("Refresh DB"));
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

	m_pToolBar->addSeparator();
	m_pBtnExpand = new QToolButton();
	m_pBtnExpand->setIcon(QIcon(":/Icons/Expand.png"));
	m_pBtnExpand->setCheckable(true);
//#ifdef _DEBUG
//	m_pBtnExpand->setChecked(true);
//#endif
	m_pBtnExpand->setToolTip(tr("Auto Expand"));
	m_pBtnExpand->setMaximumHeight(22);
	connect(m_pBtnExpand, &QToolButton::toggled, this, [&](bool checked) {
		if(checked)
			m_pTreeView->expandAll();
		else
			m_pTreeView->collapseAll();
		});
	m_pToolBar->addWidget(m_pBtnExpand);


	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);


	AddPanelItemsToMenu();
}

CHashDBView::~CHashDBView()
{
	theConf->SetBlob("MainWindow/HashDBView_Columns", m_pTreeView->saveState());
}

void CHashDBView::Sync(const QSet<CProgramFilePtr>& Programs, bool bAllPrograms, const QFlexGuid& EnclaveGuid)
{
	if (m_CurPrograms != Programs || m_CurEnclaveGuid != EnclaveGuid || m_FullRefresh) {
		m_CurPrograms = Programs;
		m_CurEnclaveGuid = EnclaveGuid;
		m_EntryMap.clear();
		m_pItemModel->Clear();
		m_FullRefresh = false;
		m_RefreshCount++;
		m_PathUIDs.clear();
	}

	QHash<QByteArray, QList<QString>> Hashes;
	//if (!bAllPrograms) {
	QSet<quint64> LibraryIDs;
	foreach(const CProgramFilePtr& pProgram, Programs) {
		QMultiMap<quint64, SLibraryInfo> pLibInto = pProgram->GetLibraries();
		LibraryIDs |= ListToSet(pLibInto.keys());

		CImageSignInfo SignInfo = pProgram->GetSignInfo();
		if(SignInfo.HashFileHash())
			Hashes[SignInfo.GetFileHash()].append(pProgram->GetPath());
		if(SignInfo.HashSignerHash())
			Hashes[SignInfo.GetSignerHash()].append(pProgram->GetPath());
		if(SignInfo.HashIssuerHash())
			Hashes[SignInfo.GetIssuerHash()].append(pProgram->GetPath());
	}

	foreach(const quint64& LibId, LibraryIDs) {
		CProgramLibraryPtr pLibrary = theCore->ProgramManager()->GetLibraryByUID(LibId, true);
		if (!pLibrary) continue;

		CImageSignInfo SignInfo = pLibrary->GetSignInfo();
		if(SignInfo.HashFileHash())
			Hashes[SignInfo.GetFileHash()].append(pLibrary->GetPath());
		if(SignInfo.HashSignerHash())
			Hashes[SignInfo.GetSignerHash()].append(pLibrary->GetPath());
		if(SignInfo.HashIssuerHash())
			Hashes[SignInfo.GetIssuerHash()].append(pLibrary->GetPath());
	}
	//}

	auto OldMap = m_EntryMap;

	auto HashTable = theCore->HashDB()->GetAllHashes();

	for (const auto& pEntry : HashTable) {

		if (!EnclaveGuid.IsNull() && !pEntry->HasEnclave(EnclaveGuid))
			continue;

		auto I = Hashes.find(pEntry->GetHash());
		if (!bAllPrograms && I == Hashes.end())
			continue;

		SHashItemPtr pItem = OldMap.take(qMakePair((uint64)pEntry.get(), 0));
		if(pItem.isNull())
		{
			pItem = SHashItemPtr(new SHashItem());
			pItem->ID = QString("%1_%2").arg((uint64)pEntry.get()).arg(0);
			pItem->Name = pEntry->GetType() == EHashType::eFileHash ? Split2(pEntry->GetName(), "\\", true).second : pEntry->GetName();
			pItem->Type = pEntry->GetType();
			m_EntryMap.insert(qMakePair((uint64)pEntry.get(), 0), pItem);
		}

		SHashItemPtr pSubItem = OldMap.take(qMakePair((uint64)pEntry.get(), (uint64)pItem.get()));
		if (!pSubItem) {
			pSubItem = SHashItemPtr(new SHashItem());
			pSubItem->ID = QString("%1_%2").arg((uint64)pEntry.get()).arg((uint64)pItem.get());
			pSubItem->Name = pItem->Name;
			pSubItem->Type = pEntry->GetType();
			pSubItem->pEntry = pEntry;
			pSubItem->Parents.append(QString("%1_%2").arg((uint64)pEntry.get()).arg(0));
			m_EntryMap.insert(qMakePair((uint64)pEntry.get(), (uint64)pItem.get()), pSubItem);
		}

		if (I == Hashes.end()) 
			continue;
		
		foreach(const QString& Path, *I)
		{
			int uid = m_PathUIDs[Path];
			if(uid == 0) {
				uid = m_PathUIDs.size() + 1; 
				m_PathUIDs[Path] = uid;
			}

			SHashItemPtr pAuxItem = OldMap.take(qMakePair((uint64)pItem.get(), uid));
			if (!pAuxItem) {
				pAuxItem = SHashItemPtr(new SHashItem());
				pAuxItem->ID = QString("%1_%2").arg((uint64)pItem.get()).arg(uid);
				pAuxItem->Name = Path;
				pAuxItem->Type = pEntry->GetType();
				pAuxItem->Parents = pSubItem->Parents;
				pAuxItem->Parents.append(QString("%1_%2").arg((uint64)pEntry.get()).arg((uint64)pItem.get()));
				m_EntryMap.insert(qMakePair((uint64)pItem.get(), uid), pAuxItem);
			}
		}
	}

	foreach(SHashItemKey Key, OldMap.keys())
		m_EntryMap.remove(Key);

	QList<QModelIndex> Added = m_pItemModel->Sync(m_EntryMap);

	if (m_pBtnExpand->isChecked()) 
	{
		int CurCount = m_RefreshCount;
		QTimer::singleShot(10, this, [this, Added, CurCount]() {
			if(CurCount != m_RefreshCount)
				return; // ignore if refresh was called again
			foreach(const QModelIndex & Index, Added)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
		});
	}
}

void CHashDBView::OnDoubleClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	auto pRule = m_pItemModel->GetItem(ModelIndex);
	if (pRule.isNull() || pRule->pEntry.isNull())
		return;

	OpenHashDialog(pRule->pEntry);
}

void CHashDBView::OpenHashDialog(const CHashPtr& pEntry)
{
	CHashEntryWnd* pHashEntryWnd = new CHashEntryWnd(pEntry);
	pHashEntryWnd->show();
}

void CHashDBView::OnMenu(const QPoint& Point)
{
	int iSelectedCount = 0;
	int iEnabledCount = 0;
	int iDisabledCount = 0;
	int iGroupCount = 0;
	foreach(auto pItem, m_SelectedItems) {
		if(pItem->pEntry.isNull())
			iGroupCount++;
		else if (pItem->pEntry->IsEnabled())
			iEnabledCount++;
		else
			iDisabledCount++;
		iSelectedCount++;
	}

	m_pEnableHash->setEnabled(iDisabledCount > 0 && iGroupCount == 0);
	m_pDisableHash->setEnabled(iEnabledCount > 0 && iGroupCount == 0);
	m_pRemoveHash->setEnabled(iSelectedCount > 0 && iGroupCount == 0);
	m_pEditHash->setEnabled(iSelectedCount == 1 && iGroupCount == 0);

	CPanelView::OnMenu(Point);
}

void CHashDBView::OnAddHash()
{
	CHashEntryWnd* pHashEntryWnd = new CHashEntryWnd(CHashPtr());
	pHashEntryWnd->show();
}

void CHashDBView::OnHashAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());

	QList<STATUS> Results;

	if (pAction == m_pEnableHash)
	{
		foreach(const SHashItemPtr & pItem, m_SelectedItems) {
			if (pItem->pEntry && !pItem->pEntry->IsEnabled())
			{
				pItem->pEntry->SetEnabled(true);
				STATUS Status = theCore->HashDB()->SetHash(pItem->pEntry);
				if(Status.IsError())
					pItem->pEntry->SetEnabled(false);
				Results << Status;
			}
		}
	}
	else if (pAction == m_pDisableHash)
	{
		foreach(const SHashItemPtr & pItem, m_SelectedItems) {
			if (pItem->pEntry && pItem->pEntry->IsEnabled())
			{
				pItem->pEntry->SetEnabled(false);
				STATUS Status = theCore->HashDB()->SetHash(pItem->pEntry);
				if(Status.IsError())
					pItem->pEntry->SetEnabled(true);
				Results << Status;
			}
		}
	}
	else if (pAction == m_pRemoveHash)
	{
		if(QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete selected Entries?") , QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes)
			return;

		foreach(const SHashItemPtr & pItem, m_SelectedItems) {
			if(pItem->pEntry)
				Results << theCore->DelHashEntry(pItem->pEntry->GetHash());
		}
	}
	else if (pAction == m_pEditHash)
	{
		OpenHashDialog(m_CurrentItem->pEntry);
	}

	theGUI->CheckResults(Results, this);
}

void CHashDBView::Refresh()
{
	theCore->HashDB()->UpdateAllHashes();
	m_FullRefresh = true;
}

//void CHashDBView::CleanTemp()
//{
//}