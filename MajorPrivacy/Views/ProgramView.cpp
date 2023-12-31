#include "pch.h"
#include "../Core/PrivacyCore.h"
#include "../Core/ProcessList.h"
#include "../Core/Programs/ProgramManager.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "ProgramView.h"
#include "../../MiscHelpers/Common/CustomStyles.h"

CProgramView::CProgramView(QWidget* parent)
	: CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pProgramModel = new CProgramModel();
	//connect(m_pProgramModel, SIGNAL(CheckChanged(quint64, bool)), this, SLOT(OnCheckChanged(quint64, bool)));
	//connect(m_pProgramModel, SIGNAL(Updated()), this, SLOT(OnUpdated()));

	//m_pProgramModel->SetTree(false);

	m_pSortProxy = new CSortFilterProxyModel(this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pProgramModel);
	m_pSortProxy->setDynamicSortFilter(true);


#ifdef SPLIT_TREE
	m_pTreeList = new CSplitTreeView(m_pSortProxy);
#else
	m_pTreeList = new QTreeViewEx();
	m_pTreeList->setModel(m_pSortProxy);

	m_pTreeList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeList->setSortingEnabled(true);
	m_pTreeList->setExpandsOnDoubleClick(false);
#endif
	m_pMainLayout->addWidget(m_pTreeList);
	m_pTreeList->setMinimumHeight(50);

	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeList->setStyle(pStyle);
#ifdef SPLIT_TREE
	m_pTreeList->GetView()->setItemDelegate(new CTreeItemDelegate());
	m_pTreeList->GetTree()->setItemDelegate(new CTreeItemDelegate());

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

	connect(m_pTreeList, SIGNAL(MenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));
#else
	m_pTreeList->setItemDelegate(new CTreeItemDelegate());

	m_pMainLayout->addWidget(CFinder::AddFinder(m_pTreeList, m_pSortProxy));

	m_pTreeList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTreeList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));
#endif
	
	//connect(theGUI, SIGNAL(ReloadPanels()), m_pProgramModel, SLOT(Clear()));

	connect(m_pTreeList, SIGNAL(SelectionChanged(const QModelIndexList&)), this, SLOT(OnProgramChanged(const QModelIndexList&)));

	//m_pTreeList->setColumnReset(2);
	//connect(m_pTreeList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	QByteArray ProgColumns = theConf->GetBlob("MainWindow/ProgramView_Columns");
	if (ProgColumns.isEmpty())
#ifdef SPLIT_TREE
		m_pTreeList->GetView()->setColumnWidth(0, 300);
#else
		m_pTreeList->setColumnWidth(0, 300);
#endif
	else
		m_pTreeList->restoreState(ProgColumns);

	AddPanelItemsToMenu();
}

CProgramView::~CProgramView()
{	
	theConf->SetBlob("MainWindow/ProgramView_Columns", m_pTreeList->saveState());
}

void CProgramView::Update()
{
	QList<QVariant> AddedProgs = m_pProgramModel->Sync(theCore->Programs()->GetRoot());

	if (m_pProgramModel->IsTree())
	{
		QTimer::singleShot(10, this, [this, AddedProgs]() {
			foreach(const QVariant ID, AddedProgs) {

				QModelIndex ModelIndex = m_pProgramModel->FindIndex(ID);
				
				m_pTreeList->expand(m_pSortProxy->mapFromSource(ModelIndex));
			}
		});
	}
}

void CProgramView::OnProgramChanged(const QModelIndexList& Selection)
{
	m_CurPrograms.clear();
	foreach(const QModelIndex& index, Selection)
	{
		if (index.column() != 0)
			continue;
		CProgramItemPtr pProgram = m_pProgramModel->GetItem(m_pSortProxy->mapToSource(index));
		if (pProgram) m_CurPrograms.append(pProgram);
	}
}