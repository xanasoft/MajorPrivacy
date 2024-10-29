#include "pch.h"
#include "EnclaveView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Processes/EnclaveList.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CEnclaveView::CEnclaveView(QWidget *parent)
	:CPanelViewEx(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	QByteArray Columns = theConf->GetBlob("MainWindow/EnclaveView_Columns");
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

CEnclaveView::~CEnclaveView()
{
	theConf->SetBlob("MainWindow/EnclaveView_Columns", m_pTreeView->saveState());
}

void CEnclaveView::Sync()
{
	const QMap<quint64, CEnclavePtr>& EnclaveMap = theCore->EnclaveList()->List();

	QList<QVariant> Added = m_pItemModel->Sync(EnclaveMap);

	if (m_pItemModel->IsTree()) {
		QTimer::singleShot(10, this, [this, Added]() {
			foreach(const QVariant ID, Added) {
				QModelIndex ModelIndex = m_pItemModel->FindIndex(ID);	
				m_pTreeView->expand(m_pSortProxy->mapFromSource(ModelIndex));
			}
		});
	}
}

void CEnclaveView::OnDoubleClicked(const QModelIndex& Index)
{

}

void CEnclaveView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
