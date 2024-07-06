#include "pch.h"
#include "AccessView.h"
#include "../Core/PrivacyCore.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CAccessView::CAccessView(QWidget *parent)
	:CPanelViewEx<CAccessModel>(parent)
{
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());

	m_pTreeView->setColumnReset(2);
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	connect(m_pItemModel, SIGNAL(CheckChanged(const QModelIndex&, bool)), this, SLOT(OnCheckChanged(const QModelIndex&, bool)));

	QByteArray Columns = theConf->GetBlob("MainWindow/AccessView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	AddPanelItemsToMenu();
}

CAccessView::~CAccessView()
{
	theConf->SetBlob("MainWindow/AccessView_Columns", m_pTreeView->saveState());
}

void CAccessView::Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services)
{
	if (m_CurPrograms != Programs || m_CurServices != Services) {
		m_CurPrograms = Programs;
		m_CurServices = Services;
		m_CurAccess.clear();
		m_pItemModel->Clear();
	}

	auto OldMap = m_CurAccess;

	std::function<void(const CProgramItemPtr&, const SAccessStatsPtr&)> AddEntry = 
		[&](const CProgramItemPtr& pProg, const SAccessStatsPtr& pStats) {

		QString Path = pStats->Path.Get(EPathType::eWin32);
		if (Path.isEmpty())
			return;

		SAccessItemPtr pItem = OldMap.take(Path);
		if (pItem.isNull()) {
			pItem = SAccessItemPtr(new SAccessItem());
			m_CurAccess.insert(Path, pItem);
		}
		
		pItem->Stats[pProg] = pStats;
	};

	foreach(const CProgramFilePtr& pProgram, Programs) {
		QMap<quint64, SAccessStatsPtr> Log = pProgram->GetAccessStats();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			AddEntry(pProgram, I.value());
		}
	}
	foreach(const CWindowsServicePtr& pService, Services) {
		QMap<quint64, SAccessStatsPtr> Log = pService->GetAccessStats();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			AddEntry(pService, I.value());
		}
	}

	foreach(QString Key, OldMap.keys())
		m_CurAccess.remove(Key);

	QList<QModelIndex> Added = m_pItemModel->Sync(m_CurAccess);

	if (m_CurPrograms.count() + m_CurServices.count() > 1)
	{
		QTimer::singleShot(10, this, [this, Added]() {
			foreach(const QModelIndex & Index, Added)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
		});
	}
}

void CAccessView::OnDoubleClicked(const QModelIndex& Index)
{
	//QPair<CProgramItemPtr, CTrafficEntryPtr> pItem = m_pItemModel->GetItem(Index);
}

void CAccessView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
