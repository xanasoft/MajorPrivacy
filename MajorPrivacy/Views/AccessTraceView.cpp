#include "pch.h"
#include "AccessTraceView.h"
#include "../Core/PrivacyCore.h"

CAccessTraceView::CAccessTraceView(QWidget *parent)
	:CTraceView(new CAccessTraceModel(), parent)
{
	QByteArray Columns = theConf->GetBlob("MainWindow/AccessTraceView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);
}

CAccessTraceView::~CAccessTraceView()
{
	theConf->SetBlob("MainWindow/AccessTraceView_Columns", m_pTreeView->saveState());
}
