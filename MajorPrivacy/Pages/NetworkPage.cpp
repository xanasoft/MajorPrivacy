#include "pch.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramManager.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "../../MiscHelpers/Common/Common.h"
#include "NetworkPage.h"
#include "../../Library/API/ServiceAPI.h"
#include "MajorPrivacy.h"
#include "../Core/Network/NetworkManager.h"
#include "../Views/FwRuleView.h"
#include "../Views/SocketView.h"
#include "../Views/NetTraceView.h"
#include "../Views/TrafficView.h"

CNetworkPage::CNetworkPage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);
	
	m_pRuleTabs = new QTabWidget();
	
	m_pRuleView = new CFwRuleView();
	m_pRuleTabs->addTab(m_pRuleView,  QIcon(":/Icons/Rules.png"), tr("Firewall Rules"));
	
	//m_pProxyView = new QWidget();
	//m_pRuleTabs->addTab(m_pProxyView, tr("Proxy Injection"));


	m_pTabs = new QTabWidget();

	m_pSocketView = new CSocketView();
	m_pTabs->addTab(m_pSocketView, QIcon(":/Icons/Port.png"), tr("Open Socket"));

	m_pTrafficView = new CTrafficView();
	m_pTabs->addTab(m_pTrafficView, QIcon(":/Icons/Internet.png"), tr("Traffic Monitor"));

	m_pTraceView = new CNetTraceView();
	m_pTabs->addTab(m_pTraceView, QIcon(":/Icons/SetLogging.png"), tr("Connection Log"));


	m_pVSplitter = new QSplitter(Qt::Vertical);
	m_pMainLayout->addWidget(m_pVSplitter);
	m_pVSplitter->addWidget(m_pRuleTabs);
	m_pVSplitter->setCollapsible(0, false);
	m_pVSplitter->addWidget(m_pTabs);
	m_pVSplitter->setCollapsible(1, false);
}

CNetworkPage::~CNetworkPage()
{
	theConf->SetValue("MainWindow/NetworkTab", m_pTabs->currentIndex());
}

void CNetworkPage::LoadState()
{
	m_pTabs->setCurrentIndex(theConf->GetInt("MainWindow/NetworkTab", 0));
}

void CNetworkPage::SetMergePanels(bool bMerge)
{
	if (!m_pVSplitter == bMerge)
		return;

	if (bMerge)
	{
		m_pMainLayout->addWidget(m_pTabs);
		m_pTabs->insertTab(0, m_pRuleView, QIcon(":/Icons/Rules.png"), tr("Firewall Rules"));
		delete m_pVSplitter;
		m_pVSplitter = nullptr;
	}
	else
	{
		m_pVSplitter = new QSplitter(Qt::Vertical);
		m_pRuleTabs = new QTabWidget();
		m_pRuleTabs->addTab(m_pRuleView,  QIcon(":/Icons/Rules.png"), tr("Firewall Rules"));
		m_pVSplitter->addWidget(m_pRuleTabs);
		m_pRuleView->setVisible(true);
		m_pVSplitter->setCollapsible(0, false);
		m_pVSplitter->addWidget(m_pTabs);
		m_pVSplitter->setCollapsible(1, false);
		m_pMainLayout->addWidget(m_pVSplitter);
	}

	LoadState();
}

void CNetworkPage::Update()
{
	if (!isVisible())
		return;
	m_pTraceView->setEnabled(theCore->GetConfigBool("Service/NetLog", false));

	auto Current = theGUI->GetCurrentItems();

	QSet<CProgramFilePtr> Programs = Current.ProgramsEx;
	if (m_pTrafficView->isVisible() ||m_pTraceView->isVisible()) {
		for (auto& pProgram : Current.ProgramsIm) {
			if(pProgram->GetNetTrace() != ETracePreset::ePrivate)
				Programs.insert(pProgram);
		}
	}
	QSet<CWindowsServicePtr> Services = Current.ServicesEx;
	if (m_pTrafficView->isVisible()) {
		for (auto& pService : Current.ServicesIm) {
			if (pService->GetNetTrace() != ETracePreset::ePrivate)
				Services.insert(pService);
		}
	}

	if (m_pRuleView->isVisible())
	{
		if(Current.bAllPrograms)
			m_pRuleView->Sync(theCore->NetworkManager()->GetFwRules().values());
		else {
			QSet<QFlexGuid> RuleIDs;
			foreach(CProgramItemPtr pItem, Current.Items)
				RuleIDs.unite(pItem->GetFwRules());
			m_pRuleView->Sync(theCore->NetworkManager()->GetFwRules(RuleIDs));
		}
	}

	if (m_pSocketView->isVisible())
	{
		m_pSocketView->Sync(theGUI->GetCurrentProcesses(), Current.ServicesEx);
	}

	if (m_pTrafficView->isVisible())
	{
		m_pTrafficView->Sync(Programs, Services);
	}

	if (m_pTraceView->isVisible())
	{
		m_pTraceView->Sync(ETraceLogs::eNetLog, Programs, Current.ServicesEx);
	}
}

void CNetworkPage::Clear()
{
	m_pSocketView->Clear();

	m_pTrafficView->Clear();

	m_pTraceView->Clear();
}