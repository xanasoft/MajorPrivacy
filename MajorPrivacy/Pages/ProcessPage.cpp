#include "pch.h"
#include "MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramManager.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "ProcessPage.h"
#include "../Views/ProcessView.h"
#include "../Views/ProgramRuleView.h"
#include "../Views/LibraryView.h"
#include "../Views/LibraryInfoView.h"
#include "../Views/ProcessTraceView.h"
#include "../Views/IngressView.h"
#include "../Views/ExecutionView.h"

CProcessPage::CProcessPage(bool bEmbedded, QWidget* parent)
	: QWidget(parent)
{
	m_bEmbedded = bEmbedded;

	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);

	m_pRuleTabs = new QTabWidget();

	m_pRuleView = new CProgramRuleView();
	m_pRuleTabs->addTab(m_pRuleView,  QIcon(":/Icons/Rules.png"), tr("Process Rules"));


	m_pTabs = new QTabWidget();

	if (!bEmbedded) {
		m_pProcessView = new CProcessView();
		m_pTabs->addTab(m_pProcessView, QIcon(":/Icons/Process.png"), tr("Running Processes"));
	}

	m_pLibrarySplitter = new QSplitter(Qt::Vertical);
	m_pTabs->addTab(m_pLibrarySplitter, QIcon(":/Icons/Dll.png"), tr("Loaded Modules"));
	
	m_pLibraryView = new CLibraryView();
	m_pLibrarySplitter->addWidget(m_pLibraryView);
	m_pLibrarySplitter->setCollapsible(0, false);

	m_pLibraryInfoView = new CLibraryInfoView();
	m_pLibrarySplitter->addWidget(m_pLibraryInfoView);
	m_pLibrarySplitter->setStretchFactor(0, 1);
	m_pLibrarySplitter->setStretchFactor(1, 0);
	m_pLibraryInfoView->setVisible(false);

	QAction* pToggleInfo = new QAction(m_pLibrarySplitter);
	pToggleInfo->setShortcut(QKeySequence("Ctrl+I"));
	pToggleInfo->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	addAction(pToggleInfo);
	connect(pToggleInfo, &QAction::triggered, m_pLibrarySplitter, [&]() { m_pLibraryInfoView->setVisible(!m_pLibraryInfoView->isVisible()); });

	connect(m_pLibraryView, &CLibraryView::SelectionChanged, m_pLibraryInfoView, &CLibraryInfoView::Sync);

	m_pExecutionView = new CExecutionView();
	m_pTabs->addTab(m_pExecutionView, QIcon(":/Icons/Run.png"), tr("Execution Monitor"));

	m_pIngressView = new CIngressView();
	m_pTabs->addTab(m_pIngressView, QIcon(":/Icons/finder.png"), tr("Ingress Monitor"));

	m_pTraceView = new CProcessTraceView();
	m_pTabs->addTab(m_pTraceView, QIcon(":/Icons/SetLogging.png"), tr("Trace Log"));

	m_pVSplitter = new QSplitter(Qt::Vertical);
	m_pMainLayout->addWidget(m_pVSplitter);
	m_pVSplitter->addWidget(m_pRuleTabs);
	m_pVSplitter->setCollapsible(0, false);
	m_pVSplitter->addWidget(m_pTabs);
	m_pVSplitter->setCollapsible(1, false);
}

CProcessPage::~CProcessPage()
{
	theConf->SetValue("MainWindow/" + QString(m_bEmbedded ? "ProcessPanel" : "ProcessTab"), m_pTabs->currentIndex());
}

void CProcessPage::LoadState()
{
	m_pTabs->setCurrentIndex(theConf->GetInt(QString("MainWindow/" + QString(m_bEmbedded ? "ProcessPanel" : "ProcessTab")).toStdString().c_str(), 0));
}

void CProcessPage::SetMergePanels(bool bMerge)
{
	if (!m_pVSplitter == bMerge)
		return;

	if (bMerge)
	{
		m_pMainLayout->addWidget(m_pTabs);
		m_pTabs->insertTab(0, m_pRuleView, QIcon(":/Icons/Rules.png"), tr("Process Rules"));
		delete m_pVSplitter;
		m_pVSplitter = nullptr;
	}
	else
	{
		m_pVSplitter = new QSplitter(Qt::Vertical);
		m_pRuleTabs = new QTabWidget();
		m_pRuleTabs->addTab(m_pRuleView,  QIcon(":/Icons/Rules.png"), tr("Process Rules"));
		m_pVSplitter->addWidget(m_pRuleTabs);
		m_pRuleView->setVisible(true);
		m_pVSplitter->setCollapsible(0, false);
		m_pVSplitter->addWidget(m_pTabs);
		m_pVSplitter->setCollapsible(1, false);
		m_pMainLayout->addWidget(m_pVSplitter);
	}

	LoadState();
}

void CProcessPage::Update()
{
	if (!isVisible() || m_bEmbedded)
		return;
	m_pTraceView->setEnabled(theCore->GetConfigBool("Service/ExecLog", true));

	auto Current = theGUI->GetCurrentItems();

	if (m_pRuleView->isVisible())
	{
		if(Current.bAllPrograms)
			m_pRuleView->Sync(theCore->ProgramManager()->GetProgramRules());
		else {
			QSet<QFlexGuid> RuleIDs;
			foreach(CProgramItemPtr pItem, Current.Items)
				RuleIDs.unite(pItem->GetProgRules());
			m_pRuleView->Sync(theCore->ProgramManager()->GetProgramRules(RuleIDs));
		}
	}

	if (m_pProcessView->isVisible())
	{
		m_pProcessView->Sync(theGUI->GetCurrentProcesses());
	}

	if (m_pLibraryView->isVisible())
	{
		m_pLibraryView->Sync(Current.Programs);
	}

	if (m_pExecutionView->isVisible())
	{
		m_pExecutionView->Sync(Current.Programs, Current.ServicesEx | Current.ServicesIm);
	}

	if (m_pIngressView->isVisible())
	{
		m_pIngressView->Sync(Current.Programs, Current.ServicesEx | Current.ServicesIm);
	}

	if (m_pTraceView->isVisible())
	{
		m_pTraceView->Sync(ETraceLogs::eExecLog, Current.Programs, Current.ServicesEx);
	}
}

void CProcessPage::Update(const QFlexGuid& EnclaveGuid)
{
	if (!isVisible() || !m_bEmbedded)
		return;

	if (m_pRuleView->isVisible())
	{
		m_pRuleView->Sync(theCore->ProgramManager()->GetProgramRules(), EnclaveGuid);
	}

	if (m_pLibraryView->isVisible())
	{
		m_pLibraryView->Sync(theCore->ProgramManager()->GetPrograms(), EnclaveGuid);
	}

	if (m_pExecutionView->isVisible())
	{
		m_pExecutionView->Sync(theCore->ProgramManager()->GetPrograms(), theCore->ProgramManager()->GetServices(), EnclaveGuid);
	}

	if (m_pIngressView->isVisible())
	{
		m_pIngressView->Sync(theCore->ProgramManager()->GetPrograms(), theCore->ProgramManager()->GetServices(), EnclaveGuid);
	}

	if (m_pTraceView->isVisible())
	{
		m_pTraceView->Sync(ETraceLogs::eExecLog, theCore->ProgramManager()->GetPrograms(), theCore->ProgramManager()->GetServices(), EnclaveGuid);
	}
}