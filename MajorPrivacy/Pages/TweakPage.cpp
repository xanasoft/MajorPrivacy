#include "pch.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Processes/ProcessList.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "TweakPage.h"
#include "../Views/TweakView.h"
#include "../Core/Tweaks/TweakManager.h"

CTweakPage::CTweakPage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);
	
	m_pVSplitter = new QSplitter(Qt::Vertical);
	m_pMainLayout->addWidget(m_pVSplitter);

	m_pTweakView = new CTweakView();
	m_pVSplitter->addWidget(m_pTweakView);
}

void CTweakPage::Update()
{
	if (!isVisible())
		return;
	
	m_pTweakView->Sync(theCore->TweakManager()->GetRoot());
}