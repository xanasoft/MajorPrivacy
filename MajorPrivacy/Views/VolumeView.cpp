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
#include "../Wizards/VolumeWizard.h"
#include <QtConcurrent/QtConcurrent>

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
	m_pMenu->addSeparator();
	m_pChangeVolumePassword = m_pMenu->addAction(QIcon(":/Icons/VolumePW.png"), tr("Change Volume Password"), this, SLOT(OnChangeVolumePassword()));
	m_pBackupVolumeHeader = m_pMenu->addAction(QIcon(":/Icons/Export.png"), tr("Backup Volume Header"), this, SLOT(OnBackupVolumeHeader()));
	m_pRestoreVolumeHeader = m_pMenu->addAction(QIcon(":/Icons/Import.png"), tr("Restore Volume Header"), this, SLOT(OnRestoreVolumeHeader()));
	m_pExpandVolume = m_pMenu->addAction(QIcon(":/Icons/Expand.png"), tr("Expand Volume"), this, SLOT(OnExpandVolume()));
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
	m_pUnmountAllVolumes = m_pToolBar->addAction(QIcon(":/Icons/UnmountVolume.png"), tr("Unmount All Volumes"), this, SLOT(OnUnmountAllVolumes()));
	m_pCleanupVolumes = m_pToolBar->addAction(QIcon(":/Icons/Clean.png"), tr("Cleanup Missing Entries"), this, SLOT(OnCleanupVolumes()));

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

void CVolumeView::Clear()
{
	m_pItemModel->Clear();
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
	if (pVolume.isNull() || pVolume->IsFolder() || pVolume->IsBusy())
		return;

	if (!pVolume->IsMounted())
		MountVolume(pVolume->GetImagePath(), pVolume);
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
	int iBusyCount = 0;
	foreach(CVolumePtr pVolume, Volumes) {
		if (pVolume->IsBusy())
			iBusyCount++;
		if (pVolume->IsFolder())
			iFolderCount++;
		else {
			if (pVolume->IsMounted())
				iMountedCount++;
			iVolumeCount++;
		}
	}

	m_pMountVolume->setEnabled(iBusyCount == 0 && iMountedCount == 0 && iVolumeCount == 1);
	m_pUnmountVolume->setEnabled(iBusyCount == 0 && iMountedCount > 0);
	m_pChangeVolumePassword->setEnabled(iBusyCount == 0 && iMountedCount == 0 && iVolumeCount == 1 && iFolderCount == 0);
	m_pBackupVolumeHeader->setEnabled(iBusyCount == 0 && iMountedCount == 0 && iVolumeCount == 1 && iFolderCount == 0);
	m_pRestoreVolumeHeader->setEnabled(iBusyCount == 0 && iMountedCount == 0 && iVolumeCount == 1 && iFolderCount == 0);
	m_pExpandVolume->setEnabled(iBusyCount == 0 && iMountedCount == 1 && iFolderCount == 0);
	m_pAddVolumeEnclave->setEnabled(iBusyCount == 0 && iMountedCount > 0);
	m_pChangeVolumeConfig->setEnabled(iBusyCount == 0 && iMountedCount == 1);
	m_pRenameVolume->setEnabled(iBusyCount == 0 && iVolumeCount + iFolderCount == 1);
	m_pRemoveVolume->setEnabled(iBusyCount == 0 && iMountedCount == 0 && iVolumeCount + iFolderCount > 0);

	CPanelView::OnMenu(Point);
}

void CVolumeView::OnMountVolume()
{
	QList<CVolumePtr> Volumes = GetSelectedItems();
	if (Volumes.count() != 1) return;

	MountVolume(Volumes[0]->GetImagePath(), Volumes[0]);
}

void CVolumeView::MountVolume(QString Path, CVolumePtr pVolume)
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
	int iArgon2Cost = window.GetArgon2Cost();

	RunVolumeOperation(tr("Mounting volume..."), [=]() {
		return theCore->MountVolume(Path, MountPoint, Password, bProtect, bLockdown, iArgon2Cost);
	}, nullptr, pVolume ? QList<CVolumePtr>() << pVolume : QList<CVolumePtr>());
}

void CVolumeView::OnUnmountVolume()
{
	QList<CVolumePtr> Volumes = GetSelectedItems();
	if (Volumes.isEmpty())
		return;

	QStringList DevicePaths;
	foreach(CVolumePtr pVolume, Volumes)
		DevicePaths.append(pVolume->GetDevicePath());

	RunVolumeOperation(tr("Unmounting volume..."), [DevicePaths]() {
		STATUS LastStatus = OK;
		foreach(const QString& DevicePath, DevicePaths) {
			STATUS Status = theCore->DismountVolume(DevicePath);
			if (Status.IsError())
				LastStatus = Status;
		}
		return LastStatus;
	}, nullptr, Volumes);
}

void CVolumeView::OnUnmountAllVolumes()
{
	RunVolumeOperation(tr("Unmounting all volumes..."), [=]() {
		return theCore->DismountAllVolumes();
	});
}

void CVolumeView::OnCreateVolume()
{
	CVolumeWizard::ShowWizard(this);
}

