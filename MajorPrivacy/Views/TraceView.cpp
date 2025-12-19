#include "pch.h"
#include "TraceView.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include "..\..\MiscHelpers\Common\Common.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../Library/Common/DbgHelp.h"
#include "../Models/TraceModel.h"

CTraceView::CTraceView(CTraceModel* pModel, QWidget *parent)
	:CPanelView(parent)
{
	m_FullRefresh = true;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	m_pTreeView = new QTreeViewEx();
	m_pMainLayout->addWidget(m_pTreeView);

	m_pItemModel = pModel;
	//m_pItemModel->SetTree(false);

	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	m_pTreeView->setMinimumHeight(50);

	m_pTreeView->setModel(m_pItemModel);

	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTreeView, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	//m_pMainLayout->addWidget(CFinder::AddFinder(m_pTreeList, this, CFinder::eHighLightDefault, &m_pFinder));
	m_pMainLayout->addWidget(CFinder::AddFinder(m_pTreeView, this, CFinder::eHighLightDefault, &m_pFinder));
	m_pFinder->SetModel(m_pItemModel);

	m_pTreeView->setColumnReset(2);
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	AddPanelItemsToMenu();

	connect(theCore, SIGNAL(CleanUpDone()), this, SLOT(OnCleanUpDone()));
}

CTraceView::~CTraceView()
{
}

void CTraceView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}

void CTraceView::SetFilter(const QRegularExpression& RegExp, int iOptions, int Column)
{
	QString Pattern = RegExp.pattern();
	QString Exp;
	if (Pattern.length() > 4) {
		Exp = Pattern.mid(2, Pattern.length() - 4); // remove leading and tailing ".*" from the pattern
		// unescape, remove backslashes but not double backslashes
		for (int i = 0; i < Exp.length(); i++) {
			if (Exp.at(i) == '\\')
				Exp.remove(i, 1);
		}
	}
	bool bReset = m_bHighLight != ((iOptions & CFinder::eHighLight) != 0) || (!m_bHighLight && m_FilterExp != Exp);

	//QString ExpStr = ((iOptions & CFinder::eRegExp) == 0) ? Exp : (".*" + QRegularExpression::escape(Exp) + ".*");
	//QRegularExpression RegExp(ExpStr, (iOptions & CFinder::eCaseSens) != 0 ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
	//m_FilterExp = RegExp;
	m_FilterExp = Exp;
	m_bHighLight = (iOptions & CFinder::eHighLight) != 0;
	//m_FilterCol = Col;

	if (bReset)
		m_FullRefresh = true;
}

