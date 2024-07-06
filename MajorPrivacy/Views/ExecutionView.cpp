#include "pch.h"
#include "ExecutionView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CExecutionView::CExecutionView(QWidget *parent)
	:CPanelViewEx<CExecutionModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	connect(m_pItemModel, SIGNAL(CheckChanged(const QModelIndex&, bool)), this, SLOT(OnCheckChanged(const QModelIndex&, bool)));

	QByteArray Columns = theConf->GetBlob("MainWindow/ExecutionView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	AddPanelItemsToMenu();
}

CExecutionView::~CExecutionView()
{
	theConf->SetBlob("MainWindow/ExecutionView_Columns", m_pTreeView->saveState());
}

void CExecutionView::Sync(const QSet<CProgramFilePtr>& Programs, bool bAllPrograms)
{
	if (m_CurPrograms != Programs) {
		m_CurPrograms = Programs;
		m_ParentMap.clear();
		m_ExecutionMap.clear();
		m_pItemModel->Clear();
	}

	auto OldMap = m_ExecutionMap;

	std::function<void(const CProgramFilePtr&, const CProgramFilePtr&, const CProgramFile::SExecutionInfo&)> AddEntry = 
		[&](const CProgramFilePtr& pProg1, const CProgramFilePtr& pProg2, const CProgramFile::SExecutionInfo& Info) {

		SExecutionItemPtr& pItem = m_ParentMap[qMakePair((uint64)pProg1.get(), 0)];
		if(pItem.isNull())
		{
			pItem = SExecutionItemPtr(new SExecutionItem());
			pItem->pProg1 = pProg1;
			m_ExecutionMap.insert(qMakePair((uint64)pProg1.get(), 0), pItem);
		}
		else
			OldMap.remove(qMakePair((uint64)pProg1.get(), 0));

		SExecutionItemPtr pSubItem = OldMap.take(qMakePair((uint64)pProg1.get(), (uint64)pProg2.get()));
		if (!pSubItem) {
			pSubItem = SExecutionItemPtr(new SExecutionItem());
			pSubItem->pProg2 = pProg2;
			pSubItem->Parent = QString("%1_%2").arg((uint64)pProg1.get()).arg(0);
			m_ExecutionMap.insert(qMakePair((uint64)pProg1.get(), (uint64)pProg2.get()), pSubItem);
		}

		pSubItem->Info = Info;
	};

	foreach(const CProgramFilePtr& pProgram, Programs) {
		QMap<quint64, CProgramFile::SExecutionInfo> Log = pProgram->GetExecStats();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			CProgramFilePtr pProgram2 = theCore->ProgramManager()->GetProgramByUID(I.value().ProgramUID).objectCast<CProgramFile>();
			if(!pProgram2) continue;
			AddEntry(pProgram, pProgram2, I.value());
		}
	}

	foreach(SExecutionKey Key, OldMap.keys()) {
		SExecutionItemPtr pOld = m_ExecutionMap.take(Key);
		if(!pOld.isNull())
			m_ParentMap.remove(qMakePair(pOld->Parent.toULongLong(), 0));
	}

	QList<QModelIndex> Added = m_pItemModel->Sync(m_ExecutionMap);

	if (m_CurPrograms.count() == 1)
	{
		QTimer::singleShot(10, this, [this, Added]() {
			foreach(const QModelIndex & Index, Added)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
		});
	}
}

void CExecutionView::OnDoubleClicked(const QModelIndex& Index)
{
	//QPair<CProgramItemPtr, CTrafficEntryPtr> pItem = m_pItemModel->GetItem(Index);
}

void CExecutionView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
