#include "pch.h"
#include "HandleView.h"
#include "../Core/PrivacyCore.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../MiscHelpers/Common/Common.h"

CHandleView::CHandleView(QWidget *parent)
	:CPanelViewEx<CHandleModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
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

void CHandleView::Sync(const QHash<quint64, CProcessPtr>& Processes, const QString& RootPath)
{
	bool bLogPipes = theCore->GetConfigBool("Service/LogPipes", false);

	QList<CHandlePtr> HandleList;
	foreach(CProcessPtr pProcess, Processes) {
		QList<CHandlePtr> Handles = pProcess->GetHandles();
		foreach(CHandlePtr pHandle, Handles) {
			if (!bLogPipes && pHandle->GetNtPath().startsWith("\\Device\\NamedPipe", Qt::CaseInsensitive))
				continue;
			HandleList.append(pHandle);
		}
	}

	if (!RootPath.isNull())
	{
		if(RootPath.isEmpty())
			HandleList.clear();
		else

		for (auto I = HandleList.begin(); I != HandleList.end();)
		{
			if (!PathStartsWith((*I)->GetNtPath(), RootPath))
				I = HandleList.erase(I);
			else
				++I;
		}
	}

	m_pItemModel->Sync(HandleList);
}

void CHandleView::Clear()
{
	m_pItemModel->Clear();
}

/*void CHandleView::OnDoubleClicked(const QModelIndex& Index)
{

}*/

void CHandleView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
