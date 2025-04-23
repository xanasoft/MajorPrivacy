#include "pch.h"
#include "LibraryView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../MajorPrivacy.h"
#include "Helpers/WinHelper.h"

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

	m_pSignFile = m_pMenu->addAction(QIcon(":/Icons/Cert.png"), tr("Sign File"), this, SLOT(OnFileAction()));
	m_pRemoveSig = m_pMenu->addAction(QIcon(":/Icons/EmptyAll.png"), tr("Remove File Signature"), this, SLOT(OnFileAction()));
	m_pMenu->addSeparator();
	m_pSignCert = m_pMenu->addAction(QIcon(":/Icons/SignFile.png"), tr("Sign Certificate"), this, SLOT(OnFileAction()));
	m_pRemoveCert = m_pMenu->addAction(QIcon(":/Icons/Uninstall.png"), tr("Remove Cert Signature"), this, SLOT(OnFileAction()));
	m_pMenu->addSeparator();
	m_pExplore = m_pMenu->addAction(QIcon(":/Icons/Folder.png"), tr("Open Parent Folder"), this, SLOT(OnFileAction()));
	m_pProperties = m_pMenu->addAction(QIcon(":/Icons/Presets.png"), tr("File Properties"), this, SLOT(OnFileAction()));

	QByteArray Columns = theConf->GetBlob("MainWindow/LibraryView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbGrouping = new QComboBox();
	m_pCmbGrouping->addItem(QIcon(":/Icons/Process.png"), tr("By Program"));
	m_pCmbGrouping->addItem(QIcon(":/Icons/Dll.png"), tr("By Library"));
	connect(m_pCmbGrouping, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbGrouping);

	m_pCmbSign = new QComboBox();
	m_pCmbSign->setMinimumHeight(22);
	m_pCmbSign->addItem(QIcon(":/Icons/Policy.png"), tr("All Signatures"), (int)KPH_VERIFY_AUTHORITY::KphUntestedAuthority);
	m_pCmbSign->addItem(QIcon(":/Icons/Cert.png"), tr("User Sign. ONLY"), (int)KPH_VERIFY_AUTHORITY::KphUserAuthority);
	m_pCmbSign->addItem(QIcon(":/Icons/Windows.png"), tr("Microsoft/AV"), (int)KPH_VERIFY_AUTHORITY::KphMsAuthority);
	m_pCmbSign->addItem(QIcon(":/Icons/SignFile.png"), tr("Trusted by MSFT"), (int)KPH_VERIFY_AUTHORITY::KphMsCodeAuthority);
	m_pCmbSign->addItem(QIcon(":/Icons/Anon.png"), tr("Untrusted/None"), (int)KPH_VERIFY_AUTHORITY::KphUnkAuthority);
	connect(m_pCmbSign, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbSign);

	m_pCmbAction = new QComboBox();
	m_pCmbAction->addItem(QIcon(":/Icons/NoAccess.png"), tr("Any Status"), (qint32)EEventStatus::eUndefined);
	m_pCmbAction->addItem(QIcon(":/Icons/Go.png"), tr("Allowed"), (qint32)EEventStatus::eAllowed);
	m_pCmbAction->addItem(QIcon(":/Icons/Go2.png"), tr("Untrusted"), (qint32)EEventStatus::eUntrusted);
	m_pCmbAction->addItem(QIcon(":/Icons/Disable.png"), tr("Blocked"), (qint32)EEventStatus::eBlocked);
	connect(m_pCmbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbAction);


	m_pToolBar->addSeparator();

	m_pBtnCleanUp = new QToolButton();
	m_pBtnCleanUp->setIcon(QIcon(":/Icons/Clean.png"));
	m_pBtnCleanUp->setToolTip(tr("CleanUp Libraries"));
	m_pBtnCleanUp->setFixedHeight(22);
	connect(m_pBtnCleanUp, SIGNAL(clicked()), this, SLOT(OnCleanUpLibraries()));
	m_pToolBar->addWidget(m_pBtnCleanUp);

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
}

CLibraryView::~CLibraryView()
{
	theConf->SetBlob("MainWindow/LibraryView_Columns", m_pTreeView->saveState());
}

