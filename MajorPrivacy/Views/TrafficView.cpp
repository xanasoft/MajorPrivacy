#include "pch.h"
#include "TrafficView.h"
#include "../Core/PrivacyCore.h"

CTrafficView::CTrafficView(QWidget *parent)
	:CPanelViewEx<CTrafficModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	connect(m_pItemModel, SIGNAL(CheckChanged(const QModelIndex&, bool)), this, SLOT(OnCheckChanged(const QModelIndex&, bool)));

	QByteArray Columns = theConf->GetBlob("MainWindow/TrafficView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	AddPanelItemsToMenu();
}

CTrafficView::~CTrafficView()
{
	theConf->SetBlob("MainWindow/TrafficView_Columns", m_pTreeView->saveState());
}

void CTrafficView::Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services)
{
	if (m_CurPrograms != Programs || m_CurServices != Services) {
		m_CurPrograms = Programs;
		m_CurServices = Services;
		m_HostMap.clear();
		m_CurTraffic.clear();
		m_pItemModel->Clear();
	}

	auto OldTraffic = m_CurTraffic;

	foreach(auto& pItem, m_HostMap)
		pItem->pEntry->Reset();

	std::function<void(const CProgramItemPtr&, const CTrafficEntryPtr&)> AddEntry = [&](const CProgramItemPtr& pProg, const CTrafficEntryPtr& pEntry) {

		QString HostName = pEntry->GetHostName();

		STrafficItemPtr& pItem = m_HostMap[HostName];
		if(pItem.isNull())
		{
			pItem = STrafficItemPtr(new STrafficItem());
			pItem->pEntry = CTrafficEntryPtr(new CTrafficEntry());
			pItem->pEntry->SetHostName(HostName);
			m_CurTraffic.insert((uint64)pItem->pEntry.get(), pItem);
		}
		else
			OldTraffic.remove((uint64)pItem->pEntry.get());

		STrafficItemPtr pSubItem = OldTraffic.take((uint64)pEntry.get());
		if (!pSubItem) {
			pSubItem = STrafficItemPtr(new STrafficItem());
			pSubItem->pEntry = pEntry;	
			pSubItem->pProg = pProg;
			pSubItem->Parent = (uint64)pItem->pEntry.get();
			m_CurTraffic.insert((uint64)pSubItem->pEntry.get(), pSubItem);
		}

		pItem->pEntry->Merge(pEntry);
	};

	foreach(const CProgramFilePtr& pProgram, Programs) {
		QMap<QString, CTrafficEntryPtr> Log = pProgram->GetTrafficLog();
		for (auto I = Log.begin(); I != Log.end(); I++)
			AddEntry(pProgram, I.value());
	}
	foreach(const CWindowsServicePtr& pService, Services) {
		QMap<QString, CTrafficEntryPtr> Log = pService->GetTrafficLog();
		for (auto I = Log.begin(); I != Log.end(); I++)
			AddEntry(pService, I.value());
	}

	foreach(quint64 Key, OldTraffic.keys()) {
		STrafficItemPtr pOld = m_CurTraffic.take(Key);
		if(pOld->pProg.isNull())
			m_HostMap.remove(pOld->pEntry->GetHostName());
	}

	QList<QModelIndex> Added = m_pItemModel->Sync(m_CurTraffic);

	if (m_CurPrograms.count() + m_CurServices.count() > 1)
	{
		QTimer::singleShot(10, this, [this, Added]() {
			foreach(const QModelIndex & Index, Added)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
			});
	}
}

void CTrafficView::OnDoubleClicked(const QModelIndex& Index)
{
	//QPair<CProgramItemPtr, CTrafficEntryPtr> pItem = m_pItemModel->GetItem(Index);
}

void CTrafficView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
