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
	((CSortFilterProxyModel*)m_pSortProxy)->SetShowFilteredHierarchy(true);

	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	//connect(m_pItemModel, SIGNAL(CheckChanged(const QModelIndex&, bool)), this, SLOT(OnCheckChanged(const QModelIndex&, bool)));

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
	m_AreaFilter = theConf->GetInt("Options/TrafficAreaFilter", 0);
	if (m_AreaFilter)
	{
		m_pAreaFilter->setChecked(true);
		m_pInternet->setChecked((m_AreaFilter & CTrafficEntry::eInternet) != 0);
		m_pLocalArea->setChecked((m_AreaFilter & CTrafficEntry::eLocalAreaEx) == CTrafficEntry::eLocalAreaEx);
		m_pLocalHost->setChecked((m_AreaFilter & CTrafficEntry::eLocalHost) != 0);
	}
	m_pToolBar->addWidget(m_pAreaFilter);

	m_pToolBar->addSeparator();
	m_pBtnTree = new QToolButton();
	m_pBtnTree->setIcon(QIcon(":/Icons/Tree.png"));
	m_pBtnTree->setCheckable(true);
	m_pBtnTree->setToolTip(tr("Break Domains into Sub-Branches"));
	m_pBtnTree->setMaximumHeight(22);
	m_pBtnTree->setChecked(theConf->GetBool("Options/SubdomainTree", true));
	connect(m_pBtnTree, &QToolButton::toggled, this, [&](bool checked) {
		theConf->SetValue("Options/SubdomainTree", checked);
		});
	m_pToolBar->addWidget(m_pBtnTree);

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

	theConf->SetValue("Options/TrafficAreaFilter", m_AreaFilter);

	m_CurPrograms.clear();
	m_CurServices.clear();
	m_CachedTrafficMap.clear();
	m_TrafficLogTimestamps.clear();
}

void CTrafficView::OnRefresh()
{
	foreach(const CProgramFilePtr& pProgram, m_CurPrograms)
		pProgram->ClearTrafficLog();

	foreach(const CWindowsServicePtr& pService, m_CurServices)
		pService->ClearTrafficLog();

	m_CachedTrafficMap.clear();
	m_TrafficLogTimestamps.clear();
	m_FullRefresh = true;
}

