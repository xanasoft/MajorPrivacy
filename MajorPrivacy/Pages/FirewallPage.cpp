#include "pch.h"
#include "../Core/PrivacyCore.h"
#include "../Core/ProcessList.h"
#include "../Core/Programs/ProgramManager.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "../../MiscHelpers/Common/Common.h"
#include "FirewallPage.h"
#include "../../Library/API/ServiceAPI.h"
#include "MajorPrivacy.h"
#include "../Core/Network/NetworkManager.h"
#include "../Views/FwRuleView.h"
#include "../Views/SocketView.h"
#include "../Views/NetTraceView.h"
#include "../Views/TrafficView.h"

CFirewallPage::CFirewallPage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);
	
	m_pVSplitter = new QSplitter(Qt::Vertical);
	m_pMainLayout->addWidget(m_pVSplitter);

	m_pRuleView = new CFwRuleView();
	m_pVSplitter->addWidget(m_pRuleView);

	m_pTabs = new QTabWidget();
	m_pVSplitter->addWidget(m_pTabs);

	m_pSocketView = new CSocketView();
	m_pTabs->addTab(m_pSocketView, tr("Open Socket"));

	m_pTraceView = new CNetTraceView();
	m_pTabs->addTab(m_pTraceView, tr("Connection Log"));

	m_pTrafficView = new CTrafficView();
	m_pTabs->addTab(m_pTrafficView, tr("Traffic Guard"));
}

CFirewallPage::~CFirewallPage()
{
}

void CFirewallPage::Update()
{
	if (!isVisible())
		return;

	auto Current = theGUI->GetCurrentItems();

	if (m_pRuleView->isVisible())
	{
		QSet<QString> RuleIDs;
		foreach(CProgramItemPtr pItem, Current.Items)
			RuleIDs.unite(pItem->GetFwRules());
		m_pRuleView->Sync(theCore->Network()->GetFwRules(RuleIDs));
	}

	if (m_pSocketView->isVisible())
	{
		m_pSocketView->Sync(Current.Processes, Current.ServicesEx);
	}

	if (m_pTraceView->isVisible())
	{
		MergeTraceLogs(&m_Log, ETraceLogs::eNetLog, Current.Programs, Current.ServicesEx);
		m_pTraceView->Sync(&m_Log);
	}

	if (m_pTrafficView->isVisible())
	{
		m_pTrafficView->Sync(Current.Programs, Current.ServicesEx | Current.ServicesIm);
	}
}
