#include "pch.h"
#include "TrafficView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Network/NetworkManager.h"
#include "../MajorPrivacy.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../MiscHelpers/Common/ComboInputDialog.h"

CTrafficView::CTrafficView(QWidget *parent)
	:CPanelViewEx<CTrafficModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	connect(m_pItemModel, SIGNAL(CheckChanged(const QModelIndex&, bool)), this, SLOT(OnCheckChanged(const QModelIndex&, bool)));

	m_pBlockFW = m_pMenu->addAction(QIcon(":/Icons/Wall2.png"), tr("Block Program in Firewall"), this, SLOT(OnItemAction()));
	m_pFilterDNS = m_pMenu->addAction(QIcon(":/Icons/Disable.png"), tr("Add Domain to DNS Blocklist"), this, SLOT(OnItemAction()));

	QByteArray Columns = theConf->GetBlob("MainWindow/TrafficView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbGrouping = new QComboBox();
	m_pCmbGrouping->addItem(QIcon(":/Icons/Internet.png"), tr("By Host"));
	m_pCmbGrouping->addItem(QIcon(":/Icons/Process.png"), tr("By Program"));
	m_pToolBar->addWidget(m_pCmbGrouping);

	m_pToolBar->addSeparator();
	m_pAreaFilter = new QToolButton();
	m_pAreaFilter->setIcon(QIcon(":/Icons/ActivityFilter.png"));
	m_pAreaFilter->setToolTip(tr("Area Filter"));
	//m_pAreaFilter->setText(tr(""));
	m_pAreaFilter->setCheckable(true);
	connect(m_pAreaFilter, SIGNAL(clicked()), this, SLOT(OnAreaFilter()));
	m_pAreaMenu = new QMenu();
		m_pInternet = m_pAreaMenu->addAction(tr("Internet"), this, SLOT(OnAreaFilter()));
		m_pInternet->setCheckable(true);
		m_pLocalArea = m_pAreaMenu->addAction(tr("Local Area"), this, SLOT(OnAreaFilter()));
		m_pLocalArea->setCheckable(true);
		m_pLocalHost = m_pAreaMenu->addAction(tr("Local Host"), this, SLOT(OnAreaFilter()));
		m_pLocalHost->setCheckable(true);
	m_pAreaFilter->setPopupMode(QToolButton::MenuButtonPopup);
	m_pAreaFilter->setMenu(m_pAreaMenu);
	m_pAreaFilter->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pAreaFilter);

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

CTrafficView::~CTrafficView()
{
	theConf->SetBlob("MainWindow/TrafficView_Columns", m_pTreeView->saveState());
}

void CTrafficView::OnAreaFilter()
{
	if (sender() == m_pAreaFilter)
	{
		if (m_pAreaFilter->isChecked())
		{
			m_AreaFilter = theConf->GetInt("Options/DefaultAreaFilter", 0);
			if(m_AreaFilter == 0)
				m_AreaFilter = CTrafficEntry::eInternet;
		}
		else
		{
			theConf->SetValue("Options/DefaultAreaFilter", m_AreaFilter);
			m_AreaFilter = 0;
		}

		m_pInternet->setChecked((m_AreaFilter & CTrafficEntry::eInternet) != 0);
		m_pLocalArea->setChecked((m_AreaFilter & CTrafficEntry::eLocalAreaEx) == CTrafficEntry::eLocalAreaEx);
		m_pLocalHost->setChecked((m_AreaFilter & CTrafficEntry::eLocalHost) != 0);
	}
	else
	{
		m_AreaFilter = 0;
		if (m_pInternet->isChecked())
			m_AreaFilter |= CTrafficEntry::eInternet;
		if (m_pLocalArea->isChecked())
			m_AreaFilter |= CTrafficEntry::eLocalAreaEx;
		if (m_pLocalHost->isChecked())
			m_AreaFilter |= CTrafficEntry::eLocalHost;

		m_pAreaFilter->setChecked(m_AreaFilter != 0);
	}

	m_CurPrograms.clear();
	m_CurServices.clear();
}

void CTrafficView::OnRefresh()
{
	foreach(const CProgramFilePtr& pProgram, m_CurPrograms)
		pProgram->ClearTrafficLog();

	foreach(const CWindowsServicePtr& pService, m_CurServices)
		pService->ClearTrafficLog();

	m_FullRefresh = true;
}

