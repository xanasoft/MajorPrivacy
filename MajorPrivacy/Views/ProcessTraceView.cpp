#include "pch.h"
#include "ProcessTraceView.h"
#include "../Core/PrivacyCore.h"

CProcessTraceView::CProcessTraceView(QWidget *parent)
	:CTraceView(new CProcessTraceModel(), parent)
{
	QByteArray Columns = theConf->GetBlob("MainWindow/ProcessTraceView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbRole = new QComboBox();
	m_pCmbRole->addItem(tr("Any Role"), (qint32)EExecLogRole::eUndefined);
	m_pCmbRole->addItem(tr("Actor"), (qint32)EExecLogRole::eActor);
	m_pCmbRole->addItem(tr("Target"), (qint32)EExecLogRole::eTarget);
	connect(m_pCmbRole, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbRole);

	int comboBoxHeight = m_pCmbRole->sizeHint().height();

	m_pCmbAction = new QComboBox();
	m_pCmbAction->addItem(tr("Any Action"), (qint32)EEventStatus::eUndefined);
	m_pCmbAction->addItem(tr("Allowed"), (qint32)EEventStatus::eAllowed);
	m_pCmbAction->addItem(tr("Blocked"), (qint32)EEventStatus::eBlocked);
	connect(m_pCmbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbAction);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(comboBoxHeight);
	m_pToolBar->addWidget(pBtnSearch);
}

CProcessTraceView::~CProcessTraceView()
{
	theConf->SetBlob("MainWindow/ProcessTraceView_Columns", m_pTreeView->saveState());
}

void CProcessTraceView::UpdateFilter()
{
	((CProcessTraceModel*)m_pItemModel)->SetFilter((EExecLogRole)m_pCmbRole->currentData().toInt(), (EEventStatus)m_pCmbAction->currentData().toInt());
	m_FullRefresh = true;
}