#include "pch.h"
#include "AccessView.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../MiscHelpers/Common/Common.h"
#include "../../Library/Helpers/NtUtil.h"
#include "../Library/Common/DbgHelp.h"

CAccessView::CAccessView(QWidget *parent)
	:CPanelViewEx<CAccessModel>(parent)
{
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());

	m_pTreeView->setColumnReset(2);
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	connect(m_pItemModel, SIGNAL(CheckChanged(const QModelIndex&, bool)), this, SLOT(OnCheckChanged(const QModelIndex&, bool)));

	QByteArray Columns = theConf->GetBlob("MainWindow/AccessView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbAccess = new QComboBox();
	m_pCmbAccess->addItem(QIcon(":/Icons/Fence.png"), tr("Any Access"), (qint32)EAccessRuleType::eNone);
	m_pCmbAccess->addItem(QIcon(":/Icons/Go.png"), tr("Read/Write"), (qint32)EAccessRuleType::eAllow);
	m_pCmbAccess->addItem(QIcon(":/Icons/Go3.png"), tr("Write Only"), (qint32)EAccessRuleType::eAllowRO); // WO
	//connect(m_pCmbAccess, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbAccess);

	m_pToolBar->addSeparator();

	m_pBtnHold = new QToolButton();
	m_pBtnHold->setIcon(QIcon(":/Icons/Hold.png"));
	m_pBtnHold->setCheckable(true);
	m_pBtnHold->setToolTip(tr("Hold updates"));
	m_pBtnHold->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnHold);

	m_pBtnRefresh = new QToolButton();
	m_pBtnRefresh->setIcon(QIcon(":/Icons/Refresh.png"));
	m_pBtnRefresh->setToolTip(tr("Reload Access Tree"));
	m_pBtnRefresh->setFixedHeight(22);
	m_pBtnRefresh->setShortcut(QKeySequence::fromString("F5"));
	connect(m_pBtnRefresh, SIGNAL(clicked()), this, SLOT(OnRefresh()));
	m_pToolBar->addWidget(m_pBtnRefresh);

	m_pToolBar->addSeparator();

	m_pBtnCleanUp = new QToolButton();
	m_pBtnCleanUp->setIcon(QIcon(":/Icons/Clean.png"));
	m_pBtnCleanUp->setToolTip(tr("CleanUp Access Tree"));
	m_pBtnCleanUp->setFixedHeight(22);
	connect(m_pBtnCleanUp, SIGNAL(clicked()), this, SLOT(CleanUpTree()));
	m_pToolBar->addWidget(m_pBtnCleanUp);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	m_pCopyPath = m_pMenu->addAction(tr("Copy Path"), this, SLOT(OnMenuAction()));

	AddPanelItemsToMenu();

	connect(theCore, SIGNAL(CleanUpDone()), this, SLOT(OnCleanUpDone()));
}

CAccessView::~CAccessView()
{
	theConf->SetBlob("MainWindow/AccessView_Columns", m_pTreeView->saveState());
}

