#include "pch.h"
#include "IngressView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CIngressView::CIngressView(QWidget *parent)
	:CPanelViewEx<CIngressModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	connect(m_pItemModel, SIGNAL(CheckChanged(const QModelIndex&, bool)), this, SLOT(OnCheckChanged(const QModelIndex&, bool)));

	QByteArray Columns = theConf->GetBlob("MainWindow/IngressView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	AddPanelItemsToMenu();
}

CIngressView::~CIngressView()
{
	theConf->SetBlob("MainWindow/IngressView_Columns", m_pTreeView->saveState());
}

void CIngressView::Sync(const QSet<CProgramFilePtr>& Programs, bool bAllPrograms)
{
	if (m_CurPrograms != Programs) {
		m_CurPrograms = Programs;
		m_ParentMap.clear();
		m_IngressMap.clear();
		m_pItemModel->Clear();
	}

	auto OldMap = m_IngressMap;

	std::function<void(const CProgramFilePtr&, const CProgramFilePtr&, const CProgramFile::SIngressInfo&)> AddEntry = 
		[&](const CProgramFilePtr& pProg1, const CProgramFilePtr& pProg2, const CProgramFile::SIngressInfo& Info) {

		SIngressItemPtr& pItem = m_ParentMap[qMakePair((uint64)pProg1.get(), 0)];
		if(pItem.isNull())
		{
			pItem = SIngressItemPtr(new SIngressItem());
			pItem->pProg1 = pProg1;
			m_IngressMap.insert(qMakePair((uint64)pProg1.get(), 0), pItem);
		}
		else
			OldMap.remove(qMakePair((uint64)pProg1.get(), 0));

		SIngressItemPtr pSubItem = OldMap.take(qMakePair((uint64)pProg1.get(), (uint64)pProg2.get()));
		if (!pSubItem) {
			pSubItem = SIngressItemPtr(new SIngressItem());
			pSubItem->pProg2 = pProg2;
			pSubItem->Parent = QString("%1_%2").arg((uint64)pProg1.get()).arg(0);
			m_IngressMap.insert(qMakePair((uint64)pProg1.get(), (uint64)pProg2.get()), pSubItem);
		}

		pSubItem->Info = Info;
	};

	foreach(const CProgramFilePtr& pProgram, Programs) {
		QMap<quint64, CProgramFile::SIngressInfo> Log = pProgram->GetIngressStats();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			CProgramFilePtr pProgram2 = theCore->ProgramManager()->GetProgramByUID(I.value().ProgramUID).objectCast<CProgramFile>();
			if(!pProgram2) continue;
			AddEntry(pProgram, pProgram2, I.value());
		}
	}

	foreach(SIngressKey Key, OldMap.keys()) {
		SIngressItemPtr pOld = m_IngressMap.take(Key);
		if(!pOld.isNull())
			m_ParentMap.remove(qMakePair(pOld->Parent.toULongLong(), 0));
	}

	QList<QModelIndex> Added = m_pItemModel->Sync(m_IngressMap);

	if (m_CurPrograms.count() == 1)
	{
		QTimer::singleShot(10, this, [this, Added]() {
			foreach(const QModelIndex & Index, Added)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
		});
	}
}

void CIngressView::OnDoubleClicked(const QModelIndex& Index)
{
	//QPair<CProgramItemPtr, CTrafficEntryPtr> pItem = m_pItemModel->GetItem(Index);
}

void CIngressView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
