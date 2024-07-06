#include "pch.h"
#include "ProcessView.h"
#include "../Core/PrivacyCore.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CProcessView::CProcessView(QWidget *parent)
	:CPanelViewEx(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	QByteArray Columns = theConf->GetBlob("MainWindow/ProcessView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	AddPanelItemsToMenu();
}

CProcessView::~CProcessView()
{
	theConf->SetBlob("MainWindow/ProcessView_Columns", m_pTreeView->saveState());
}

void CProcessView::Sync(const QMap<quint64, CProcessPtr>& ProcessMap)
{
	QSet<quint64> AddedProcs = m_pItemModel->Sync(ProcessMap);

	if (m_pItemModel->IsTree()) {
		QTimer::singleShot(10, this, [this, AddedProcs]() {
			foreach(const QVariant ID, AddedProcs) {
				QModelIndex ModelIndex = m_pItemModel->FindIndex(ID);	
				m_pTreeView->expand(m_pSortProxy->mapFromSource(ModelIndex));
			}
		});
	}
}

void CProcessView::OnDoubleClicked(const QModelIndex& Index)
{

}

void CProcessView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