/*
// Old implementation - replaced by VolumeWizard
void CVolumeView::OnCreateVolume_Old()
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
*/

void CVolumeView::OnChangeVolumePassword()
{
	QList<CVolumePtr> Volumes = GetSelectedItems();
	if (Volumes.count() != 1) return;

	CVolumePtr pVolume = Volumes[0];
	QString Path = pVolume->GetImagePath();
	//QString Path = QFileDialog::getOpenFileName(this, tr("Select Private Volume"), QString(), tr("Private Volume Files (*.pv)"));
	//if (Path.isEmpty()) return;

	CVolumeWindow window(tr("Change Volume Password."), CVolumeWindow::eChange, this);
	if (theGUI->SafeExec(&window) != 1)
		return;
	QString OldPassword = window.GetPassword();
	QString NewPassword = window.GetNewPassword();
	int iArgon2Cost = window.GetArgon2Cost();
	int iNewArgon2Cost = window.GetNewArgon2Cost();

	RunVolumeOperation(tr("Changing volume password..."), [=]() {
		return theCore->ChangeVolumePassword(Path, OldPassword, NewPassword, iArgon2Cost, iNewArgon2Cost);
	}, nullptr, QList<CVolumePtr>() << pVolume);
}

void CVolumeView::OnBackupVolumeHeader()
{
	QList<CVolumePtr> Volumes = GetSelectedItems();
	if (Volumes.count() != 1) return;

	CVolumePtr pVolume = Volumes[0];
	QString VolumePath = pVolume->GetImagePath();

	QString BackupPath = QFileDialog::getSaveFileName(this, tr("Select Backup Location"),
		QString(), tr("Volume Header Backup (*.pvh);;All Files (*)"));
	if (BackupPath.isEmpty())
		return;
	BackupPath = BackupPath.replace("/", "\\");
	if (!BackupPath.endsWith(".pvh", Qt::CaseInsensitive))
		BackupPath += ".pvh";

	CVolumeWindow window(tr("Enter the volume password to backup the header."), CVolumeWindow::eGetPW, this);
	if (theGUI->SafeExec(&window) != 1)
		return;
	QString Password = window.GetPassword();
	int iArgon2Cost = window.GetArgon2Cost();

	RunVolumeOperation(tr("Backing up volume header..."), [=]() {
		return theCore->BackupVolumeHeader(VolumePath, BackupPath, Password, iArgon2Cost);
	}, nullptr, QList<CVolumePtr>() << pVolume);
}

