#include "pch.h"
#include "DnsCacheView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Network/NetworkManager.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CDnsCacheView::CDnsCacheView(QWidget *parent)
	:CPanelViewEx<CDnsCacheModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	QByteArray Columns = theConf->GetBlob("MainWindow/DnsCacheView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	AddPanelItemsToMenu();
}

CDnsCacheView::~CDnsCacheView()
{
	theConf->SetBlob("MainWindow/DnsCacheView_Columns", m_pTreeView->saveState());
}

void CDnsCacheView::Sync()
{
	theCore->NetworkManager()->UpdateDnsCache();
	m_pItemModel->Sync(theCore->NetworkManager()->GetDnsCache());
}

void CDnsCacheView::OnDoubleClicked(const QModelIndex& Index)
{

}

void CDnsCacheView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
