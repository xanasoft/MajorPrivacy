#include "pch.h"
#include "HandleView.h"
#include "../Core/PrivacyCore.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CHandleView::CHandleView(QWidget *parent)
	:CPanelViewEx<CHandleModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	QByteArray Columns = theConf->GetBlob("MainWindow/HandleView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	AddPanelItemsToMenu();
}

CHandleView::~CHandleView()
{
	theConf->SetBlob("MainWindow/HandleView_Columns", m_pTreeView->saveState());
}

void CHandleView::Sync(const QMap<quint64, CProcessPtr>& Processes)
{
	QList<CHandlePtr> HandleList;
	foreach(CProcessPtr pProcess, Processes)
		HandleList.append(pProcess->GetHandles());

	m_pItemModel->Sync(HandleList);
}

void CHandleView::OnDoubleClicked(const QModelIndex& Index)
{

}

void CHandleView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
