#include "pch.h"
#include "LibraryInfoView.h"
#include "../MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Library/Helpers/CertUtil.h"

CLibraryInfoView::CLibraryInfoView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QGridLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pInfo = new CPanelWidgetEx();
	//m_pInfo->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	m_pInfo->GetTree()->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	m_pInfo->GetTree()->setHeaderLabels(tr("Name|Signer|Valid|SHA1 Fingerprint|Certificate Fingerprint").split("|"));
	m_pMainLayout->addWidget(m_pInfo);

	QByteArray Columns = theConf->GetBlob("MainWindow/LibraryInfoView_Columns");
	if (Columns.isEmpty()) {
		m_pInfo->GetTree()->setColumnWidth(0, 300);
	} else
		m_pInfo->GetTree()->header()->restoreState(Columns);
}

CLibraryInfoView::~CLibraryInfoView()
{
	theConf->SetBlob("MainWindow/LibraryInfoView_Columns", m_pInfo->GetTree()->header()->saveState());
}

static QString ToHex(const std::vector<BYTE>& Data)
{
	QString Result;
	for (auto Byte : Data)
		Result += QString("%1").arg(Byte, 2, 16, QChar('0')).toUpper();
	return Result;
}

static SCICertificatePtr AddCertificateChain(QTreeWidgetItem* pItem, const SCICertificatePtr& pCert, const QByteArray& Hash = QByteArray(), const QByteArray& CAHash = QByteArray())
{
	QTreeWidgetItem* pSigner = new QTreeWidgetItem();
	pSigner->setText(CLibraryInfoView::eName, QString::fromStdWString(pCert->Subject));
	//pSigner->setText(CLibraryInfoView::eSigner, );
	pSigner->setText(CLibraryInfoView::eSHA1, ToHex(pCert->SHA1));
	pItem->addChild(pSigner);
	pSigner->setExpanded(true);

	if(pCert->Issuer)
		AddCertificateChain(pSigner, pCert->Issuer, CAHash);

	if (!Hash.isEmpty())
	{
		QByteArray SignerHash;
		switch (Hash.size())
		{
		case 20:	SignerHash = QByteArray((char*)pCert->SHA1.data(), pCert->SHA1.size()); break;
		case 32:	SignerHash = QByteArray((char*)pCert->SHA256.data(), pCert->SHA256.size()); break;
		case 48:	SignerHash = QByteArray((char*)pCert->SHA384.data(), pCert->SHA384.size()); break;
		case 64:	SignerHash = QByteArray((char*)pCert->SHA512.data(), pCert->SHA512.size()); break;
		}
		pSigner->setText(CLibraryInfoView::eSHAX, SignerHash.toHex().toUpper());
		if (SignerHash == Hash) {
			QFont fnt = pSigner->font(CLibraryInfoView::eName); fnt.setBold(true); pSigner->setFont(CLibraryInfoView::eName, fnt); pSigner->setFont(CLibraryInfoView::eSHAX, fnt);
			return pCert;
		}
	}

	pSigner->setText(CLibraryInfoView::eSHAX, ToHex(pCert->SHA256));
	return nullptr;
}

