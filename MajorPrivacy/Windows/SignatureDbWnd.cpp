#include "pch.h"
#include "SignatureDbWnd.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include "../Library/Helpers/NtPathMgr.h"
#include "./Common/QtVariant.h"	

CSignatureDbWnd::CSignatureDbWnd(QWidget *parent)
:QMainWindow(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);

	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout();
	m_pMainWidget->setLayout(m_pMainLayout);
	setCentralWidget(m_pMainWidget);

	m_pPanel = new CPanelWidgetEx();
	//m_pPanel->GetView()->setItemDelegate(theGUI->GetItemDelegate());

	m_pDelete = new QAction(QIcon(":/Icons/Remove.png"), tr("Delete Signature"), this);
	connect(m_pDelete, SIGNAL(triggered()), this, SLOT(OnAction()));
	m_pPanel->GetMenu()->insertAction(m_pPanel->GetMenu()->actions().first(), m_pDelete);
	m_pDelete->setShortcut(QKeySequence::Delete);
	m_pDelete->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pDelete);

	m_pTree = m_pPanel->GetTree();
	m_pTree->setHeaderLabels(tr("Name|Path").split("|"));

	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTree->setStyle(pStyle);
	
	m_pMainLayout->addWidget(m_pTree);

	m_pButtons = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Reset, Qt::Horizontal, this);
	QObject::connect(m_pButtons, SIGNAL(clicked(QAbstractButton *)), this, SLOT(OnClicked(QAbstractButton*)));
	m_pButtons->button(QDialogButtonBox::Reset)->setText(tr("Refresh"));
	m_pMainLayout->addWidget(m_pButtons);
	
	setWindowState(Qt::WindowNoState);
	restoreGeometry(theConf->GetBlob("SignatureDbWnd/Window_Geometry"));
	restoreState(theConf->GetBlob("SignatureDbWnd/Window_State"));

	m_pTree->header()->restoreState(theConf->GetBlob("SignatureDbWnd/TreeView_Columns"));

	Load();

	connect(theCore, &CPrivacyCore::DevicesChanged, this, [this]() {
		static bool DeviceChangePending = false;
		if(DeviceChangePending) return;
		DeviceChangePending = true;
		QTimer::singleShot(100, this, [this]() {
			DeviceChangePending = false;
			Load();
		});
	});
}

CSignatureDbWnd::~CSignatureDbWnd()
{
	theConf->SetBlob("SignatureDbWnd/TreeView_Columns",m_pTree->header()->saveState());

	theConf->SetBlob("SignatureDbWnd/Window_Geometry", saveGeometry());
	theConf->SetBlob("SignatureDbWnd/Window_State", saveState());
}

void CSignatureDbWnd::OnClicked(QAbstractButton* pButton)
{
	ASSERT(m_pButtons);
	switch(m_pButtons->buttonRole(pButton))
	{
	case QDialogButtonBox::RejectRole:
		close();
		break;
	case QDialogButtonBox::ResetRole:
		Load();
		break;
	}
}

void CSignatureDbWnd::Load()
{
	std::function<void(const QString&, QTreeWidgetItem*)> ProcessDir;

	auto MakeEntry = [&](const QString& Path, const int Pos, QMap<QString, QTreeWidgetItem*>& Old, QTreeWidgetItem* pParent) {

		QString Name = Path.mid(Pos);

		QTreeWidgetItem* pItem = Old.take(Path);
		if (!pItem) {
			pItem = new QTreeWidgetItem();
			pItem->setData(eName, Qt::UserRole, Path);
			if (Name.endsWith("\\")) {
				pItem->setText(eName, Name.left(Name.length() - 1));
				pItem->setIcon(eName, QIcon(":/Icons/Directory.png"));
			} else {
				pItem->setText(eName, Name);
				pItem->setIcon(eName, QIcon(":/Icons/SignFile.png"));
			}
			if(!pParent)
				m_pTree->addTopLevelItem(pItem);
			else
				pParent->addChild(pItem);
		}

		if (Path.endsWith("\\"))
			ProcessDir(Path, pItem);
		else
		{
			auto ret = theCore->ReadConfigFile(Path);
			if (ret.IsError()) 
				return;

			QByteArray Data = ret.GetValue();
			CBuffer Buffer(Data.constData(), Data.size(), true);

			QtVariant SigData;
			//try {
			auto res = SigData.FromPacket(&Buffer, true);
			//} catch (const CException&) {
			//	return ERR(STATUS_UNSUCCESSFUL);
			//}
			if (res != QtVariant::eErrNone) 
				return;

			if (SigData[API_S_TYPE].AsQStr() == L"File") {
				pItem->setText(ePath, SigData[API_S_FILE_PATH].AsQStr());
			}
		}
	};

	ProcessDir = [&](const QString& Path, QTreeWidgetItem* pParent) {

		auto ret = theCore->ListConfigFiles(Path);
		if (ret.IsError()) 
			return;
		QStringList SubFiles = ret.GetValue();

		QMap<QString, QTreeWidgetItem*> Old;
		for (int i = 0; i < pParent->childCount(); i++) {
			QTreeWidgetItem* pItem = pParent->child(i);
			Old.insert(pItem->data(eName, Qt::UserRole).toString(), pItem);
		}

		foreach(const QString & FileName, SubFiles)
			MakeEntry(FileName, Path.length(), Old, pParent);

		foreach(QTreeWidgetItem* pItem, Old)
			delete pItem;
	};

	QString SignDB = "\\sig_db\\";
	auto ret = theCore->ListConfigFiles(SignDB);
	if (ret.IsError()) {
		m_pTree->clear();
		return;
	}
	QStringList Files = ret.GetValue();

	QMap<QString, QTreeWidgetItem*> Old;
	for(int i = 0; i < m_pTree->topLevelItemCount(); i++) {
		QTreeWidgetItem* pItem = m_pTree->topLevelItem(i);
		Old.insert(pItem->data(eName, Qt::UserRole).toString(), pItem);
	}	

	foreach(const QString & FileName, Files)
		MakeEntry(FileName, SignDB.length(), Old, NULL);

	foreach(QTreeWidgetItem* pItem, Old)
		delete pItem;

	m_pTree->expandAll();
}

void CSignatureDbWnd::OnAction()
{
	QTreeWidgetItem* pItem = m_pTree->currentItem();
	if (!pItem) return;

	QString Path = pItem->data(eName, Qt::UserRole).toString();
	if (Path.isEmpty()) return;
	/*if (Path.endsWith("\\")) {
		QMessageBox::information(this, tr("MajorPrivacy"), tr("You can only delete files, not directories."));
		return;
	}*/
	if (QMessageBox::question(this, tr("MajorPrivacy"), tr("Are you sure you want to delete the signature?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	STATUS Status = theCore->RemoveConfigFile(Path);
	if (!Status.IsError())
		Load();
	theGUI->CheckResults(QList<STATUS>() << Status, this);

}