void CAccessView::Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, QString RootPath)
{
	if(m_pBtnHold->isChecked())
		return;

	if (m_CurPrograms != Programs || m_CurServices != Services || m_iAccessFilter != m_pCmbAccess->currentData().toInt() || m_RecentLimit != theGUI->GetRecentLimit() || m_RootPath != RootPath) 
	{
		m_CurPrograms = Programs;
		m_CurServices = Services;
		m_CurRoot.clear();
		m_CurItems.clear();
		m_iAccessFilter = m_pCmbAccess->currentData().toInt();
		m_RecentLimit = theGUI->GetRecentLimit();
		m_RootPath = RootPath;
		m_pItemModel->Clean();
	}

	RootPath = theCore->NormalizePath(RootPath);

	quint64 uRecentLimit = m_RecentLimit ? QDateTime::currentMSecsSinceEpoch() - m_RecentLimit : 0;

	if (m_CurRoot.isNull())
	{
		m_CurRoot = SAccessItemPtr(new SAccessItem());

		auto addAccessItem = [&](SAccessItemPtr& root, const QString& name, const QString& title, SAccessItem::EType type) {
			SAccessItemPtr pItem = SAccessItemPtr(new SAccessItem());
			pItem->Name = title;
			pItem->Type = type;
			root->Branches.insert(name, pItem);
			return pItem;
		};

		auto Computer = addAccessItem(m_CurRoot, "computer", tr("My Computer"), SAccessItem::eComputer);
		//addAccessItem(Computer, "nt_kernel", tr("Nt Namespace"), SAccessItem::eObjects);
		addAccessItem(Computer, "devices", tr("\\\\.\\"), SAccessItem::eObjects);
		if (RootPath.isNull()) {
			if (theCore->GetConfigBool("Service/LogRegistry", false))
				addAccessItem(Computer, "registry", tr("Registry"), SAccessItem::eRegistry);
			addAccessItem(m_CurRoot, "network", tr("Network"), SAccessItem::eNetwork);
		}
	}

	std::function<void(const SAccessItemPtr&, SAccessItem::EType, const QString&, int, const CProgramItemPtr&, const SAccessStatsPtr&)> AddTreeEntry = 
		[&](const SAccessItemPtr& pParent, SAccessItem::EType Type, const QString& Path, int uOffset, const CProgramItemPtr& pProg, const SAccessStatsPtr& pStats) {

		while (uOffset < Path.length() && Path.at(uOffset) == L'\\')
			uOffset++;

		size_t uPos = Path.indexOf('\\', uOffset);
		if (uPos == -1 && uOffset < Path.length())
			uPos = Path.length();

		QString Name;
		if (uPos != -1)
			Name = Path.mid(uOffset, uPos - uOffset);

		if (uPos == -1)
		{
			pParent->Type = Type;
			pParent->Path = pStats->Path;
			pParent->Stats[pProg] = pStats;
			if(pParent->LastAccess < pStats->LastAccessTime)
				pParent->LastAccess = pStats->LastAccessTime;
			pParent->AccessMask = pStats->AccessMask;
			return;
		}

		auto &pBranch = pParent->Branches[Name.toLower()];
		if (!pBranch) {
			pBranch = SAccessItemPtr(new SAccessItem());
			pBranch->Name = Name;
			if (pBranch->Type == SAccessItem::eDevice)
				pBranch->Type = Type;
			//pBranch->Type = Type;
		}

		AddTreeEntry(pBranch, Type, Path, uPos + 1, pProg, pStats);

		if(pParent->LastAccess < pBranch->LastAccess)
			pParent->LastAccess = pBranch->LastAccess;
		pParent->AccessMask |= pBranch->AccessMask;
	};

	std::function<void(const CProgramItemPtr&, const SAccessStatsPtr&)> AddEntry = 
		[&](const CProgramItemPtr& pProg, const SAccessStatsPtr& pStats) {

		//if(!pStats->LastAccessTime)
		//	return;

		if (m_iAccessFilter) {

			uint32 uAccessMask = pStats->AccessMask;
			
			bool bWriteAccess = false;
			bool bReadAccess = false;

			if((uAccessMask & GENERIC_WRITE) == GENERIC_WRITE
				|| (uAccessMask & GENERIC_ALL) == GENERIC_ALL
				|| (uAccessMask & DELETE) == DELETE
				|| (uAccessMask & WRITE_DAC) == WRITE_DAC
				|| (uAccessMask & WRITE_OWNER) == WRITE_OWNER
				|| (uAccessMask & FILE_WRITE_DATA) == FILE_WRITE_DATA
				|| (uAccessMask & FILE_APPEND_DATA) == FILE_APPEND_DATA)
				bWriteAccess = true;

			else if((uAccessMask & FILE_WRITE_ATTRIBUTES) == FILE_WRITE_ATTRIBUTES
				|| (uAccessMask & FILE_WRITE_EA) == FILE_WRITE_EA)
				bWriteAccess = true;

			else if((uAccessMask & GENERIC_READ) == GENERIC_READ
				|| (uAccessMask & GENERIC_EXECUTE) == GENERIC_EXECUTE
				|| (uAccessMask & READ_CONTROL) == READ_CONTROL
				|| (uAccessMask & FILE_READ_DATA) == FILE_READ_DATA
				|| (uAccessMask & FILE_EXECUTE) == FILE_EXECUTE)
				bReadAccess = true;

			else if((uAccessMask & SYNCHRONIZE) == SYNCHRONIZE
				|| (uAccessMask & FILE_READ_ATTRIBUTES) == FILE_READ_ATTRIBUTES
				|| (uAccessMask & FILE_READ_EA) == FILE_READ_EA)
				;

			if(m_iAccessFilter == (qint32)EAccessRuleType::eAllow && !(bWriteAccess || bReadAccess))
				return;
			if(m_iAccessFilter == (qint32)EAccessRuleType::eAllowRO && !bWriteAccess)
				return;
		}

		if (uRecentLimit) {
			if (FILETIME2ms(pStats->LastAccessTime) < uRecentLimit)
				return;
		}

		if (!RootPath.isNull())
		{
			if(RootPath.isEmpty())
				return;

			QString Path = pStats->Path;
			if (!PathStartsWith(Path, RootPath))
				return;
		}

		QString Path = pStats->Path;

		SAccessItem::EType Type = SAccessItem::eDevice;

		if (Path.length() >= 2 && Path[1] == ':' && ((Path[0] >= 'A' && Path[0] <= 'Z') || (Path[0] >= 'a' && Path[0] <= 'z'))) {
			if(Path.length() >= 4)
				Type = pStats->IsDirectory ? SAccessItem::eFolder : SAccessItem::eFile;
			else
				Type = SAccessItem::eDrive;
			Path = "computer\\" + Path;
		}
		else if (Path.length() > 12 && Path.startsWith("\\Device\\Mup\\", Qt::CaseInsensitive) && Path.at(12).isLetterOrNumber()) {
			if(Path.indexOf("\\", 12) != -1)
				Type = pStats->IsDirectory ? SAccessItem::eFolder : SAccessItem::eFile;
			else
				Type = SAccessItem::eHost;
			Path = "network" + Path.mid(11);
		}
		//else if (Path.length() > 2 && Path.startsWith("\\\\", Qt::CaseInsensitive)) {
		//	if(Path.indexOf("\\", 2) != -1)
		//		Type = pStats->IsDirectory ? SAccessItem::eFolder : SAccessItem::eFile;
		//	else
		//		Type = SAccessItem::eHost;
		//	Path = "network" + Path.mid(1);
		//}
		else if (Path.startsWith("\\REGISTRY\\", Qt::CaseInsensitive)) {
			Type = SAccessItem::eRegistry;
			Path = "computer\\registry" + Path.mid(9);
		}
		else {
			if (Path.startsWith("\\Device\\", Qt::CaseInsensitive) && Path.indexOf("\\", 8) != -1)
				Type = pStats->IsDirectory ? SAccessItem::eFolder : SAccessItem::eFile;
			//Path = "computer\\nt_kernel" + Path;
			Path = "computer\\devices" + Path.mid(7);
		}
		
		AddTreeEntry(m_CurRoot, Type, Path, 0, pProg, pStats);
	};

#ifdef _DEBUG
	quint64 start = GetUSTickCount();
#endif

	quint64 LastTime = GetUSTickCount();
	int TotalCount = Programs.count() + Services.count();
	int ProcessedCount = 0;
	theGUI->m_pProgressDialog->ResetCanceled();

	auto ShowProgress = [&](const QString& ProgName) {
		quint64 ElapsedTimeMs = (GetUSTickCount() - LastTime) / 1000;
		ProcessedCount++;
		if (theGUI->m_pProgressDialog->isVisible()) {
			if (ElapsedTimeMs > 50) {
				LastTime = GetUSTickCount();
				theGUI->m_pProgressDialog->ShowProgress(tr("Loading %1").arg(ProgName), 100 * ProcessedCount / TotalCount);
				QCoreApplication::processEvents();
			}
		} else if(ElapsedTimeMs > 500)
			theGUI->m_pProgressDialog->show();

		return !theGUI->m_pProgressDialog->IsCancelled();
	};

	foreach(const CProgramFilePtr& pProgram, Programs) 
	{
		auto &Item = m_CurItems[pProgram->GetUID()];
		qint64 LastAccess = (qint64)qMax(pProgram->GetAccessLastActivity(), pProgram->GetAccessLastEvent());
		if(Item.LastAccess >= LastAccess)
			continue;
		Item.LastAccess = LastAccess;

		if (!ShowProgress(pProgram->GetName())) {
			m_pBtnHold->setChecked(true);
			break;
		}
		QMap<quint64, SAccessStatsPtr> Log = pProgram->GetAccessStats();
		for (auto I = Log.constBegin(); I != Log.constEnd(); I++) {
			auto &Value = Item.Items[(quint64)I.value().data()];
			if(Value.Value >= (qint64)I.value()->LastAccessTime)
				continue;
			Value.Value = (qint64)I.value()->LastAccessTime;
			AddEntry(pProgram, I.value());
		}
	}
	foreach(const CWindowsServicePtr& pService, Services) 
	{
		SItem &Item = m_CurItems[pService->GetUID()];
		qint64 LastAccess = (qint64)qMax(pService->GetAccessLastActivity(), pService->GetAccessLastEvent());
		if(Item.LastAccess >= LastAccess)
			continue;
		Item.LastAccess = LastAccess;

		if (!ShowProgress(pService->GetName())) {
			m_pBtnHold->setChecked(true);
			break;
		}
		QMap<quint64, SAccessStatsPtr> Log = pService->GetAccessStats();
		for (auto I = Log.constBegin(); I != Log.constEnd(); I++) {
			auto &Value = Item.Items[(quint64)I.value().data()];
			if(Value.Value >= (qint64)I.value()->LastAccessTime)
				continue;
			Value.Value = (qint64)I.value()->LastAccessTime;
			AddEntry(pService, I.value());
		}
	}

	if(theGUI->m_pProgressDialog->isVisible())
		theGUI->m_pProgressDialog->hide();

#ifdef _DEBUG
	quint64 elapsed = (GetUSTickCount() - start) / 1000;
	if (elapsed > 100)
		qDebug() << "Access Tree Sync took" << (GetUSTickCount() - start) / 1000000.0 << "s";
#endif

	//foreach(QString Key, OldMap.keys())
	//	m_CurAccess.remove(Key);

	//start = GetUSTickCount();
	m_pItemModel->Update(m_CurRoot);
	//qDebug() << "Access Tree Update took" << (GetUSTickCount() - start) / 1000000.0 << "s";

	/*if (m_CurPrograms.count() + m_CurServices.count() > 1)
	{
		QTimer::singleShot(10, this, [this, Added]() {
			foreach(const QModelIndex & Index, Added)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
		});
	}*/
}

