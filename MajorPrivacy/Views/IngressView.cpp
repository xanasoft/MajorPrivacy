#include "pch.h"
#include "IngressView.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../../Library/Helpers/NtUtil.h"

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

CIngressView::~CIngressView()
{
	theConf->SetBlob("MainWindow/IngressView_Columns", m_pTreeView->saveState());
}

void CIngressView::Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, const QFlexGuid& EnclaveGuid)
{
	qint32 FilterRole = m_pCmbRole->currentData().toInt();
	qint32 FilterAction = m_pCmbAction->currentData().toInt();

	if (m_CurPrograms != Programs || m_CurEnclaveGuid != EnclaveGuid || m_CurServices != Services || m_FilterRole != FilterRole || m_FilterAction != FilterAction || m_RecentLimit != theGUI->GetRecentLimit()) {
		m_CurPrograms = Programs;
		m_CurServices = Services;
		m_CurEnclaveGuid = EnclaveGuid;
		m_RecentLimit = theGUI->GetRecentLimit();
		m_ParentMap.clear();
		m_IngressMap.clear();
		m_pItemModel->Clear();
	}

	quint64 uRecentLimit = m_RecentLimit ? QDateTime::currentMSecsSinceEpoch() - m_RecentLimit : 0;

	m_FilterRole = FilterRole;
	m_FilterAction = FilterAction;

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

		quint64 SubID = Info.SubjectPID ? Info.SubjectPID : -1;
		//SIngressItemPtr pSubItem = OldMap.take(qMakePair((uint64)pProg1.get(), (uint64)pProg2.get()));
		SIngressItemPtr pSubItem = OldMap.take(qMakePair((uint64)pProg1.get(), SubID));
		if (!pSubItem) {
			pSubItem = SIngressItemPtr(new SIngressItem());
			pSubItem->pProg2 = pProg2;
			pSubItem->Parent = QString("%1_%2").arg((uint64)pProg1.get()).arg(0);
			//m_IngressMap.insert(qMakePair((uint64)pProg1.get(), (uint64)pProg2.get()), pSubItem);
			m_IngressMap.insert(qMakePair((uint64)pProg1.get(), SubID), pSubItem);
		}

		pSubItem->Info = Info;
	};

	auto FilterEntry = [&](const CProgramFile::SIngressInfo& Info) -> bool {
		if (m_FilterRole && Info.Role != (EExecLogRole)m_FilterRole)
			return false;
		if (m_FilterAction != -1 && (uint32)(Info.bBlocked ? EEventStatus::eBlocked : EEventStatus::eAllowed) != m_FilterAction)
			return false;
		if (uRecentLimit && FILETIME2ms(Info.LastAccessTime) < uRecentLimit)
			return false;
		if (!m_CurEnclaveGuid.IsNull() && Info.EnclaveGuid != m_CurEnclaveGuid)
			return false;
		return true;
	};

	foreach(const CProgramFilePtr & pProgram, Programs) {
		QMap<quint64, CProgramFile::SIngressInfo> Log = pProgram->GetIngressStats();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			if (!FilterEntry(I.value()))
				continue;
			CProgramFilePtr pProgram2 = theCore->ProgramManager()->GetProgramByUID(I.value().SubjectUID).objectCast<CProgramFile>();
			if (!pProgram2) continue;
			AddEntry(pProgram, pProgram2, I.value());
		}
	}
	foreach(const CWindowsServicePtr& pService, Services) {
		QMap<quint64, CProgramFile::SIngressInfo> Log = pService->GetIngressStats();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			if (!FilterEntry(I.value()))
				continue;
			CProgramFilePtr pProgram2 = theCore->ProgramManager()->GetProgramByUID(I.value().SubjectUID).objectCast<CProgramFile>();
			if (!pProgram2) continue;
			AddEntry(pService->GetProgramFile(), pProgram2, I.value());
		}
	}

	foreach(SIngressKey Key, OldMap.keys()) {
		SIngressItemPtr pOld = m_IngressMap.take(Key);
		if(!pOld.isNull())
			m_ParentMap.remove(qMakePair(pOld->Parent.toULongLong(), 0));
	}

	QList<QModelIndex> Added = m_pItemModel->Sync(m_IngressMap);

	if (m_pBtnExpand->isChecked()) 
	{
		QTimer::singleShot(10, this, [this, Added]() {
			foreach(const QModelIndex & Index, Added)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
			});
	}
}

/*void CIngressView::OnDoubleClicked(const QModelIndex& Index)
{
	//QPair<CProgramItemPtr, CTrafficEntryPtr> pItem = m_pItemModel->GetItem(Index);
}*/

void CIngressView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}

void CIngressView::OnCleanUpDone()
{
	// refresh
	m_CurPrograms.clear();
	m_CurServices.clear();
}

