#include "pch.h"
#include "VolumeView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Volumes/VolumeManager.h"
#include "../MiscHelpers/Common/CheckableMessageBox.h"
#include "Windows/VolumeWindow.h"
#include "Windows/VolumeConfigWnd.h"
#include "../MajorPrivacy.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Core/Access/AccessManager.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Library/Helpers/NtPathMgr.h"
#include "../Core/Enclaves/EnclaveManager.h"

CVolumeView::CVolumeView(QWidget *parent)
	:CPanelViewEx<CVolumeModel>(parent)
{
	m_pTreeView->setColumnReset(2);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));
	
	connect(theGUI, SIGNAL(OnMountVolume()), this, SLOT(MountVolume()));
	connect(theGUI, SIGNAL(OnUnmountAllVolumes()), this, SLOT(OnUnmountAllVolumes()));
	connect(theGUI, SIGNAL(OnCreateVolume()), this, SLOT(OnCreateVolume()));

	m_pMountVolume = m_pMenu->addAction(QIcon(":/Icons/MountVolume.png"), tr("Mount Volume"), this, SLOT(OnMountVolume()));
	m_pUnmountVolume = m_pMenu->addAction(QIcon(":/Icons/UnmountVolume.png"), tr("Unmount Volume"), this, SLOT(OnUnmountVolume()));
	m_pChangeVolumePassword = m_pMenu->addAction(QIcon(":/Icons/VolumePW.png"), tr("Change Volume Password"), this, SLOT(OnChangeVolumePassword()));
	m_pChangeVolumeConfig = m_pMenu->addAction(QIcon(":/Icons/EditIni.png"), tr("Change Volume Configuration"), this, SLOT(OnChangeVolumeConfig()));
	m_pAddVolumeEnclave = m_pMenu->addAction(QIcon(":/Icons/Enclave.png"), tr("Add Volume Enclave"), this, SLOT(OnAddVolumeEnclave()));
	m_pRenameVolume = m_pMenu->addAction(QIcon(":/Icons/Rename.png"), tr("Rename Volume/Folder"), this, SLOT(OnRenameVolume()));
	m_pRemoveVolume = m_pMenu->addAction(QIcon(":/Icons/Remove.png"), tr("Remove Volume/Folder"), this, SLOT(OnRemoveVolume()));
	m_pRemoveVolume->setShortcut(QKeySequence::Delete);
	m_pRemoveVolume->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pRemoveVolume);
	m_pMenu->addSeparator();
	m_pCreateVolume = m_pMenu->addAction(QIcon(":/Icons/AddVolume.png"), tr("Create Volume"), this, SLOT(OnCreateVolume()));
	m_pMountAndAddVolume = m_pMenu->addAction(QIcon(":/Icons/MountVolume.png"), tr("Add and Mount Volume Image"), this, SLOT(MountVolume()));
	m_pAddFolder = m_pMenu->addAction(QIcon(":/Icons/AddFolder.png"), tr("Add Folder"), this, SLOT(OnAddFolder()));
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
	m_pToolBar->addAction(m_pMountAndAddVolume);
	m_pToolBar->addAction(m_pAddFolder);
	m_pToolBar->addSeparator();
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
	QString DevicePath;
	if (Volumes.at(0)->IsFolder()) 
		DevicePath = QString::fromStdWString(CNtPathMgr::Instance()->TranslateDosToNtPath(Volumes.at(0)->GetImagePath().toStdWString()));
	else
		DevicePath = Volumes.at(0)->GetDevicePath();
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

void CVolumeView::OnDoubleClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	CVolumePtr pVolume = m_pItemModel->GetItem(ModelIndex);
	if (pVolume.isNull() || pVolume->IsFolder())
		return;

	if (!pVolume->IsMounted())
		MountVolume(pVolume->GetImagePath());
	/*else if (QMessageBox::question(this, "MajorPrivacy", tr("Do you want to unmount the selected volume?"), QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
	{
		STATUS Status = theCore->DismountVolume(pVolume->GetDevicePath());
		theGUI->CheckResults(QList<STATUS>() << Status, this);
	}*/
	else
		OpenVolumeDialog(pVolume);
}

void CVolumeView::OpenVolumeDialog(const CVolumePtr& pVolume)
{
	CVolumeConfigWnd* pVolumeWnd = new CVolumeConfigWnd(pVolume);
	pVolumeWnd->show();
}

