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
}

CProcessTraceView::~CProcessTraceView()
{
	theConf->SetBlob("MainWindow/ProcessTraceView_Columns", m_pTreeView->saveState());
}
