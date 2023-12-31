#include "pch.h"
#include "ProcessView.h"
#include "../Core/PrivacyCore.h"

CProcessView::CProcessView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	m_pTreeView = new QTreeViewEx();
	m_pMainLayout->addWidget(m_pTreeView);

	m_pItemModel = new CProcessModel();
	//connect(m_pItemModel, SIGNAL(CheckChanged(quint64, bool)), this, SLOT(OnCheckChanged(quint64, bool)));
	//connect(m_pItemModel, SIGNAL(Updated()), this, SLOT(OnUpdated()));

	//m_pItemModel->SetTree(false);

	m_pSortProxy = new CSortFilterProxyModel(this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pItemModel);
	m_pSortProxy->setDynamicSortFilter(true);

	//m_pTreeView->setItemDelegate(theGUI->GetItemDelegate());
	m_pTreeView->setMinimumHeight(50);

	m_pTreeView->setModel(m_pSortProxy);

	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeView->setSortingEnabled(true);

	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTreeView, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	//connect(theGUI, SIGNAL(ReloadPanels()), m_pItemModel, SLOT(Clear()));

	connect(m_pTreeView, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));
	connect(m_pTreeView, SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(OnCurrentChanged(QModelIndex,QModelIndex)));
	connect(m_pTreeView, SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this, SLOT(OnSelectionChanged(QItemSelection,QItemSelection)));

	m_pTreeView->setColumnReset(2);
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	QByteArray Columns = theConf->GetBlob("MainWindow/ProcessView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	AddPanelItemsToMenu();
}

CProcessView::~CProcessView()
{
	theConf->SetBlob("MainWindow/ProcessView_Columns", m_pTreeView->saveState());
}

void CProcessView::Sync(const QMap<quint64, CProcessPtr>& ProcessMap)
{
	QSet<quint64> AddedProcs = m_pItemModel->Sync(ProcessMap);

	if (m_pItemModel->IsTree()) {
		QTimer::singleShot(10, this, [this, AddedProcs]() {
			foreach(const QVariant ID, AddedProcs) {
				QModelIndex ModelIndex = m_pItemModel->FindIndex(ID);	
				m_pTreeView->expand(m_pSortProxy->mapFromSource(ModelIndex));
			}
		});
	}
}

void CProcessView::OnDoubleClicked(const QModelIndex& Index)
{

}

void CProcessView::OnCurrentChanged(const QModelIndex& current, const QModelIndex& previous)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(current);
	CProcessPtr pProcess = m_pItemModel->GetItem(ModelIndex);

	m_CurrentProcess = pProcess;

	emit CurrentChanged(pProcess);
}

void CProcessView::OnSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected)
{
	QList<CProcessPtr> List;
	foreach(const QModelIndex& Index, m_pTreeView->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CProcessPtr pProcess = m_pItemModel->GetItem(ModelIndex);
		if (!pProcess)
			continue;
		List.append(pProcess);
	}

	m_SelectedProcesses = List;

	emit SelectionChanged(List);
}

void CProcessView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