void CTrafficView::Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services)
{
	bool bGroupByProgram = m_pCmbGrouping->currentIndex() == 1;

	if (m_CurPrograms != Programs || m_CurServices != Services || m_FullRefresh || bGroupByProgram != m_bGroupByProgram || m_RecentLimit != theGUI->GetRecentLimit()) {
		m_CurPrograms = Programs;
		m_CurServices = Services;
		m_RecentLimit = theGUI->GetRecentLimit();
		m_ParentMap.clear();
		m_TrafficMap.clear();
		m_pTreeView->collapseAll();
		m_pItemModel->Clear();
		m_FullRefresh = false;
		m_RefreshCount++;
	}
	else if(m_pBtnHold->isChecked() /*&& !theGUI->m_IgnoreHold*/)
		return;

	quint64 uRecentLimit = m_RecentLimit ? QDateTime::currentMSecsSinceEpoch() - m_RecentLimit : 0;

	m_bGroupByProgram = bGroupByProgram;

	auto OldTraffic = m_TrafficMap;

	foreach(auto& pItem, m_ParentMap)
		pItem->pEntry->Reset();

	std::function<void(const CProgramItemPtr&, const CTrafficEntryPtr&)> AddByDomain = [&](const CProgramItemPtr& pProg, const CTrafficEntryPtr& pEntry) {

		STrafficItemPtr& pItem = m_ParentMap[pEntry->GetHostName()];
		if(pItem.isNull())
		{
			pItem = STrafficItemPtr(new STrafficItem());
			pItem->pEntry = CTrafficEntryPtr(new CTrafficEntry());
			pItem->pEntry->SetHostName(pEntry->GetHostName());
			pItem->pEntry->SetIpAddress(pEntry->GetIpAddress());
			m_TrafficMap.insert((uint64)pItem->pEntry.get(), pItem);
		}
		else
			OldTraffic.remove((uint64)pItem->pEntry.get());

		STrafficItemPtr pSubItem = OldTraffic.take((uint64)pEntry.get());
		if (!pSubItem) {
			pSubItem = STrafficItemPtr(new STrafficItem());
			pSubItem->pEntry = pEntry;	
			pSubItem->pProg = pProg;
			pSubItem->Parent = (uint64)pItem->pEntry.get();
			m_TrafficMap.insert((uint64)pSubItem->pEntry.get(), pSubItem);
		}

		pItem->pEntry->Merge(pEntry);
	};

	std::function<void(const CProgramItemPtr&, const CTrafficEntryPtr&)> AddByProgram = [&](const CProgramItemPtr& pProg, const CTrafficEntryPtr& pEntry) {

		STrafficItemPtr& pItem = m_ParentMap[QString::number((quint64)pProg.get())];
		if(pItem.isNull())
		{
			pItem = STrafficItemPtr(new STrafficItem());
			pItem->pEntry = CTrafficEntryPtr(new CTrafficEntry());
			pItem->pProg = pProg;
			pItem->pEntry->SetHostName(pEntry->GetHostName());
			pItem->pEntry->SetIpAddress(pEntry->GetIpAddress());
			m_TrafficMap.insert((uint64)pItem->pEntry.get(), pItem);
		}
		else
			OldTraffic.remove((uint64)pItem->pEntry.get());

		STrafficItemPtr pSubItem = OldTraffic.take((uint64)pEntry.get());
		if (!pSubItem) {
			pSubItem = STrafficItemPtr(new STrafficItem());
			pSubItem->pEntry = pEntry;	
			pSubItem->Parent = (uint64)pItem->pEntry.get();
			m_TrafficMap.insert((uint64)pSubItem->pEntry.get(), pSubItem);
		}

		pItem->pEntry->Merge(pEntry);
	};

	CProgressDialogHelper ProgressHelper(theGUI->m_pProgressDialog, tr("Loading %1"), Programs.count() + Services.count());

	foreach(const CProgramFilePtr& pProgram, Programs) {

		if (!ProgressHelper.Next(pProgram->GetName())) {
			m_pBtnHold->setChecked(true);
			break;
		}

		QHash<QString, CTrafficEntryPtr> Log = pProgram->GetTrafficLog();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			if (uRecentLimit && I.value()->GetLastActivity() < uRecentLimit)
				continue;
			if (m_AreaFilter && (m_AreaFilter & I.value()->GetNetType()) == 0)
				continue;
			if (m_bGroupByProgram)
				AddByProgram(pProgram, I.value());
			else
				AddByDomain(pProgram, I.value());
		}
	}
	foreach(const CWindowsServicePtr& pService, Services) {

		if (!ProgressHelper.Next(pService->GetName())) {
			m_pBtnHold->setChecked(true);
			break;
		}

		QHash<QString, CTrafficEntryPtr> Log = pService->GetTrafficLog();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			if (uRecentLimit && I.value()->GetLastActivity() < uRecentLimit)
				continue;
			if (m_AreaFilter && (m_AreaFilter & I.value()->GetNetType()) == 0)
				continue;
			if (m_bGroupByProgram)
				AddByProgram(pService, I.value());
			else
				AddByDomain(pService, I.value());
		}
	}

	if (ProgressHelper.Done()) {
		if (/*!theGUI->m_IgnoreHold &&*/ ++m_SlowCount == 3) {
			m_SlowCount = 0;
			m_pBtnHold->setChecked(true);
		}
	} else
		m_SlowCount = 0;

	foreach(quint64 Key, OldTraffic.keys()) {
		STrafficItemPtr pOld = m_TrafficMap.take(Key);
		if(pOld->pProg.isNull())
			m_ParentMap.remove(pOld->pEntry->GetHostName());
	}

	QList<QModelIndex> Added = m_pItemModel->Sync(m_TrafficMap);

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

