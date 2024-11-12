#include "pch.h"
#include "SocketView.h"
#include "../Core/PrivacyCore.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CSocketView::CSocketView(QWidget *parent)
	:CPanelViewEx<CSocketModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	QByteArray Columns = theConf->GetBlob("MainWindow/SocketView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbType = new QComboBox();
	m_pCmbType->addItem(QIcon(":/Icons/Network.png"), tr("All Protocols"), (qint32)ENetProtocols::eAny);
	m_pCmbType->addItem(QIcon(":/Icons/Network.png"), tr("HTTP(S)"), (qint32)ENetProtocols::eWeb);
	m_pCmbType->addItem(QIcon(":/Icons/Network.png"), tr("TCP Sockets"), (qint32)ENetProtocols::eTCP);
	m_pCmbType->addItem(QIcon(":/Icons/Network.png"), tr("TCP Clients"), (qint32)ENetProtocols::eTCP_Client);
	m_pCmbType->addItem(QIcon(":/Icons/Network.png"), tr("TCP Servers"), (qint32)ENetProtocols::eTCP_Server);
	m_pCmbType->addItem(QIcon(":/Icons/Network.png"), tr("UDP Sockets"), (qint32)ENetProtocols::eUDP);
	m_pToolBar->addWidget(m_pCmbType);

	int comboBoxHeight = m_pCmbType->sizeHint().height();

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(comboBoxHeight);
	m_pToolBar->addWidget(pBtnSearch);

	AddPanelItemsToMenu();
}

CSocketView::~CSocketView()
{
	theConf->SetBlob("MainWindow/SocketView_Columns", m_pTreeView->saveState());
}

void CSocketView::Sync(const QMap<quint64, CProcessPtr>& Processes, const QSet<CWindowsServicePtr>& ServicesEx)
{
	ENetProtocols Protocol = (ENetProtocols)m_pCmbType->currentData().toInt();

	auto FilterSocket = [Protocol](const CSocketPtr& pSocket) -> bool {
		if (Protocol != ENetProtocols::eAny)
		{
			switch (Protocol)
			{
			case ENetProtocols::eWeb:			if (pSocket->GetProtocolType() != (quint32)EFwKnownProtocols::TCP || (pSocket->GetRemotePort() != 80 && pSocket->GetRemotePort() != 443)) return false; break;
			case ENetProtocols::eTCP:			if (pSocket->GetProtocolType() != (quint32)EFwKnownProtocols::TCP) return false; break;
			case ENetProtocols::eTCP_Server:	if (pSocket->GetProtocolType() != (quint32)EFwKnownProtocols::TCP || pSocket->GetState() != MIB_TCP_STATE_LISTEN) return false; break;
			case ENetProtocols::eTCP_Client:	if (pSocket->GetProtocolType() != (quint32)EFwKnownProtocols::TCP || pSocket->GetState() != MIB_TCP_STATE_ESTAB) return false; break;
			case ENetProtocols::eUDP:			if (pSocket->GetProtocolType() != (quint32)EFwKnownProtocols::UDP) return false; break;
			}
		}
		return true;
	};

	QMap<quint64, CSocketPtr> SocketList;
	foreach(CProcessPtr pProcess, Processes) {
		foreach(CSocketPtr pSocket, pProcess->GetSockets()) {
			if (FilterSocket(pSocket))
				SocketList[pSocket->GetSocketRef()] = pSocket;
		}
	}
	foreach(CWindowsServicePtr pService, ServicesEx) {
		CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pService->GetProcessId());
		if (!pProcess) continue;
		foreach(CSocketPtr pSocket, pProcess->GetSockets()) {
			if (pSocket->GetOwnerService().compare(pService->GetSvcTag(), Qt::CaseInsensitive) == 0 && FilterSocket(pSocket))
				SocketList[pSocket->GetSocketRef()] = pSocket;
		}
	}

	m_pItemModel->Sync(SocketList);
}

/*void CSocketView::OnDoubleClicked(const QModelIndex& Index)
{

}*/

void CSocketView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
