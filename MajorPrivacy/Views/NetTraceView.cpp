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

	m_pCmbDir = new QComboBox();
	m_pCmbDir->addItem(QIcon(":/Icons/ArrowUpDown.png"), tr("All Directions"), (qint32)EFwDirections::Bidirectional);
	m_pCmbDir->addItem(QIcon(":/Icons/ArrowDown.png"), tr("Inbound"), (qint32)EFwDirections::Inbound);
	m_pCmbDir->addItem(QIcon(":/Icons/ArrowUp.png"), tr("Outbound"), (qint32)EFwDirections::Outbound);
	connect(m_pCmbDir, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbDir);

	m_pCmbAction = new QComboBox();
	m_pCmbAction->addItem(QIcon(":/Icons/NoAccess.png"), tr("Any Status"), (qint32)EEventStatus::eUndefined);
	m_pCmbAction->addItem(QIcon(":/Icons/Go.png"), tr("Allowed"), (qint32)EEventStatus::eAllowed);
	m_pCmbAction->addItem(QIcon(":/Icons/Disable.png"), tr("Blocked"), (qint32)EEventStatus::eBlocked);
	connect(m_pCmbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbAction);

	m_pCmbType = new QComboBox();
	m_pCmbType->addItem(QIcon(":/Icons/Network.png"), tr("All Protocols"), (qint32)ENetProtocols::eAny);
	m_pCmbType->addItem(QIcon(":/Icons/Network.png"), tr("HTTP(S)"), (qint32)ENetProtocols::eWeb);
	m_pCmbType->addItem(QIcon(":/Icons/Network.png"), tr("TCP Sockets"), (qint32)ENetProtocols::eTCP);
	m_pCmbType->addItem(QIcon(":/Icons/Network.png"), tr("TCP Clients"), (qint32)ENetProtocols::eTCP_Client);
	m_pCmbType->addItem(QIcon(":/Icons/Network.png"), tr("TCP Servers"), (qint32)ENetProtocols::eTCP_Server);
	m_pCmbType->addItem(QIcon(":/Icons/Network.png"), tr("UDP Sockets"), (qint32)ENetProtocols::eUDP);
	connect(m_pCmbType, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbType);

	m_pToolBar->addSeparator();

	m_pBtnScroll = new QToolButton();
	m_pBtnScroll->setIcon(QIcon(":/Icons/Scroll.png"));
	m_pBtnScroll->setCheckable(true);
	m_pBtnScroll->setToolTip(tr("Auto Scroll"));
	m_pBtnScroll->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnScroll);

	m_pBtnHold = new QToolButton();
	m_pBtnHold->setIcon(QIcon(":/Icons/Hold.png"));
	m_pBtnHold->setCheckable(true);
	m_pBtnHold->setToolTip(tr("Hold updates"));
	m_pBtnHold->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnHold);

	m_pToolBar->addSeparator();

	m_pBtnClear = new QToolButton();
	m_pBtnClear->setIcon(QIcon(":/Icons/Trash.png"));
	m_pBtnClear->setToolTip(tr("Clear Trace Log"));
	m_pBtnClear->setFixedHeight(22);
	connect(m_pBtnClear, SIGNAL(clicked()), this, SLOT(OnClearTraceLog()));
	m_pToolBar->addWidget(m_pBtnClear);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);
}

CNetTraceView::~CNetTraceView()
{
	theConf->SetBlob("MainWindow/NetTraceView_Columns", m_pTreeView->saveState());
}

void CNetTraceView::Sync(ETraceLogs Log, const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services)
{
	CTraceView::Sync(Log, Programs, Services);
}

void CNetTraceView::UpdateFilter()
{
	((CNetTraceModel*)m_pItemModel)->SetFilter((EFwDirections)m_pCmbDir->currentData().toInt(), (EEventStatus)m_pCmbAction->currentData().toInt(), (ENetProtocols)m_pCmbType->currentData().toInt());
	m_FullRefresh = true;
}

void CNetTraceView::OnClearTraceLog()
{
	ClearTraceLog(ETraceLogs::eNetLog);
}