void CLibraryInfoView::Sync()
{
	if (!isVisible())
		return;

	QList<SLibraryItemPtr> Items = ((CLibraryView*)sender())->GetSelectedItemsWithChildren();

	m_pInfo->GetTree()->clear();

	static QIcon DllIcon = QIcon(":/Icons/Dll.png");
	
	QMap<QString, QTreeWidgetItem*> Old;
	for(int i = 0; i < m_pInfo->GetTree()->topLevelItemCount(); i++) {
		QTreeWidgetItem* pItem = m_pInfo->GetTree()->topLevelItem(i);
		QString Path = pItem->data(eName, Qt::UserRole).toString();
		Q_ASSERT(!Old.contains(Path));
		Old.insert(Path, pItem);
	}	

	CProgressDialogHelper ProgressHelper(theGUI->m_pProgressDialog, tr("Loading %1"), Items.count());

	QSet<QString> Paths;
	for (auto pInfo : Items)
	{
		QString Path = pInfo->pLibrary ? pInfo->pLibrary->GetPath() : pInfo->pProg->GetPath();
		QTreeWidgetItem* pItem = Old.take(Path);
		if (pItem || Paths.contains(Path))
			continue; 
		
		QByteArray Hash = pInfo->pLibrary ? pInfo->Info.SignInfo.GetSignerHash() : pInfo->pProg->GetSignInfo().GetSignerHash();
		QByteArray CAHash = pInfo->pLibrary ? pInfo->Info.SignInfo.GetIssuerHash() : pInfo->pProg->GetSignInfo().GetIssuerHash();

		Paths.insert(Path);
		pItem = new QTreeWidgetItem();
		m_pInfo->GetTree()->addTopLevelItem(pItem);

		pItem->setText(eName, pInfo->pLibrary ? pInfo->pLibrary->GetName() : pInfo->pProg->GetNameEx());
		pItem->setToolTip(eName, Path);
		pItem->setIcon(eName, pInfo->pLibrary ? DllIcon : pInfo->pProg->GetIcon());	
		pItem->setData(eName, Qt::UserRole, Path);

		pItem->setText(CLibraryInfoView::eSHAX, Hash.toHex().toUpper());

		if (!ProgressHelper.Next(Path))
			break;

		std::wstring filePath = Path.toLower().toStdWString();

		SCICertificatePtr FoundCert;
		SEmbeddedCIInfoPtr pEmbeddedInfo = theCore->GetEmbeddedCIInfo(filePath);
		if (pEmbeddedInfo)
		{
			pItem->setText(eValid, pEmbeddedInfo->EmbeddedSignatureValid ? tr("Yes") : tr("INVALID"));

			for (auto& Signer : pEmbeddedInfo->EmbeddedSigners)
				FoundCert = AddCertificateChain(pItem, Signer, Hash, CAHash);
		}
		
		SCatalogCIInfoPtr pCatalogInfo = theCore->GetCatalogCIInfo(filePath);
		if (pCatalogInfo && !pCatalogInfo->CatalogSigners.empty())
		{
			if(!pEmbeddedInfo)
				pItem->setText(eValid, tr("Yes (Catalog)"));

			for (auto& Catalog : pCatalogInfo->CatalogSigners)
			{
				QTreeWidgetItem* pCatalog = new QTreeWidgetItem();
				pCatalog->setText(eName, Split2(QString::fromStdWString(Catalog.first), "\\", true).second);
				pItem->addChild(pCatalog);

				for (auto& Signer : Catalog.second) {
					SCICertificatePtr FoundCert2 = AddCertificateChain(pCatalog, Signer, Hash, CAHash);
					if (FoundCert2)
					{
						pCatalog->setText(CLibraryInfoView::eSigner, QString::fromStdWString(FoundCert2->Subject));
						pCatalog->setText(CLibraryInfoView::eSHA1, ToHex(FoundCert2->SHA1));
						pCatalog->setText(CLibraryInfoView::eSHAX, Hash.toHex().toUpper());
						QFont fnt = pCatalog->font(CLibraryInfoView::eName); fnt.setBold(true); pCatalog->setFont(CLibraryInfoView::eName, fnt); pCatalog->setFont(CLibraryInfoView::eSHAX, fnt);
						if (!FoundCert)
							FoundCert = FoundCert2;
					}
				}
			}
		}

		if (FoundCert)
		{
			QByteArray SignerHash;
			switch (Hash.size())
			{
			case 20:	SignerHash = QByteArray((char*)FoundCert->SHA1.data(), FoundCert->SHA1.size()); break;
			case 32:	SignerHash = QByteArray((char*)FoundCert->SHA256.data(), FoundCert->SHA256.size()); break;
			case 48:	SignerHash = QByteArray((char*)FoundCert->SHA384.data(), FoundCert->SHA384.size()); break;
			case 64:	SignerHash = QByteArray((char*)FoundCert->SHA512.data(), FoundCert->SHA512.size()); break;
			}
			if (SignerHash == Hash) {
				pItem->setText(CLibraryInfoView::eSigner, QString::fromStdWString(FoundCert->Subject));
				pItem->setText(CLibraryInfoView::eSHA1, ToHex(FoundCert->SHA1));
				QFont fnt = pItem->font(CLibraryInfoView::eSHAX); fnt.setBold(true); pItem->setFont(CLibraryInfoView::eSHAX, fnt);
			}
		}
	}

	ProgressHelper.Done();

	foreach(QTreeWidgetItem* pItem, Old)
		delete pItem;
}