/*void CAccessView::OnDoubleClicked(const QModelIndex& index)
{

}*/

void CAccessView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}

void CAccessView::OnRefresh()
{
	m_iAccessFilter = -1; // force full reload
}

void CAccessView::CleanUpTree()
{
	if (QMessageBox::question(this, "MajorPrivacy", tr("Do you want to clean up the Access Tree? This will remove all Files and folders which are no longer present on the System. "
	  "It will also remove all references to files and folders existing but contained in not currently mounted volumes. "
	  "The operation will have to attempt to access all logged locations and will take some time, once done the view will refresh."), QMessageBox::Yes, QMessageBox::Cancel) != QMessageBox::Yes)
		return;
	
	QList<STATUS> Results = QList<STATUS>() << theCore->CleanUpAccessTree();
	theGUI->CheckResults(Results, this);
}

void CAccessView::OnCleanUpDone()
{
	// refresh
	m_CurPrograms.clear();
	m_CurServices.clear();
}

void CAccessView::OnMenuAction()
{
	QAction* pAction = (QAction*)sender();

	if (pAction == m_pCopyPath)
	{
		QStringList Paths;
		foreach(const QModelIndex & Index, m_pTreeView->selectedRows())
		{
			QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
			SAccessItemPtr pItem = m_pItemModel->GetItem(ModelIndex);
			if (!pItem)
				continue;
			Paths.append(pItem->Path);
		}
		QApplication::clipboard()->setText(Paths.join("\n"));
	}
}