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
	
	//m_pVSplitter = new QSplitter(Qt::Vertical);
	//m_pMainLayout->addWidget(m_pVSplitter);

	m_pDnsCacheView = new CDnsCacheView();

	m_pMainLayout->addWidget(m_pDnsCacheView);
}

void CDnsPage::Update()
{
	if (!isVisible())
		return;
	
	m_pDnsCacheView->Sync();
}