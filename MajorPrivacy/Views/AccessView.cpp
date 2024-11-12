#include "pch.h"
#include "AccessView.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../MiscHelpers/Common/Common.h"
#include "../../Library/Helpers/NtUtil.h"

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

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	AddPanelItemsToMenu();
}

CAccessView::~CAccessView()
{
	theConf->SetBlob("MainWindow/AccessView_Columns", m_pTreeView->saveState());
}

void CAccessView::Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services)
{
	if (m_CurPrograms != Programs || m_CurServices != Services || m_iAccessFilter != m_pCmbAccess->currentData().toInt() || m_RecentLimit != theGUI->GetRecentLimit()) {
		m_CurPrograms = Programs;
		m_CurServices = Services;
#ifndef USE_ACCESS_TREE
		m_CurAccess.clear();
#else
		m_CurRoot.clear();
#endif
		m_CurItems.clear();
		//m_pItemModel->Clear();
		CAccessModel* pItemModel = new CAccessModel();
		m_pSortProxy->setSourceModel(pItemModel);
		m_pItemModel->deleteLater();
		m_pItemModel = pItemModel;

		m_iAccessFilter = m_pCmbAccess->currentData().toInt();
		m_RecentLimit = theGUI->GetRecentLimit();
	}

	quint64 uRecentLimit = m_RecentLimit ? QDateTime::currentMSecsSinceEpoch() - m_RecentLimit : 0;

#ifndef USE_ACCESS_TREE

	//auto OldMap = m_CurAccess;

	std::function<void(const CProgramItemPtr&, const SAccessStatsPtr&)> AddEntry = 
		[&](const CProgramItemPtr& pProg, const SAccessStatsPtr& pStats) {

		QString Path = pStats->Path.Get(EPathType::eDisplay);
		if (Path.isEmpty() || Path.startsWith("\\Device\\NamedPipe", Qt::CaseInsensitive))
			return;

		//SAccessItemPtr pItem = OldMap.take(Path.toLower());
		SAccessItemPtr& pItem = m_CurAccess[Path.toLower()];
		if (pItem.isNull()) {
			pItem = SAccessItemPtr(new SAccessItem());
			if (Path.length() >= 2 && Path[1] == ':' && ((Path[0] >= 'A' && Path[0] <= 'Z') || (Path[0] >= 'a' && Path[0] <= 'z'))) {
				pItem->Path = "$pc\\" + Path;
				if(Path.length() >= 4)
					pItem->Type = pStats->IsDirectory ? SAccessItem::eFolder : SAccessItem::eFile;
				else
					pItem->Type = SAccessItem::eDrive;
			}
			else if (Path.length() > 12 && Path.startsWith("\\Device\\Mup\\", Qt::CaseInsensitive) && Path.at(12).isLetterOrNumber()) {
				pItem->Path = "$net" + Path.mid(11);
				if(Path.indexOf("\\", 12) != -1)
					pItem->Type = pStats->IsDirectory ? SAccessItem::eFolder : SAccessItem::eFile;
				else
					pItem->Type = SAccessItem::eHost;
			}
			else if (Path.startsWith("\\REGISTRY", Qt::CaseInsensitive)) {
				pItem->Path = "Registry" + Path.mid(9);
				pItem->Type = SAccessItem::eRegistry;
			}
			else {
				pItem->Path = "$nt" + Path;
				if (Path.startsWith("\\Device\\", Qt::CaseInsensitive) && Path.indexOf("\\", 8) != -1)
					pItem->Type = pStats->IsDirectory ? SAccessItem::eFolder : SAccessItem::eFile;
			}
			pItem->NtPath = pStats->Path.Get(EPathType::eNative);
			//m_CurAccess.insert(Path.toLower(), pItem);

			pItem->Name = Split2(pItem->Path, "\\", true).second;

			pItem->Path = pItem->Path.toLower();
		}
		
		pItem->Stats[pProg] = pStats;
	};

	if (m_CurAccess.isEmpty()) {
		auto addAccessItem = [&](const QString &path, const QString &name, SAccessItem::EType type) {
			SAccessItemPtr pItem = SAccessItemPtr(new SAccessItem());
			pItem->Path = path;
			pItem->Name = name;
			pItem->Type = type;
			m_CurAccess.insert("\\" + path, pItem);
		};

		addAccessItem("$pc", tr("My Computer"), SAccessItem::eComputer);
		addAccessItem("$nt", tr("Nt Namespace"), SAccessItem::eObjects);
		addAccessItem("registry", tr("Registry"), SAccessItem::eRegistry);
		addAccessItem("$net", tr("Network"), SAccessItem::eNetwork);
	}
	//else {
	//	OldMap.remove("$pc");
	//	OldMap.remove("$nt");
	//	OldMap.remove("Registry");
	//	OldMap.remove("$net");
	//}

#else

	if (m_CurRoot.isNull())
	{
		m_CurRoot = SAccessItemPtr(new SAccessItem());

		auto addAccessItem = [&](const QString &name, const QString &title, SAccessItem::EType type) {
			SAccessItemPtr pItem = SAccessItemPtr(new SAccessItem());
			pItem->Name = title;
			pItem->Type = type;
			m_CurRoot->Branches.insert(name, pItem);
		};

		addAccessItem("dos_drives", tr("Dos Drives"), SAccessItem::eComputer);
		addAccessItem("nt_kernel", tr("Nt Namespace"), SAccessItem::eObjects);
		addAccessItem("registry", tr("Registry"), SAccessItem::eRegistry);
		addAccessItem("network", tr("Network"), SAccessItem::eNetwork);
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
			pParent->NtPath = pStats->Path.Get(EPathType::eNative);
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

		QString Path = pStats->Path.Get(EPathType::eDisplay);
		if (Path.isEmpty() || Path.startsWith("\\Device\\NamedPipe", Qt::CaseInsensitive))
			return;

		SAccessItem::EType Type = SAccessItem::eDevice;

		if (Path.length() >= 2 && Path[1] == ':' && ((Path[0] >= 'A' && Path[0] <= 'Z') || (Path[0] >= 'a' && Path[0] <= 'z'))) {
			if(Path.length() >= 4)
				Type = pStats->IsDirectory ? SAccessItem::eFolder : SAccessItem::eFile;
			else
				Type = SAccessItem::eDrive;
			Path = "dos_drives\\" + Path;
		}
		else if (Path.length() > 12 && Path.startsWith("\\Device\\Mup\\", Qt::CaseInsensitive) && Path.at(12).isLetterOrNumber()) {
			if(Path.indexOf("\\", 12) != -1)
				Type = pStats->IsDirectory ? SAccessItem::eFolder : SAccessItem::eFile;
			else
				Type = SAccessItem::eHost;
			Path = "network" + Path.mid(11);
		}
		else if (Path.startsWith("\\REGISTRY", Qt::CaseInsensitive)) {
			Type = SAccessItem::eRegistry;
			Path = "Registry" + Path.mid(9);
		}
		else {
			if (Path.startsWith("\\Device\\", Qt::CaseInsensitive) && Path.indexOf("\\", 8) != -1)
				Type = pStats->IsDirectory ? SAccessItem::eFolder : SAccessItem::eFile;
			Path = "nt_kernel" + Path;
		}
		
		AddTreeEntry(m_CurRoot, Type, Path, 0, pProg, pStats);
	};

#endif

	foreach(const CProgramFilePtr& pProgram, Programs) 
	{
		auto &Item = m_CurItems[pProgram->GetUID()];
		qint64 LastAccess = (qint64)qMax(pProgram->GetAccessLastActivity(), pProgram->GetAccessLastEvent());
		if(Item.LastAccess >= LastAccess)
			continue;
		Item.LastAccess = LastAccess;

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

		QMap<quint64, SAccessStatsPtr> Log = pService->GetAccessStats();
		for (auto I = Log.constBegin(); I != Log.constEnd(); I++) {
			auto &Value = Item.Items[(quint64)I.value().data()];
			if(Value.Value >= (qint64)I.value()->LastAccessTime)
				continue;
			Value.Value = (qint64)I.value()->LastAccessTime;
			AddEntry(pService, I.value());
		}
	}

	//foreach(QString Key, OldMap.keys())
	//	m_CurAccess.remove(Key);

#ifndef USE_ACCESS_TREE
	QList<QModelIndex> Added = m_pItemModel->Sync(m_CurAccess);
#else
	QList<QModelIndex> Added = m_pItemModel->Sync(m_CurRoot);
#endif

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
