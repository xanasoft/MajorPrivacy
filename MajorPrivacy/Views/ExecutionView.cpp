#include "pch.h"
#include "ExecutionView.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../../Library/Helpers/NtUtil.h"

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

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbRole = new QComboBox();
	m_pCmbRole->addItem(QIcon(":/Icons/EFence.png"), tr("Any Role"), (qint32)EExecLogRole::eUndefined);
	m_pCmbRole->addItem(QIcon(":/Icons/Export.png"), tr("Actor"), (qint32)EExecLogRole::eTarget);
	m_pCmbRole->addItem(QIcon(":/Icons/Entry.png"), tr("Target"), (qint32)EExecLogRole::eActor);
	m_pToolBar->addWidget(m_pCmbRole);

	m_pCmbAction = new QComboBox();
	m_pCmbAction->addItem(QIcon(":/Icons/NoAccess.png"), tr("Any Status"), (qint32)EEventStatus::eUndefined);
	m_pCmbAction->addItem(QIcon(":/Icons/Go.png"), tr("Allowed"), (qint32)EEventStatus::eAllowed);
	m_pCmbAction->addItem(QIcon(":/Icons/Disable.png"), tr("Blocked"), (qint32)EEventStatus::eBlocked);
	m_pToolBar->addWidget(m_pCmbAction);

	m_pToolBar->addSeparator();
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

	connect(theCore, SIGNAL(CleanUpDone()), this, SLOT(OnCleanUpDone()));
}

CExecutionView::~CExecutionView()
{
	theConf->SetBlob("MainWindow/ExecutionView_Columns", m_pTreeView->saveState());
}

void CExecutionView::Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, const QFlexGuid& EnclaveGuid)
{
	qint32 FilterRole = m_pCmbRole->currentData().toInt();
	qint32 FilterAction = m_pCmbAction->currentData().toInt();

	if (m_CurPrograms != Programs || m_CurEnclaveGuid != EnclaveGuid || m_CurServices != Services || m_FilterRole != FilterRole || m_FilterAction != FilterAction || m_RecentLimit != theGUI->GetRecentLimit()) {
		m_CurPrograms = Programs;
		m_CurServices = Services;
		m_CurEnclaveGuid = EnclaveGuid;
		m_RecentLimit = theGUI->GetRecentLimit();
		m_ParentMap.clear();
		m_ExecutionMap.clear();
		m_pItemModel->Clear();
	}

	quint64 uRecentLimit = m_RecentLimit ? QDateTime::currentMSecsSinceEpoch() - m_RecentLimit : 0;

	m_FilterRole = FilterRole;
	m_FilterAction = FilterAction;

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

		quint64 SubID = Info.SubjectPID ? Info.SubjectPID : -1;
		//SExecutionItemPtr pSubItem = OldMap.take(qMakePair((uint64)pProg1.get(), (uint64)pProg2.get()));
		SExecutionItemPtr pSubItem = OldMap.take(qMakePair((uint64)pProg1.get(), SubID));
		if (!pSubItem) {
			pSubItem = SExecutionItemPtr(new SExecutionItem());
			pSubItem->pProg2 = pProg2;
			pSubItem->Parent = QString("%1_%2").arg((uint64)pProg1.get()).arg(0);
			//m_ExecutionMap.insert(qMakePair((uint64)pProg1.get(), (uint64)pProg2.get()), pSubItem);
			m_ExecutionMap.insert(qMakePair((uint64)pProg1.get(), SubID), pSubItem);
		}

		pSubItem->Info = Info;
	};

	auto FilterEntry = [&](const CProgramFile::SExecutionInfo& Info) -> bool {
		if (m_FilterRole && Info.Role != (EExecLogRole)m_FilterRole)
			return false;
		if (m_FilterAction != -1 && (uint32)(Info.bBlocked ? EEventStatus::eBlocked : EEventStatus::eAllowed) != m_FilterAction)
			return false;
		if (uRecentLimit && FILETIME2ms(Info.LastExecTime) < uRecentLimit)
			return false;
		if (!m_CurEnclaveGuid.IsNull() && Info.EnclaveGuid != m_CurEnclaveGuid)
			return false;
		return true;
	};

	foreach(const CProgramFilePtr & pProgram, Programs) {
		QMap<quint64, CProgramFile::SExecutionInfo> Log = pProgram->GetExecStats();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			if (!FilterEntry(I.value()))
				continue;
			CProgramFilePtr pProgram2 = theCore->ProgramManager()->GetProgramByUID(I.value().SubjectUID).objectCast<CProgramFile>();
			if (!pProgram2) continue;
			AddEntry(pProgram, pProgram2, I.value());
		}
	}
	foreach(const CWindowsServicePtr& pService, Services) {
		QMap<quint64, CProgramFile::SExecutionInfo> Log = pService->GetExecStats();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			if (!FilterEntry(I.value()))
				continue;
			CProgramFilePtr pProgram2 = theCore->ProgramManager()->GetProgramByUID(I.value().SubjectUID).objectCast<CProgramFile>();
			if (!pProgram2) continue;
			AddEntry(pService->GetProgramFile(), pProgram2, I.value());
		}
	}

	foreach(SExecutionKey Key, OldMap.keys()) {
		SExecutionItemPtr pOld = m_ExecutionMap.take(Key);
		if(!pOld.isNull())
			m_ParentMap.remove(qMakePair(pOld->Parent.toULongLong(), 0));
	}

	QList<QModelIndex> Added = m_pItemModel->Sync(m_ExecutionMap);

	if (m_pBtnExpand->isChecked()) 
	{
		QTimer::singleShot(10, this, [this, Added]() {
			foreach(const QModelIndex & Index, Added)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
			});
	}
}

/*void CExecutionView::OnDoubleClicked(const QModelIndex& Index)
{
	//QPair<CProgramItemPtr, CTrafficEntryPtr> pItem = m_pItemModel->GetItem(Index);
}*/

void CExecutionView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}

void CExecutionView::OnCleanUpDone()
{
	// refresh
	m_CurPrograms.clear();
	m_CurServices.clear();
}
