#include "pch.h"
#include "VolumeView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Volumes/VolumeManager.h"
#include "../MiscHelpers/Common/CheckableMessageBox.h"
#include "Windows/VolumeWindow.h"
#include "../MajorPrivacy.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CVolumeView::CVolumeView(QWidget *parent)
	:CPanelViewEx<CVolumeModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	connect(theGUI, SIGNAL(OnMountVolume()), this, SLOT(MountVolume()));
	connect(theGUI, SIGNAL(OnUnmountAllVolumes()), this, SLOT(OnUnmountAllVolumes()));
	connect(theGUI, SIGNAL(OnCreateVolume()), this, SLOT(OnCreateVolume()));

	m_pCreateVolume = m_pMenu->addAction(QIcon(":/Icons/AddVolume.png"), tr("Create Volume"), this, SLOT(OnCreateVolume()));
	m_pMenu->addSeparator();
	m_pMountVolume = m_pMenu->addAction(QIcon(":/Icons/MountVolume.png"), tr("Mount Volume"), this, SLOT(OnMountVolume()));
	m_pUnmountVolume = m_pMenu->addAction(QIcon(":/Icons/UnmountVolume.png"), tr("Unmount Volume"), this, SLOT(OnUnmountVolume()));
	m_pChangeVolumePassword = m_pMenu->addAction(QIcon(":/Icons/VolumePW.png"), tr("Change Volume Password"), this, SLOT(OnChangeVolumePassword()));
	m_pRemoveVolume = m_pMenu->addAction(QIcon(":/Icons/Remove.png"), tr("Remove Volume"), this, SLOT(OnRemoveVolume()));
	m_pMenu->addSeparator();
	m_pMountAndAddVolume = m_pMenu->addAction(QIcon(":/Icons/MountVolume.png"), tr("Add and Mount Volume Image"), this, SLOT(MountVolume()));
	m_pUnmountAllVolumes = m_pMenu->addAction(QIcon(":/Icons/UnmountVolume.png"), tr("Unmount All Volumes"), this, SLOT(OnUnmountAllVolumes()));

	QByteArray Columns = theConf->GetBlob("MainWindow/VolumeView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pToolBar->setFixedHeight(30);
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pToolBar->addAction(m_pCreateVolume);
	m_pToolBar->addSeparator();
	m_pToolBar->addAction(m_pMountAndAddVolume);
	m_pToolBar->addAction(m_pUnmountAllVolumes);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	//pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);

	AddPanelItemsToMenu();
}

CVolumeView::~CVolumeView()
{
	theConf->SetBlob("MainWindow/VolumeView_Columns", m_pTreeView->saveState());
}

void CVolumeView::Sync()
{
	theCore->VolumeManager()->Update();
	m_pItemModel->Sync(theCore->VolumeManager()->List());
}

QString CVolumeView::GetSelectedVolumePath()
{
	// Note: This function must return an Empty but not a Null string when no volume is selected

	QList<CVolumePtr> Volumes = GetSelectedItems();
	if (Volumes.count() != 1) return "";
	QString DevicePath = Volumes.at(0)->GetDevicePath();
	if(DevicePath.isNull()) return "";
	return DevicePath;
}

QString CVolumeView::GetSelectedVolumeImage()
{
	// Note: This function must return an Empty but not a Null string when no volume is selected

	QList<CVolumePtr> Volumes = GetSelectedItems();
	if (Volumes.count() != 1) return "";
	QString ImagePath = Volumes.at(0)->GetImagePath();
	if (ImagePath.isNull()) return "";
	return ImagePath;
}

/*void CVolumeView::OnDoubleClicked(const QModelIndex& Index)
{

}*/

void CVolumeView::OnMenu(const QPoint& Point)
{
	QList<CVolumePtr> Volumes = GetSelectedItems();

	int iMountedCount = 0;
	int iSelectedCount = 0;
	foreach(CVolumePtr pVolume, Volumes) {
		if (pVolume->GetStatus() == CVolume::eMounted)
			iMountedCount++;
		iSelectedCount++;
	}

	m_pMountVolume->setEnabled(iMountedCount == 0 && iSelectedCount == 1);
	m_pUnmountVolume->setEnabled(iMountedCount > 0);
	m_pChangeVolumePassword->setEnabled(iMountedCount == 0 && iSelectedCount == 1);
	m_pRemoveVolume->setEnabled(iMountedCount == 0 && iSelectedCount > 0);

	CPanelView::OnMenu(Point);
}

