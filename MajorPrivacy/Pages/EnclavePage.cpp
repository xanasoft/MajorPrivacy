#include "pch.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Processes/ProcessList.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "EnclavePage.h"
#include "../Views/EnclaveView.h"

CEnclavePage::CEnclavePage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);

	m_pHSplitter = new QSplitter(Qt::Horizontal);
	m_pMainLayout->addWidget(m_pHSplitter);

	m_pEnclaveView = new CEnclaveView(this);
	m_pHSplitter->addWidget(m_pEnclaveView);

	m_pProcessPage = new CProcessPage(true, this);
	m_pProcessPage->LoadState();
	m_pHSplitter->addWidget(m_pProcessPage);

	m_pHSplitter->restoreState(theConf->GetBlob("MainWindow/EnclaveSplitter"));
}

CEnclavePage::~CEnclavePage()
{
	theConf->SetBlob("MainWindow/EnclaveSplitter", m_pHSplitter->saveState());
}

void CEnclavePage::Update()
{
	if (!isVisible())
		return;
	
	m_pEnclaveView->Sync();

	m_pProcessPage->Update(m_pEnclaveView->GetSelectedEnclave());
}