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

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pBtnClear = new QToolButton();
	m_pBtnClear->setIcon(QIcon(":/Icons/Clean.png"));
	m_pBtnClear->setToolTip(tr("Clear System DNS Cache"));
	m_pBtnClear->setFixedHeight(22);
	connect(m_pBtnClear, SIGNAL(clicked()), this, SLOT(FlushDnsCache()));
	m_pToolBar->addWidget(m_pBtnClear);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setFixedHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

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

/*void CDnsCacheView::OnDoubleClicked(const QModelIndex& Index)
{

}*/

void CDnsCacheView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}

void CDnsCacheView::FlushDnsCache()
{
	theCore->FlushDnsCache();
}