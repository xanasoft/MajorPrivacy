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
#include "../Views/ProcessTraceView.h"
#include "../Views/IngressView.h"
#include "../Views/ExecutionView.h"

CProcessPage::CProcessPage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);

	m_pRuleView = new CProgramRuleView();


	m_pTabs = new QTabWidget();

	m_pProcessView = new CProcessView();
	m_pTabs->addTab(m_pProcessView, tr("Running Processes"));

	m_pLibraryView = new CLibraryView();
	m_pTabs->addTab(m_pLibraryView, tr("Loaded Modules"));

	m_pExecutionView = new CExecutionView();
	m_pTabs->addTab(m_pExecutionView, tr("Execution Monitor"));

	m_pIngressView = new CIngressView();
	m_pTabs->addTab(m_pIngressView, tr("Ingress Monitor"));

	m_pTraceView = new CProcessTraceView();
	m_pTabs->addTab(m_pTraceView, tr("Trace Log"));

	m_pVSplitter = new QSplitter(Qt::Vertical);
	m_pVSplitter->addWidget(m_pRuleView);
	m_pVSplitter->addWidget(m_pTabs);
	m_pMainLayout->addWidget(m_pVSplitter);
}

CProcessPage::~CProcessPage()
{
	theConf->SetValue("MainWindow/ProcessTab", m_pTabs->currentIndex());
}

void CProcessPage::LoadState()
{
	m_pTabs->setCurrentIndex(theConf->GetInt("MainWindow/ProcessTab", 0));
}

void CProcessPage::SetMergePanels(bool bMerge)
{
	if (!m_pVSplitter == bMerge)
		return;

	if (bMerge)
	{
		m_pMainLayout->addWidget(m_pTabs);
		m_pTabs->insertTab(0, m_pRuleView, tr("Process Rules"));
		delete m_pVSplitter;
		m_pVSplitter = nullptr;
	}
	else
	{
		m_pVSplitter = new QSplitter(Qt::Vertical);
		m_pVSplitter->addWidget(m_pRuleView);
		m_pRuleView->setVisible(true);
		m_pVSplitter->addWidget(m_pTabs);
		m_pMainLayout->addWidget(m_pVSplitter);
	}

	LoadState();
}

void CProcessPage::Update()
{
	if (!isVisible())
		return;

	auto Current = theGUI->GetCurrentItems();

	if (m_pRuleView->isVisible())
	{
		if(Current.bAllPrograms)
			m_pRuleView->Sync(theCore->ProgramManager()->GetProgramRules());
		else {
			QSet<QString> RuleIDs;
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
		m_pLibraryView->Sync(Current.Programs, Current.bAllPrograms);
	}

	if (m_pExecutionView->isVisible())
	{
		m_pExecutionView->Sync(Current.Programs, Current.ServicesEx | Current.ServicesIm, Current.bAllPrograms);
	}

	if (m_pIngressView->isVisible())
	{
		m_pIngressView->Sync(Current.Programs, Current.ServicesEx | Current.ServicesIm, Current.bAllPrograms);
	}

	if (m_pTraceView->isVisible())
	{
		m_pTraceView->Sync(ETraceLogs::eExecLog, Current.Programs, Current.ServicesEx);
	}
}