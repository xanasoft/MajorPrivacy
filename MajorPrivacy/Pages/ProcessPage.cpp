#include "pch.h"
#include "MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Core/ProcessList.h"
#include "../Core/Programs/ProgramManager.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "ProcessPage.h"
#include "../Views/ProcessView.h"

CProcessPage::CProcessPage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);

	m_pVSplitter = new QSplitter(Qt::Vertical);
	m_pMainLayout->addWidget(m_pVSplitter);
	m_pRuleView = new QTreeViewEx();
	m_pVSplitter->addWidget(m_pRuleView);
	m_pTabs = new QTabWidget();
	m_pVSplitter->addWidget(m_pTabs);
	m_pProcessView = new CProcessView();
	m_pTabs->addTab(m_pProcessView, tr("Running Processes"));
	m_pTraceView = new QTreeViewEx();
	m_pTabs->addTab(m_pTraceView, tr("Trace Log"));
}

CProcessPage::~CProcessPage()
{
}

void CProcessPage::Update()
{
	if (!isVisible())
		return;

	if (!isVisible())
		return;

	auto Current = theGUI->GetCurrentItems();

	m_pProcessView->Sync(Current.Processes);

}