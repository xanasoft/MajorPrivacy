#include "pch.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramManager.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "ProgramView.h"
#include "../../MiscHelpers/Common/CustomStyles.h"
#include "../Windows/ProgramWnd.h"
#include "../MajorPrivacy.h"
#include "InfoView.h"

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

	m_pProgramWidget = new QWidget();
	m_pProgramLayout = new QVBoxLayout(m_pProgramWidget);
	m_pProgramLayout->setContentsMargins(0,0,0,0);

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
	m_pTreeList->GetView()->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	m_pTreeList->GetTree()->setItemDelegate(new CTreeItemDelegate());
	m_pTreeList->GetTree()->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));

	m_pFinder = new CFinder(m_pSortProxy, this);
	m_pMainLayout->addWidget(m_pFinder);

	connect(m_pTreeList, SIGNAL(MenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));
#else
	m_pTreeList->setItemDelegate(new CTreeItemDelegate());
	m_pTreeList->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));

	m_pProgramLayout->addWidget(CFinder::AddFinder(m_pTreeList, m_pSortProxy, CFinder::eDefault, &m_pFinder));

	m_pTreeList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTreeList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));
#endif

	m_pInfoSplitter = new QSplitter();
	m_pInfoSplitter->addWidget(m_pProgramWidget);
	m_pInfoSplitter->setCollapsible(0, false);
	m_pInfoSplitter->setOrientation(Qt::Vertical);
	
	m_pInfoView = new CInfoView();
	m_pInfoSplitter->addWidget(m_pInfoView);
	m_pInfoSplitter->setStretchFactor(0, 1);
	m_pInfoSplitter->setStretchFactor(1, 0);
	/*auto Sizes = m_pInfoSplitter->sizes();
	Sizes[1] = 0;
	m_pInfoSplitter->setSizes(Sizes);*/
	m_pInfoView->setVisible(false);

	QAction* pToggleInfo = new QAction(m_pInfoSplitter);
	pToggleInfo->setShortcut(QKeySequence("Ctrl+I"));
	pToggleInfo->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	addAction(pToggleInfo);
	connect(pToggleInfo, &QAction::triggered, m_pInfoSplitter, [&]() { m_pInfoView->setVisible(!m_pInfoView->isVisible()); });

	m_pMainLayout->addWidget(m_pInfoSplitter);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pToolBar->setFixedHeight(30);
	m_pMainLayout->insertWidget(0, m_pToolBar);

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

	m_pToolBar->addSeparator();

	m_pBtnRefresh = new QToolButton();
	m_pBtnRefresh->setIcon(QIcon(":/Icons/Recover.png"));
	m_pBtnRefresh->setToolTip(tr("Refresh Programs"));
	m_pBtnRefresh->setMaximumHeight(22);
	connect(m_pBtnRefresh, SIGNAL(clicked()), this, SLOT(OnRefresh()));
	m_pToolBar->addWidget(m_pBtnRefresh);

	m_pBtnCleanUp = new QToolButton();
	m_pBtnCleanUp->setIcon(QIcon(":/Icons/Clean.png"));
	m_pBtnCleanUp->setToolTip(tr("CleanUp Program List"));
	m_pBtnCleanUp->setFixedHeight(22);
	connect(m_pBtnCleanUp, SIGNAL(clicked()), theGUI, SLOT(CleanUpPrograms()));
	m_pCleanUpMenu = new QMenu();
	m_pReGroup = m_pCleanUpMenu->addAction(QIcon(":/Icons/ReGroup.png"), tr("Re-Group all Programs"), theGUI, SLOT(ReGroupPrograms()));
	m_pBtnCleanUp->setPopupMode(QToolButton::MenuButtonPopup);
	m_pBtnCleanUp->setMenu(m_pCleanUpMenu);
	m_pToolBar->addWidget(m_pBtnCleanUp);

	m_pBtnAdd = new QToolButton();
	m_pBtnAdd->setIcon(QIcon(":/Icons/Add.png"));
	m_pBtnAdd->setToolTip(tr("Add Program Item"));
	m_pBtnAdd->setMaximumHeight(22);
	connect(m_pBtnAdd, SIGNAL(clicked()), this, SLOT(OnAddProgram()));
	m_pToolBar->addWidget(m_pBtnAdd);

	m_pToolBar->addSeparator();
	m_pBtnTree = new QToolButton();
	m_pBtnTree->setIcon(QIcon(":/Icons/Tree.png"));
	m_pBtnTree->setCheckable(true);
	m_pBtnTree->setToolTip(tr("Show Tree"));
	m_pBtnTree->setMaximumHeight(22);
	m_pBtnTree->setChecked(theConf->GetBool("Options/UseProgramTree", true));
	connect(m_pBtnTree, &QToolButton::toggled, this, [&](bool checked) {
		theConf->SetValue("Options/UseProgramTree", checked);
		m_pProgramModel->Clear();
		m_pBtnExpand->setEnabled(checked);
		Update();
	});
	m_pToolBar->addWidget(m_pBtnTree);

	m_pBtnExpand = new QToolButton();
	m_pBtnExpand->setIcon(QIcon(":/Icons/Expand.png"));
	m_pBtnExpand->setCheckable(true);
	m_pBtnExpand->setToolTip(tr("Auto Expand"));
	m_pBtnExpand->setMaximumHeight(22);
	m_pBtnExpand->setChecked(true);
	m_pBtnExpand->setEnabled(m_pBtnTree->isChecked());
	connect(m_pBtnExpand, &QToolButton::toggled, this, [&](bool checked) {
		if(checked)
			m_pTreeList->expandAll();
		else
			m_pTreeList->collapseAll();
	});
	m_pToolBar->addWidget(m_pBtnExpand);


	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setFixedHeight(22);
	m_pToolBar->addWidget(pBtnSearch);
	connect(m_pFinder, &CFinder::Toggled, this, [&] {
		emit ProgramsChanged(m_CurPrograms);
	});

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

	m_pEditProgram = m_pMenu->addAction(QIcon(":/Icons/EditIni.png"), tr("Edit Program"), this, SLOT(OnProgramAction()));
	
	m_pClear = m_pMenu->addMenu(QIcon(":/Icons/Trash.png"), tr("Clear Trace Logs"));
		(m_pClearExecLog = m_pClear->addAction(QIcon(":/Icons/Process.png"), tr("Execution/Ingress Trace Log"), this, SLOT(OnProgramAction())))->setData((int)ETraceLogs::eExecLog);
		(m_pClearResLog = m_pClear->addAction(QIcon(":/Icons/Ampel.png"), tr("Resource Access Trace Log"), this, SLOT(OnProgramAction())))->setData((int)ETraceLogs::eResLog);
		(m_pClearNetLog = m_pClear->addAction(QIcon(":/Icons/Wall3.png"), tr("Network Access Trace Log"), this, SLOT(OnProgramAction())))->setData((int)ETraceLogs::eNetLog);
		m_pClear->addSeparator();
		(m_pClearExecRec = m_pClear->addAction(QIcon(":/Icons/Process.png"), tr("Execution/Ingress Records"), this, SLOT(OnProgramAction())))->setData((int)ETraceLogs::eExecLog);
		(m_pClearResRec = m_pClear->addAction(QIcon(":/Icons/Ampel.png"), tr("Resource Access Records"), this, SLOT(OnProgramAction())))->setData((int)ETraceLogs::eResLog);
		(m_pClearNetRec = m_pClear->addAction(QIcon(":/Icons/Wall3.png"), tr("Network Access Records"), this, SLOT(OnProgramAction())))->setData((int)ETraceLogs::eNetLog);

	m_pTraceConfig = m_pMenu->addMenu(QIcon(":/Icons/SetLogging.png"), tr("Trace Config"));
		m_pExecTraceConfig = m_pTraceConfig->addMenu(QIcon(":/Icons/Process.png"), tr("Execution Trace"));
			m_pExecTraceConfig->addAction(tr("Use Global Preset"), this, SLOT(OnProgramAction()))->setData((int)ETracePreset::eDefault);
			m_pExecTraceConfig->addAction(tr("Private Trace"), this, SLOT(OnProgramAction()))->setData((int)ETracePreset::ePrivate);
		m_pResTraceConfig = m_pTraceConfig->addMenu(QIcon(":/Icons/Ampel.png"), tr("Resource Trace"));
			m_pResTraceConfig->addAction(tr("Use Global Preset"), this, SLOT(OnProgramAction()))->setData((int)ETracePreset::eDefault);
			m_pResTraceConfig->addAction(tr("Trace"), this, SLOT(OnProgramAction()))->setData((int)ETracePreset::eTrace);
			m_pResTraceConfig->addAction(tr("Private Trace"), this, SLOT(OnProgramAction()))->setData((int)ETracePreset::ePrivate);
			m_pResTraceConfig->addAction(tr("Don't Trace"), this, SLOT(OnProgramAction()))->setData((int)ETracePreset::eNoTrace);
		m_pNetTraceConfig = m_pTraceConfig->addMenu(QIcon(":/Icons/Wall3.png"), tr("Network Trace"));
			m_pNetTraceConfig->addAction(tr("Use Global Preset"), this, SLOT(OnProgramAction()))->setData((int)ETracePreset::eDefault);
			m_pNetTraceConfig->addAction(tr("Trace"), this, SLOT(OnProgramAction()))->setData((int)ETracePreset::eTrace);
			m_pNetTraceConfig->addAction(tr("Private Trace"), this, SLOT(OnProgramAction()))->setData((int)ETracePreset::ePrivate);
			m_pNetTraceConfig->addAction(tr("Don't Trace"), this, SLOT(OnProgramAction()))->setData((int)ETracePreset::eNoTrace);
		m_pTraceConfig->addSeparator();
		m_pStoreTraceConfig = m_pTraceConfig->addMenu(QIcon(":/Icons/Compatibility.png"), tr("Store Trace"));
			m_pStoreTraceConfig->addAction(tr("Use Global Preset"), this, SLOT(OnProgramAction()))->setData((int)ESavePreset::eDefault);
			m_pStoreTraceConfig->addAction(tr("Save to Disk"), this, SLOT(OnProgramAction()))->setData((int)ESavePreset::eSaveToDisk);
			m_pStoreTraceConfig->addAction(tr("Cache only in RAM"), this, SLOT(OnProgramAction()))->setData((int)ESavePreset::eDontSave);
	foreach(QAction* pAction, m_pExecTraceConfig->actions() + m_pResTraceConfig->actions() + m_pNetTraceConfig->actions() + m_pStoreTraceConfig->actions())
		pAction->setCheckable(true);


	m_pAddToGroup = m_pMenu->addMenu(QIcon(":/Icons/MkLink.png"), tr("Groups"));

	m_pRemoveItem = m_pMenu->addAction(QIcon(":/Icons/Remove.png"), tr("Remove"), this, SLOT(OnProgramAction()));
	m_pRemoveItem->setShortcut(QKeySequence::Delete);
	m_pRemoveItem->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pRemoveItem);
	m_pMenu->addSeparator();
	m_pCreateProgram = m_pMenu->addAction(QIcon(":/Icons/Add.png"), tr("Add Program"), this, SLOT(OnAddProgram()));

	AddPanelItemsToMenu();
}

