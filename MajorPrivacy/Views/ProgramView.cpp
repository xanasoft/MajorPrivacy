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

	m_pTypeFilter = new QToolButton();
	m_pTypeFilter->setIcon(QIcon(":/Icons/ProgFilter.png"));
	m_pTypeFilter->setToolTip(tr("Type Filters"));
	//m_pTypeFilter->setText(tr(""));
	m_pTypeFilter->setCheckable(true);
	connect(m_pTypeFilter, SIGNAL(clicked()), this, SLOT(OnTypeFilter()));
	m_pTypeMenu = new QMenu();
		m_pPrograms = m_pTypeMenu->addAction(tr("Programs"), this, SLOT(OnTypeFilter()));
		m_pPrograms->setCheckable(true);
		m_pApps = m_pTypeMenu->addAction(tr("Apps"), this, SLOT(OnTypeFilter()));
		m_pApps->setCheckable(true);
		m_pSystem = m_pTypeMenu->addAction(tr("System"), this, SLOT(OnTypeFilter()));
		m_pSystem->setCheckable(true);
		m_pGroups = m_pTypeMenu->addAction(tr("Groups"), this, SLOT(OnTypeFilter()));
		m_pGroups->setCheckable(true);
	m_pTypeFilter->setPopupMode(QToolButton::MenuButtonPopup);
	m_pTypeFilter->setMenu(m_pTypeMenu);
	m_pToolBar->addWidget(m_pTypeFilter);

	m_pRunFilter = new QToolButton();
	m_pRunFilter->setIcon(QIcon(":/Icons/RunFilter.png"));
	m_pRunFilter->setToolTip(tr("Run Filters"));
	//m_pRunFilter->setText(tr(""));
	m_pRunFilter->setCheckable(true);
	connect(m_pRunFilter, SIGNAL(clicked()), this, SLOT(OnRunFilter()));
	m_pRunMenu = new QMenu();
		m_pRanRecently = m_pRunMenu->addAction(tr("Ran Recently"), this, SLOT(OnRunFilter()));
		m_pRanRecently->setCheckable(true);
	m_pRunFilter->setPopupMode(QToolButton::MenuButtonPopup);
	m_pRunFilter->setMenu(m_pRunMenu);
	m_pToolBar->addWidget(m_pRunFilter);

	m_pRulesFilter = new QToolButton();
	m_pRulesFilter->setIcon(QIcon(":/Icons/RuleFilter.png"));
	m_pRulesFilter->setToolTip(tr("Rule Filters"));
	//m_pRulesFilter->setText(tr(""));
	m_pRulesFilter->setCheckable(true);
	connect(m_pRulesFilter, SIGNAL(clicked()), this, SLOT(OnRulesFilter()));
	m_pRulesMenu = new QMenu();
		m_pExecRules = m_pRulesMenu->addAction(tr("Execution Rules"), this, SLOT(OnRulesFilter()));
		m_pExecRules->setCheckable(true);
		m_pAccessRules = m_pRulesMenu->addAction(tr("Access Rules"), this, SLOT(OnRulesFilter()));
		m_pAccessRules->setCheckable(true);
		m_pFwRules = m_pRulesMenu->addAction(tr("Firewall Rules"), this, SLOT(OnRulesFilter()));
		m_pFwRules->setCheckable(true);
	m_pRulesFilter->setPopupMode(QToolButton::MenuButtonPopup);
	m_pRulesFilter->setMenu(m_pRulesMenu);
	m_pToolBar->addWidget(m_pRulesFilter);

	m_pTrafficFilter = new QToolButton();
	m_pTrafficFilter->setIcon(QIcon(":/Icons/ActivityFilter.png"));
	m_pTrafficFilter->setToolTip(tr("Traffic Filters"));
	//m_pTrafficFilter->setText(tr(""));
	m_pTrafficFilter->setCheckable(true);
	connect(m_pTrafficFilter, SIGNAL(clicked()), this, SLOT(OnTrafficFilter()));
	m_pTrafficMenu = new QMenu();
		m_pActionGroup = new QActionGroup(this);
		m_pTrafficRecent = m_pTrafficMenu->addAction(tr("Recent Traffic"), this, SLOT(OnTrafficFilter()));
		m_pTrafficRecent->setCheckable(true);
		m_pActionGroup->addAction(m_pTrafficRecent);
		m_pTrafficBlocked = m_pTrafficMenu->addAction(tr("Blocked Traffic"), this, SLOT(OnTrafficFilter()));
		m_pTrafficBlocked->setCheckable(true);
		m_pActionGroup->addAction(m_pTrafficBlocked);
		m_pTrafficAllowed = m_pTrafficMenu->addAction(tr("Allowed Traffic"), this, SLOT(OnTrafficFilter()));
		m_pTrafficAllowed->setCheckable(true);
		m_pActionGroup->addAction(m_pTrafficAllowed);
	m_pTrafficFilter->setPopupMode(QToolButton::MenuButtonPopup);
	m_pTrafficFilter->setMenu(m_pTrafficMenu);
	m_pToolBar->addWidget(m_pTrafficFilter);

	m_pSocketFilter = new QToolButton();
	m_pSocketFilter->setIcon(QIcon(":/Icons/SocketFilter.png"));
	m_pSocketFilter->setToolTip(tr("Socket Filters"));
	//m_pSocketFilter->setText(tr(""));
	m_pSocketFilter->setCheckable(true);
	connect(m_pSocketFilter, SIGNAL(clicked()), this, SLOT(OnSocketFilter()));
	/*m_pSocketMenu = new QMenu();
		m_pAnySockets = m_pSocketMenu->addAction(tr("Any Sockets"), this, SLOT(OnSocketFilter()));
		m_pAnySockets->setCheckable(true);
		m_pWebSockets = m_pSocketMenu->addAction(tr("Web Sockets"), this, SLOT(OnSocketFilter()));
		m_pWebSockets->setCheckable(true);
		m_pTcpSockets = m_pSocketMenu->addAction(tr("TCP Sockets"), this, SLOT(OnSocketFilter()));
		m_pTcpSockets->setCheckable(true);
		m_pTcpClients = m_pSocketMenu->addAction(tr("TCP Clients"), this, SLOT(OnSocketFilter()));
		m_pTcpClients->setCheckable(true);
		m_pTcpServers = m_pSocketMenu->addAction(tr("TCP Servers"), this, SLOT(OnSocketFilter()));
		m_pTcpServers->setCheckable(true);
		m_pUdpSockets = m_pSocketMenu->addAction(tr("UDP Sockets"), this, SLOT(OnSocketFilter()));
		m_pUdpSockets->setCheckable(true);
	m_pSocketFilter->setPopupMode(QToolButton::MenuButtonPopup);
	m_pSocketFilter->setMenu(m_pSocketMenu);*/
	m_pToolBar->addWidget(m_pSocketFilter);


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