void CLibraryView::Sync(const QSet<CProgramFilePtr>& Programs, const QFlexGuid& EnclaveGuid)
{
	bool bGroupByLibrary = m_pCmbGrouping->currentIndex() == 1;
	KPH_VERIFY_AUTHORITY Authority = (KPH_VERIFY_AUTHORITY)m_pCmbSign->currentData().toInt();
	EEventStatus Status = (EEventStatus)m_pCmbAction->currentData().toInt();

	if (m_CurPrograms != Programs || m_CurEnclaveGuid != EnclaveGuid || m_FullRefresh) {
		m_CurPrograms = Programs;
		m_CurEnclaveGuid = EnclaveGuid;
		m_ParentMap.clear();
		m_LibraryMap.clear();
		m_pItemModel->Clear();
		m_FullRefresh = false;
	}

	auto OldMap = m_LibraryMap;
	QSet<SLibraryKey> Handled;

	auto FilterSig = [&](const UCISignInfo Sign) -> bool {
		switch (Authority)
		{
		case KPH_VERIFY_AUTHORITY::KphUserAuthority:	return Sign.Authority == KPH_VERIFY_AUTHORITY::KphUserAuthority || Sign.Authority == KPH_VERIFY_AUTHORITY::KphDevAuthority;
		case KPH_VERIFY_AUTHORITY::KphMsAuthority:		return Sign.Authority == KPH_VERIFY_AUTHORITY::KphMsAuthority || Sign.Authority == KPH_VERIFY_AUTHORITY::KphAvAuthority;
		case KPH_VERIFY_AUTHORITY::KphMsCodeAuthority:	return Sign.Authority == KPH_VERIFY_AUTHORITY::KphMsCodeAuthority;// || Sign.Authority == KPH_VERIFY_AUTHORITY::KphStoreAuthority;
		case KPH_VERIFY_AUTHORITY::KphUnkAuthority:		return Sign.Authority == KPH_VERIFY_AUTHORITY::KphUnkAuthority || Sign.Authority == KPH_VERIFY_AUTHORITY::KphNoAuthority;
		}
		return true;
	};

	std::function<void(const CProgramFilePtr&, const CProgramLibraryPtr&, const SLibraryInfo&)> AddByProgram = 
		[&](const CProgramFilePtr& pProg, const CProgramLibraryPtr& pLibrary, const SLibraryInfo& Info) {

		if (Status != EEventStatus::eUndefined && Info.LastStatus != Status)
			return;
		bool Pass = FilterSig(Info.SignInfo.GetInfo());

		SLibraryItemPtr& pItem = m_ParentMap[qMakePair((uint64)pProg.get(), 0)];
		if(pItem.isNull())
		{
			if (!Pass && !FilterSig(pProg->GetSignInfo().GetInfo()))
				return;
			pItem = SLibraryItemPtr(new SLibraryItem());
			pItem->pProg = pProg;
			m_LibraryMap.insert(qMakePair((uint64)pProg.get(), 0), pItem);
		}
		else
			OldMap.remove(qMakePair((uint64)pProg.get(), 0));

		if (!Pass)
			return;

		SLibraryItemPtr pSubItem;
		if (Handled.contains(qMakePair((uint64)pProg.get(), (uint64)pLibrary.get()))) 
			pSubItem = m_LibraryMap.value(qMakePair((uint64)pProg.get(), (uint64)pLibrary.get()));
		else {
			Handled.insert(qMakePair((uint64)pProg.get(), (uint64)pLibrary.get()));
			pSubItem = OldMap.take(qMakePair((uint64)pProg.get(), (uint64)pLibrary.get()));
			if (!pSubItem) {
				pSubItem = SLibraryItemPtr(new SLibraryItem());
				pSubItem->pLibrary = pLibrary;
				pSubItem->Parent = QString("%1_%2").arg((uint64)pProg.get()).arg(0);
				m_LibraryMap.insert(qMakePair((uint64)pProg.get(), (uint64)pLibrary.get()), pSubItem);
				pItem->Count++;
			} else {
				pSubItem->Info.TotalLoadCount = 0;
			}
		}

		pSubItem->Info.TotalLoadCount += Info.TotalLoadCount;
		if (Info.LastLoadTime > pSubItem->Info.LastLoadTime) {
			pSubItem->Info.LastLoadTime = Info.LastLoadTime;
			pSubItem->Info.SignInfo = Info.SignInfo;
			if(m_CurEnclaveGuid.IsNull() && Info.EnclaveGuid.IsNull()) // when no nenclave take teh status for no enclave only
				pSubItem->Info.LastStatus = Info.LastStatus;
		}
	};

	std::function<void(const CProgramFilePtr&, const CProgramLibraryPtr&, const SLibraryInfo&)> AddByLibrary = 
		[&](const CProgramFilePtr& pProg, const CProgramLibraryPtr& pLibrary, const SLibraryInfo& Info) {

		if (Status != EEventStatus::eUndefined && Info.LastStatus != Status)
			return;
		if (!FilterSig(Info.SignInfo.GetInfo()))
			return;

		SLibraryItemPtr& pItem = m_ParentMap[qMakePair((uint64)pLibrary.get(), 0)];
		if(pItem.isNull())
		{
			pItem = SLibraryItemPtr(new SLibraryItem());
			pItem->pLibrary = pLibrary;
			m_LibraryMap.insert(qMakePair((uint64)pLibrary.get(), 0), pItem);
		}
		else
			OldMap.remove(qMakePair((uint64)pLibrary.get(), 0));

		if (Handled.contains(qMakePair((uint64)pProg.get(), (uint64)pLibrary.get())))
			return;
		Handled.insert(qMakePair((uint64)pProg.get(), (uint64)pLibrary.get()));

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
			
			if (!m_CurEnclaveGuid.IsNull() && I.value().EnclaveGuid != m_CurEnclaveGuid)
				continue;

			if(bGroupByLibrary)
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

	if (m_pBtnExpand->isChecked()) 
	{
		QTimer::singleShot(10, this, [this, Added]() {
			foreach(const QModelIndex & Index, Added)
				m_pTreeView->expand(m_pSortProxy->mapFromSource(Index));
			});
	}
}

/*void CLibraryView::OnDoubleClicked(const QModelIndex& Index)
{
	//QPair<CProgramItemPtr, CTrafficEntryPtr> pItem = m_pItemModel->GetItem(Index);
}*/

void CLibraryView::OnMenu(const QPoint& Point)
{
	auto Items = GetSelectedItems();

	QStringList Files;
	int FilesSigned = 0;
	QMap<QByteArray, QString> Certs;
	int CertsSigned = 0;
	foreach(SLibraryItemPtr pItem, Items)
	{
		QString Path;
		QByteArray SignerHash;
		QString SignerName;
		if (pItem->pLibrary)
		{
			Path = pItem->pLibrary->GetPath();
			SignerHash = pItem->Info.SignInfo.GetSignerHash();
			SignerName = pItem->Info.SignInfo.GetSignerName();
		}
		else if (pItem->pProg)
		{
			Path = pItem->pProg->GetPath();
			SignerHash = pItem->pProg->GetSignInfo().GetSignerHash();	
			SignerName = pItem->pProg->GetSignInfo().GetSignerName();
		}
		Files.append(Path);
		if (theCore->HasFileSignature(Path))
			FilesSigned++;
		if (!SignerHash.isEmpty()) {
			Certs[SignerHash] = SignerName;
			if (theCore->HasCertSignature(SignerName, SignerHash))
				CertsSigned++;
		}
	}

	m_pSignFile->setEnabled(Files.count() > FilesSigned);
	m_pRemoveSig->setEnabled(FilesSigned > 0);

	m_pSignCert->setEnabled(Certs.count() > CertsSigned);
	m_pRemoveCert->setEnabled(CertsSigned > 0);

	m_pExplore->setEnabled(Items.count() == 1);
	m_pProperties->setEnabled(Items.count() == 1);

	CPanelView::OnMenu(Point);
}

void CLibraryView::OnFileAction()
{
	QAction* pAction = (QAction*)sender();

	auto Items = GetSelectedItems();

	QStringList Files;
	QMap<QByteArray, QString> Certs;
	foreach(SLibraryItemPtr pItem, Items)
	{
		if (pItem->pLibrary)
		{
			Files.append(pItem->pLibrary->GetPath());
			Certs[pItem->Info.SignInfo.GetSignerHash()] = pItem->Info.SignInfo.GetSignerName();
		}
		else if (pItem->pProg)
		{
			Files.append(pItem->pProg->GetPath());
			Certs[pItem->pProg->GetSignInfo().GetSignerHash()] = pItem->pProg->GetSignInfo().GetSignerName();
		}
	}

	STATUS Status;
	if (pAction == m_pSignFile)
		Status = theGUI->SignFiles(Files);
	else if (pAction == m_pRemoveSig)
		Status = theCore->RemoveFileSignature(Files);
	else if (pAction == m_pSignCert)
		Status = theGUI->SignCerts(Certs);
	else if (pAction == m_pRemoveCert)
		Status = theCore->RemoveCertSignature(Certs);
	else if (pAction == m_pExplore || pAction == m_pProperties)
	{
		QString Path;
		if (!Items.isEmpty()) {
			SLibraryItemPtr pItem = Items.first();
			if (pItem->pLibrary)
				Path = pItem->pLibrary->GetPath();
			else if (pItem->pProg)
				Path = pItem->pProg->GetPath();
		}

		if(pAction == m_pExplore)
			OpenFileFolder(Path);
		else
			OpenFileProperties(Path);
	}
	
	theGUI->CheckResults(QList<STATUS>() << Status, this);
}

void CLibraryView::OnCleanUpLibraries()
{
	/*if (QMessageBox::question(this, "MajorPrivacy", tr("Are you sure you want to cleanup the library list for the current program items?"), 
		QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;*/

	auto Current = theGUI->GetCurrentItems();

	if (Current.bAllPrograms)
		theCore->CleanUpLibraries();
	else
	{
		foreach(CProgramFilePtr pProgram, Current.ProgramsEx | Current.ProgramsIm)
			theCore->CleanUpLibraries(pProgram);
	}

	emit theCore->CleanUpDone();

	m_FullRefresh = true;
}