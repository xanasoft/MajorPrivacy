#include "pch.h"
#include "NetTraceView.h"
#include "../Core/PrivacyCore.h"

CNetTraceView::CNetTraceView(QWidget *parent)
	:CTraceView(new CNetTraceModel(), parent)
{
	QByteArray Columns = theConf->GetBlob("MainWindow/NetTraceView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);	

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbAction = new QComboBox();
	m_pCmbAction->addItem(tr("Any Action"), (qint32)EEventStatus::eUndefined);
	m_pCmbAction->addItem(tr("Allowed"), (qint32)EEventStatus::eAllowed);
	m_pCmbAction->addItem(tr("Blocked"), (qint32)EEventStatus::eBlocked);
	connect(m_pCmbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbAction);

	m_pCmbType = new QComboBox();
	m_pCmbType->addItem(tr("All Protocols"), (qint32)ENetProtocols::eAny);
	m_pCmbType->addItem(tr("HTTP(S)"), (qint32)ENetProtocols::eWeb);
	m_pCmbType->addItem(tr("TCP Sockets"), (qint32)ENetProtocols::eTCP);
	m_pCmbType->addItem(tr("TCP Clients"), (qint32)ENetProtocols::eTCP_Client);
	m_pCmbType->addItem(tr("TCP Servers"), (qint32)ENetProtocols::eTCP_Server);
	m_pCmbType->addItem(tr("UDP Sockets"), (qint32)ENetProtocols::eUDP);
	connect(m_pCmbType, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbType);

	m_pBtnScroll = new QToolButton();
	m_pBtnScroll->setIcon(QIcon(":/Icons/Scroll.png"));
	m_pBtnScroll->setCheckable(true);
	m_pBtnScroll->setToolTip(tr("Auto Scroll"));
	m_pBtnScroll->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnScroll);

	int comboBoxHeight = m_pCmbType->sizeHint().height();

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(comboBoxHeight);
	m_pToolBar->addWidget(pBtnSearch);
}

CNetTraceView::~CNetTraceView()
{
	theConf->SetBlob("MainWindow/NetTraceView_Columns", m_pTreeView->saveState());
}

void CNetTraceView::Sync(const struct SMergedLog* pLog)
{
	CTraceView::Sync(pLog);

	if(m_pBtnScroll->isChecked())
		m_pTreeView->scrollToBottom();
}

void CNetTraceView::UpdateFilter()
{
	((CNetTraceModel*)m_pItemModel)->SetFilter((EEventStatus)m_pCmbAction->currentData().toInt(), (ENetProtocols)m_pCmbType->currentData().toInt());
	m_FullRefresh = true;
}