void CVolumeView::OnRestoreVolumeHeader()
{
	QList<CVolumePtr> Volumes = GetSelectedItems();
	if (Volumes.count() != 1) return;

	CVolumePtr pVolume = Volumes[0];
	QString VolumePath = pVolume->GetImagePath();

	if (QMessageBox::warning(this, "MajorPrivacy",
		tr("WARNING: Restoring a volume header will overwrite the current header. "
		   "This operation cannot be undone. Make sure you have a backup of the current header before proceeding.\n\n"
		   "Are you sure you want to restore the volume header?"),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		return;

	QString BackupPath = QFileDialog::getOpenFileName(this, tr("Select Header Backup File"),
		QString(), tr("Volume Header Backup (*.pvh);;All Files (*)"));
	if (BackupPath.isEmpty())
		return;
	BackupPath = BackupPath.replace("/", "\\");

	CVolumeWindow window(tr("Enter the password used when the header backup was created."), CVolumeWindow::eGetPW, this);
	if (theGUI->SafeExec(&window) != 1)
		return;
	QString Password = window.GetPassword();
	int iArgon2Cost = window.GetArgon2Cost();

	RunVolumeOperation(tr("Restoring volume header..."), [=]() {
		return theCore->RestoreVolumeHeader(VolumePath, BackupPath, Password, iArgon2Cost);
	}, nullptr, QList<CVolumePtr>() << pVolume);
}

void CVolumeView::OnExpandVolume()
{
	QList<CVolumePtr> Volumes = GetSelectedItems();
	if (Volumes.count() != 1) return;

	CVolumePtr pVolume = Volumes[0];
	if (!pVolume->IsMounted() || pVolume->IsFolder())
		return;

	quint64 CurrentSize = pVolume->GetVolumeSize();
	QString MountPoint = pVolume->GetDevicePath();

	// Create expand volume dialog
	QDialog dialog(this);
	dialog.setWindowTitle(tr("Expand Volume"));
	dialog.setMinimumWidth(400);

	QVBoxLayout* layout = new QVBoxLayout(&dialog);

	QLabel* infoLabel = new QLabel(tr("Current volume size: <b>%1</b>").arg(FormatSize(CurrentSize)));
	layout->addWidget(infoLabel);

	layout->addSpacing(10);

	QLabel* promptLabel = new QLabel(tr("Enter the amount of space to add:"));
	layout->addWidget(promptLabel);

	QHBoxLayout* sizeLayout = new QHBoxLayout();
	QLineEdit* sizeEdit = new QLineEdit();
	sizeEdit->setText("1024");
	sizeEdit->setMaximumWidth(150);
	sizeLayout->addWidget(sizeEdit);

	QComboBox* unitCombo = new QComboBox();
	unitCombo->addItem(tr("KB"), 0);
	unitCombo->addItem(tr("MB"), 1);
	unitCombo->addItem(tr("GB"), 2);
	unitCombo->setCurrentIndex(1); // Default to MB
	sizeLayout->addWidget(unitCombo);
	sizeLayout->addStretch();
	layout->addLayout(sizeLayout);

	QLabel* newSizeLabel = new QLabel();
	layout->addWidget(newSizeLabel);

	// Update new size label when values change
	auto updateNewSize = [&]() {
		bool ok;
		quint64 addSize = sizeEdit->text().toULongLong(&ok);
		if (!ok || addSize == 0) {
			newSizeLabel->setText(tr("<span style='color:red;'>Please enter a valid size.</span>"));
			return;
		}

		quint64 addSizeBytes;
		switch (unitCombo->currentIndex()) {
		case 0: addSizeBytes = addSize * 1024ULL; break;
		case 1: addSizeBytes = addSize * 1024ULL * 1024ULL; break;
		case 2: addSizeBytes = addSize * 1024ULL * 1024ULL * 1024ULL; break;
		default: addSizeBytes = addSize * 1024ULL * 1024ULL;
		}

		quint64 newTotalSize = CurrentSize + addSizeBytes;
		newSizeLabel->setText(tr("New total size: <b>%1</b> (adding %2)")
			.arg(FormatSize(newTotalSize))
			.arg(FormatSize(addSizeBytes)));
	};

	connect(sizeEdit, &QLineEdit::textChanged, &dialog, updateNewSize);
	connect(unitCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog, updateNewSize);
	updateNewSize();

	layout->addSpacing(10);

	QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttonBox);

	if (dialog.exec() != QDialog::Accepted)
		return;

	bool ok;
	quint64 addSize = sizeEdit->text().toULongLong(&ok);
	if (!ok || addSize == 0) {
		QMessageBox::warning(this, tr("Expand Volume"), tr("Please enter a valid size to add."));
		return;
	}

	quint64 addSizeBytes;
	switch (unitCombo->currentIndex()) {
	case 0: addSizeBytes = addSize * 1024ULL; break;
	case 1: addSizeBytes = addSize * 1024ULL * 1024ULL; break;
	case 2: addSizeBytes = addSize * 1024ULL * 1024ULL * 1024ULL; break;
	default: addSizeBytes = addSize * 1024ULL * 1024ULL;
	}

	RunVolumeOperation(tr("Expanding volume..."), [=]() {
		return theCore->ExpandVolume(MountPoint, addSizeBytes);
	}, nullptr, QList<CVolumePtr>() << pVolume);
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

void CVolumeView::OnCleanupVolumes()
{
	QList<CVolumePtr> Volumes = theCore->VolumeManager()->List();
	QStringList MissingPaths;

	foreach(CVolumePtr pVolume, Volumes) {
		if (pVolume->IsMounted() || pVolume->IsBusy())
			continue;
		QString ImagePath = pVolume->GetImagePath();
		if (pVolume->IsFolder()) {
			if (!QDir(ImagePath).exists())
				MissingPaths.append(ImagePath);
		} else {
			if (!QFile::exists(ImagePath))
				MissingPaths.append(ImagePath);
		}
	}

	if (MissingPaths.isEmpty()) {
		QMessageBox::information(this, "MajorPrivacy", tr("No missing entries found."));
		return;
	}

	if (QMessageBox::question(this, "MajorPrivacy",
		tr("Found %1 entries with missing images/folders. Do you want to remove them from the list?").arg(MissingPaths.count()),
		QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
		return;

	QList<STATUS> Results;
	foreach(const QString& Path, MissingPaths)
		Results.append(theCore->VolumeManager()->RemoveVolume(Path));

	theGUI->CheckResults(Results, this);
}

void CVolumeView::RunVolumeOperation(const QString& progressText, std::function<STATUS()> operation, std::function<void()> onSuccess, const QList<CVolumePtr>& Volumes)
{
	foreach(CVolumePtr pVolume, Volumes)
		pVolume->SetBusyStatus(progressText);

	QFutureWatcher<STATUS>* pWatcher = new QFutureWatcher<STATUS>(this);

	connect(pWatcher, &QFutureWatcher<STATUS>::finished, this, [this, pWatcher, onSuccess, Volumes]() {
		STATUS Status = pWatcher->result();

		foreach(CVolumePtr pVolume, Volumes)
			pVolume->ClearBusyStatus();

		if (Status.IsSuccess() && onSuccess)
			onSuccess();

		theGUI->CheckResults(QList<STATUS>() << Status, this);
		pWatcher->deleteLater();
		Sync(); // Refresh the view
	});

	QFuture<STATUS> future = QtConcurrent::run(operation);
	pWatcher->setFuture(future);
	Sync(); // Refresh to show busy status
}

void CVolumeView::OnVolumeOperationFinished()
{
	// This slot is no longer needed as we handle completion in the lambda
	// Kept for potential future use
}