CProgramView::~CProgramView()
{	
	SetColumnSet("");
	theConf->SetBlob("MainWindow/ProgramView_Columns", m_pTreeList->saveState());
}

bool CProgramView::HasFinder() const
{
	return m_pFinder->isVisible();
}

void CProgramView::SetColumnSet(const QString& ColumnSet)
{
	QString CurSet;
	for (int i = 0; i < m_pProgramModel->columnCount(); i++)
		CurSet += m_pTreeList->isColumnHidden(i) ? "H" : "V";
		
	theConf->SetValue("MainWindow/ProgramView_ColumnSet" + m_ColumnSet, CurSet);

	m_ColumnSet = ColumnSet;

	CurSet = theConf->GetString("MainWindow/ProgramView_ColumnSet" + m_ColumnSet, QString());
	if (CurSet.count() != m_pProgramModel->columnCount())
		return;
	
	for (int i = 0; i < m_pProgramModel->columnCount(); i++)
		m_pTreeList->setColumnHidden(i, CurSet[i] == 'H');
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
	}

	QList<QVariant> AddedProgs;
	if(m_pBtnTree->isChecked())
		AddedProgs = m_pProgramModel->Sync(theCore->ProgramManager()->GetRoot());
	else // flat mode, dont show groups
	{
		QHash<quint64, CProgramItemPtr> Items = theCore->ProgramManager()->GetItems();
		for (auto I = Items.begin(); I != Items.end(); )
		{
			if((*I)->GetType() == EProgramType::eProgramGroup)
				I = Items.erase(I);
			else
				I++;
		}
		AddedProgs = m_pProgramModel->Sync(Items);
	}

	if (m_pProgramModel->IsTree())
	{
		QTimer::singleShot(10, this, [this, AddedProgs]() {
			foreach(const QVariant ID, AddedProgs) {

				QModelIndex ModelIndex = m_pProgramModel->FindIndex(ID);
				
				if(m_pBtnExpand->isChecked())
					m_pTreeList->expand(m_pSortProxy->mapFromSource(ModelIndex));
			}
		});
	}
}

