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

	AddPanelItemsToMenu();
}

CSocketView::~CSocketView()
{
	theConf->SetBlob("MainWindow/SocketView_Columns", m_pTreeView->saveState());
}

void CSocketView::Sync(const QMap<quint64, CProcessPtr>& Processes, const QSet<CWindowsServicePtr>& ServicesEx)
{
	QList<CSocketPtr> SocketList;
	foreach(CProcessPtr pProcess, Processes)
		SocketList.append(pProcess->GetSockets());
	foreach(CWindowsServicePtr pService, ServicesEx) {
		CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pService->GetProcessId());
		if (!pProcess) continue;
		foreach(CSocketPtr pSocket, pProcess->GetSockets()) {
			if (pSocket->GetOwnerService().compare(pService->GetSvcTag(), Qt::CaseInsensitive) == 0)
				SocketList.append(pSocket);
		}
	}

	m_pItemModel->Sync(SocketList);
}

void CSocketView::OnDoubleClicked(const QModelIndex& Index)
{

}

void CSocketView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
