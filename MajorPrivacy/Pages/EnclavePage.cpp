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


	m_pEnclaveView = new CEnclaveView(this);
	m_pMainLayout->addWidget(m_pEnclaveView);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);
	
	//m_pVSplitter = new QSplitter(Qt::Vertical);
	//m_pMainLayout->addWidget(m_pVSplitter);

}

void CEnclavePage::Update()
{
	if (!isVisible())
		return;
	
	m_pEnclaveView->Sync();
}