void CProgramView::Clear()
{
	m_pProgramModel->Clear();
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

	m_pInfoView->Sync(m_CurPrograms);

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
	QSet<CProgramItemPtr> Programs;
	QSet<quint64> SetGroups;
	foreach(const QModelIndex & index, m_pTreeList->selectionModel()->selectedIndexes())
	{
		if (index.column() != 0)
			continue;
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(index);

		CProgramItemPtr pProgram = m_pProgramModel->GetItem(ModelIndex);
		if (pProgram) 
		{
			Programs.insert(pProgram);

			foreach(auto Group, pProgram->GetGroups())
			{
				CProgramSetPtr pGroup = Group.lock().objectCast<CProgramGroup>();
				if(pGroup)
					SetGroups.insert(pGroup->GetUID());
			}
		}
	}

	m_pEditProgram->setEnabled(Programs.count() == 1);

	QMap<QString, CProgramGroupPtr> Groups; // use map to sort groups alphabetically
	for(auto pGroup: theCore->ProgramManager()->GetGroups())
		Groups[pGroup->GetNameEx()] = pGroup;

	QVector<QString> Names = Groups.keys().toVector();

	for (int index = 0; index < Names.size() || index < m_Groups.size(); ++index) {
		bool bMerge = index < Names.size() && index < m_Groups.size();
		if (index < Names.size() || (bMerge && Names[index] < m_Groups[index]->text())) {
			QString Name = Names[index];
			auto pGroup = Groups[Name];
			QAction* Action = new QAction(pGroup->GetIcon(), Name);
			Action->setCheckable(true);
			connect(Action, SIGNAL(triggered()), this, SLOT(OnAddToGroup()));
			Action->setData(pGroup->GetUID());
			if (index < Names.size())
				m_pAddToGroup->addAction(Action);
			else
				m_pAddToGroup->insertAction(m_Groups[index], Action);
			m_Groups.insert(index, Action);
		}
		else if (index < m_Groups.size() || (bMerge && Names[index] > m_Groups[index]->text())) {
			delete m_Groups[index];
			m_Groups.removeAt(index);
			--index;
		}
	}

	for (int index = 0; index < m_Groups.size(); index++) {
		m_Groups[index]->setChecked(SetGroups.contains(m_Groups[index]->data().toUInt()));
	}

	int ExecTrace = -1;
	int ResTrace = -1;
	int NetTrace = -1;
	int SaveTrace = -1;
	int TraceCount = 0;

	foreach(const CProgramItemPtr& pProgram, Programs)
	{
		CProgramFilePtr pFile = pProgram.objectCast<CProgramFile>();
		CWindowsServicePtr pService = pProgram.objectCast<CWindowsService>();
		if (!pFile && !pService) 
			continue;

		if (ExecTrace == -1)
			ExecTrace = (int)pProgram->GetExecTrace();
		else if (ExecTrace != (int)pProgram->GetExecTrace())
			ExecTrace = -2;
		if (ResTrace == -1)
			ResTrace = (int)pProgram->GetResTrace();
		else if (ResTrace != (int)pProgram->GetResTrace())
			ResTrace = -2;
		if (NetTrace == -1)
			NetTrace = (int)pProgram->GetNetTrace();
		else if (NetTrace != (int)pProgram->GetNetTrace())
			NetTrace = -2;

		if (SaveTrace == -1)
			SaveTrace = (int)pProgram->GetSaveTrace();
		else if (SaveTrace != (int)pProgram->GetSaveTrace())
			SaveTrace = -2;

		TraceCount++;
	}
	
	foreach(QAction * pAction, m_pExecTraceConfig->actions())
		pAction->setChecked(pAction->data().toInt() == ExecTrace);
	foreach(QAction * pAction, m_pResTraceConfig->actions())
		pAction->setChecked(pAction->data().toInt() == ResTrace);
	foreach(QAction * pAction, m_pNetTraceConfig->actions())
		pAction->setChecked(pAction->data().toInt() == NetTrace);
	foreach(QAction * pAction, m_pStoreTraceConfig->actions())
		pAction->setChecked(pAction->data().toInt() == SaveTrace);
	m_pClear->setEnabled(TraceCount > 0);
	m_pTraceConfig->setEnabled(TraceCount > 0);
	
	CPanelView::OnMenu(Point);
}