void CTrafficView::Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services)
{
	bool bGroupByProgram = m_pCmbGrouping->currentIndex() == 1;
	bool bTreeDomains = m_pBtnTree->isChecked();

	// Check if we need to clear cache and force full rebuild
	if (m_CurPrograms != Programs || m_CurServices != Services || m_FullRefresh || bGroupByProgram != m_bGroupByProgram || bTreeDomains != m_bTreeDomains || m_RecentLimit != theGUI->GetRecentLimit()) {
		m_CurPrograms = Programs;
		m_CurServices = Services;
		m_RecentLimit = theGUI->GetRecentLimit();
		m_pTreeView->collapseAll();
		m_pItemModel->Clear();
		m_FullRefresh = false;
		m_RefreshCount++;
		m_CachedTrafficMap.clear();
		m_TrafficLogTimestamps.clear();
	}
	else if(m_pBtnHold->isChecked())
		return;

	//uint64 uStart = GetTickCount64();

	quint64 uRecentLimit = m_RecentLimit ? QDateTime::currentMSecsSinceEpoch() - m_RecentLimit : 0;
	m_bGroupByProgram = bGroupByProgram;
	m_bTreeDomains = bTreeDomains;

	QMap<QVariant, CTrafficEntryPtr> AggregatedEntries; // For aggregating stats
	QMap<QVariant, CTrafficEntry::ENetType> AggregatedNetTypes; // Track network types

	// Helper to extract multiple hostnames from a single entry
	// Handles cases like "(host1 | host2)" or "host1, host2"
	auto ExtractHostNames = [](const QString& hostName) -> QStringList {
		QStringList result;
		QString cleaned = hostName.trimmed();

		// Handle "(host1 | host2 | host3)" format
		if (cleaned.startsWith("(") && cleaned.contains("|")) {
			// Remove parentheses
			cleaned = cleaned.mid(1);
			if (cleaned.endsWith(")"))
				cleaned.chop(1);

			// Split by |
			QStringList parts = cleaned.split("|", Qt::SkipEmptyParts);
			for (const QString& part : parts) {
				QString trimmed = part.trimmed();
				if (!trimmed.isEmpty())
					result.append(trimmed);
			}
		}
		// Handle "host1, host2, host3" format
		else if (cleaned.contains(", ")) {
			QStringList parts = cleaned.split(", ", Qt::SkipEmptyParts);
			for (const QString& part : parts) {
				QString trimmed = part.trimmed();
				if (!trimmed.isEmpty())
					result.append(trimmed);
			}
		}
		// Single hostname
		else {
			result.append(cleaned);
		}

		return result;
	};

	// Helper to get or create aggregated traffic entry
	auto GetOrCreateAggregatedEntry = [&](const QVariant& id) -> CTrafficEntryPtr {
		if (!AggregatedEntries.contains(id)) {
			AggregatedEntries[id] = CTrafficEntryPtr(new CTrafficEntry());
		}
		return AggregatedEntries[id];
	};

	// Helper to merge entry and track network type
	auto MergeEntryWithNetType = [&](const QVariant& id, const CTrafficEntryPtr& aggregated, const CTrafficEntryPtr& source) {
		aggregated->Merge(source);

		// Track the most restrictive network type
		// Priority: LocalHost > LocalArea > Internet
		CTrafficEntry::ENetType sourceType = source->GetNetType();
		if (!AggregatedNetTypes.contains(id)) {
			AggregatedNetTypes[id] = sourceType;
		} else {
			CTrafficEntry::ENetType currentType = AggregatedNetTypes[id];
			// LocalHost is most restrictive
			if (sourceType == CTrafficEntry::eLocalHost) {
				AggregatedNetTypes[id] = CTrafficEntry::eLocalHost;
			}
			// LocalArea types are next
			else if ((sourceType & CTrafficEntry::eLocalAreaEx) && currentType == CTrafficEntry::eInternet) {
				AggregatedNetTypes[id] = sourceType;
			}
		}

		// Set the IP address from the source to ensure NetType is properly set
		if (aggregated->GetIpAddress().isEmpty() && !source->GetIpAddress().isEmpty()) {
			aggregated->SetIpAddress(source->GetIpAddress());
		}
	};

	// Helper to split hostname into domain parts in reverse order
	// For "123.abc.www.domain.com" returns ["domain.com", "www", "abc", "123"]
	auto SplitHostName = [](const QString& hostName) -> QStringList {
		QStringList parts;
		QString name = hostName;

		// Skip if it's an IP address or starts with '['
		if (name.startsWith("["))
			return parts;

		// Simple check for IPv4 address - all segments are numeric
		QStringList segments = name.split(".");
		if (segments.count() == 4) {
			bool isIPv4 = true;
			for (const QString& seg : segments) {
				bool ok;
				seg.toInt(&ok);
				if (!ok) {
					isIPv4 = false;
					break;
				}
			}
			if (isIPv4)
				return parts;
		}

		// Remove port if present
		int portIdx = name.indexOf(":");
		if (portIdx != -1)
			name = name.left(portIdx);

		segments = name.split(".", Qt::SkipEmptyParts);
		if (segments.count() < 2)
			return parts;

		// Build domain parts in reverse order
		// Start with "domain.com"
		if (segments.count() >= 2) {
			parts.append(segments[segments.count() - 2] + "." + segments[segments.count() - 1]);
		}
		// Add subdomain parts in order: www, abc, 123
		for (int i = segments.count() - 3; i >= 0; i--) {
			parts.append(segments[i]);
		}

		return parts;
	};

	// Build hierarchy for Domain-First view
	// Root: domain.com -> www -> abc -> 123 -> programs
	auto BuildDomainHierarchy = [&](const CProgramItemPtr& pProg, const CTrafficEntryPtr& pEntry) {
		QString hostName = pEntry->GetHostName();
		QStringList domainParts = m_bTreeDomains ? SplitHostName(hostName) : QStringList();

		if (domainParts.isEmpty()) {
			// Use raw hostname for IPs or unparseable names
			QString rootID = QString("domain_%1").arg(hostName);

			// Create domain entry and merge stats
			STrafficItemPtr& pDomainItem = m_CachedTrafficMap[rootID];
			if (pDomainItem.isNull()) {
				pDomainItem = STrafficItemPtr(new STrafficItem());
				pDomainItem->pEntry = GetOrCreateAggregatedEntry(rootID);
				pDomainItem->pEntry->SetHostName(hostName);
				pDomainItem->Parent = QVariant();
			}
			MergeEntryWithNetType(rootID, pDomainItem->pEntry, pEntry);

			// Create program leaf
			QString progID = QString("domain_%1_prog_%2").arg(hostName).arg((quint64)pProg.data());
			STrafficItemPtr& pProgItem = m_CachedTrafficMap[progID];
			if (pProgItem.isNull()) {
				pProgItem = STrafficItemPtr(new STrafficItem());
				pProgItem->pProg = pProg;
				pProgItem->pEntry = pEntry;
				pProgItem->Parent = rootID;
			}
		}
		else {
			// Build domain hierarchy
			QVariant parentID;
			QString pathSoFar;
			QString fullDomainSoFar;

			for (int i = 0; i < domainParts.count(); i++) {
				if (i == 0) {
					pathSoFar = domainParts[i];
					fullDomainSoFar = domainParts[i]; // "domain.com"
				}
				else {
					pathSoFar = pathSoFar + "_" + domainParts[i];
					// Build full domain: "www.domain.com", "abc.www.domain.com", etc.
					fullDomainSoFar = domainParts[i] + "." + fullDomainSoFar;
				}

				QString nodeID = QString("domain_%1").arg(pathSoFar);

				STrafficItemPtr& pDomainItem = m_CachedTrafficMap[nodeID];
				if (pDomainItem.isNull()) {
					pDomainItem = STrafficItemPtr(new STrafficItem());
					pDomainItem->pEntry = GetOrCreateAggregatedEntry(nodeID);
					pDomainItem->pEntry->SetHostName(fullDomainSoFar);
					pDomainItem->Parent = parentID;
				}
				MergeEntryWithNetType(nodeID, pDomainItem->pEntry, pEntry);

				parentID = nodeID;
			}

			// Add program as leaf node
			QString progID = QString("domain_%1_prog_%2").arg(pathSoFar).arg((quint64)pProg.data());
			STrafficItemPtr& pProgItem = m_CachedTrafficMap[progID];
			if (pProgItem.isNull()) {
				pProgItem = STrafficItemPtr(new STrafficItem());
				pProgItem->pProg = pProg;
				pProgItem->pEntry = pEntry;
				pProgItem->Parent = parentID;
			}
		}
	};

	// Build hierarchy for Program-First view
	// Root: program -> domain.com -> www -> abc -> 123
	auto BuildProgramHierarchy = [&](const CProgramItemPtr& pProg, const CTrafficEntryPtr& pEntry) {
		// Create program root node
		QString progID = QString("prog_%1").arg((quint64)pProg.data());
		STrafficItemPtr& pProgItem = m_CachedTrafficMap[progID];
		if (pProgItem.isNull()) {
			pProgItem = STrafficItemPtr(new STrafficItem());
			pProgItem->pProg = pProg;
			pProgItem->pEntry = GetOrCreateAggregatedEntry(progID);
			pProgItem->Parent = QVariant();
		}
		MergeEntryWithNetType(progID, pProgItem->pEntry, pEntry);

		QString hostName = pEntry->GetHostName();
		QStringList domainParts = m_bTreeDomains ? SplitHostName(hostName) : QStringList();

		if (domainParts.isEmpty()) {
			// Use raw hostname for IPs
			QString entryID = QString("prog_%1_domain_%2").arg((quint64)pProg.data()).arg(hostName);
			STrafficItemPtr& pDomainItem = m_CachedTrafficMap[entryID];
			if (pDomainItem.isNull()) {
				pDomainItem = STrafficItemPtr(new STrafficItem());
				pDomainItem->pEntry = pEntry;
				pDomainItem->Parent = progID;
			}
		}
		else {
			// Build domain hierarchy under program
			QVariant parentID = progID;
			QString pathSoFar;
			QString fullDomainSoFar;

			for (int i = 0; i < domainParts.count(); i++) {
				if (i == 0) {
					pathSoFar = domainParts[i];
					fullDomainSoFar = domainParts[i]; // "domain.com"
				}
				else {
					pathSoFar = pathSoFar + "_" + domainParts[i];
					// Build full domain: "www.domain.com", "abc.www.domain.com", etc.
					fullDomainSoFar = domainParts[i] + "." + fullDomainSoFar;
				}

				QString nodeID = QString("prog_%1_domain_%2").arg((quint64)pProg.data()).arg(pathSoFar);

				STrafficItemPtr& pDomainItem = m_CachedTrafficMap[nodeID];
				if (pDomainItem.isNull()) {
					pDomainItem = STrafficItemPtr(new STrafficItem());
					pDomainItem->pEntry = GetOrCreateAggregatedEntry(nodeID);
					pDomainItem->pEntry->SetHostName(fullDomainSoFar);
					pDomainItem->Parent = parentID;
				}
				MergeEntryWithNetType(nodeID, pDomainItem->pEntry, pEntry);

				parentID = nodeID;
			}
		}
	};

	// Process all programs and services
	CProgressDialogHelper ProgressHelper(tr("Loading %1"), Programs.count() + Services.count(), theGUI);

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

			// Check if this entry has changed using timestamp
			QString cacheKey = QString("%1_%2").arg((quint64)pProgram.data()).arg(I.key());
			quint64 lastActivity = I.value()->GetLastActivity();

			if (m_TrafficLogTimestamps.contains(cacheKey) &&
				m_TrafficLogTimestamps[cacheKey] == lastActivity) {
				// Entry hasn't changed, skip rebuilding
				continue;
			}

			// Update timestamp
			m_TrafficLogTimestamps[cacheKey] = lastActivity;

			// Extract individual hostnames from the entry
			QStringList hostNames = ExtractHostNames(I.value()->GetHostName());

			// Create entries for each individual hostname
			for (const QString& individualHost : hostNames) {
				// Create a copy of the entry with the individual hostname
				CTrafficEntryPtr pIndividualEntry = CTrafficEntryPtr(new CTrafficEntry());
				pIndividualEntry->SetHostName(individualHost);
				pIndividualEntry->SetIpAddress(I.value()->GetIpAddress()); // Copy IP to preserve network type
				pIndividualEntry->Merge(I.value());

				if (m_bGroupByProgram)
					BuildProgramHierarchy(pProgram, pIndividualEntry);
				else
					BuildDomainHierarchy(pProgram, pIndividualEntry);
			}
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

			// Check if this entry has changed using timestamp
			QString cacheKey = QString("%1_%2").arg((quint64)pService.data()).arg(I.key());
			quint64 lastActivity = I.value()->GetLastActivity();

			if (m_TrafficLogTimestamps.contains(cacheKey) &&
				m_TrafficLogTimestamps[cacheKey] == lastActivity) {
				// Entry hasn't changed, skip rebuilding
				continue;
			}

			// Update timestamp
			m_TrafficLogTimestamps[cacheKey] = lastActivity;

			// Extract individual hostnames from the entry
			QStringList hostNames = ExtractHostNames(I.value()->GetHostName());

			// Create entries for each individual hostname
			for (const QString& individualHost : hostNames) {
				// Create a copy of the entry with the individual hostname
				CTrafficEntryPtr pIndividualEntry = CTrafficEntryPtr(new CTrafficEntry());
				pIndividualEntry->SetHostName(individualHost);
				pIndividualEntry->SetIpAddress(I.value()->GetIpAddress()); // Copy IP to preserve network type
				pIndividualEntry->Merge(I.value());

				if (m_bGroupByProgram)
					BuildProgramHierarchy(pService, pIndividualEntry);
				else
					BuildDomainHierarchy(pService, pIndividualEntry);
			}
		}
	}

	if (ProgressHelper.Done()) {
		if (++m_SlowCount == 3) {
			m_SlowCount = 0;
			m_pBtnHold->setChecked(true);
		}
	} else
		m_SlowCount = 0;

	//uint64 uNow = GetTickCount64();
	//if(uNow - uStart > 100)
	//	DbgPrint("CTrafficView::Sync took %llu ms\n", uNow - uStart);

	// Update model with built hierarchy (using cached map directly)
	QList<QModelIndex> Added = m_pItemModel->Sync(m_CachedTrafficMap);

	// Auto-expand new branches if enabled
	if (m_pBtnExpand->isChecked()) {
		int CurCount = m_RefreshCount;
		QTimer::singleShot(10, this, [this, Added, CurCount]() {
			if(CurCount != m_RefreshCount)
				return;
			foreach(const QModelIndex & Index, Added)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
		});
	}
}

void CTrafficView::Clear()
{
	m_CurPrograms.clear();
	m_CurServices.clear();
	m_CachedTrafficMap.clear();
	m_TrafficLogTimestamps.clear();
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
	m_CachedTrafficMap.clear();
	m_TrafficLogTimestamps.clear();
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