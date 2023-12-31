#include "pch.h"
#include "NetTraceView.h"
#include "../Core/PrivacyCore.h"

CNetTraceView::CNetTraceView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	m_pTreeView = new QTreeViewEx();
	m_pMainLayout->addWidget(m_pTreeView);

	m_pItemModel = new CNetTraceModel();
	//connect(m_pItemModel, SIGNAL(CheckChanged(quint64, bool)), this, SLOT(OnCheckChanged(quint64, bool)));
	//connect(m_pItemModel, SIGNAL(Updated()), this, SLOT(OnUpdated()));

	//m_pItemModel->SetTree(false);

	//m_pSortProxy = new CSortFilterProxyModel(this);
	//m_pSortProxy->setSortRole(Qt::EditRole);
    //m_pSortProxy->setSourceModel(m_pItemModel);
	//m_pSortProxy->setDynamicSortFilter(true);

	//m_pTreeView->setItemDelegate(theGUI->GetItemDelegate());
	m_pTreeView->setMinimumHeight(50);

	//m_pTreeView->setModel(m_pSortProxy);
	m_pTreeView->setModel(m_pItemModel);

	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	//m_pTreeView->setSortingEnabled(true);

	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTreeView, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	//connect(theGUI, SIGNAL(ReloadPanels()), m_pItemModel, SLOT(Clear()));

	connect(m_pTreeView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));
	connect(m_pTreeView, SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));
	connect(m_pTreeView, SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(OnSelectionChanged(QItemSelection,QItemSelection)));

	m_pTreeView->setColumnReset(2);
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	QByteArray Columns = theConf->GetBlob("MainWindow/NetTraceView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	AddPanelItemsToMenu();
}

CNetTraceView::~CNetTraceView()
{
	theConf->SetBlob("MainWindow/NetTraceView_Columns", m_pTreeView->saveState());
}

void CNetTraceView::Sync(const struct SMergedLog* pLog)
{
	m_pItemModel->Sync(pLog);
}

void CNetTraceView::OnDoubleClicked(const QModelIndex& Index)
{

}

void CNetTraceView::OnCurrentChanged(const QModelIndex& current, const QModelIndex& previous)
{
	/*QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);
	CLogEntryPtr pEntry = m_pItemModel->GetEntry(ModelIndex);

	m_CurrentEntry = pEntry;

	emit CurrentChanged(pEntry);*/
}

void CNetTraceView::OnSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected)
{
	/*QList<CLogEntryPtr> List;
	foreach(const QModelIndex& Index, m_pTreeView->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CLogEntryPtr pEntry = m_pItemModel->GetEntry(ModelIndex);
		if (!pEntry)
			continue;
		List.append(pEntry);
	}

	m_SelectedEntries = List;

	emit SelectionChanged(List);*/
}

void CNetTraceView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