void CProgramView::OnTypeFilter()
{
	if (sender() == m_pTypeFilter)
	{
		if (!m_pPrograms->isChecked() && !m_pApps->isChecked() && !m_pSystem->isChecked() && !m_pGroups->isChecked()) {
			m_pPrograms->setChecked(true);
			m_pGroups->setChecked(true);
		}
	}
	else if (qobject_cast<QAction*>(sender())->isChecked())
	{
		if (!m_pTypeFilter->isChecked())
			m_pTypeFilter->setChecked(true);
	}

	FilterUpdate();
}

void CProgramView::OnRunFilter()
{
	if (sender() == m_pRunFilter)
		;
	else if (qobject_cast<QAction*>(sender())->isChecked())
	{
		if (!m_pRunFilter->isChecked())
			m_pRunFilter->setChecked(true);
	}

	FilterUpdate();
}

void CProgramView::OnRulesFilter()
{
	if (sender() == m_pRulesFilter)
	{
		if (!m_pExecRules->isChecked() && !m_pAccessRules->isChecked() && !m_pFwRules->isChecked()) {
			m_pExecRules->setChecked(true);
			m_pAccessRules->setChecked(true);
			m_pFwRules->setChecked(true);
		}
	}
	else if (qobject_cast<QAction*>(sender())->isChecked())
	{
		if (!m_pRulesFilter->isChecked())
			m_pRulesFilter->setChecked(true);
	}

	FilterUpdate();
}

void CProgramView::OnTrafficFilter()
{
	if (sender() == m_pTrafficFilter) 
	{
		if (m_pActionGroup->checkedAction() == nullptr)
			m_pTrafficRecent->setChecked(true);
	}
	else
	{
		if (!m_pTrafficFilter->isChecked())
			m_pTrafficFilter->setChecked(true);
	}

	FilterUpdate();
}

void CProgramView::OnSocketFilter()
{
	FilterUpdate();
}

void CProgramView::FilterUpdate()
{
	Update();

	m_CurPrograms.clear();
	emit ProgramsChanged(m_CurPrograms);
}

void CProgramView::Update()
{
	int Filter = CProgramModel::EFilters::eAll;

	if (m_pTypeFilter->isChecked())
	{
		if (m_pPrograms->isChecked())
			Filter |= CProgramModel::EFilters::ePrograms;
		if (m_pApps->isChecked())
			Filter |= CProgramModel::EFilters::eApps;
		if (m_pSystem->isChecked())
			Filter |= CProgramModel::EFilters::eSystem;
		if (m_pGroups->isChecked())
			Filter |= CProgramModel::EFilters::eGroups;
	}

	if (m_pRunFilter->isChecked()) {
		Filter |= CProgramModel::EFilters::eRunning;
		if (m_pRanRecently->isChecked())
			Filter |= CProgramModel::EFilters::eRanRecently;
	}


	if (m_pRulesFilter->isChecked())
	{
		if (m_pExecRules->isChecked())
			Filter |= CProgramModel::EFilters::eWithProgRules;
		if (m_pAccessRules->isChecked())
			Filter |= CProgramModel::EFilters::eWithResRules;
		if (m_pFwRules->isChecked())
			Filter |= CProgramModel::EFilters::eWithFwRules;
	}

	if (m_pTrafficFilter->isChecked())
	{
		if (m_pTrafficRecent->isChecked())
			Filter |= CProgramModel::EFilters::eRecentTraffic;
		if (m_pTrafficBlocked->isChecked())
			Filter |= CProgramModel::EFilters::eBlockedTraffic;
		if (m_pTrafficAllowed->isChecked())
			Filter |= CProgramModel::EFilters::eAllowedTraffic;
	}

	if (m_pSocketFilter->isChecked())
		Filter |= CProgramModel::EFilters::eWithSockets;

	quint64 RecentLimit = theGUI->GetRecentLimit();
	if (Filter != m_pProgramModel->GetFilter() || RecentLimit != m_pProgramModel->GetRecentLimit()) {
		m_pProgramModel->SetFilter(Filter);
		m_pProgramModel->SetRecentLimit(RecentLimit);
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