void CVolumeView::OnMenu(const QPoint& Point)
{
	QList<CVolumePtr> Volumes = GetSelectedItems();

	int iMountedCount = 0;
	int iVolumeCount = 0;
	int iFolderCount = 0;
	foreach(CVolumePtr pVolume, Volumes) {
		if (pVolume->IsFolder())
			iFolderCount++;
		else {
			if (pVolume->IsMounted())
				iMountedCount++;
			iVolumeCount++;
		}
	}

	m_pMountVolume->setEnabled(iMountedCount == 0 && iVolumeCount == 1);
	m_pUnmountVolume->setEnabled(iMountedCount > 0);
	m_pChangeVolumePassword->setEnabled(iMountedCount == 0 && iVolumeCount == 1 && iFolderCount == 0);
	m_pAddVolumeEnclave->setEnabled(iMountedCount > 0);
	m_pChangeVolumeConfig->setEnabled(iMountedCount == 1);
	m_pRenameVolume->setEnabled(iVolumeCount + iFolderCount == 1);
	m_pRemoveVolume->setEnabled(iMountedCount == 0 && iVolumeCount + iFolderCount > 0);

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
	bool bLockdown = window.UseLockdown();

	STATUS Status = theCore->MountVolume(Path, MountPoint, Password, bProtect, bLockdown);
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
	QList<CVolumePtr> Volumes = GetSelectedItems();
	if (Volumes.count() != 1) return;

	QString Path = Volumes[0]->GetImagePath();
	//QString Path = QFileDialog::getOpenFileName(this, tr("Select Private Volume"), QString(), tr("Private Volume Files (*.pv)"));
	//if (Path.isEmpty()) return;

	CVolumeWindow window(tr("Change Volume Password."), CVolumeWindow::eChange, this);
	if (theGUI->SafeExec(&window) != 1)
		return;
	QString OldPassword = window.GetPassword();
	QString NewPassword = window.GetNewPassword();

	STATUS Status = theCore->ChangeVolumePassword(Path, OldPassword, NewPassword);
	theGUI->CheckResults(QList<STATUS>() << Status, this);
}

void CVolumeView::OnChangeVolumeConfig()
{
	QList<CVolumePtr> Volumes = GetSelectedItems();
	if (Volumes.count() != 1) return;

	OpenVolumeDialog(Volumes.at(0));
}

void CVolumeView::OnAddVolumeEnclave()
{
	QList<CVolumePtr> Volumes = GetSelectedItems();

	QList<STATUS> Results;

	foreach(CVolumePtr pVolume, Volumes) 
	{
		CEnclavePtr pEnclave = CEnclavePtr(new CEnclave());
		pEnclave->SetName(tr("%1 Enclave").arg(pVolume->GetName()));
		pEnclave->SetVolumeGuid(pVolume->GetGuid());
		Results << theCore->EnclaveManager()->SetEnclave(pEnclave);
	}

	theGUI->CheckResults(Results, this);
}

void CVolumeView::OnRenameVolume()
{
	QList<CVolumePtr> Volumes = GetSelectedItems();
	if (Volumes.count() != 1) return;

	QString Value = QInputDialog::getText(this, "MajorPrivacy", tr("Please enter a name for the item"), QLineEdit::Normal, Volumes.at(0)->GetName());
	if (Value.isEmpty())
		return;

	Volumes.at(0)->SetName(Value);
}

void CVolumeView::OnRemoveVolume()
{
	if (QMessageBox::question(this, "MajorPrivacy", tr("Do you want to remove selected Entries from list?\nNote: The volume image files will remain intact and must be manually deleted."), QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
		return;

	QList<STATUS> Results;

	QList<CVolumePtr> Volumes = GetSelectedItems();
	foreach(CVolumePtr pVolume, Volumes)
		Results.append(theCore->VolumeManager()->RemoveVolume(pVolume->GetImagePath()));

	theGUI->CheckResults(Results, this);
}

void CVolumeView::OnAddFolder()
{
	if (theConf->GetBool("Options/WarnFolderProtection", true)) {
		bool State = false;
		if(CCheckableMessageBox::question(this, "MajorPrivacy",
			tr("The security of files and folders on unencrypted volumes cannot be guaranteed if MajorPrivacy is not running or if the Kernel Isolator driver is not loaded. In this state, folder access control will not be enforced.\n"
				"For non-critical data, folder protection may still provide a basic level of security. However, for highly confidential or sensitive information, it is strongly recommended to use a secure, encrypted volume to ensure robust protection against unauthorized access.")
			, tr("Don't show this message again."), &State, QDialogButtonBox::Ok | QDialogButtonBox::Cancel, QDialogButtonBox::Cancel, QMessageBox::Warning) != QDialogButtonBox::Ok)
			return;

		if (State)
			theConf->SetValue("Options/WarnFolderProtection", false);
	}

	QString Value = QFileDialog::getExistingDirectory(this, tr("Select Directory")).replace("/", "\\");
	if (Value.isEmpty())
		return;

	if (!Value.endsWith("\\"))
		Value.append("\\");

	STATUS Status = theCore->VolumeManager()->AddVolume(Value);
	theGUI->CheckResults(QList<STATUS>() << Status, this);
}