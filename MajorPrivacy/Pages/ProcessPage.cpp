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
#include "../Views/HashDBView.h"
#include "../Views/UsageView.h"

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
	m_pRuleTabs->addTab(m_pRuleView,  QIcon(":/Icons/Rules.png"), tr("Program Rules"));

	m_pHashDBView = new CHashDBView();
	m_pRuleTabs->addTab(m_pHashDBView,  QIcon(":/Icons/CertDB.png"), tr("Hash Database"));

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

	/*QAction* pToggleInfo = new QAction(m_pLibrarySplitter);
	pToggleInfo->setShortcut(QKeySequence("Ctrl+I"));
	pToggleInfo->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	addAction(pToggleInfo);
	connect(pToggleInfo, &QAction::triggered, m_pLibrarySplitter, [&]() { m_pLibraryInfoView->setVisible(!m_pLibraryInfoView->isVisible()); m_pLibraryView->m_pBtnInfo->setChecked(m_pLibraryInfoView->isVisible());});*/
	connect(m_pLibraryView->m_pBtnInfo, &QToolButton::toggled, this, [&](bool checked) { 
		m_pLibraryInfoView->setVisible(checked); 
	});

	connect(m_pLibraryView, &CLibraryView::SelectionChanged, m_pLibraryInfoView, &CLibraryInfoView::Sync);

	m_pExecutionView = new CExecutionView();
	m_pTabs->addTab(m_pExecutionView, QIcon(":/Icons/Run.png"), tr("Execution Monitor"));

	m_pIngressView = new CIngressView();
	m_pTabs->addTab(m_pIngressView, QIcon(":/Icons/finder.png"), tr("Ingress Monitor"));

	m_pTraceView = new CProcessTraceView();
	m_pTabs->addTab(m_pTraceView, QIcon(":/Icons/SetLogging.png"), tr("Trace Log"));

	if (!bEmbedded) {
		m_pCpuView = new CUsageView(EUsageMetric::eCpu);
		m_pTabs->addTab(m_pCpuView, QIcon(":/Icons/CPU.png"), tr("CPU Usage"));
	}

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
		m_pTabs->insertTab(0, m_pRuleView, QIcon(":/Icons/Rules.png"), tr("Program Rules"));
		m_pTabs->insertTab(1, m_pHashDBView, QIcon(":/Icons/CertDB.png"), tr("Hash Database"));
		delete m_pVSplitter;
		m_pVSplitter = nullptr;
	}
	else
	{
		m_pVSplitter = new QSplitter(Qt::Vertical);
		m_pRuleTabs = new QTabWidget();
		m_pRuleTabs->addTab(m_pRuleView,  QIcon(":/Icons/Rules.png"), tr("Program Rules"));
		m_pRuleTabs->addTab(m_pHashDBView, QIcon(":/Icons/CertDB.png"), tr("Hash Database"));
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
	m_pTraceView->setEnabled(theCore->GetConfigBool("Service/ExecLog", false));

	auto Current = theGUI->GetCurrentItems();

	//
	// Explicitly selected items are always shown. The implicit members of a group or of
	// "All Programs" with a private execution trace preset only when the trace view's own
	// "show private" button, or the global one, is on.
	//
	bool bShowPrivate = theGUI->GetShowPrivate();
	auto Programs = [&](bool bPrivate) {
		QSet<CProgramFilePtr> Set = Current.ProgramsEx;
		for (auto& pProgram : Current.ProgramsIm) {
			if (bShowPrivate || bPrivate || pProgram->GetExecTrace() != ETracePreset::ePrivate)
				Set.insert(pProgram);
		}
		return Set;
	};

	if (m_pRuleView->isVisible())
	{
		if (Current.bAllProgramsOnly) {
			//
			// The all programs item stands for the rules that apply to every program,
			// not for the union of what the individual programs have.
			//
			QSet<QFlexGuid> RuleIDs;
			CProgramSetPtr pAll = theCore->ProgramManager()->GetAll();
			if (pAll) RuleIDs = pAll->GetProgRules();
			m_pRuleView->Sync(theCore->ProgramManager()->GetProgramRules(RuleIDs));
		}
		else if(Current.bAllPrograms)
			m_pRuleView->Sync(theCore->ProgramManager()->GetProgramRules());
		else {
			QSet<QFlexGuid> RuleIDs;
			foreach(CProgramItemPtr pItem, Current.Items)
				RuleIDs.unite(pItem->GetProgRules());
			m_pRuleView->Sync(theCore->ProgramManager()->GetProgramRules(RuleIDs));
		}
	}

	if (m_pHashDBView->isVisible())
	{
		m_pHashDBView->Sync(Current.ProgramsEx | Current.ProgramsIm, Current.bAllPrograms);
	}

	if (m_pProcessView->isVisible())
	{
		m_pProcessView->Sync(theGUI->GetCurrentProcesses());
	}

	if (m_pLibraryView->isVisible())
	{
		m_pLibraryView->Sync(Current.ProgramsEx | Current.ProgramsIm);
	}

	if (m_pExecutionView->isVisible())
	{
		m_pExecutionView->Sync(Current.ProgramsEx | Current.ProgramsIm, Current.ServicesEx | Current.ServicesIm);
	}

	if (m_pIngressView->isVisible())
	{
		m_pIngressView->Sync(Current.ProgramsEx | Current.ProgramsIm, Current.ServicesEx | Current.ServicesIm);
	}

	if (m_pTraceView->isVisible())
	{
		m_pTraceView->Sync(ETraceLogs::eExecLog, Programs(m_pTraceView->ShowPrivate()), Current.ServicesEx);
	}

	if (m_pCpuView && m_pCpuView->isVisible())
	{
		m_pCpuView->setEnabled(theCore->GetConfigBool("Service/UsageStats", true));
		m_pCpuView->Sync(Current.ProgramsEx | Current.ProgramsIm, Current.ServicesEx | Current.ServicesIm);
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

	if (m_pHashDBView->isVisible())
	{
		m_pHashDBView->Sync(theCore->ProgramManager()->GetPrograms(), true, EnclaveGuid);
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

void CProcessPage::Clear()
{
	if (m_pCpuView) m_pCpuView->Clear();

	m_pRuleView->Clear();

	m_pHashDBView->Clear();

	if(m_pProcessView)
		m_pProcessView->Clear();

	m_pLibraryView->Clear();

	m_pLibraryInfoView->Clear();

	m_pExecutionView->Clear();

	m_pIngressView->Clear();

	m_pTraceView->Clear();
}