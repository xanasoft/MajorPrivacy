#include "pch.h"
#include "LibraryView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CLibraryView::CLibraryView(QWidget *parent)
	:CPanelViewEx<CLibraryModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	connect(m_pItemModel, SIGNAL(CheckChanged(const QModelIndex&, bool)), this, SLOT(OnCheckChanged(const QModelIndex&, bool)));

	QByteArray Columns = theConf->GetBlob("MainWindow/LibraryView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbGrouping = new QComboBox();
	m_pCmbGrouping->addItems(QStringList() << tr("By Program") << tr("By Library"));
	m_pToolBar->addWidget(m_pCmbGrouping);

	int comboBoxHeight = m_pCmbGrouping->sizeHint().height();

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(comboBoxHeight);
	m_pToolBar->addWidget(pBtnSearch);

	AddPanelItemsToMenu();
}

CLibraryView::~CLibraryView()
{
	theConf->SetBlob("MainWindow/LibraryView_Columns", m_pTreeView->saveState());
}

void CLibraryView::Sync(const QSet<CProgramFilePtr>& Programs, bool bAllPrograms)
{
	bool bGroupByLibrary = m_pCmbGrouping->currentIndex() == 1;

	if (m_CurPrograms != Programs || bGroupByLibrary != m_bGroupByLibrary) {
		m_CurPrograms = Programs;
		m_ParentMap.clear();
		m_LibraryMap.clear();
		m_pItemModel->Clear();
	}

	m_bGroupByLibrary = bGroupByLibrary;

	auto OldMap = m_LibraryMap;

	std::function<void(const CProgramFilePtr&, const CProgramLibraryPtr&, const SLibraryInfo&)> AddByProgram = 
		[&](const CProgramFilePtr& pProg, const CProgramLibraryPtr& pLibrary, const SLibraryInfo& Info) {

		SLibraryItemPtr& pItem = m_ParentMap[qMakePair((uint64)pProg.get(), 0)];
		if(pItem.isNull())
		{
			pItem = SLibraryItemPtr(new SLibraryItem());
			pItem->pProg = pProg;
			m_LibraryMap.insert(qMakePair((uint64)pProg.get(), 0), pItem);
		}
		else
			OldMap.remove(qMakePair((uint64)pProg.get(), 0));

		SLibraryItemPtr pSubItem = OldMap.take(qMakePair((uint64)pProg.get(), (uint64)pLibrary.get()));
		if (!pSubItem) {
			pSubItem = SLibraryItemPtr(new SLibraryItem());
			pSubItem->pLibrary = pLibrary;
			pSubItem->Parent = QString("%1_%2").arg((uint64)pProg.get()).arg(0);
			m_LibraryMap.insert(qMakePair((uint64)pProg.get(), (uint64)pLibrary.get()), pSubItem);
			pItem->Count++;
		}

		pSubItem->Info = Info;
	};

	std::function<void(const CProgramFilePtr&, const CProgramLibraryPtr&, const SLibraryInfo&)> AddByLibrary = 
		[&](const CProgramFilePtr& pProg, const CProgramLibraryPtr& pLibrary, const SLibraryInfo& Info) {

		SLibraryItemPtr& pItem = m_ParentMap[qMakePair((uint64)pLibrary.get(), 0)];
		if(pItem.isNull())
		{
			pItem = SLibraryItemPtr(new SLibraryItem());
			pItem->pLibrary = pLibrary;
			m_LibraryMap.insert(qMakePair((uint64)pLibrary.get(), 0), pItem);
		}
		else
			OldMap.remove(qMakePair((uint64)pLibrary.get(), 0));

		SLibraryItemPtr pSubItem = OldMap.take(qMakePair((uint64)pLibrary.get(), (uint64)pProg.get()));
		if (!pSubItem) {
			pSubItem = SLibraryItemPtr(new SLibraryItem());
			pSubItem->pProg = pProg;
			pSubItem->Parent = QString("%1_%2").arg((uint64)pLibrary.get()).arg(0);
			m_LibraryMap.insert(qMakePair((uint64)pLibrary.get(), (uint64)pProg.get()), pSubItem);
			pItem->Count++;
		}

		pSubItem->Info = Info;
	};

	foreach(const CProgramFilePtr& pProgram, Programs) {
		QMap<quint64, SLibraryInfo> Log = pProgram->GetLibraries();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			CProgramLibraryPtr pLibrary = theCore->ProgramManager()->GetLibraryByUID(I.key());
			if(!pLibrary) continue; // todo 
			if(m_bGroupByLibrary) // if(bAllPrograms)
				AddByLibrary(pProgram, pLibrary, I.value());
			else
				AddByProgram(pProgram, pLibrary, I.value());
		}
	}

	foreach(SLibraryKey Key, OldMap.keys()) {
		SLibraryItemPtr pOld = m_LibraryMap.take(Key);
		if(!pOld.isNull())
			m_ParentMap.remove(qMakePair(pOld->Parent.toULongLong(), 0));
	}

	QList<QModelIndex> Added = m_pItemModel->Sync(m_LibraryMap);

	if (m_CurPrograms.count() == 1)
	{
		QTimer::singleShot(10, this, [this, Added]() {
			foreach(const QModelIndex & Index, Added)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
		});
	}
}

void CLibraryView::OnDoubleClicked(const QModelIndex& Index)
{
	//QPair<CProgramItemPtr, CTrafficEntryPtr> pItem = m_pItemModel->GetItem(Index);
}

void CLibraryView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}