void CProgramView::OnAddToGroup()
{
	QAction* Action = (QAction*)sender();
	bool bChecked = Action->isChecked();
	quint64 UID = Action->data().toUInt();
	auto pItem = theCore->ProgramManager()->GetProgramByUID(UID);
	auto pGroup = pItem.objectCast<CProgramGroup>();
	if(!pGroup) 
		return;

	QList<STATUS> Results;

	foreach(const QModelIndex& index, m_pTreeList->selectionModel()->selectedIndexes())
	{
		if (index.column() != 0)
			continue;

		QModelIndex ModelIndex = m_pSortProxy->mapToSource(index);

		CProgramItemPtr pProgram = m_pProgramModel->GetItem(ModelIndex);
		if (pProgram == pItem)
			continue;

		if (bChecked)
			Results << theCore->ProgramManager()->AddProgramTo(pProgram, pGroup);
		else
			Results << theCore->ProgramManager()->RemoveProgramFrom(pProgram, pGroup, false, true);
	}

	theGUI->CheckResults(Results, this);
}

void CProgramView::OnAddProgram()
{
	CProgramWnd* pProgramWnd = new CProgramWnd(NULL);
	if (pProgramWnd->exec()) {
		/*QModelIndexList Sel = m_pTreeList->selectionModel()->selectedIndexes();
		if (!Sel.isEmpty()) {
		CProgramItemPtr pParent = m_pProgramModel->GetItem(m_pSortProxy->mapToSource(Sel.first()));
		CProgramItemPtr pProgram = pProgramWnd->GetProgram();
		Results.append(theCore->ProgramManager()->AddProgramTo(pProgram, pParent));
		}*/
	}
}

