#include "pch.h"
#include "IngressView.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"

CIngressView::CIngressView(QWidget *parent)
	:CPanelViewEx<CIngressModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
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

	m_pCmbOperation = new QComboBox();
	AddColoredComboBoxEntry(m_pCmbOperation, tr("All Operations"), CExecLogEntry::GetAccessColor(EProcAccessClass::eNone), (qint32)EProcAccessClass::eNone);
	AddColoredComboBoxEntry(m_pCmbOperation, tr("Control Execution"), CExecLogEntry::GetAccessColor(EProcAccessClass::eControlExecution), (qint32)EProcAccessClass::eControlExecution);
	AddColoredComboBoxEntry(m_pCmbOperation, tr("Change Permissions"), CExecLogEntry::GetAccessColor(EProcAccessClass::eChangePermissions), (qint32)EProcAccessClass::eChangePermissions);
	AddColoredComboBoxEntry(m_pCmbOperation, tr("Write Memory"), CExecLogEntry::GetAccessColor(EProcAccessClass::eWriteMemory), (qint32)EProcAccessClass::eWriteMemory);
	AddColoredComboBoxEntry(m_pCmbOperation, tr("Read Memory"), CExecLogEntry::GetAccessColor(EProcAccessClass::eReadMemory), (qint32)EProcAccessClass::eReadMemory);
	AddColoredComboBoxEntry(m_pCmbOperation, tr("Special Permissions"), CExecLogEntry::GetAccessColor(EProcAccessClass::eSpecial), (qint32)EProcAccessClass::eSpecial);
	AddColoredComboBoxEntry(m_pCmbOperation, tr("Write Properties"), CExecLogEntry::GetAccessColor(EProcAccessClass::eWriteProperties), (qint32)EProcAccessClass::eWriteProperties);
	AddColoredComboBoxEntry(m_pCmbOperation, tr("Read Properties"), CExecLogEntry::GetAccessColor(EProcAccessClass::eReadProperties), (qint32)EProcAccessClass::eReadProperties);
	m_pCmbOperation->setMinimumHeight(24);
	m_pToolBar->addWidget(m_pCmbOperation);

	m_pBtnPrivate = new QToolButton();
	m_pBtnPrivate->setIcon(QIcon(":/Icons/Invisible.png"));
	m_pBtnPrivate->setCheckable(true);
	m_pBtnPrivate->setToolTip(tr("Show Private Entries"));
	m_pBtnPrivate->setMaximumHeight(22);
	connect(m_pBtnPrivate, SIGNAL(clicked()), this, SLOT(OnRefresh()));
	m_pToolBar->addWidget(m_pBtnPrivate);

	m_pBtnAll = new QToolButton();
	m_pBtnAll->setIcon(QIcon(":/Icons/all.png"));
	m_pBtnAll->setCheckable(true);
	m_pBtnAll->setToolTip(tr("Show entries per UPID"));
	m_pBtnAll->setMaximumHeight(22);
	connect(m_pBtnAll, SIGNAL(clicked()), this, SLOT(OnRefresh()));
	m_pToolBar->addWidget(m_pBtnAll);

	m_pToolBar->addSeparator();

	m_pBtnHold = new QToolButton();
	m_pBtnHold->setIcon(QIcon(":/Icons/Hold.png"));
	m_pBtnHold->setCheckable(true);
	m_pBtnHold->setToolTip(tr("Hold updates"));
	m_pBtnHold->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnHold);

	m_pBtnRefresh = new QToolButton();
	m_pBtnRefresh->setIcon(QIcon(":/Icons/Refresh.png"));
	m_pBtnRefresh->setToolTip(tr("Reload"));
	m_pBtnRefresh->setFixedHeight(22);
	m_pBtnRefresh->setShortcut(QKeySequence::fromString("F5"));
	connect(m_pBtnRefresh, SIGNAL(clicked()), this, SLOT(OnRefresh()));
	m_pToolBar->addWidget(m_pBtnRefresh);

	m_pToolBar->addSeparator();
	m_pBtnClear = new QToolButton();
	m_pBtnClear->setIcon(QIcon(":/Icons/Trash.png"));
	m_pBtnClear->setToolTip(tr("Clear Records"));
	m_pBtnClear->setFixedHeight(22);
	connect(m_pBtnClear, SIGNAL(clicked()), this, SLOT(OnClearRecords()));
	m_pToolBar->addWidget(m_pBtnClear);

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

