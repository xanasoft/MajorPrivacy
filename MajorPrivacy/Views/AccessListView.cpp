#include "pch.h"
#include "AccessListView.h"
#include "AccessView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Programs/ProgramManager.h"
#include "../../Library/Helpers/NtUtil.h"
#include <QFileIconProvider>
#include "../MiscHelpers/Common/CustomStyles.h"
#include "..\Core\Access\ResLogEntry.h"
#include "../MajorPrivacy.h"

CAccessListView::CAccessListView(QWidget *parent)
	:CPanelViewEx<CAccessListModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	QByteArray Columns = theConf->GetBlob("MainWindow/AccessListView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pBtnExpand = new QToolButton();
	m_pBtnExpand->setIcon(QIcon(":/Icons/Expand.png"));
	m_pBtnExpand->setCheckable(true);
	m_pBtnExpand->setToolTip(tr("Auto Expand"));
	m_pBtnExpand->setMaximumHeight(22);
	connect(m_pBtnExpand, &QToolButton::toggled, this, [&](bool checked) {
		if(checked)
			m_pTreeView->expandAll();
		else
			m_pTreeView->collapseAll();
		});
	m_pToolBar->addWidget(m_pBtnExpand);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	AddPanelItemsToMenu();
}

CAccessListView::~CAccessListView()
{
	theConf->SetBlob("MainWindow/AccessListView_Columns", m_pTreeView->saveState());
}

void CAccessListView::OnSelectionChanged(const QList<SAccessItemPtr>& Items)
{
	CProgressDialogHelper ProgressHelper(theGUI->m_pProgressDialog, tr("Loading %1"), Items.count());

	QMap<CProgramItemPtr, QPair<quint64,QList<QPair<SAccessStatsPtr,SAccessItem::EType>>>> Map;
	for (auto pItem : Items) {

		if (!ProgressHelper.Next(pItem->Name)) {
			break;
		}

		for (auto I = pItem->Stats.begin(); I != pItem->Stats.end(); ++I) {
			auto& Pair = Map[I.key()];
			if (Pair.first < pItem->LastAccess)
				Pair.first = pItem->LastAccess;
			Pair.second.append(qMakePair(I.value(),pItem->Type));
		}
	}

	ProgressHelper.Done();

	QList<QModelIndex> Added = m_pItemModel->Sync(Map);

	if (m_pBtnExpand->isChecked()) 
	{
		QTimer::singleShot(10, this, [this, Added]() {
			foreach(const QModelIndex & Index, Added)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
		});
	}
}

void CAccessListView::Clear()
{
	m_pItemModel->Clear();
}

/*void CAccessListView::OnDoubleClicked(const QModelIndex& Index)
{

}*/

void CAccessListView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
