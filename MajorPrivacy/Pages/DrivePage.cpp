#include "pch.h"
#include "../Core/PrivacyCore.h"
#include "../Core/ProcessList.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "DrivePage.h"

CDrivePage::CDrivePage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);
	
	//m_pVSplitter = new QSplitter(Qt::Vertical);
	//m_pMainLayout->addWidget(m_pVSplitter);

	m_pMainLayout->addWidget(new QLabel("Protected encrepted drives"));
}

void CDrivePage::Update()
{
	if (!isVisible())
		return;
	
}