void CIngressView::OnRefresh()
{
	m_FullRefresh = true;
}

void CIngressView::Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, const QFlexGuid& EnclaveGuid)
{
	qint32 FilterRole = m_pCmbRole->currentData().toInt();
	qint32 FilterAction = m_pCmbAction->currentData().toInt();
	qint32 FilterOperation = m_pCmbOperation->currentData().toInt();

	if (m_CurPrograms != Programs || m_CurEnclaveGuid != EnclaveGuid || m_CurServices != Services || m_FullRefresh
	 || m_FilterRole != FilterRole 
	 || m_FilterAction != FilterAction 
	 || m_FilterOperation != FilterOperation
	 || m_FilterPrivate != !m_pBtnPrivate->isChecked()
	 || m_FilterAll != m_pBtnAll->isChecked()
	 || m_RecentLimit != theGUI->GetRecentLimit()) 
	{
		m_CurPrograms = Programs;
		m_CurServices = Services;
		m_CurEnclaveGuid = EnclaveGuid;
		m_RecentLimit = theGUI->GetRecentLimit();
		m_IngressMap.clear();
		m_pTreeView->collapseAll();
		m_pItemModel->Clear();
		m_FullRefresh = false;
		m_RefreshCount++;
	}
	else if(m_pBtnHold->isChecked() /*&& !theGUI->m_IgnoreHold*/)
		return;

	quint64 uRecentLimit = m_RecentLimit ? QDateTime::currentMSecsSinceEpoch() - m_RecentLimit : 0;

	m_FilterRole = FilterRole;
	m_FilterAction = FilterAction;
	m_FilterOperation = FilterOperation;
	m_FilterPrivate = !m_pBtnPrivate->isChecked();
	m_FilterAll = m_pBtnAll->isChecked();

	auto OldMap = m_IngressMap;

	std::function<void(const CProgramFilePtr&, const CProgramFilePtr&, const CProgramFile::SIngressInfo&)> AddEntry = 
		[&](const CProgramFilePtr& pProg1, const CProgramFilePtr& pProg2, const CProgramFile::SIngressInfo& Info) {

		SIngressItemPtr& pItem = m_IngressMap[qMakePair((uint64)pProg1.get(), 0)];
		if(pItem.isNull())
		{
			pItem = SIngressItemPtr(new SIngressItem());
			pItem->ID = QString("%1_%2").arg((uint64)pProg1.get()).arg(0);
			pItem->pProg1 = pProg1;
		}
		else
			OldMap.remove(qMakePair((uint64)pProg1.get(), 0));


		quint64 SubID = (m_FilterAll && Info.SubjectPID) ? Info.SubjectPID : -1;
		if (SubID == -1) {
			SubID = (uint64)pProg2.get();
			if (Info.bBlocked)
				SubID |= 0x8000000000000000;
		}
		//SIngressItemPtr pSubItem = OldMap.take(qMakePair((uint64)pProg1.get(), (uint64)pProg2.get()));
		SIngressItemPtr& pSubItem = m_IngressMap[qMakePair((uint64)pProg1.get(), SubID)];
		if (pSubItem.isNull()) {
			pSubItem = SIngressItemPtr(new SIngressItem());
			pSubItem->ID = QString("%1_%2").arg((uint64)pProg1.get()).arg(SubID);
			pSubItem->pProg2 = pProg2;
			pSubItem->Parent = QString("%1_%2").arg((uint64)pProg1.get()).arg(0);
			//m_IngressMap.insert(qMakePair((uint64)pProg1.get(), (uint64)pProg2.get()), pSubItem);
			pSubItem->Info = Info;
		}
		else {
			OldMap.remove(qMakePair((uint64)pProg1.get(), SubID));
			if (pSubItem->Info.LastAccessTime < Info.LastAccessTime)
				pSubItem->Info.LastAccessTime = Info.LastAccessTime;
			pSubItem->Info.ProcessAccessMask |= Info.ProcessAccessMask;
			pSubItem->Info.ThreadAccessMask |= Info.ThreadAccessMask;
		}
	};

	auto FilterEntry = [&](const CProgramFile::SIngressInfo& Info) -> bool {
		if (m_FilterRole && Info.Role != (EExecLogRole)m_FilterRole)
			return false;
		if (m_FilterAction != -1 && (qint32)(Info.bBlocked ? EEventStatus::eBlocked : EEventStatus::eAllowed) != m_FilterAction)
			return false;
		if (m_FilterOperation)
		{
			EProcAccessClass ProcessClass = CExecLogEntry::ClassifyProcessAccess(Info.ProcessAccessMask);
			EProcAccessClass ThreadClass = CExecLogEntry::ClassifyThreadAccess(Info.ThreadAccessMask);

			EProcAccessClass AccessClass = (EProcAccessClass)std::max((int)ProcessClass, (int)ThreadClass);
			if ((qint32)AccessClass < m_FilterOperation)
				return false;
		}
		if (uRecentLimit && FILETIME2ms(Info.LastAccessTime) < uRecentLimit)
			return false;
		if (!m_CurEnclaveGuid.IsNull() && Info.EnclaveGuid != m_CurEnclaveGuid)
			return false;
		return true;
	};

	auto FilterPrivate = [](CProgramFilePtr pProgram2, QString ServiceTag) -> bool {
		// todo: use service tag
		if(pProgram2->GetExecTrace() == ETracePreset::ePrivate)
			return false;
		return true;
	};

	CProgressDialogHelper ProgressHelper(theGUI->m_pProgressDialog, tr("Loading %1"), Programs.count() + Services.count());

	foreach(const CProgramFilePtr & pProgram, Programs) {

		if (!ProgressHelper.Next(pProgram->GetName())) {
			m_pBtnHold->setChecked(true);
			break;
		}

		QMap<quint64, CProgramFile::SIngressInfo> Log = pProgram->GetIngressStats();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			if (!FilterEntry(I.value()))
				continue;
			CProgramFilePtr pProgram2 = theCore->ProgramManager()->GetProgramByUID(I.value().SubjectUID).objectCast<CProgramFile>();
			if (!pProgram2) continue;
			if (m_FilterPrivate && I.value().Role == EExecLogRole::eActor && !FilterPrivate(pProgram2, I.value().ActorSvcTag))
				continue;
			AddEntry(pProgram, pProgram2, I.value());
		}
	}
	foreach(const CWindowsServicePtr& pService, Services) {

		if (!ProgressHelper.Next(pService->GetName())) {
			m_pBtnHold->setChecked(true);
			break;
		}

		QMap<quint64, CProgramFile::SIngressInfo> Log = pService->GetIngressStats();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			if (!FilterEntry(I.value()))
				continue;
			CProgramFilePtr pProgram2 = theCore->ProgramManager()->GetProgramByUID(I.value().SubjectUID).objectCast<CProgramFile>();
			if (!pProgram2) continue;
			if (m_FilterPrivate && I.value().Role == EExecLogRole::eActor && !FilterPrivate(pProgram2, I.value().ActorSvcTag))
				continue;
			CProgramFilePtr pProgram = pService->GetProgramFile();
			if (!pProgram) continue;
			AddEntry(pProgram, pProgram2, I.value());
		}
	}

	if (ProgressHelper.Done()) {
		if (/*!theGUI->m_IgnoreHold &&*/ ++m_SlowCount == 3) {
			m_SlowCount = 0;
			m_pBtnHold->setChecked(true);
		}
	} else
		m_SlowCount = 0;

	foreach(SIngressKey Key, OldMap.keys())
		m_IngressMap.remove(Key);

	QList<QModelIndex> Added = m_pItemModel->Sync(m_IngressMap);

	if (m_pBtnExpand->isChecked()) 
	{
		int CurCount = m_RefreshCount;
		QTimer::singleShot(10, this, [this, Added, CurCount]() {
			if(CurCount != m_RefreshCount)
				return; // ignore if refresh was called again
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
	m_FullRefresh = true;
}

void CIngressView::OnClearRecords()
{
	auto Current = theGUI->GetCurrentItems();

	if (Current.bAllPrograms)
	{
		if (QMessageBox::warning(this, "MajorPrivacy", tr("Are you sure you want to clear the all Execution and Ingress records for ALL program items?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;

		theCore->ClearRecords(ETraceLogs::eExecLog);
	}
	else
	{
		if (QMessageBox::question(this, "MajorPrivacy", tr("Are you sure you want to clear the all Execution and Ingress records for the current program items?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		foreach(CProgramFilePtr pProgram, Current.ProgramsEx | Current.ProgramsIm)
			theCore->ClearRecords(ETraceLogs::eExecLog, pProgram);

		foreach(CWindowsServicePtr pService, Current.ServicesEx | Current.ServicesIm)
			theCore->ClearRecords(ETraceLogs::eExecLog, pService);
	}

	OnCleanUpDone();
}

