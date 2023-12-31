#include "pch.h"
#include "../Core/PrivacyCore.h"
#include "../Core/ProcessList.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "RegistryPage.h"

CRegistryPage::CRegistryPage(QWidget* parent)
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
	m_pActivityView = new QTreeViewEx();
	m_pTabs->addTab(m_pActivityView, tr("Open Keys"));
	m_pAccessView = new QTreeViewEx();
	m_pTabs->addTab(m_pAccessView, tr("Access Monitor"));
	m_pTraceView = new QTreeViewEx();
	m_pTabs->addTab(m_pTraceView, tr("Access Log"));

}

void CRegistryPage::Update()
{
	if (!isVisible())
		return;
	
}