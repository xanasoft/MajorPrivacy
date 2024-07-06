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
}

CNetTraceView::~CNetTraceView()
{
	theConf->SetBlob("MainWindow/NetTraceView_Columns", m_pTreeView->saveState());
}
