#include "pch.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Processes/ProcessList.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "TweakPage.h"
#include "../Views/TweakView.h"
#include "../Core/Tweaks/TweakManager.h"
#include "../Views/TweakInfo.h"

CTweakPage::CTweakPage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	//m_pToolBar = new QToolBar();
	//m_pMainLayout->addWidget(m_pToolBar);
	
	m_pHSplitter = new QSplitter(Qt::Horizontal);
	m_pMainLayout->addWidget(m_pHSplitter);

	m_pTweakView = new CTweakView();
	m_pHSplitter->addWidget(m_pTweakView);

	m_pTweakInfo = new CTweakInfo();
	m_pHSplitter->addWidget(m_pTweakInfo);

	connect(m_pTweakView, SIGNAL(CurrentChanged()), this, SLOT(OnCurrentChanged()));
}

void CTweakPage::Update()
{
	if (!isVisible())
		return;
	
	m_pTweakView->Sync(theCore->TweakManager()->GetRoot());
}

void CTweakPage::OnCurrentChanged()
{
	CTweakPtr pTweak = m_pTweakView->GetCurrentItem();
	if (!pTweak) return;
	
	m_pTweakInfo->ShowTweak(pTweak);
}