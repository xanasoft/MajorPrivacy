#include "pch.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramManager.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "ProgramView.h"
#include "../../MiscHelpers/Common/CustomStyles.h"
#include "../Windows/ProgramWnd.h"
#include "../MajorPrivacy.h"

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

	m_pFinder = new CFinder(m_pSortProxy, this);
	m_pMainLayout->addWidget(m_pFinder);

	connect(m_pTreeList, SIGNAL(MenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));
#else
	m_pTreeList->setItemDelegate(new CTreeItemDelegate());

	m_pMainLayout->addWidget(CFinder::AddFinder(m_pTreeList, m_pSortProxy, CFinder::eDefault, &m_pFinder));

	m_pTreeList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTreeList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));
#endif
	
	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pToolBar->setFixedHeight(30);
	m_pMainLayout->insertWidget(0, m_pToolBar);

	/*m_pBtnClear = new QToolButton();
	m_pBtnClear->setIcon(QIcon(":/Icons/Clean.png"));
	m_pBtnClear->setToolTip(tr("Cleanup Program List"));
	m_pBtnClear->setFixedHeight(22);
	connect(m_pBtnClear, SIGNAL(clicked()), this, SLOT(CleanUpPrograms()));
	m_pToolBar->addWidget(m_pBtnClear);*/

	m_pBtnPrograms = new QToolButton();
	m_pBtnPrograms->setIcon(QIcon(":/Icons/Program.png"));
	m_pBtnPrograms->setToolTip(tr("Show Win32 Programs"));
	m_pBtnPrograms->setCheckable(true);
	m_pToolBar->addWidget(m_pBtnPrograms);

	m_pBtnApps = new QToolButton();
	m_pBtnApps->setIcon(QIcon(":/Icons/StoreApp.png"));
	m_pBtnApps->setToolTip(tr("Show Modern Apps"));
	m_pBtnApps->setCheckable(true);
	m_pToolBar->addWidget(m_pBtnApps);

	m_pBtnSystem = new QToolButton();
	m_pBtnSystem->setIcon(QIcon(":/Icons/System.png"));
	m_pBtnSystem->setToolTip(tr("Show System Components"));
	m_pBtnSystem->setCheckable(true);
	m_pToolBar->addWidget(m_pBtnSystem);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setFixedHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	//connect(theGUI, SIGNAL(ReloadPanels()), m_pProgramModel, SLOT(Clear()));

	connect(m_pTreeList, SIGNAL(SelectionChanged(const QModelIndexList&)), this, SLOT(OnProgramChanged(const QModelIndexList&)));
	connect(m_pTreeList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));

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

	m_pCreateProgram = m_pMenu->addAction(QIcon(":/Icons/Process.png"), tr("Add Program"), this, SLOT(OnProgramAction()));
	//m_pCreateGroup = m_pMenu->addAction(QIcon(":/Icons/Group.png"), tr("Create Group"), this, SLOT(OnProgramAction())); // todo
	//m_pMenu->addSeparator();
	//m_pAddToGroup = m_pMenu->addAction(QIcon(":/Icons/MkLink.png"), tr("Add to Group"), this, SLOT(OnProgramAction())); // todo
	//m_pRenameItem = m_pMenu->addAction(QIcon(":/Icons/Rename.png"), tr("Rename"), this, SLOT(OnProgramAction()));
	m_pRemoveItem = m_pMenu->addAction(QIcon(":/Icons/Remove.png"), tr("Remove"), this, SLOT(OnProgramAction()));

	AddPanelItemsToMenu();
}

CProgramView::~CProgramView()
{	
	theConf->SetBlob("MainWindow/ProgramView_Columns", m_pTreeList->saveState());
}

void CProgramView::Update()
{
	int Filter = CProgramModel::EFilters::eAll;
	if(m_pBtnPrograms->isChecked())
		Filter |= CProgramModel::EFilters::ePrograms;
	if (m_pBtnApps->isChecked())
		Filter |= CProgramModel::EFilters::eApps;
	if (m_pBtnSystem->isChecked())
		Filter |= CProgramModel::EFilters::eSystem;
	if (Filter != m_pProgramModel->GetFilter()) {
		m_pProgramModel->SetFilter(Filter);
		m_pProgramModel->Clear();
	}

	QList<QVariant> AddedProgs = m_pProgramModel->Sync(theCore->ProgramManager()->GetRoot());

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

	emit ProgramsChanged(m_CurPrograms);
}

void CProgramView::OnDoubleClicked(const QModelIndex& Index)
{
	CProgramItemPtr pProgram = m_pProgramModel->GetItem(m_pSortProxy->mapToSource(Index));
	if(!pProgram) return;
	CProgramWnd* pProgramWnd = new CProgramWnd(pProgram);
	pProgramWnd->show();
}

void CProgramView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}

void CProgramView::OnProgramAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());
	
	QList<STATUS> Results;
	
	if (pAction == m_pCreateProgram)
	{
		CProgramWnd* pProgramWnd = new CProgramWnd(NULL);
		pProgramWnd->exec();
		m_pProgramModel->SetFilter(-1); // force full refresh
	} 
	else if (pAction == m_pCreateGroup)
	{
	}
	else if (pAction == m_pAddToGroup)
	{
	}
	/*else if (pAction == m_pRenameItem)
	{
		if (m_CurPrograms.size() != 1)
			return;

		CProgramItemPtr pItem = m_CurPrograms[0];
		QString Value = QInputDialog::getText(this, "MajorPrivacy", tr("Please enter a new name"), QLineEdit::Normal, pItem->GetNameEx());
		if (Value.isEmpty())
			return;

		pItem->SetName(Value);
		Results.append(theCore->ProgramManager()->SetProgramInfo(pItem));
	}*/
	else if (pAction == m_pRemoveItem)
	{
		int Response = QMessageBox::NoButton;

		foreach(const QModelIndex& index, m_pTreeList->selectionModel()->selectedIndexes())
		{
			if (index.column() != 0)
				continue;
			QModelIndex ModelIndex = m_pSortProxy->mapToSource(index);

			CProgramItemPtr pProgram = m_pProgramModel->GetItem(ModelIndex);
			CProgramItemPtr pParent = ModelIndex.parent().isValid() 
				? m_pProgramModel->GetItem(ModelIndex.parent()) 
				: theCore->ProgramManager()->GetRoot();
			if (pProgram && pParent)
			{
				if (Response == QMessageBox::NoButton) {
					if(pProgram->GetGroups().count() > 1)
						Response = QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete selected Program Items?\n"
							"The selected Item belongs to more than one Group, to delete it from all groups press 'Yes to All', pressing 'Yes' will only remove it from the group its sellected in.")
							, QMessageBox::Yes, QMessageBox::YesToAll, QMessageBox::Cancel);
					else
						Response = QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete selected Program Items?")
							, QMessageBox::Yes, QMessageBox::Cancel);
					if(Response == QMessageBox::Cancel)
						break;
				}

				Results.append(theCore->ProgramManager()->RemoveProgramFrom(pProgram, Response == QMessageBox::YesToAll ? CProgramItemPtr() : pParent));
			}
		}

		m_pProgramModel->SetFilter(-1); // force full refresh
	}

	theGUI->CheckResults(Results, this);
}	
