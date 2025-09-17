#include "pch.h"
#include "LibraryView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/HashDB/HashDB.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../MajorPrivacy.h"
#include "Helpers/WinHelper.h"
#include "../../MiscHelpers/Common/Common.h"
#include "../../MiscHelpers/Common/OtherFunctions.h"

CLibraryView::CLibraryView(QWidget *parent)
	:CPanelViewEx<CLibraryModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	connect(m_pItemModel, SIGNAL(CheckChanged(const QModelIndex&, bool)), this, SLOT(OnCheckChanged(const QModelIndex&, bool)));

	m_pSignFile = m_pMenu->addAction(QIcon(":/Icons/Cert.png"), tr("Sign File"), this, SLOT(OnFileAction()));
	m_pRemoveSig = m_pMenu->addAction(QIcon(":/Icons/EmptyAll.png"), tr("Remove File Signature"), this, SLOT(OnFileAction()));
	m_pMenu->addSeparator();
	//m_pSignCert = m_pMenu->addAction(QIcon(":/Icons/SignFile.png"), tr("Sign Certificate"), this, SLOT(OnFileAction()));
	//m_pRemoveCert = m_pMenu->addAction(QIcon(":/Icons/Uninstall.png"), tr("Remove Cert Signature"), this, SLOT(OnFileAction()));
	m_pSignCert = m_pMenu->addAction(QIcon(":/Icons/SignFile.png"), tr("Allow Signer Certificate"), this, SLOT(OnFileAction()));
	m_pRemoveCert = m_pMenu->addAction(QIcon(":/Icons/Uninstall.png"), tr("Remove Signer Certificate"), this, SLOT(OnFileAction()));
	m_pSignCA = m_pMenu->addAction(QIcon(":/Icons/SignFile.png"), tr("Allow Issuer Certificate"), this, SLOT(OnFileAction()));
	m_pRemoveCA = m_pMenu->addAction(QIcon(":/Icons/Uninstall.png"), tr("Remove Issuer Certificate"), this, SLOT(OnFileAction()));
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
	connect(m_pCmbGrouping, SIGNAL(currentIndexChanged(int)), this, SLOT(OnRefresh()));
	m_pToolBar->addWidget(m_pCmbGrouping);

	/*m_pCmbSign = new QComboBox();
	m_pCmbSign->setMinimumHeight(22);
	m_pCmbSign->addItem(QIcon(":/Icons/Policy.png"), tr("All Signatures"), (int)KPH_VERIFY_AUTHORITY::KphUntestedAuthority);
	m_pCmbSign->addItem(QIcon(":/Icons/Cert.png"), tr("User Sign. ONLY"), (int)KPH_VERIFY_AUTHORITY::KphUserAuthority);
	m_pCmbSign->addItem(QIcon(":/Icons/Windows.png"), tr("Microsoft/AV"), (int)KPH_VERIFY_AUTHORITY::KphMsAuthority);
	m_pCmbSign->addItem(QIcon(":/Icons/SignFile.png"), tr("Trusted by MSFT"), (int)KPH_VERIFY_AUTHORITY::KphMsCodeAuthority);
	m_pCmbSign->addItem(QIcon(":/Icons/Anon.png"), tr("Untrusted/None"), (int)KPH_VERIFY_AUTHORITY::KphUnkAuthority);
	connect(m_pCmbSign, SIGNAL(currentIndexChanged(int)), this, SLOT(OnRefresh()));
	m_pToolBar->addWidget(m_pCmbSign);*/

	m_pCmbAction = new QComboBox();
	m_pCmbAction->addItem(QIcon(":/Icons/NoAccess.png"), tr("Any Status"), (qint32)EEventStatus::eUndefined);
	m_pCmbAction->addItem(QIcon(":/Icons/Go.png"), tr("Allowed"), (qint32)EEventStatus::eAllowed);
	m_pCmbAction->addItem(QIcon(":/Icons/Go2.png"), tr("Untrusted"), (qint32)EEventStatus::eUntrusted);
	m_pCmbAction->addItem(QIcon(":/Icons/Disable.png"), tr("Blocked"), (qint32)EEventStatus::eBlocked);
	connect(m_pCmbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(OnRefresh()));
	m_pToolBar->addWidget(m_pCmbAction);

	m_pBtnHideWin = new QToolButton();
	m_pBtnHideWin->setIcon(QIcon(QPixmap::fromImage(ImageAddOverlay(QImage(":/Icons/Windows.png"), ":/Icons/Disable.png", 40))));
	m_pBtnHideWin->setCheckable(true);
	m_pBtnHideWin->setToolTip(tr("Hide windows Libraries"));
	m_pBtnHideWin->setMaximumHeight(22);
	connect(m_pBtnHideWin, SIGNAL(clicked()), this, SLOT(OnRefresh()));
	m_pToolBar->addWidget(m_pBtnHideWin);

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

	m_pBtnInfo = new QToolButton();
	m_pBtnInfo->setIcon(QIcon(":/Icons/Cert.png"));
	m_pBtnInfo->setCheckable(true);
	m_pBtnInfo->setToolTip(tr("Show Signature Details"));
	m_pBtnInfo->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnInfo);

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
	//KPH_VERIFY_AUTHORITY Authority = (KPH_VERIFY_AUTHORITY)m_pCmbSign->currentData().toInt();
	EEventStatus Status = (EEventStatus)m_pCmbAction->currentData().toInt();
	bool bHideMS = m_pBtnHideWin->isChecked();

	if (m_CurPrograms != Programs || m_CurEnclaveGuid != EnclaveGuid || m_FullRefresh) {
		m_CurPrograms = Programs;
		m_CurEnclaveGuid = EnclaveGuid;
		m_LibraryMap.clear();
		m_pItemModel->Clear();
		m_FullRefresh = false;
		m_RefreshCount++;
	}
	else if(m_pBtnHold->isChecked() /*&& !theGUI->m_IgnoreHold*/)
		return;

	auto OldMap = m_LibraryMap;
	QSet<SLibraryKey> Handled;

	//auto FilterSig = [&](const CImageSignInfo& SignInfo) -> bool {
	//	auto PrivateAuthority = SignInfo.GetPrivateAuthority();
	//	switch (Authority)
	//	{
	//	case KPH_VERIFY_AUTHORITY::KphUserAuthority:	return PrivateAuthority == KPH_VERIFY_AUTHORITY::KphUserAuthority || PrivateAuthority == KPH_VERIFY_AUTHORITY::KphDevAuthority;
	//	case KPH_VERIFY_AUTHORITY::KphMsAuthority:		return PrivateAuthority == KPH_VERIFY_AUTHORITY::KphMsAuthority || PrivateAuthority == KPH_VERIFY_AUTHORITY::KphAvAuthority;
	//	case KPH_VERIFY_AUTHORITY::KphMsCodeAuthority:	return PrivateAuthority == KPH_VERIFY_AUTHORITY::KphMsCodeAuthority;// || PrivateAuthority == KPH_VERIFY_AUTHORITY::KphStoreAuthority;
	//	case KPH_VERIFY_AUTHORITY::KphUnkAuthority:		return PrivateAuthority == KPH_VERIFY_AUTHORITY::KphUnkAuthority || PrivateAuthority == KPH_VERIFY_AUTHORITY::KphNoAuthority;
	//	}
	//	return true;
	//};

	std::function<void(const CProgramFilePtr&, const CProgramLibraryPtr&, const SLibraryInfo&)> AddByProgram = 
		[&](const CProgramFilePtr& pProg, const CProgramLibraryPtr& pLibrary, const SLibraryInfo& Info) {

		if (Status != EEventStatus::eUndefined && Info.LastStatus != Status)
			return;
		//bool Pass = FilterSig(Info.SignInfo);

		SLibraryItemPtr& pItem = m_LibraryMap[qMakePair((uint64)pProg.get(), 0)];
		if(pItem.isNull())
		{
			//if (!Pass && !FilterSig(pProg->GetSignInfo()))
			//	return;
			pItem = SLibraryItemPtr(new SLibraryItem());
			pItem->ID = QString("%1_%2").arg((uint64)pProg.get()).arg(0);
			pItem->pProg = pProg;
		}
		else
			OldMap.remove(qMakePair((uint64)pProg.get(), 0));

		//if (!Pass)
		//	return;

		SLibraryItemPtr pSubItem;
		if (Handled.contains(qMakePair((uint64)pProg.get(), (uint64)pLibrary.get()))) 
			pSubItem = m_LibraryMap.value(qMakePair((uint64)pProg.get(), (uint64)pLibrary.get()));
		else {
			Handled.insert(qMakePair((uint64)pProg.get(), (uint64)pLibrary.get()));
			pSubItem = OldMap.take(qMakePair((uint64)pProg.get(), (uint64)pLibrary.get()));
			if (!pSubItem) {
				pSubItem = SLibraryItemPtr(new SLibraryItem());
				pSubItem->ID = QString("%1_%2").arg((uint64)pProg.get()).arg((uint64)pLibrary.get());
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
			if(m_CurEnclaveGuid == Info.EnclaveGuid)
				pSubItem->Info.LastStatus = Info.LastStatus;
		}
	};

	std::function<void(const CProgramFilePtr&, const CProgramLibraryPtr&, const SLibraryInfo&)> AddByLibrary = 
		[&](const CProgramFilePtr& pProg, const CProgramLibraryPtr& pLibrary, const SLibraryInfo& Info) {

		if (Status != EEventStatus::eUndefined && Info.LastStatus != Status)
			return;
		//if (!FilterSig(Info.SignInfo))
		//	return;

		SLibraryItemPtr& pItem = m_LibraryMap[qMakePair((uint64)pLibrary.get(), 0)];
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

		//pItem->Info.TotalLoadCount += ;
		if (Info.LastLoadTime > pItem->Info.LastLoadTime) {
			pItem->Info.LastLoadTime = Info.LastLoadTime;
			pItem->Info.SignInfo = Info.SignInfo;
			if(m_CurEnclaveGuid == Info.EnclaveGuid)
				pItem->Info.LastStatus = Info.LastStatus;
		}
	};

	CProgressDialogHelper ProgressHelper(theGUI->m_pProgressDialog, tr("Loading %1"), Programs.count());

	foreach(const CProgramFilePtr& pProgram, Programs) {

		if (!ProgressHelper.Next(pProgram->GetName())) {
			m_pBtnHold->setChecked(true);
			break;
		}

		QMultiMap<quint64, SLibraryInfo> Log = pProgram->GetLibraries();
		for (auto I = Log.begin(); I != Log.end(); I++) {
			CProgramLibraryPtr pLibrary = theCore->ProgramManager()->GetLibraryByUID(I.key());
			if(!pLibrary) continue; // todo 
			
			if (!m_CurEnclaveGuid.IsNull() && I.value().EnclaveGuid != m_CurEnclaveGuid)
				continue;

			if(bHideMS && I.value().SignInfo.GetSignatures().Windows)
				continue;

			if(bGroupByLibrary)
				AddByLibrary(pProgram, pLibrary, I.value());
			else
				AddByProgram(pProgram, pLibrary, I.value());
		}
	}

	if (ProgressHelper.Done()) {
		if (/*!theGUI->m_IgnoreHold &&*/ ++m_SlowCount == 3) {
			m_SlowCount = 0;
			m_pBtnHold->setChecked(true);
		}
	} else
		m_SlowCount = 0;

	foreach(SLibraryKey Key, OldMap.keys())
		m_LibraryMap.remove(Key);

	QList<QModelIndex> Added = m_pItemModel->Sync(m_LibraryMap);

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
	QMap<QByteArray, QString> CAs;
	int CAsSigned = 0;
	foreach(SLibraryItemPtr pItem, Items)
	{
		QString Path;
		QByteArray FileHash;
		QByteArray SignerHash;
		QString SignerName;
		QByteArray IssuerHash;
		QString IssuerName;
		if (pItem->pLibrary)
		{
			Path = pItem->pLibrary->GetPath();
			FileHash = pItem->Info.SignInfo.GetFileHash();
			SignerHash = pItem->Info.SignInfo.GetSignerHash();
			SignerName = pItem->Info.SignInfo.GetSignerName();
			IssuerHash = pItem->Info.SignInfo.GetIssuerHash();
			IssuerName = pItem->Info.SignInfo.GetIssuerName();
		}
		else if (pItem->pProg)
		{
			Path = pItem->pProg->GetPath();
			FileHash = pItem->pProg->GetSignInfo().GetFileHash();
			SignerHash = pItem->pProg->GetSignInfo().GetSignerHash();	
			SignerName = pItem->pProg->GetSignInfo().GetSignerName();
			IssuerHash = pItem->pProg->GetSignInfo().GetIssuerHash();
			IssuerName = pItem->pProg->GetSignInfo().GetIssuerName();
		}
		Files.append(Path);
		if (auto pHash = theCore->HashDB()->GetHash(FileHash)) {
			if(m_CurEnclaveGuid.IsNull() || pHash->HasEnclave(m_CurEnclaveGuid))
				FilesSigned++;
		}
		if (!SignerHash.isEmpty()) {
			Certs[SignerHash] = SignerName;
			if (auto pHash = theCore->HashDB()->GetHash(SignerHash))
				if(m_CurEnclaveGuid.IsNull() || pHash->HasEnclave(m_CurEnclaveGuid))
					CertsSigned++;
		}
		if (!IssuerHash.isEmpty()) {
			CAs[IssuerHash] = IssuerName;
			if (auto pHash = theCore->HashDB()->GetHash(IssuerHash))
				if(m_CurEnclaveGuid.IsNull() || pHash->HasEnclave(m_CurEnclaveGuid))
					CAsSigned++;
		}
	}

	m_pSignFile->setEnabled(Files.count() > FilesSigned);
	m_pRemoveSig->setEnabled(FilesSigned > 0);

	m_pSignCert->setEnabled(Certs.count() > CertsSigned);
	m_pRemoveCert->setEnabled(CertsSigned > 0);

	m_pSignCA->setEnabled(CAs.count() > CAsSigned);
	m_pRemoveCA->setEnabled(CAsSigned > 0);

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
	QMap<QByteArray, QString> CAs;
	foreach(SLibraryItemPtr pItem, Items)
	{
		if (pItem->pLibrary)
		{
			Files.append(pItem->pLibrary->GetPath());
			Certs[pItem->Info.SignInfo.GetSignerHash()] = pItem->Info.SignInfo.GetSignerName();
			CAs[pItem->Info.SignInfo.GetIssuerHash()] = pItem->Info.SignInfo.GetIssuerName();
		}
		else if (pItem->pProg)
		{
			Files.append(pItem->pProg->GetPath());
			Certs[pItem->pProg->GetSignInfo().GetSignerHash()] = pItem->pProg->GetSignInfo().GetSignerName();
			CAs[pItem->pProg->GetSignInfo().GetIssuerHash()] = pItem->pProg->GetSignInfo().GetIssuerName();
		}
	}

	QList<STATUS> Results;
	if (pAction == m_pSignFile) {
		foreach(const QString& File, Files)
			Results << theCore->HashDB()->AllowFile(File, m_CurEnclaveGuid);
		//Status = theGUI->SignFiles(Files);
	}
	else if (pAction == m_pRemoveSig) {
		foreach(const QString& File, Files)
			Results << theCore->HashDB()->ClearFile(File, m_CurEnclaveGuid);
		//Status = theCore->RemoveFileSignature(Files);
	}
	else if (pAction == m_pSignCert) {
		for(auto I = Certs.begin(); I != Certs.end(); ++I)
			Results << theCore->HashDB()->AllowCert(I.key(), I.value(), m_CurEnclaveGuid);
		//Status = theGUI->SignCerts(Certs);
	}
	else if (pAction == m_pRemoveCert) {
		for(auto I = Certs.begin(); I != Certs.end(); ++I)
			Results << theCore->HashDB()->ClearCert(I.key(), m_CurEnclaveGuid);
		//Status = theCore->RemoveCertSignature(Certs);
	}
	else if (pAction == m_pSignCA) {
		if (QMessageBox::question(this, "MajorPrivacy", tr("Do you really want to allow the Issuing Certificate Authoricy Certificate? This will allow all Signign Certificate issued by this CA."), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
			return;

		for(auto I = CAs.begin(); I != CAs.end(); ++I) 
			Results << theCore->HashDB()->AllowCert(I.key(), I.value(), m_CurEnclaveGuid);
		//Status = theGUI->SignCACerts(Certs);
	}
	else if (pAction == m_pRemoveCA) {
		for(auto I = CAs.begin(); I != CAs.end(); ++I)
			Results <<  theCore->HashDB()->ClearCert(I.key(), m_CurEnclaveGuid);
		//Status = theCore->RemoveCACertSignature(Certs);
	}
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
	theGUI->CheckResults(Results, this);
}

void CLibraryView::OnCleanUpLibraries()
{
	if (QMessageBox::question(this, "MajorPrivacy", tr("Are you sure you want to cleanup the library list for the current program items?\nThis will remove all library entries for for not currently loaded libraries."), 
		QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

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

void CLibraryView::OnRefresh() 
{ 
	if(m_pBtnInfo->isChecked())
		theCore->ClearCIInfoCache();
	m_FullRefresh = true; 
}