void CTrafficView::Clear()
{
	m_CurPrograms.clear();
	m_CurServices.clear();
	m_ParentMap.clear();
	m_TrafficMap.clear();
	m_pItemModel->Clear();
}

/*void CTrafficView::OnDoubleClicked(const QModelIndex& Index)
{
	//QPair<CProgramItemPtr, CTrafficEntryPtr> pItem = m_pItemModel->GetItem(Index);
}*/

void CTrafficView::OnMenu(const QPoint& Point)
{
	auto Items = GetSelectedItems();

	int iEntries = 0;
	int iEntriesEx = 0;
	int iPrograms = 0;
	foreach(const STrafficItemPtr& pItem, Items)
	{
		if (pItem->pProg)
			iPrograms++;
		else //if (pItem->pEntry)
		{
			if(pItem->pEntry->GetHostName().startsWith("["))
				continue; // not resolved IP only
			if(pItem->pEntry->GetHostName().contains("("))
				iEntriesEx++;
			iEntries++;
		}
	}

	m_pBlockFW->setEnabled(iPrograms > 0);
	m_pFilterDNS->setEnabled(iEntries > 0 && iEntriesEx <= 1);

	CPanelView::OnMenu(Point);
}

void CTrafficView::OnItemAction()
{
	auto Items = GetSelectedItems();

	QList<STATUS> Results;
	if(sender() == m_pBlockFW)
	{
		foreach(const STrafficItemPtr & pItem, Items)
		{
			if (!pItem->pProg)
				continue;
			
			CFwRulePtr pRule = CFwRulePtr(new CFwRule(pItem->pProg->GetID()));
			pRule->SetApproved();
			pRule->SetSource(EFwRuleSource::eMajorPrivacy);
			pRule->SetEnabled(true);
			pRule->SetName(tr("MajorPrivacy-Rule, %1").arg(pItem->pProg->GetNameEx()));
			pRule->SetProfile((int)EFwProfiles::All);
			pRule->SetProtocol(EFwKnownProtocols::Any);
			pRule->SetInterface((int)EFwInterfaces::All);
			pRule->SetAction(EFwActions::Block);
			pRule->SetDirection(EFwDirections::Outbound);
			Results << theCore->NetworkManager()->SetFwRule(pRule);
		}
	}
	else if(sender() == m_pFilterDNS)
	{
		if(!theCore->GetConfigBool("Service/DnsEnableFilter", false))
		{
			QMessageBox::critical(this, "MajorPrivacy", tr("DNS filtering is not enabled!"), QMessageBox::Ok);
			return;
		}

		foreach(const STrafficItemPtr& pItem, Items)
		{
			if (pItem->pProg)
				continue;

			QString HostName = pItem->pEntry->GetHostName();
			if (HostName.contains("("))
			{
				QStringList HostNames = SplitStr(Split2(Split2(HostName, "(").second, ")").first, "|");

				CComboInputDialog progDialog(this);
				progDialog.setText(tr("Select host name to block:"));
				progDialog.setEditable(true);

				foreach(const QString & Name, HostNames)
					progDialog.addItem(Name, Name);

				progDialog.setValue(HostNames[0]);

				if (!progDialog.exec())
					continue;

				QString HostName = progDialog.value(); 
				int Index = progDialog.findValue(HostName);
				if (Index != -1 && progDialog.data().isValid())
					HostName = progDialog.data().toString();
			}

			if (HostName.isEmpty())
				continue;

			CDnsRulePtr pRule = CDnsRulePtr(new CDnsRule());
			pRule->SetAction(CDnsRule::EAction::eBlock);
			pRule->SetHostName(HostName);
			Results << theCore->NetworkManager()->SetDnsRule(pRule);
		}
	}
	theGUI->CheckResults(Results, this);
}

void CTrafficView::OnCleanUpDone()
{
	// refresh
	m_CurPrograms.clear();
	m_CurServices.clear();
	m_FullRefresh = true;
}

void CTrafficView::OnClearRecords()
{
	auto Current = theGUI->GetCurrentItems();

	if (Current.bAllPrograms)
	{
		if (QMessageBox::warning(this, "MajorPrivacy", tr("Are you sure you want to clear the all execution and Network records for the ALL program items?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;

		theCore->ClearRecords(ETraceLogs::eNetLog);
	}
	else
	{
		if (QMessageBox::question(this, "MajorPrivacy", tr("Are you sure you want to clear the all execution and Network records for the current program items?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		foreach(CProgramFilePtr pProgram, Current.ProgramsEx | Current.ProgramsIm)
			theCore->ClearRecords(ETraceLogs::eNetLog, pProgram);

		foreach(CWindowsServicePtr pService, Current.ServicesEx | Current.ServicesIm)
			theCore->ClearRecords(ETraceLogs::eNetLog, pService);
	}

	OnCleanUpDone();
}