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

	m_pVolumeView = new CVolumeView(this);
	m_pMainLayout->addWidget(m_pVolumeView);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);
	
	//m_pVSplitter = new QSplitter(Qt::Vertical);
	//m_pMainLayout->addWidget(m_pVSplitter);
}

void CVolumePage::Update()
{
	if (!isVisible())
		return;
	
	m_pVolumeView->Sync();
}