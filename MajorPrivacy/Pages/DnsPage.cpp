#include "pch.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Processes/ProcessList.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "DnsPage.h"

CDnsPage::CDnsPage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);

	m_pRuleTabs = new QTabWidget();

	m_pRuleView = new CDnsRuleView();
	m_pRuleTabs->addTab(m_pRuleView,  QIcon(":/Icons/Rules.png"), tr("Dns Rules"));

	//m_pProxyView = new QWidget();
	//m_pRuleTabs->addTab(m_pProxyView, tr("Proxy Injection"));


	m_pTabs = new QTabWidget();

	m_pDnsCacheView = new CDnsCacheView();
	m_pTabs->addTab(m_pDnsCacheView, QIcon(":/Icons/Network3.png"), tr("DnsCache"));

	m_pVSplitter = new QSplitter(Qt::Vertical);
	m_pMainLayout->addWidget(m_pVSplitter);
	m_pVSplitter->addWidget(m_pRuleTabs);
	m_pVSplitter->setCollapsible(0, false);
	m_pVSplitter->addWidget(m_pTabs);
	m_pVSplitter->setCollapsible(1, false);

}

CDnsPage::~CDnsPage()
{
	theConf->SetValue("MainWindow/DnsTab", m_pTabs->currentIndex());
}

void CDnsPage::LoadState()
{
	m_pTabs->setCurrentIndex(theConf->GetInt("MainWindow/DnsTab", 0));
}

void CDnsPage::SetMergePanels(bool bMerge)
{
	if (!m_pVSplitter == bMerge)
		return;

	if (bMerge)
	{
		m_pMainLayout->addWidget(m_pTabs);
		m_pTabs->insertTab(0, m_pRuleView, QIcon(":/Icons/Rules.png"), tr("Dns Rules"));
		delete m_pVSplitter;
		m_pVSplitter = nullptr;
	}
	else
	{
		m_pVSplitter = new QSplitter(Qt::Vertical);
		m_pRuleTabs = new QTabWidget();
		m_pRuleTabs->addTab(m_pRuleView,  QIcon(":/Icons/Rules.png"), tr("Dns Rules"));
		m_pVSplitter->addWidget(m_pRuleTabs);
		m_pRuleView->setVisible(true);
		m_pVSplitter->setCollapsible(0, false);
		m_pVSplitter->addWidget(m_pTabs);
		m_pVSplitter->setCollapsible(1, false);
		m_pMainLayout->addWidget(m_pVSplitter);
	}

	LoadState();
}

void CDnsPage::Update()
{
	if (!isVisible())
		return;
	
	m_pDnsCacheView->Sync();

	m_pRuleView->Sync(theCore->NetworkManager()->GetDnsRules());
}

void CDnsPage::Clear()
{
	m_pDnsCacheView->Clear();

	m_pRuleView->Clear();
}