#include "pch.h"
#include "TrafficView.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CTrafficView::CTrafficView(QWidget *parent)
	:CPanelViewEx<CTrafficModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	connect(m_pItemModel, SIGNAL(CheckChanged(const QModelIndex&, bool)), this, SLOT(OnCheckChanged(const QModelIndex&, bool)));

	QByteArray Columns = theConf->GetBlob("MainWindow/TrafficView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbGrouping = new QComboBox();
	m_pCmbGrouping->addItem(QIcon(":/Icons/Internet.png"), tr("By Host"));
	m_pCmbGrouping->addItem(QIcon(":/Icons/Process.png"), tr("By Program"));
	m_pToolBar->addWidget(m_pCmbGrouping);

	int comboBoxHeight = m_pCmbGrouping->sizeHint().height();

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(comboBoxHeight);
	m_pToolBar->addWidget(pBtnSearch);

	AddPanelItemsToMenu();

	connect(theCore, SIGNAL(CleanUpDone()), this, SLOT(OnCleanUpDone()));
}

CTrafficView::~CTrafficView()
{
	theConf->SetBlob("MainWindow/TrafficView_Columns", m_pTreeView->saveState());
}

void CTrafficView::Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services)
{
	bool bGroupByProgram = m_pCmbGrouping->currentIndex() == 1;

	if (m_CurPrograms != Programs || m_CurServices != Services || bGroupByProgram != m_bGroupByProgram || m_RecentLimit != theGUI->GetRecentLimit()) {
		m_CurPrograms = Programs;
		m_CurServices = Services;
		m_RecentLimit = theGUI->GetRecentLimit();
		m_ParentMap.clear();
		m_TrafficMap.clear();
		m_pItemModel->Clear();
	}

	quint64 uRecentLimit = m_RecentLimit ? QDateTime::currentMSecsSinceEpoch() - m_RecentLimit : 0;

	m_bGroupByProgram = bGroupByProgram;

	auto OldTraffic = m_TrafficMap;

	foreach(auto& pItem, m_ParentMap)
		pItem->pEntry->Reset();

	std::function<void(const CProgramItemPtr&, const CTrafficEntryPtr&)> AddByDomain = [&](const CProgramItemPtr& pProg, const CTrafficEntryPtr& pEntry) {

		STrafficItemPtr& pItem = m_ParentMap[pEntry->GetHostName()];
		if(pItem.isNull())
		{
			pItem = STrafficItemPtr(new STrafficItem());
			pItem->pEntry = CTrafficEntryPtr(new CTrafficEntry());
			pItem->pEntry->SetHostName(pEntry->GetHostName());
			pItem->pEntry->SetIpAddress(pEntry->GetIpAddress());
			m_TrafficMap.insert((uint64)pItem->pEntry.get(), pItem);
		}
		else
			OldTraffic.remove((uint64)pItem->pEntry.get());

		STrafficItemPtr pSubItem = OldTraffic.take((uint64)pEntry.get());
		if (!pSubItem) {
			pSubItem = STrafficItemPtr(new STrafficItem());
			pSubItem->pEntry = pEntry;	
			pSubItem->pProg = pProg;
			pSubItem->Parent = (uint64)pItem->pEntry.get();
			m_TrafficMap.insert((uint64)pSubItem->pEntry.get(), pSubItem);
		}

		pItem->pEntry->Merge(pEntry);
	};

	std::function<void(const CProgramItemPtr&, const CTrafficEntryPtr&)> AddByProgram = [&](const CProgramItemPtr& pProg, const CTrafficEntryPtr& pEntry) {

		STrafficItemPtr& pItem = m_ParentMap[QString::number((quint64)pProg.get())];
		if(pItem.isNull())
		{
			pItem = STrafficItemPtr(new STrafficItem());
			pItem->pEntry = CTrafficEntryPtr(new CTrafficEntry());
			pItem->pProg = pProg;
			pItem->pEntry->SetHostName(pEntry->GetHostName());
			pItem->pEntry->SetIpAddress(pEntry->GetIpAddress());
			m_TrafficMap.insert((uint64)pItem->pEntry.get(), pItem);
		}
		else
			OldTraffic.remove((uint64)pItem->pEntry.get());

		STrafficItemPtr pSubItem = OldTraffic.take((uint64)pEntry.get());
		if (!pSubItem) {
			pSubItem = STrafficItemPtr(new STrafficItem());
			pSubItem->pEntry = pEntry;	
			pSubItem->Parent = (uint64)pItem->pEntry.get();
			m_TrafficMap.insert((uint64)pSubItem->pEntry.get(), pSubItem);
		}

		pItem->pEntry->Merge(pEntry);
	};

	foreach(const CProgramFilePtr& pProgram, Programs) {
		QMap<QString, CTrafficEntryPtr> Log = pProgram->GetTrafficLog();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			if (uRecentLimit && I.value()->GetLastActivity() < uRecentLimit)
				continue;
			if (m_bGroupByProgram)
				AddByProgram(pProgram, I.value());
			else
				AddByDomain(pProgram, I.value());
		}
	}
	foreach(const CWindowsServicePtr& pService, Services) {
		QMap<QString, CTrafficEntryPtr> Log = pService->GetTrafficLog();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			if (uRecentLimit && I.value()->GetLastActivity() < uRecentLimit)
				continue;
			if (m_bGroupByProgram)
				AddByProgram(pService, I.value());
			else
				AddByDomain(pService, I.value());
		}
	}

	foreach(quint64 Key, OldTraffic.keys()) {
		STrafficItemPtr pOld = m_TrafficMap.take(Key);
		if(pOld->pProg.isNull())
			m_ParentMap.remove(pOld->pEntry->GetHostName());
	}

	QList<QModelIndex> Added = m_pItemModel->Sync(m_TrafficMap);

	if (m_CurPrograms.count() + m_CurServices.count() > 1)
	{
		QTimer::singleShot(10, this, [this, Added]() {
			foreach(const QModelIndex & Index, Added)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
		});
	}
}

/*void CTrafficView::OnDoubleClicked(const QModelIndex& Index)
{
	//QPair<CProgramItemPtr, CTrafficEntryPtr> pItem = m_pItemModel->GetItem(Index);
}*/

void CTrafficView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}

void CTrafficView::OnCleanUpDone()
{
	// refresh
	m_CurPrograms.clear();
	m_CurServices.clear();
}