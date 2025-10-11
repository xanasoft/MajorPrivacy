#include "pch.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Processes/ProcessList.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "VolumePage.h"
#include "../Views/VolumeView.h"

CVolumePage::CVolumePage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);
	
	m_pHSplitter = new QSplitter(Qt::Horizontal);
	m_pMainLayout->addWidget(m_pHSplitter);

	m_pVolumeView = new CVolumeView(this);
	m_pHSplitter->addWidget(m_pVolumeView);

	m_pAccessPage = new CAccessPage(true, this);
	m_pAccessPage->LoadState();
	m_pHSplitter->addWidget(m_pAccessPage);

	m_pHSplitter->restoreState(theConf->GetBlob("MainWindow/VolumeSplitter"));
}

CVolumePage::~CVolumePage()
{
	theConf->SetBlob("MainWindow/VolumeSplitter", m_pHSplitter->saveState());
}

void CVolumePage::LoadState()
{
	m_pAccessPage->LoadState();
}

void CVolumePage::SetMergePanels(bool bMerge)
{
	m_pAccessPage->SetMergePanels(bMerge);
}

void CVolumePage::Update()
{
	if (!isVisible())
		return;
	
	m_pVolumeView->Sync();

	m_pAccessPage->Update(m_pVolumeView->GetSelectedVolumePath(), m_pVolumeView->GetSelectedVolumeImage());
}

void CVolumePage::Clear()
{
	m_pVolumeView->Clear();

	m_pAccessPage->Clear();
}