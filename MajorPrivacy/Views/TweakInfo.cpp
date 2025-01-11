#include "pch.h"
#include "TweakInfo.h"
#include "../Core/PrivacyCore.h"

CTweakInfo::CTweakInfo(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QGridLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pToolBar = new QToolBar();
	m_pMainLayout->addWidget(m_pToolBar, 0, 0);
	m_pActivate = m_pToolBar->addAction(QIcon(":/Icons/Enable.png"), tr("Apply Tweak"), this, SLOT(OnActivate()));
	m_pDeactivate = m_pToolBar->addAction(QIcon(":/Icons/Disable.png"), tr("Undo Tweak"), this, SLOT(OnDeactivate()));

	m_pGroup = new QGroupBox(tr("Tweak Info"));
	m_pMainLayout->addWidget(m_pGroup, 1, 0);

	QGridLayout* pGroupLayout = new QGridLayout(m_pGroup);
	m_pGroup->setLayout(pGroupLayout);

	int row = 0;

	pGroupLayout->addWidget(new QLabel(tr("Name")), row, 0);
	m_pName = new QLineEdit();
	m_pName->setReadOnly(true);
	pGroupLayout->addWidget(m_pName, row++, 1);

	pGroupLayout->addWidget(new QLabel(tr("Description")), row, 0);
	m_pDescription = new QPlainTextEdit();
	m_pDescription->setReadOnly(true);
	pGroupLayout->addWidget(m_pDescription, row, 1, 2, 1);
	row+=2;

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	pGroupLayout->addWidget(pSpacer, row++, 1);
}

CTweakInfo::~CTweakInfo()
{
	
}

void CTweakInfo::ShowTweak(const CTweakPtr& pTweak)
{
	m_pTweak = pTweak;

	if (!pTweak) {
		m_pName->clear();
		m_pDescription->clear();
		return;
	}

	m_pName->setText(pTweak->GetName());

	if (auto pSet = pTweak.objectCast<CTweakSet>())
	{
		QString sText;
		foreach(const CTweakPtr & pTweak, pSet->GetList())
			sText += pTweak->GetName() + ", " + pTweak->GetInfo() + "\n\n";
		m_pDescription->setPlainText(sText);
	}
	else
		m_pDescription->setPlainText(pTweak->GetInfo());

	m_pActivate->setEnabled(pTweak->GetStatus() != ETweakStatus::eSet && pTweak->GetStatus() != ETweakStatus::eApplied);
	m_pDeactivate->setEnabled(pTweak->GetStatus() == ETweakStatus::eSet || pTweak->GetStatus() == ETweakStatus::eApplied);
}

void CTweakInfo::OnActivate()
{
	if (!m_pTweak) return;
	theCore->TweakManager()->ApplyTweak(m_pTweak);

}

void CTweakInfo::OnDeactivate()
{
	if (!m_pTweak) return;
	theCore->TweakManager()->UndoTweak(m_pTweak);
}