void CVolumeView::OnMountVolume()
{
	QList<CVolumePtr> Volumes = GetSelectedItems();
	if (Volumes.count() != 1) return;

	MountVolume(Volumes[0]->GetImagePath());
}

void CVolumeView::MountVolume(QString Path)
{
	if (Path.isEmpty()) {
		Path = QFileDialog::getOpenFileName(this, tr("Select Private Volume"), QString(), tr("Private Volume Files (*.pv)"));
		if (Path.isEmpty()) 
			return;
	}

	CVolumeWindow window(tr("Mount Secure Volume"), CVolumeWindow::eMount, this);
	if (theGUI->SafeExec(&window) != 1)
		return;
	QString Password = window.GetPassword();
	QString MountPoint = window.GetMountPoint();
	bool bProtect = window.UseProtection();

	STATUS Status = theCore->MountVolume(Path, MountPoint, Password, bProtect);
	theGUI->CheckResults(QList<STATUS>() << Status, this);
}

void CVolumeView::OnUnmountVolume()
{
	QList<STATUS> Results;

	QList<CVolumePtr> Volumes = GetSelectedItems();
	foreach(CVolumePtr pVolume, Volumes) 
	{
		STATUS Status = theCore->DismountVolume(pVolume->GetDevicePath());
		Results.append(Status);
	}

	theGUI->CheckResults(Results, this);
}

void CVolumeView::OnUnmountAllVolumes()
{
	STATUS Status = theCore->DismountAllVolumes();
	theGUI->CheckResults(QList<STATUS>() << Status, this);
}

void CVolumeView::OnCreateVolume()
{
	QString Path = QFileDialog::getSaveFileName(this, tr("Select Private Volume"), QString(), tr("Private Volume Files (*.pv)")).replace("/","\\");
	if (Path.isEmpty()) return;

	CVolumeWindow window(tr("Creating new volume image, please enter a secure password, and choose a disk image size."), CVolumeWindow::eNew, this);
	if (theGUI->SafeExec(&window) != 1)
		return;
	QString Password = window.GetPassword();
	quint64 ImageSize = window.GetImageSize();

	if (theConf->GetBool("Options/WarnBoxCrypto", true)) {
		bool State = false;
		if(CCheckableMessageBox::question(this, "MajorPrivacy",
			tr("This volume content will be placed in an encrypted container file, "
				"please note that any corruption of the container's header will render all its content permanently inaccessible. "
				"Corruption can occur as a result of a BSOD, a storage hardware failure, or a malicious application overwriting random files. "
				"This feature is provided under a strict <b>No Backup No Mercy</b> policy, YOU the user are responsible for the data you put into an encrypted box. "
				"<br /><br />"
				"IF YOU AGREE TO TAKE FULL RESPONSIBILITY FOR YOUR DATA PRESS [YES], OTHERWISE PRESS [NO].")
			, tr("Don't show this message again."), &State, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::No, QMessageBox::Warning) != QDialogButtonBox::Yes)
			return;

		if (State)
			theConf->SetValue("Options/WarnBoxCrypto", false);
	}

	STATUS Status = theCore->CreateVolume(Path, Password, ImageSize);
	if (Status.IsSuccess())
		theCore->VolumeManager()->AddVolume(Path);
	theGUI->CheckResults(QList<STATUS>() << Status, this);
}

void CVolumeView::OnChangeVolumePassword()
{
	QString Path = QFileDialog::getOpenFileName(this, tr("Select Private Volume"), QString(), tr("Private Volume Files (*.pv)"));
	if (Path.isEmpty()) return;

	CVolumeWindow window(tr("Change Volume Password."), CVolumeWindow::eChange, this);
	if (theGUI->SafeExec(&window) != 1)
		return;
	QString OldPassword = window.GetPassword();
	QString NewPassword = window.GetNewPassword();

	STATUS Status = theCore->ChangeVolumePassword(Path, OldPassword, NewPassword);
	theGUI->CheckResults(QList<STATUS>() << Status, this);
}

void CVolumeView::OnRemoveVolume()
{
	if (QMessageBox::question(this, "MajorPrivacy", tr("Do you want to remove selected Volumes from list?\nNote: The volume image files will remain intact and must be manually deleted."), QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
		return;

	//QList<STATUS> Results;

	QList<CVolumePtr> Volumes = GetSelectedItems();
	foreach(CVolumePtr pVolume, Volumes)
	{
		theCore->VolumeManager()->RemoveVolume(pVolume->GetImagePath());
	}

	//theGUI->CheckResults(Results, this);
}