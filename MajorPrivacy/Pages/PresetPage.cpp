#include "pch.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Presets/PresetManager.h"
#include "PresetPage.h"
#include "../Views/PresetView.h"
#include "../Views/PresetProperties.h"

CPresetPage::CPresetPage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pHSplitter = new QSplitter(Qt::Horizontal);
	m_pMainLayout->addWidget(m_pHSplitter);

	m_pPresetView = new CPresetView();
	m_pHSplitter->addWidget(m_pPresetView);

	m_pPresetProperties = new CPresetProperties();
	m_pHSplitter->addWidget(m_pPresetProperties);

	connect(m_pPresetView, SIGNAL(CurrentChanged(const QString&)), this, SLOT(OnCurrentChanged(const QString&)));
	connect(m_pPresetProperties, SIGNAL(Modified(bool)), this, SLOT(OnPresetModified(bool)));

	m_pHSplitter->restoreState(theConf->GetBlob("MainWindow/PresetPage_Splitter"));
}

CPresetPage::~CPresetPage()
{
	theConf->SetBlob("MainWindow/PresetPage_Splitter", m_pHSplitter->saveState());
}

void CPresetPage::Update()
{
	if (!isVisible())
		return;

	m_pPresetView->Update();

	m_pPresetProperties->Update();
}

void CPresetPage::OnCurrentChanged(const QString& presetGuid)
{
	m_CurrentPresetGuid = presetGuid;

	CPresetPtr pPreset = theCore->PresetManager()->GetPreset(presetGuid);
	m_pPresetProperties->SetPreset(pPreset);
}

void CPresetPage::OnPresetModified(bool bModified)
{
	//if (!m_CurrentPresetGuid.isEmpty()) {
	//	m_pPresetView->MarkPresetModified(m_CurrentPresetGuid, bModified);
	//}
}

void CPresetPage::Clear()
{
	m_pPresetProperties->SetPreset(CPresetPtr());
	m_CurrentPresetGuid.clear();
}
