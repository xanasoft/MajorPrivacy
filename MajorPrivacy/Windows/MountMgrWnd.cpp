#include "pch.h"
#include "MountMgrWnd.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include "../Library/Helpers/NtPathMgr.h"

CMountMgrWnd::CMountMgrWnd(QWidget *parent)
:QMainWindow(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);

	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout();
	m_pMainWidget->setLayout(m_pMainLayout);
	setCentralWidget(m_pMainWidget);

	m_pPanel = new CPanelWidgetEx();
	//m_pPanel->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	m_pPanel->GetView()->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));

	m_pTree = m_pPanel->GetTree();
	m_pTree->setHeaderLabels(tr("Volume|Device Path").split("|"));

	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTree->setStyle(pStyle);
	
	m_pMainLayout->addWidget(m_pTree);

	m_pButtons = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Reset, Qt::Horizontal, this);
	QObject::connect(m_pButtons, SIGNAL(clicked(QAbstractButton *)), this, SLOT(OnClicked(QAbstractButton*)));
	m_pButtons->button(QDialogButtonBox::Reset)->setText(tr("Refresh"));
	m_pMainLayout->addWidget(m_pButtons);
	
	setWindowState(Qt::WindowNoState);
	restoreGeometry(theConf->GetBlob("MountManagerWnd/Window_Geometry"));
	restoreState(theConf->GetBlob("MountManagerWnd/Window_State"));

	m_pTree->header()->restoreState(theConf->GetBlob("MountManagerWnd/TreeView_Columns"));

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

CMountMgrWnd::~CMountMgrWnd()
{
	theConf->SetBlob("MountManagerWnd/TreeView_Columns",m_pTree->header()->saveState());

	theConf->SetBlob("MountManagerWnd/Window_Geometry", saveGeometry());
	theConf->SetBlob("MountManagerWnd/Window_State", saveState());
}

void CMountMgrWnd::OnClicked(QAbstractButton* pButton)
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

void CMountMgrWnd::Load()
{
	QMap<QString, QTreeWidgetItem*> Old;
	for(int i = 0; i < m_pTree->topLevelItemCount(); i++) {
		QTreeWidgetItem* pItem = m_pTree->topLevelItem(i);
		QString Name = pItem->text(eName);
		Q_ASSERT(!Old.contains(Name));
		Old.insert(Name, pItem);
	}	

	auto DosDrives = CNtPathMgr::Instance()->GetDosDrives();
	auto VolumeGuids = CNtPathMgr::Instance()->GetVolumeGuids();
	auto MountPoints = CNtPathMgr::Instance()->GetMountPoints();

	//
	// Process all volumes known to the mount manager
	//

	for(auto& pGuid: VolumeGuids)
	{
		QString Guid = QString::fromWCharArray(pGuid->Guid);
		QString Name = "\\??\\Volume" + Guid;
		QString DevicePath = QString::fromStdWString(pGuid->DevicePath);
		QTreeWidgetItem* pItem = Old.take(Name);
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setText(eName, Name);
			pItem->setData(eName, Qt::UserRole, Guid);
			m_pTree->addTopLevelItem(pItem);
			pItem->setText(ePath, DevicePath);
		}

		//
		// Add all Dos Drives and Mount Points for this volume
		//

		QMap<QString, QTreeWidgetItem*> OldValues;
		for(int i = 0; i < pItem->childCount(); i++)
		{
			QTreeWidgetItem* pSubItem = pItem->child(i);
			QString Name = pSubItem->text(eName);
			Q_ASSERT(!OldValues.contains(Name));
			OldValues.insert(Name, pSubItem);
		}	

		for (auto& pDrive: DosDrives)
		{
			if(!pDrive || DevicePath.compare(QString::fromStdWString(pDrive->DevicePath), Qt::CaseInsensitive) != 0)
				continue;

			QString Name = QString(pDrive->DriveLetter) + ":";
			QTreeWidgetItem* pSubItem = OldValues.take(Name);
			if (!pSubItem)
			{
				pSubItem = new QTreeWidgetItem();
				pSubItem->setText(eName, Name);
				pItem->addChild(pSubItem);
			}

			pDrive.reset();
		}

		for (auto& pMount : MountPoints)
		{
			if (DevicePath.compare(QString::fromStdWString(pMount->DevicePath), Qt::CaseInsensitive) != 0)
				continue;

			QString Name = QString::fromStdWString(pMount->MountPoint);
			QTreeWidgetItem* pSubItem = OldValues.take(Name);
			if (!pSubItem)
			{
				pSubItem = new QTreeWidgetItem();
				pSubItem->setText(eName, Name);
				pItem->addChild(pSubItem);
			}

		}

		foreach(QTreeWidgetItem* pItem, OldValues)
			delete pItem;
	}

	//
	// Process remaining dos drives which are not know to the mount manager
	//

	for (auto& pDrive: DosDrives)
	{
		if(!pDrive)
			continue;

		QString Name = "\\??\\" + QString(pDrive->DriveLetter) + ":";
		QTreeWidgetItem* pItem = Old.take(Name);
		if (!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setText(eName, Name);
			m_pTree->addTopLevelItem(pItem);
		}

		pItem->setText(ePath, QString::fromStdWString(pDrive->DevicePath));
	}

	foreach(QTreeWidgetItem* pItem, Old)
		delete pItem;

	m_pTree->expandAll();
}