void CProgramView::OnRefresh()
{
	QList<STATUS> Results = QList<STATUS>() << theCore->RefreshPrograms();
	theGUI->CheckResults(Results, this);
}

void CProgramView::OnProgramAction()
{
	QAction* pAction = qobject_cast<QAction*>(sender());
	
	QSet<CProgramItemPtr> Programs;
	foreach(const QModelIndex & index, m_pTreeList->selectionModel()->selectedIndexes())
	{
		if (index.column() != 0)
			continue;
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(index);

		CProgramItemPtr pProgram = m_pProgramModel->GetItem(ModelIndex);
		if(pProgram)
			Programs.insert(pProgram);
	}

	QList<STATUS> Results;
	
	if (pAction == m_pEditProgram)
	{
		OnDoubleClicked(m_pTreeList->currentIndex());
	}
	else if (pAction == m_pRemoveItem)
	{
		bool bMultiGroup = false;
		bool bHasChildren = false;
		foreach(const CProgramItemPtr& pProgram, Programs)
		{
			if (pProgram->GetGroups().count() > 1)
				bMultiGroup = true;
			CProgramSetPtr pSet = pProgram.objectCast<CProgramSet>();
			if (pSet && !pSet->GetNodes().isEmpty())
				bHasChildren = true;
		}

		bool bDeleteChildren = false;
		bool bDeleteFromAll = false;
		if (bHasChildren)
		{
			int Res = QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete selected Program Items (Yes)?\n"
				"The selected Item has children, do you want to delete them as well (Yes to All)?")
				, QMessageBox::Yes, QMessageBox::YesToAll, QMessageBox::No);
			if (Res == QMessageBox::No)
				return;
			bDeleteChildren = Res == QMessageBox::YesToAll;
		}
		else if (bMultiGroup)
		{
			int Res = QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete selected Program Items?\n"
				"The selected Item belongs to more than one Group, to delete it from all groups press 'Yes to All', pressing 'Yes' will only remove it from the group its selected in.")
				, QMessageBox::Yes, QMessageBox::YesToAll, QMessageBox::No);
			if (Res == QMessageBox::No)
				return;
			bDeleteFromAll = Res == QMessageBox::YesToAll;
		}
		else
		{
			if (QMessageBox::question(this, "MajorPrivacy", tr("Do you want to delete selected Program Items?"), QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
				return;
		}

		bool bDelRules = false;
		bool bCancel = false;

		auto RemoveProgram = [&](const QModelIndex& index)
		{
			QModelIndex ModelIndex = m_pSortProxy->mapToSource(index);

			CProgramItemPtr pProgram = m_pProgramModel->GetItem(ModelIndex);
			CProgramItemPtr pParent = ModelIndex.parent().isValid()
				? m_pProgramModel->GetItem(ModelIndex.parent())
				: theCore->ProgramManager()->GetRoot();
			if (pProgram && pParent) 
			{
				STATUS Status = theCore->ProgramManager()->RemoveProgramFrom(pProgram, bDeleteFromAll ? CProgramItemPtr() : pParent, bDelRules);

				if (Status.IsError() && Status.GetStatus() == STATUS_ERR_PROG_HAS_RULES && !bDelRules)
				{
					int Res = QMessageBox::question(this, "MajorPrivacy", tr("The selected Program Item has rules, do you want to delete them as well?"), QMessageBox::Yes, QMessageBox::No, QMessageBox::Cancel);
					if (Res == QMessageBox::Yes)
					{
						bDelRules = true;
						Status = theCore->ProgramManager()->RemoveProgramFrom(pProgram, bDeleteFromAll ? CProgramItemPtr() : pParent, bDelRules);
					}
					else if (Res == QMessageBox::Cancel)
						bCancel = true;
				}

				return Status;
			}
			return ERR(STATUS_UNSUCCESSFUL);
		};

		std::function<STATUS(const QModelIndex& index)> DeleteRecursivly = [&](const QModelIndex& index)
		{
			int rowCount = m_pSortProxy->rowCount(index);
			for (int i = 0; i < rowCount && !bCancel; ++i) {
				QModelIndex childIndex = m_pSortProxy->index(i, 0, index);
				STATUS Status = DeleteRecursivly(childIndex);
				if(!Status) return Status;
			}
			return RemoveProgram(index);
		};

		foreach(const QModelIndex& index, m_pTreeList->selectionModel()->selectedIndexes())
		{
			if (index.column() != 0 || bCancel)
				continue;

			if(bDeleteChildren)
				Results.append(DeleteRecursivly(index));
			else
				Results.append(RemoveProgram(index));
		}
	}
	else if (pAction == m_pClearExecLog || pAction == m_pClearResLog || pAction == m_pClearNetLog)
	{
		if (QMessageBox::question(this, "MajorPrivacy", tr("Are you sure you want to clear the the %1 for the current program items?").arg(pAction->text()), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		foreach(const CProgramItemPtr& pProgram, Programs)
			Results << theCore->ClearTraceLog(ETraceLogs(pAction->data().toInt()), pProgram);
	} 
	else if (pAction == m_pClearExecRec || pAction == m_pClearResRec || pAction == m_pClearNetRec)
	{
		if (QMessageBox::question(this, "MajorPrivacy", tr("Are you sure you want to clear the the %1 for the current program items?").arg(pAction->text()), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		foreach(const CProgramItemPtr& pProgram, Programs)
			Results << theCore->ClearRecords(ETraceLogs(pAction->data().toInt()), pProgram);
	} 
	else
	{
		QMenu* pMenu = qobject_cast<QMenu*>(pAction->parent());
		
		foreach(const CProgramItemPtr& pProgram, Programs) 
		{
			CProgramFilePtr pFile = pProgram.objectCast<CProgramFile>();
			CWindowsServicePtr pService = pProgram.objectCast<CWindowsService>();
			if (!pFile && !pService) 
				continue;

			if (pMenu == m_pExecTraceConfig)
				pProgram->SetExecTrace(ETracePreset(pAction->data().toInt()));
			else if (pMenu == m_pResTraceConfig)
				pProgram->SetResTrace(ETracePreset(pAction->data().toInt()));
			else if (pMenu == m_pNetTraceConfig)
				pProgram->SetNetTrace(ETracePreset(pAction->data().toInt()));
			else if (pMenu == m_pStoreTraceConfig)
				pProgram->SetSaveTrace(ESavePreset(pAction->data().toInt()));

			Results << theCore->ProgramManager()->SetProgram(pProgram);
		}
	}

	theGUI->CheckResults(Results, this);
}