void CTraceView::Sync(ETraceLogs Log, const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, const QFlexGuid& EnclaveGuid)
{
	bool bReset = false;
	if (m_CurPrograms != Programs || m_CurServices != Services || m_EnclaveGuid != EnclaveGuid)
	{
		m_CurPrograms = Programs;
		m_CurServices = Services;
		m_EnclaveGuid = EnclaveGuid;

		bReset = (m_EnclaveGuid != EnclaveGuid);
	}
	else if(m_pBtnHold && m_pBtnHold->isChecked())
		return;

	QList<QPair<QVector<CLogEntryPtr>, CProgramItemPtr>> AllEntries;

#ifdef _DEBUG
	uint64 start = GetUSTickCount();
#endif

	CProgressDialogHelper ProgressHelper(tr("Loading %1"), Programs.count() + Services.count(), theGUI);

	foreach(const CProgramFilePtr& pProgram, Programs) 
	{
		SMergedLog::SLogState& State = m_Log.States[pProgram];

		if (!ProgressHelper.Next(pProgram->GetName())) {
			if(m_pBtnHold) m_pBtnHold->setChecked(true);
			break;
		}
		const QVector<CLogEntryPtr>& Entries = pProgram->GetTraceLog(Log)->Entries;
		AllEntries.append(qMakePair(Entries, pProgram));
	}

	foreach(const CWindowsServicePtr& pService, Services) 
	{
		SMergedLog::SLogState& State = m_Log.States[pService];

		CProgramFilePtr pProgram = pService->GetProgramFile();
		if (!pProgram) continue;

		if(!ProgressHelper.Next(pService->GetName())) {
			if(m_pBtnHold) m_pBtnHold->setChecked(true);
			break;
		}
		const QVector<CLogEntryPtr>& Entries = pProgram->GetTraceLog(Log)->Entries;
		AllEntries.append(qMakePair(Entries, pService));
	}

	if (ProgressHelper.Done()) {
		if (/*!theGUI->m_IgnoreHold &&*/ ++m_SlowCount == 3) {
			m_SlowCount = 0;
			m_pBtnHold->setChecked(true);
		}
	} else
		m_SlowCount = 0;

#ifdef _DEBUG
	quint64 elapsed = (GetUSTickCount() - start) / 1000;
	if (elapsed > 100)
		DbgPrint("Loading Tracelogs %d took %d ms\r\n", Log, elapsed);
#endif

	if (!m_Log.States.isEmpty()) {
		if (Programs.count() + Services.count() != m_Log.States.count())
			bReset = true;
		else
		{
			foreach(const auto& Data, AllEntries) {
				SMergedLog::SLogState& State = m_Log.States[Data.second];
				if(Data.first.count() < State.LastCount) {
					bReset = true;
					break;
				}
			}
		}
	}
	if (bReset) {
		m_Log.MergeSeqNr++;
		m_Log.LastCount = 0;
		m_Log.States.clear();
		m_Log.List.clear();
	}

	//auto AddToLog = [&](const QVector<CLogEntryPtr>& Entries, SMergedLog::SLogState& State, const CProgramFilePtr& pProgram, const QString& FilterTag = "") {
	//
	//	int i = State.LastCount;
	//
	//	//
	//	// We insert a sorted list into an other sorted list,
	//	// we know that all new entries will be added after the last entry from the previouse merge
	//	//
	//
	//	int j = m_Log.LastCount;
	//
	//	for (; i < Entries.count(); i++)
	//	{
	//		CLogEntryPtr pEntry = Entries.at(i);
	//
	//		if (FilterTag.isEmpty() || pEntry->GetOwnerService().compare(FilterTag, Qt::CaseInsensitive) == 0)
	//		{
	//			// Find the correct position using binary search
	//			int low = j, high = m_Log.List.count();
	//			while (low < high) {
	//				int mid = low + (high - low) / 2;
	//				if (m_Log.List.at(mid).second->GetTimeStamp() > pEntry->GetTimeStamp())
	//					high = mid;
	//				else
	//					low = mid + 1;
	//			}
	//			j = low;
	//
	//			m_Log.List.insert(j++, qMakePair(pProgram, pEntry));
	//		}
	//	}
	//
	//	State.LastCount = Entries.count();
	//};


#ifdef _DEBUG
	start = GetUSTickCount();
#endif

	QMultiMap<quint64, SMergedLog::TLogEntry> Map;

	auto AddToLog = [&](const QVector<CLogEntryPtr>& Entries, SMergedLog::SLogState& State, const CProgramFilePtr& pProgram, const QString& FilterTag = "") {

		int i = State.LastCount;

		//
		// We insert a sorted list into an other sorted list,
		// we know that all new entries will be added after the last entry from the previouse merge
		//

		int j = m_Log.LastCount;

		for (; i < Entries.count(); i++)
		{
			CLogEntryPtr pEntry = Entries.at(i);

			if (!FilterTag.isEmpty() && pEntry->GetOwnerService().compare(FilterTag, Qt::CaseInsensitive) != 0)
				continue;

			if (!EnclaveGuid.IsNull() && pEntry->GetEnclaveGuid() != EnclaveGuid)
				continue;
			
			Map.insert(pEntry->GetTimeStamp(), qMakePair(pProgram, pEntry));
		}

		State.LastCount = Entries.count();
	};

	foreach(const auto& Data, AllEntries) {
		SMergedLog::SLogState& State = m_Log.States[Data.second];
		if(CProgramFilePtr pProgram = Data.second.objectCast<CProgramFile>())
			AddToLog(Data.first, State, pProgram);
		else if (CWindowsServicePtr pService = Data.second.objectCast<CWindowsService>())
			if(CProgramFilePtr pProgram = pService->GetProgramFile())
				AddToLog(Data.first, State, pService->GetProgramFile(), pService->GetServiceTag());
	}

	m_Log.List.append(Map.values());

	m_Log.LastCount = m_Log.List.count();

#ifdef _DEBUG
	elapsed = (GetUSTickCount() - start) / 1000;
	if (elapsed > 100)
		DbgPrint("Merging Tracelogs %d took %d ms\r\n", Log, (GetUSTickCount() - start) / 1000);
#endif


	if (m_FullRefresh || m_RecentLimit != theGUI->GetRecentLimit()) 
	{
		//quint64 start = GetUSTickCount();
		m_pItemModel->Clear();
		//qDebug() << "Clear took" << (GetUSTickCount() - start) / 1000000.0 << "s";

		m_RecentLimit = theGUI->GetRecentLimit();

		m_FullRefresh = false;
	}

	m_pItemModel->SetTextFilter(m_FilterExp, m_bHighLight);	

	quint64 uRecentLimit = m_RecentLimit ? QDateTime::currentMSecsSinceEpoch() - m_RecentLimit : 0;

	//quint64 start = GetUSTickCount();
	QList<QModelIndex> NewBranches = m_pItemModel->Sync(m_Log.List, uRecentLimit);
	//qDebug() << "Sync took" << (GetUSTickCount() - start) / 1000000.0 << "s";

	/*if (m_pItemModel->IsTree())
	{
		QTimer::singleShot(10, this, [this, NewBranches]() {
			quint64 start = GetUSTickCount();
			foreach(const QModelIndex& Index, NewBranches)
				GetTree()->expand(Index);
			qDebug() << "Expand took" << (GetUSTickCount() - start) / 1000000.0 << "s";
			});
	}

	if(m_pAutoScroll->isChecked())
		m_pTreeList->scrollToBottom();*/

	if(m_pBtnScroll && m_pBtnScroll->isChecked())
		m_pTreeView->scrollToBottom();
}

void CTraceView::Clear()
{
	m_Log.List.clear();
	m_Log.States.clear();
	m_Log.LastCount = 0;
	m_Log.MergeSeqNr = 0;
	m_pItemModel->Clear();
}

void CTraceView::ClearTraceLog(ETraceLogs Log)
{
	auto Current = theGUI->GetCurrentItems();
	
	if (Current.bAllPrograms)
	{
		if (QMessageBox::warning(this, "MajorPrivacy", tr("Are you sure you want to clear the the trace logs for ALL program items?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;

		theCore->ClearTraceLog(Log);
	}
	else
	{
		if (QMessageBox::question(this, "MajorPrivacy", tr("Are you sure you want to clear the the trace logs for the current program items?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		foreach(CProgramFilePtr pProgram, Current.ProgramsEx | Current.ProgramsIm)
			theCore->ClearTraceLog(Log, pProgram);

		foreach(CWindowsServicePtr pService, Current.ServicesEx | Current.ServicesIm)
			theCore->ClearTraceLog(Log, pService);
	}

	emit theCore->CleanUpDone();
}

void CTraceView::OnCleanUpDone()
{
	m_FullRefresh = true;
}