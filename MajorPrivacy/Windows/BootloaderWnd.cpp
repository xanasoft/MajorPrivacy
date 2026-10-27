/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

#include "pch.h"
#include "BootloaderWnd.h"
#include "VolumeWindow.h"
#include "CertDetailsWnd.h"
#include "../Helpers/WinAdmin.h"
#include "../MiscHelpers/Common/Common.h"
#include "../../Library/Boot/EfiVars.h"
#include "../../Library/Boot/EspLocator.h"
#include "../../Library/Helpers/AppUtil.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QPushButton>
#include <QStorageInfo>
#include <QInputDialog>
#include <QLineEdit>


/* ---------------------------------------------------------------------------
 * Getting the window open
 * ------------------------------------------------------------------------ */

void CBootloaderWnd::Open(QWidget* parent)
{
	//
	// Whether this process is elevated is not the question - whether it can
	// actually reach the firmware variables is. The two are not the same here:
	// this program's own token does not always come out right when it is
	// started while the driver is loaded (see the note in main.cpp about
	// -sync_tok), and an elevated GUI can end up without
	// SeSystemEnvironmentPrivilege in its token at all. Nothing in-process can
	// add a privilege that is not there.
	//
	// A freshly spawned copy gets a clean token, so asking the firmware is the
	// test, and a process that cannot answer hands the job to the helper rather
	// than telling the user to elevate something that is already elevated.
	//
	if (IsElevated())
	{
		STATUS Status = CEfiVars::Init();

		// a failure a new process would not fix - no firmware variables at all,
		// say - belongs in the window, where it can be explained
		if (Status.IsSuccess() || Status.GetStatus() != STATUS_PRIVILEGE_NOT_HELD)
		{
			CBootloaderWnd Wnd(parent);
			Wnd.exec();
			return;
		}
	}

	//
	// Either not elevated, or elevated with a token that cannot do the work.
	// Rather than elevating this process - which would mean the whole GUI, its
	// service connection and its driver handles running as administrator for
	// the sake of one window - a second copy is started that does nothing but
	// show this window and exit.
	//
	wchar_t Path[MAX_PATH] = { 0 };
	if (!GetModuleFileNameW(NULL, Path, ARRAYSIZE(Path))) {
		QMessageBox::critical(parent, "MajorPrivacy", QObject::tr("Could not determine the path of this program."));
		return;
	}

	HANDLE hProcess = RunElevatedEx(Path, QString(BOOTLOADER_ARG).toStdWString());
	if (!hProcess) {
		DWORD uError = GetLastError();
		if (uError == ERROR_CANCELLED)
			return;		// the user said no to the prompt, which needs no message
		QMessageBox::critical(parent, "MajorPrivacy",
			QObject::tr("Could not start MajorPrivacy with administrator rights, which the boot "
				"loader window needs (error %1).").arg(uError));
		return;
	}
	CloseHandle(hProcess);
}

int CBootloaderWnd::RunStandalone()
{
	CBootloaderWnd Wnd(NULL);
	Wnd.exec();

	// closing the window is the whole exit condition; there is no failure this
	// process can report that it has not already shown
	return 0;
}

/* ---------------------------------------------------------------------------
 * The window
 * ------------------------------------------------------------------------ */

CBootloaderWnd::CBootloaderWnd(QWidget* parent)
	: QDialog(parent)
{
	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui.setupUi(this);
	setWindowTitle(tr("MajorPrivacy - EFI Boot Loader"));

	connect(ui.btnRefresh, &QPushButton::clicked, this, &CBootloaderWnd::OnRefresh);
	connect(ui.treeEsp, &QTreeWidget::itemSelectionChanged, this, &CBootloaderWnd::OnEspChanged);
	connect(ui.btnApply, &QPushButton::clicked, this, &CBootloaderWnd::OnApply);
	connect(ui.chkShim, &QCheckBox::clicked, this, &CBootloaderWnd::OnOptionChanged);
	connect(ui.chkBootEntry, &QCheckBox::clicked, this, &CBootloaderWnd::OnOptionChanged);

	connect(ui.btnInstall, &QPushButton::clicked, this, &CBootloaderWnd::OnInstall);
	connect(ui.btnUpdate, &QPushButton::clicked, this, &CBootloaderWnd::OnUpdate);
	connect(ui.btnRemove, &QPushButton::clicked, this, &CBootloaderWnd::OnRemove);
	connect(ui.btnConfigure, &QPushButton::clicked, this, &CBootloaderWnd::OnConfigure);
	connect(ui.btnCreateEsp, &QPushButton::clicked, this, &CBootloaderWnd::OnCreateEsp);
	connect(ui.btnRemoveEsp, &QPushButton::clicked, this, &CBootloaderWnd::OnRemoveEsp);
	connect(ui.btnMount, &QPushButton::clicked, this, &CBootloaderWnd::OnMount);

	//
	// One sub-tab per key list, so that each list's buttons sit with the list
	// they act on. They can share slots because the sub-tab on screen is what
	// CurrentMokList answers with.
	//
	connect(ui.btnMokRefresh, &QPushButton::clicked, this, &CBootloaderWnd::OnMokRefresh);
	connect(ui.btnMokApply, &QPushButton::clicked, this, &CBootloaderWnd::OnMokApply);
	connect(ui.chkShimValidation, &QCheckBox::clicked, this, &CBootloaderWnd::OnMokOptionChanged);
	connect(ui.chkDbUsage, &QCheckBox::clicked, this, &CBootloaderWnd::OnMokOptionChanged);
	connect(ui.tabsMok, &QTabWidget::currentChanged, this, &CBootloaderWnd::OnMokListChanged);

	connect(ui.treeMokAllow, &QTreeWidget::itemSelectionChanged, this, &CBootloaderWnd::UpdateButtons);
	connect(ui.treeMokDeny, &QTreeWidget::itemSelectionChanged, this, &CBootloaderWnd::UpdateButtons);

	//
	// A list row says what a key is called and nothing else; double-clicking is
	// the way to find out what it actually is. Every list, including the
	// firmware's own - there is nothing to change there, but plenty to read.
	//
	QTreeWidget* Lists[] = { ui.treeMokAllow, ui.treeMokDeny, ui.treePlatform, ui.treeDb, ui.treeDbx };
	for (int i = 0; i < 5; i++)
		connect(Lists[i], &QTreeWidget::itemDoubleClicked, this, &CBootloaderWnd::OnEntryActivated);

	connect(ui.btnEnroll, &QPushButton::clicked, this, &CBootloaderWnd::OnEnroll);
	connect(ui.btnDelete, &QPushButton::clicked, this, &CBootloaderWnd::OnDelete);
	connect(ui.btnExport, &QPushButton::clicked, this, &CBootloaderWnd::OnExport);
	connect(ui.btnRevoke, &QPushButton::clicked, this, &CBootloaderWnd::OnRevoke);
	connect(ui.btnReset, &QPushButton::clicked, this, &CBootloaderWnd::OnReset);

	connect(ui.btnEnrollX, &QPushButton::clicked, this, &CBootloaderWnd::OnEnroll);
	connect(ui.btnDeleteX, &QPushButton::clicked, this, &CBootloaderWnd::OnDelete);
	connect(ui.btnExportX, &QPushButton::clicked, this, &CBootloaderWnd::OnExport);
	connect(ui.btnRevokeX, &QPushButton::clicked, this, &CBootloaderWnd::OnRevoke);
	connect(ui.btnResetX, &QPushButton::clicked, this, &CBootloaderWnd::OnReset);

	connect(ui.btnSetPassword, &QPushButton::clicked, this, &CBootloaderWnd::OnSetPassword);
	connect(ui.btnClearPassword, &QPushButton::clicked, this, &CBootloaderWnd::OnClearPassword);

	connect(ui.radDrive, &QRadioButton::clicked, this, &CBootloaderWnd::OnMediaTargetChanged);
	connect(ui.radIso, &QRadioButton::clicked, this, &CBootloaderWnd::OnMediaTargetChanged);
	connect(ui.radPxe, &QRadioButton::clicked, this, &CBootloaderWnd::OnMediaTargetChanged);
	connect(ui.btnBrowse, &QToolButton::clicked, this, &CBootloaderWnd::OnMediaBrowse);
	connect(ui.btnMediaConfigure, &QPushButton::clicked, this, &CBootloaderWnd::OnMediaConfigure);
	connect(ui.btnWrite, &QPushButton::clicked, this, &CBootloaderWnd::OnWriteMedia);

	connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	ui.treeMokAllow->setColumnWidth(0, 90);
	ui.treeMokDeny->setColumnWidth(0, 90);
	ui.treeDb->setColumnWidth(0, 90);
	ui.treeDbx->setColumnWidth(0, 90);
	ui.treePlatform->setColumnWidth(0, 60);
	ui.treePlatform->setColumnWidth(1, 90);

	ui.treeEsp->setColumnWidth(0, 200);
	ui.treeEsp->setColumnWidth(1, 80);
	ui.treeEsp->setColumnWidth(2, 80);
	ui.treeEsp->setColumnWidth(3, 90);
	ui.treeEsp->setColumnWidth(4, 150);
	ui.treeEsp->setMinimumHeight(140);

	//
	// Each column of buttons gets one width - the widest label in it - rather
	// than stretching to whatever the layout has going spare. Uniform and no
	// larger than the text needs.
	//
	QPushButton* EspSide[] = { ui.btnRefresh, ui.btnCreateEsp, ui.btnRemoveEsp, ui.btnMount };
	SizeButtonColumn(EspSide, 4);

	QPushButton* MokSide[] = { ui.btnMokRefresh, ui.btnSetPassword, ui.btnClearPassword, ui.btnMokApply };
	SizeButtonColumn(MokSide, 4);

	//
	// Two of the key lists carry a row of buttons under the tree and two carry
	// a note, so the pages do not want the same height. Left alone the tab body
	// would grow and shrink as they are switched; giving the tab widget all the
	// slack and letting the trees absorb the difference keeps it still.
	//
	ui.gridLayout->setRowStretch(0, 1);
	ui.tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	ui.gridLayoutSb->setRowStretch(1, 1);
	ui.tabsMok->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	QTreeWidget* Trees[] = { ui.treeMokAllow, ui.treeMokDeny, ui.treePlatform, ui.treeDb, ui.treeDbx };
	for (int i = 0; i < 5; i++) {
		Trees[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		Trees[i]->setMinimumHeight(120);
	}

	LoadMediaDrives();
	OnMediaTargetChanged();

	//
	// One question answers most of what this window can do: is there a firmware
	// variable store at all, and may this process touch it. If not, everything
	// is disabled and the reason is on screen rather than in a message box that
	// the user has to dismiss before seeing an empty window.
	//
	STATUS Status = CEfiVars::Init();
	if (Status.IsError())
	{
		QString Text = QString::fromWCharArray(Status.GetMessageText());
		ui.lblEspInfo->setText(QString("<b>%1</b>").arg(Text.toHtmlEscaped()));
		ui.tabs->setTabEnabled(1, false);
		ui.grpDetails->setEnabled(false);
		ui.treeEsp->setEnabled(false);
		ui.btnRefresh->setEnabled(false);
		ui.btnInstall->setEnabled(false);
		ui.btnUpdate->setEnabled(false);
		ui.btnRemove->setEnabled(false);
		ui.btnConfigure->setEnabled(false);
		ui.btnCreateEsp->setEnabled(false);
		ui.btnRemoveEsp->setEnabled(false);
		ui.btnMount->setEnabled(false);
		ui.tabs->setTabEnabled(2, false);
		return;
	}

	OnRefresh();
}

CBootloaderWnd::~CBootloaderWnd()
{
}

//
// Give a column of buttons a single width, wide enough for the longest label
// in it. Mount's caption changes as it is used, so the widest it will ever be
// is measured rather than whatever it happens to say now.
//
void CBootloaderWnd::SizeButtonColumn(class QPushButton** pButtons, int iCount)
{
	int iWidth = 0;
	for (int i = 0; i < iCount; i++) {
		pButtons[i]->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		iWidth = qMax(iWidth, pButtons[i]->sizeHint().width());
	}

	for (int i = 0; i < iCount; i++)
		pButtons[i]->setFixedWidth(iWidth);
}

/* ---------------------------------------------------------------------------
 * Reading the state
 * ------------------------------------------------------------------------ */

//
// Rows carry what they stand for: an ESP row an index into m_Esps, a disk row
// -1 there and its disk number beside it. That is the whole distinction the
// rest of this window makes between the two.
//
#define ESP_ROLE_INDEX	(Qt::UserRole)
#define ESP_ROLE_DISK	(Qt::UserRole + 1)

void CBootloaderWnd::LoadEsps()
{
	m_bLoading = true;

	int iWantDisk = CurrentDisk();
	int iWantPart = CurrentPartition();
	int iWantDiskRow = CurrentDiskRow();

	ui.treeEsp->clear();
	m_Esps.clear();

	QTreeWidgetItem* pSelect = NULL;

	std::vector<SDiskInfo> Disks = CEspLocator::EnumDisks();
	for (size_t d = 0; d < Disks.size(); d++)
	{
		const SDiskInfo& Disk = Disks[d];

		QTreeWidgetItem* pDisk = new QTreeWidgetItem();

		QString Name = tr("Disk %1").arg(Disk.iDisk);
		if (!Disk.Name.empty())
			Name += QString(" - %1").arg(QString::fromStdWString(Disk.Name));
		pDisk->setText(0, Name);
		pDisk->setText(1, FormatSize(Disk.uSize));

		//
		// Whether a disk can hold an EFI partition at all is worth saying on
		// the disk row: UEFI does not boot an MBR disk, so an ESP on one is of
		// no use to the firmware.
		//
		QStringList What;
		What.append(Disk.bGpt ? tr("GPT") : tr("MBR, not bootable"));
		if (Disk.bIsOsDisk)
			What.append(tr("Windows"));
		pDisk->setText(4, What.join(", "));

		pDisk->setData(0, ESP_ROLE_INDEX, -1);
		pDisk->setData(0, ESP_ROLE_DISK, Disk.iDisk);

		ui.treeEsp->addTopLevelItem(pDisk);
		pDisk->setExpanded(true);

		if (Disk.iDisk == iWantDiskRow)
			pSelect = pDisk;

		std::vector<SEspInfo> Esps = CEspLocator::EnumEsps(Disk.iDisk);
		for (size_t j = 0; j < Esps.size(); j++)
		{
			const SEspInfo& Esp = Esps[j];
			int iIndex = (int)m_Esps.size();
			m_Esps.push_back(Esp);

			QTreeWidgetItem* pItem = new QTreeWidgetItem();
			pItem->setText(0, tr("Partition %1").arg(Esp.iPartition));
			pItem->setText(1, FormatSize(Esp.uSize));
			pItem->setText(2, Esp.uFree ? FormatSize(Esp.uFree) : QString());
			pItem->setText(3, QString::fromStdWString(Esp.Label));

			//
			// What is on it, which is what decides which one the user wants. An
			// ESP carrying the Windows loader is the one to leave alone where
			// there is a choice.
			//
			QStringList Contents;
			if (Esp.bHasDcs)		Contents.append(tr("boot loader"));
			if (Esp.bHasMicrosoft)	Contents.append(tr("Windows"));
			pItem->setText(4, Contents.isEmpty() ? tr("empty") : Contents.join(", "));

			pItem->setText(5, QString::fromStdWString(Esp.MountPoint));

			pItem->setData(0, ESP_ROLE_INDEX, iIndex);
			pItem->setData(0, ESP_ROLE_DISK, Esp.iDisk);

			pDisk->addChild(pItem);

			// keep what was selected across a refresh, otherwise prefer ours
			if (Esp.iDisk == iWantDisk && Esp.iPartition == iWantPart)
				pSelect = pItem;
			else if (!pSelect && Esp.bHasDcs)
				pSelect = pItem;
		}
	}

	if (!pSelect)
	{
		// nothing to restore - the first EFI partition there is, else the first disk
		for (int i = 0; i < ui.treeEsp->topLevelItemCount() && !pSelect; i++) {
			if (ui.treeEsp->topLevelItem(i)->childCount() > 0)
				pSelect = ui.treeEsp->topLevelItem(i)->child(0);
		}
		if (!pSelect && ui.treeEsp->topLevelItemCount() > 0)
			pSelect = ui.treeEsp->topLevelItem(0);
	}

	if (pSelect)
		ui.treeEsp->setCurrentItem(pSelect);

	m_bLoading = false;
}

int CBootloaderWnd::CurrentEspIndex() const
{
	QTreeWidgetItem* pItem = ui.treeEsp->currentItem();
	if (!pItem)
		return -1;

	int iIndex = pItem->data(0, ESP_ROLE_INDEX).toInt();
	return (iIndex >= 0 && iIndex < (int)m_Esps.size()) ? iIndex : -1;
}

int CBootloaderWnd::CurrentDiskRow() const
{
	QTreeWidgetItem* pItem = ui.treeEsp->currentItem();
	if (!pItem || pItem->data(0, ESP_ROLE_INDEX).toInt() >= 0)
		return -1;		// no selection, or a partition rather than a disk

	return pItem->data(0, ESP_ROLE_DISK).toInt();
}

int CBootloaderWnd::CurrentDisk() const
{
	int iIndex = CurrentEspIndex();
	if (iIndex >= 0)
		return m_Esps[iIndex].iDisk;

	return CurrentDiskRow();
}

int CBootloaderWnd::CurrentPartition() const
{
	int iIndex = CurrentEspIndex();
	return iIndex < 0 ? 0 : m_Esps[iIndex].iPartition;
}

void CBootloaderWnd::UpdateState()
{
	m_State = SBootLoaderState();

	if (CurrentEspIndex() < 0) {
		ui.lblEspInfo->setText(m_Esps.empty()
			? tr("<b>No EFI System Partition was found.</b> One can be created below, on a disk "
				 "with free space.")
			: tr("Select an EFI System Partition above."));
	}
	else
	{
		RESULT(SBootLoaderState) Res = CEfiInstall::GetState(CurrentDisk(), CurrentPartition());
		if (Res.IsError()) {
			ui.lblEspInfo->setText(QString("<b>%1</b>")
				.arg(QString::fromWCharArray(Res.GetMessageText()).toHtmlEscaped()));
		}
		else {
			m_State = Res.GetValue();
			ui.lblEspInfo->setText(tr("%1 - %2 free of %3")
				.arg(QString::fromStdWString(m_State.Esp.Path))
				.arg(FormatSize(m_State.Esp.uFree))
				.arg(FormatSize(m_State.Esp.uSize)));
		}
	}

	ui.lblInstalled->setText(m_State.bInstalled ? tr("installed") : tr("not installed"));
	ui.lblVersion->setText(m_State.uVersion
		? QString("0x%1").arg(m_State.uVersion, 8, 16, QChar('0')) : tr("unknown"));
	//
	// "Default loader" is the file the firmware falls back to when no boot
	// entry applies. Before an install the question is only whether there is
	// one to keep; after it, whether it is still ours - a Windows update
	// rewrites it, and Update puts ours back.
	//
	QString DefaultPath = QString::fromWCharArray(CEfiInstall::GetDefaultLoaderPath());
	if (!m_State.bInstalled)
		ui.lblDefault->setText(m_State.bHasDefaultLoader ? tr("present, kept by Install") : tr("none"));
	else if (m_State.bIsDefaultLoader)
		ui.lblDefault->setText(tr("ours"));
	else
		ui.lblDefault->setText(m_State.bHasDefaultLoader
			? tr("replaced since the install - Update puts ours back")
			: tr("missing - Update puts ours back"));
	ui.lblDefaultCaption->setToolTip(tr("%1 - the file the firmware runs when no boot entry applies. "
		"Windows updates rewrite it.").arg(DefaultPath));

	if (m_State.bOriginalSaved)
		ui.lblOriginal->setText(m_State.bInstalled
			? tr("saved - Remove puts it back") : tr("left over from an earlier install, kept"));
	else
		ui.lblOriginal->setText(m_State.bInstalled ? tr("none") : tr("-"));
	ui.lblOriginalCaption->setToolTip(tr("The file that was at %1 before the install, saved so that "
		"Remove can put it back.").arg(DefaultPath));

	ui.lblConfig->setText(m_State.bHasConfig ? tr("present") : tr("none"));

	//
	// The GPT identity of the partition, which is also what a firmware boot
	// entry names it by - worth showing when two ESPs look otherwise alike.
	//
	QString Guid;
	int iEsp = CurrentEspIndex();
	if (iEsp >= 0 && m_Esps[iEsp].bGpt)
	{
		const unsigned char* p = m_Esps[iEsp].PartitionGuid;
		Guid = QString("%1-%2-%3-%4-%5")
			.arg(qFromLittleEndian<quint32>(p), 8, 16, QChar('0'))
			.arg(qFromLittleEndian<quint16>(p + 4), 4, 16, QChar('0'))
			.arg(qFromLittleEndian<quint16>(p + 6), 4, 16, QChar('0'))
			.arg(QString(QByteArray((const char*)p + 8, 2).toHex()))
			.arg(QString(QByteArray((const char*)p + 10, 6).toHex()));
	}
	ui.lblPartGuid->setText(Guid);

	//
	// The two settings are the state as well as the request: they show what is
	// on the partition now, and changing one and pressing Apply is what alters
	// it. Before anything is installed they are simply the install options.
	//
	m_bLoading = true;
	//
	// Once installed the boxes mirror what is actually on the partition. Before an install they
	// are the defaults Install will use: a boot entry always, and - the point of the shim - it is
	// pre-selected when Secure Boot is on (and a shim ships) because the firmware would otherwise
	// refuse the loader. But not when the loader is already trusted by the firmware's own db
	// (a self-rolled db / PK): then it boots directly and the shim is just in the way.
	//
	ui.chkShim->setChecked(m_State.bInstalled
		? m_State.bShim
		: (m_State.bSecureBoot && m_State.bShimAvailable && !m_State.bDcsSignedByDb));
	ui.chkBootEntry->setChecked(m_State.bInstalled ? m_State.bBootEntrySet : true);
	//
	// DiskCryptor support is an install-time choice. Once installed the box just
	// reflects what is there (an update keeps it in step); a fresh install defaults
	// it off - MajorPrivacy does not need it.
	//
	ui.chkDiskCryptor->setChecked(m_State.bInstalled ? m_State.bDiskCryptorInstalled : false);
	m_bLoading = false;

	ui.chkDiskCryptor->setEnabled(m_State.bDiskCryptorAvailable && !m_State.bInstalled);
	if (!m_State.bDiskCryptorAvailable)
		ui.chkDiskCryptor->setToolTip(tr("No DiskCryptor modules ship with this build."));
	else if (m_State.bInstalled)
		ui.chkDiskCryptor->setToolTip(tr("Whether the DiskCryptor modules are installed on this "
			"partition. An update keeps them in step with the loader; it is set at install time."));
	else
		ui.chkDiskCryptor->setToolTip(tr("Also install the DiskCryptor full-disk-encryption modules "
			"(DcsInt, DcsTpm). MajorPrivacy does not need them; enable this on a machine that also "
			"uses DiskCryptor, which shares this same boot loader."));

	//
	// The two Secure Boot trust states, so the config is readable at a glance: whether the firmware
	// db would run the loader and the shim directly as they are signed.
	//
	ui.lblLoaderSb->setText(m_State.bDcsSignedByDb
		? tr("trusted by firmware db") : tr("not in firmware db"));
	ui.lblLoaderSb->setToolTip(m_State.bDcsSignedByDb
		? tr("The boot loader is signed by a certificate enrolled in the firmware's db, so Secure Boot "
			 "runs it directly - no shim is needed.")
		: tr("The boot loader is not signed by any certificate in the firmware's db, so under Secure "
			 "Boot it needs the shim in front of it. Enrolling your own signing certificate in db "
			 "(your own PK/db) is the alternative to the shim."));

	if (!m_State.bShimAvailable)
		ui.lblShimSb->setText(tr("no shim in this build"));
	else
		ui.lblShimSb->setText(m_State.bShimSignedByDb
			? tr("trusted by firmware db") : tr("not in firmware db"));
	ui.lblShimSb->setToolTip(tr("Whether the shim binary chains to a certificate in the firmware's db - "
		"the Microsoft UEFI CA for the shipped shim, or your own certificate if you signed it. If it is "
		"trusted, Secure Boot runs the shim; the shim then runs the loader via its own MOK list."));

	ui.chkShim->setToolTip(m_State.bShimAvailable
		? tr("Puts a signed shim in front of the loader. Without it the firmware will refuse to "
			 "run the loader while Secure Boot is on.")
		: tr("No shim package ships with this build."));

	ui.chkBootEntry->setToolTip(tr("An entry in the firmware boot menu, ahead of Windows. Without one "
		"the loader is only reached as %1, the fallback file a Windows update can overwrite.")
		.arg(DefaultPath));

	LoadMediaConfig();
	UpdateButtons();
}

bool CBootloaderWnd::OptionsChanged() const
{
	// only meaningful once there is an installation to change
	if (!m_State.bInstalled)
		return false;

	return ui.chkShim->isChecked() != m_State.bShim
		|| ui.chkBootEntry->isChecked() != m_State.bBootEntrySet;
}

void CBootloaderWnd::OnOptionChanged()
{
	if (m_bLoading)
		return;
	UpdateButtons();
}

void CBootloaderWnd::OnApply()
{
	if (!OptionsChanged())
		return;

	bool bShim = ui.chkShim->isChecked();
	bool bEntry = ui.chkBootEntry->isChecked();

	if (bShim != m_State.bShim && bShim && !RequirePackage(eShimPackage))
		return;

	//
	// The shim goes first: a boot entry points at whichever loader is actually
	// there, so writing the entry after the shim means it names the right one.
	//
	if (bShim != m_State.bShim)
	{
		if (!bShim && m_State.bSecureBoot && !m_State.bDcsSignedByDb
			&& !Confirm(tr("Remove the shim?\n\nWith Secure Boot on, the firmware will then refuse to "
			"run the boot loader.")))
			return;

		STATUS Status = bShim
			? CEfiInstall::InstallShim(CurrentDisk(), CurrentPartition())
			: CEfiInstall::RemoveShim(CurrentDisk(), CurrentPartition());

		if (Status.IsError()) {
			ShowError(bShim ? tr("The shim could not be installed.")
							: tr("The shim could not be removed."), Status);
			OnRefresh();
			return;
		}

		//
		// Adding the shim: pre-queue the loader's signing key into the MOK list
		// too, the same as a fresh install does, so the user only confirms it at
		// the next boot instead of finding it on disk.
		//
		if (bShim)
		{
			QString MokPassword;
			if (AskMokEnrollPassword(tr("The shim will only run the loader once its signing key is enrolled,\n"
				"so the next boot stops at MokManager for you to confirm that enrollment.\n\n"
				"Set the password MokManager will ask for there to confirm it.\n"
				"It is shown here rather than hidden, so you can note it down for that step:"), MokPassword)
				&& EnrollSignerForShim(MokPassword))
				ShowDone(MokEnrollSteps(MokPassword));
		}
	}

	if (bEntry != m_State.bBootEntrySet || bShim != m_State.bShim)
	{
		STATUS Status = bEntry ? CEfiInstall::SetBootEntry(GetOptions()) : CEfiInstall::DelBootEntry();
		if (Status.IsError()) {
			ShowError(bEntry ? tr("The firmware boot entry could not be written.")
							 : tr("The firmware boot entry could not be removed."), Status);
			OnRefresh();
			return;
		}
	}

	OnRefresh();
}

void CBootloaderWnd::UpdateSecureBoot()
{
	m_SbState = CMokManager::GetState();

	ui.lblSecureBoot->setText(m_SbState.bSecureBoot ? tr("on") : tr("off"));
	ui.lblSetupMode->setText(m_SbState.bSetupMode
		? tr("yes - the firmware will accept new platform keys") : tr("no"));

	if (!m_SbState.bShimPresent)
		ui.lblShimPresent->setText(tr("has never run on this machine"));
	else
		ui.lblShimPresent->setText(tr("present - %1 key(s) enrolled, %2 blocked")
			.arg(m_SbState.iEnrolledCount).arg(m_SbState.iDeniedCount));

	ui.lblMokPassword->setText(m_SbState.bPasswordSet ? tr("set") : tr("not set"));

	QStringList Pending;
	if (m_SbState.bPendingEnroll)		Pending.append(tr("key enrollment"));
	if (m_SbState.bPendingDelete)		Pending.append(tr("key deletion"));
	if (m_SbState.bPendingReset)		Pending.append(tr("list reset"));
	if (m_SbState.bPendingEnrollDeny)	Pending.append(tr("block list enrollment"));
	if (m_SbState.bPendingDeleteDeny)	Pending.append(tr("block list deletion"));

	ui.lblPending->setText(Pending.isEmpty() ? tr("none")
		: tr("%1 - confirm at the next boot, in MokManager").arg(Pending.join(", ")));

	//
	// Both are shown as what they are - the state of a switch - rather than as
	// a button whose label has to be read to work out which way it goes. A
	// setting shim has never been asked about reads as off, which is what it
	// behaves as.
	//
	m_bLoading = true;
	ui.chkShimValidation->setChecked(m_SbState.iShimValidation == 1);
	ui.chkDbUsage->setChecked(m_SbState.iDbIgnored == 1);
	m_bLoading = false;

	OnMokListChanged();
}

bool CBootloaderWnd::MokOptionsChanged() const
{
	return ui.chkShimValidation->isChecked() != (m_SbState.iShimValidation == 1)
		|| ui.chkDbUsage->isChecked() != (m_SbState.iDbIgnored == 1);
}

void CBootloaderWnd::OnMokOptionChanged()
{
	if (m_bLoading)
		return;
	UpdateButtons();
}

void CBootloaderWnd::OnMokApply()
{
	if (!MokOptionsChanged())
		return;

	bool bNoValidation = ui.chkShimValidation->isChecked();
	bool bIgnoreDb = ui.chkDbUsage->isChecked();

	//
	// Both weaken Secure Boot, and both are easier to turn on than to reason
	// about afterwards, so each says what it costs before it is queued.
	//
	if (bNoValidation != (m_SbState.iShimValidation == 1) && bNoValidation)
	{
		if (!Confirm(tr("Turn shim's signature checking off?\n\nShim will then load whatever it is "
			"pointed at without checking a signature. Secure Boot still covers shim itself, but "
			"nothing after it - this is a real weakening and should be temporary.")))
			return;
	}

	if (bIgnoreDb != (m_SbState.iDbIgnored == 1) && bIgnoreDb)
	{
		if (!Confirm(tr("Tell shim to ignore the firmware's key database?\n\nOnly keys enrolled "
			"here will then be trusted. Anything the firmware vouches for - including, on most "
			"machines, the Windows boot loader - will no longer load through shim.")))
			return;
	}

	//
	// One password for both, because it is the same password and asking twice
	// for it would only teach the user to type it without reading.
	//
	CSecurePassword Password;
	if (!AskMokPassword(tr("Enter the MOK password. Leave it empty if no password is set."), Password))
		return;

	if (bNoValidation != (m_SbState.iShimValidation == 1))
	{
		STATUS Status = CMokManager::SetShimValidation(!bNoValidation, Password);
		if (Status.IsError()) {
			ShowError(tr("The shim validation setting could not be queued."), Status);
			UpdateSecureBoot();
			return;
		}
	}

	if (bIgnoreDb != (m_SbState.iDbIgnored == 1))
	{
		STATUS Status = CMokManager::SetDbUsage(!bIgnoreDb, Password);
		if (Status.IsError()) {
			ShowError(tr("The firmware database setting could not be queued."), Status);
			UpdateSecureBoot();
			return;
		}
	}

	UpdateSecureBoot();
	ShowDone(tr("The change is queued. Restart this machine and confirm it in MokManager."));
}

EMokList CBootloaderWnd::CurrentMokList() const
{
	return ui.tabsMok->currentIndex() == 1 ? eMokDeny : eMokAllow;
}

bool CBootloaderWnd::IsFirmwareList() const
{
	return ui.tabsMok->currentIndex() >= 2;
}

QTreeWidget* CBootloaderWnd::CurrentMokTree() const
{
	switch (ui.tabsMok->currentIndex()) {
	case 1:  return ui.treeMokDeny;
	case 2:  return ui.treePlatform;
	case 3:  return ui.treeDb;
	case 4:  return ui.treeDbx;
	default: return ui.treeMokAllow;
	}
}

void CBootloaderWnd::UpdateButtons()
{
	//
	// What is enabled follows from the state of the machine, not from whether
	// the prerequisites happen to be in place. A missing package or a missing
	// EFI partition is something to say on the way in, not a reason to leave a
	// dead button with no explanation - so those are checked when the button is
	// pressed, in RequirePackage and RequireEsp.
	//
	bool bHaveEsp = CurrentEspIndex() >= 0;
	bool bHaveDisk = CurrentDiskRow() >= 0;

	//
	// Creating a partition needs a disk selected; removing one needs the
	// partition. Both buttons stay where they are either way - only what they
	// can do changes, and the tooltip says what is missing.
	//
	ui.btnCreateEsp->setEnabled(bHaveDisk);
	ui.btnCreateEsp->setToolTip(bHaveDisk
		? tr("Create a second EFI System Partition for the loader, so that the one Windows "
			 "boots from is left alone.")
		: tr("Select a disk to create an EFI System Partition on it."));

	if (!bHaveEsp) {
		ui.btnRemoveEsp->setEnabled(false);
		ui.btnRemoveEsp->setToolTip(tr("Select an EFI System Partition to remove it."));
	}
	else {
		const SEspInfo& Esp = m_Esps[CurrentEspIndex()];

		ui.btnRemoveEsp->setEnabled(!Esp.bHasMicrosoft);
		ui.btnRemoveEsp->setToolTip(Esp.bHasMicrosoft
			? tr("This is the EFI partition Windows boots from; removing it would leave this "
				 "machine unable to start.")
			: tr("Delete the selected EFI System Partition."));
	}

	ui.btnInstall->setEnabled(!m_State.bInstalled);
	ui.btnUpdate->setEnabled(m_State.bInstalled);
	ui.btnRemove->setEnabled(m_State.bInstalled);
	ui.btnConfigure->setEnabled(m_State.bInstalled);

	//
	// The two settings are only a request once there is something installed to
	// apply them to; before that they are the options Install will use, and
	// Apply has nothing to do.
	//
	ui.btnApply->setEnabled(OptionsChanged());

	QString Mounted = CurrentMountPoint();
	ui.btnMount->setText(Mounted.isEmpty() ? tr("Mount...") : tr("Unmount %1").arg(Mounted));
	ui.btnMount->setEnabled(bHaveEsp);
	ui.btnMount->setToolTip(bHaveEsp ? QString()
		: tr("Select an EFI System Partition to give it a drive letter."));

	/* --- Secure Boot --- */

	ui.btnMokApply->setEnabled(MokOptionsChanged());
	ui.btnClearPassword->setEnabled(m_SbState.bPasswordSet);

	//
	// m_MokEntries holds the sub-tab on screen, so only that tab's buttons are
	// the ones this knows anything about. Export needs something in the list;
	// removing a key needs one picked out of it. The three firmware tabs have
	// no buttons at all - changing those is the firmware setup's business.
	//
	bool bHaveEntries = !m_MokEntries.empty();

	if (ui.tabsMok->currentIndex() == 0) {
		ui.btnExport->setEnabled(bHaveEntries);
		ui.btnDelete->setEnabled(ui.treeMokAllow->currentItem() != NULL);
	}
	else if (ui.tabsMok->currentIndex() == 1) {
		ui.btnExportX->setEnabled(bHaveEntries);
		ui.btnDeleteX->setEnabled(ui.treeMokDeny->currentItem() != NULL);
	}
}

void CBootloaderWnd::OnRefresh()
{
	LoadEsps();
	UpdateState();
	UpdateSecureBoot();
}

void CBootloaderWnd::OnEspChanged()
{
	if (m_bLoading)
		return;
	UpdateState();
}

void CBootloaderWnd::OnMokRefresh()
{
	UpdateSecureBoot();
}

void CBootloaderWnd::OnMokListChanged()
{
	//
	// Only the list on screen is read. The firmware ones can run to hundreds of
	// entries and there is no reason to parse them all to show one.
	//
	QTreeWidget* pTree = CurrentMokTree();
	pTree->clear();
	m_MokEntries.clear();

	//
	// The platform tab is two lists in one: the platform key that owns the
	// machine, and the key exchange keys it delegates to. Showing them together
	// is the only way the relationship is visible - and the PK is set in bold,
	// because it is the one that matters.
	//
	if (ui.tabsMok->currentIndex() == 2)
	{
		std::vector<SMokEntry> Pk = CMokManager::ListFirmwarePk();
		std::vector<SMokEntry> Kek = CMokManager::ListFirmwareKek();

		for (size_t i = 0; i < Pk.size() + Kek.size(); i++)
		{
			bool bIsPk = i < Pk.size();
			const SMokEntry& Entry = bIsPk ? Pk[i] : Kek[i - Pk.size()];

			QTreeWidgetItem* pItem = new QTreeWidgetItem();
			pItem->setText(0, bIsPk ? tr("PK") : tr("KEK"));
			pItem->setText(1, QString::fromStdWString(Entry.TypeName));
			pItem->setText(2, Entry.bIsCert && !Entry.Subject.empty()
				? QString::fromStdWString(Entry.Subject)
				: QString::fromStdWString(Entry.ToHex()));

			if (bIsPk) {
				QFont Font = pItem->font(0);
				Font.setBold(true);
				for (int c = 0; c < 3; c++)
					pItem->setFont(c, Font);
			}

			pItem->setData(0, Qt::UserRole, (int)m_MokEntries.size());
			m_MokEntries.push_back(Entry);

			pTree->addTopLevelItem(pItem);
		}

		UpdateButtons();
		return;
	}

	switch (ui.tabsMok->currentIndex()) {
	case 1:  m_MokEntries = CMokManager::List(eMokDeny); break;
	case 3:  m_MokEntries = CMokManager::ListFirmwareDb(); break;
	case 4:  m_MokEntries = CMokManager::ListFirmwareDbx(); break;
	default: m_MokEntries = CMokManager::List(eMokAllow); break;
	}

	for (size_t i = 0; i < m_MokEntries.size(); i++)
	{
		const SMokEntry& Entry = m_MokEntries[i];

		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		pItem->setText(0, QString::fromStdWString(Entry.TypeName));
		pItem->setText(1, Entry.bIsCert && !Entry.Subject.empty()
			? QString::fromStdWString(Entry.Subject)
			: QString::fromStdWString(Entry.ToHex()));
		pItem->setData(0, Qt::UserRole, (int)i);
		pTree->addTopLevelItem(pItem);
	}

	UpdateButtons();
}

void CBootloaderWnd::OnEntryActivated()
{
	QTreeWidgetItem* pItem = CurrentMokTree()->currentItem();
	if (!pItem)
		return;

	int iIndex = pItem->data(0, Qt::UserRole).toInt();
	if (iIndex < 0 || iIndex >= (int)m_MokEntries.size())
		return;

	CCertDetailsWnd::Show(m_MokEntries[iIndex], this);
}

/* ---------------------------------------------------------------------------
 * Small helpers
 * ------------------------------------------------------------------------ */

//
// The prerequisites, checked when something is actually asked for. Each says
// what is missing and what to do about it, and returns false if the caller
// should stop.
//
bool CBootloaderWnd::RequirePackage(EBootPackage Package)
{
	if (CBootPackage::IsAvailable(Package))
		return true;

	QMessageBox::critical(this, "MajorPrivacy",
		tr("The boot loader files are not present in this installation.\n\n"
		   "Put the folder %1, or the archive %2, next to MajorPrivacy.exe in:\n%3\n\n"
		   "An unpacked folder is used as it is; the archive is only unpacked when no "
		   "folder is there.")
		.arg(QString::fromStdWString(CBootPackage::GetFolderName(Package)))
		.arg(QString::fromStdWString(CBootPackage::GetArchiveName(Package)))
		.arg(QString::fromStdWString(CBootPackage::GetAppDirectory())));

	return false;
}

bool CBootloaderWnd::RequireEsp()
{
	if (!m_Esps.empty())
		return true;

	QMessageBox::critical(this, "MajorPrivacy",
		tr("There is no EFI System Partition on this machine to install onto.\n\n"
		   "Use \"Create Partition...\" to make one."));

	return false;
}

bool CBootloaderWnd::Confirm(const QString& Text)
{
	return QMessageBox::question(this, "MajorPrivacy", Text,
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes;
}

void CBootloaderWnd::ShowError(const QString& What, const STATUS& Status)
{
	QString Message = QString::fromWCharArray(Status.GetMessageText());
	if (Message.isEmpty())
		Message = tr("Error 0x%1.").arg((quint32)Status.GetStatus(), 8, 16, QChar('0'));

	QMessageBox::critical(this, "MajorPrivacy", QString("%1\n\n%2").arg(What).arg(Message));
}

void CBootloaderWnd::ShowDone(const QString& Text)
{
	QMessageBox::information(this, "MajorPrivacy", Text);
}

bool CBootloaderWnd::AskMokPassword(const QString& Prompt, CSecurePassword& Password)
{
	//
	// The same prompt as everywhere else in this program, so that this password
	// can be typed on the secure desktop too. It is not feeding a key
	// derivation, so the KDF row comes off.
	//
	CVolumeWindow Wnd(Prompt, CVolumeWindow::eGetPW, this);
	Wnd.SetNoKdf();
	if (Wnd.exec() != QDialog::Accepted)
		return false;

	Password = Wnd.GetPassword();
	return true;
}

bool CBootloaderWnd::AskMokEnrollPassword(const QString& Prompt, QString& OutPassword)
{
	//
	// A plain, readable text box prefilled with the default - not a hidden
	// password field. The MOK password is not a secret the requester keeps from
	// anyone (the requester chooses it); it is a value the user reads here and
	// types back at the MokManager screen. Hiding it would only make it harder
	// to copy down for that step.
	//
	bool bOk = false;
	QString Password = QInputDialog::getText(this, "MajorPrivacy", Prompt,
		QLineEdit::Normal, "123", &bOk);
	if (!bOk)
		return false;

	OutPassword = Password;
	return true;
}

bool CBootloaderWnd::EnrollSignerForShim(const QString& MokPassword)
{
	//
	// The shim only runs the loader if the loader's signing certificate is in
	// shim's MOK allow list. Pre-queue it so the next boot only asks the user to
	// confirm, rather than making them find the certificate on disk in
	// MokManager.
	//
	CSecurePassword Password;
	Password.SetPassword(MokPassword.toStdWString());

	STATUS Status = CEfiInstall::EnrollShimSigner(Password);
	if (Status.IsError()) {
		ShowError(tr("The signing key could not be queued for MOK enrollment. You can still enroll it "
			"by hand from the Secure Boot tab (Enroll)."), Status);
		return false;
	}

	UpdateSecureBoot();
	return true;
}

QString CBootloaderWnd::MokEnrollSteps(const QString& MokPassword) const
{
	//
	// The next boot stops at the shim's blue MokManager screen. It is a plain
	// text menu that times out, so the steps are spelled out rather than left to
	// the user to work out under a countdown.
	//
	return tr("The boot loader is installed and the MajorPrivacy signing key is queued for enrollment.\n\n"
		"At the next restart a blue \"Shim UEFI key management\" screen appears before Windows. Step "
		"through it with the arrow keys and Enter:\n\n"
		"  1.  \"Press any key to perform MOK management\"  -  press a key within 10 seconds.\n"
		"  2.  Select  \"Enroll MOK\"  and press Enter.\n"
		"  3.  Select  \"Continue\"  and press Enter.\n"
		"  4.  Select  \"Yes\"  and press Enter to confirm.\n"
		"  5.  Type the password  %1  and press Enter.\n"
		"  6.  Select  \"Reboot\"  and press Enter.\n\n"
		"Windows then starts normally and the loader runs. If you miss the screen or cancel, nothing "
		"is enrolled - you can queue it again from the Secure Boot tab.").arg(MokPassword);
}

/* ---------------------------------------------------------------------------
 * The boot loader
 * ------------------------------------------------------------------------ */

SInstallOptions CBootloaderWnd::GetOptions() const
{
	SInstallOptions Options;
	Options.iDisk = CurrentDisk();
	Options.iPartition = CurrentPartition();
	Options.bShim = ui.chkShim->isChecked();
	Options.bBootEntry = ui.chkBootEntry->isChecked();
	Options.bDiskCryptorSupport = ui.chkDiskCryptor->isChecked();
	return Options;
}

void CBootloaderWnd::OnInstall()
{
	if (!RequireEsp() || !RequirePackage(eDcsPackage))
		return;
	if (ui.chkShim->isChecked() && !RequirePackage(eShimPackage))
		return;

	if (m_State.bSecureBoot && !ui.chkShim->isChecked() && !m_State.bDcsSignedByDb)
	{
		if (!Confirm(tr("Secure Boot is on and no shim is being installed. The firmware will "
			"refuse to run the boot loader: at best it skips to Windows and the loader never "
			"runs, at worst this machine stops at a Secure Boot error until the loader is "
			"removed again or Secure Boot is turned off.\n\nInstall anyway?")))
			return;
	}
	//
	// When the loader is trusted by the firmware's own db (self-rolled db / PK) it boots directly,
	// so no shim and no warning - that is the whole point of rolling your own keys.
	//

	//
	// Say what happens to the file already at the fallback path, by name. The
	// library keeps it only when there is one and no earlier backup exists, so
	// the message follows the same three cases rather than promising a backup
	// that may not be made.
	//
	QString DefaultPath = QString::fromWCharArray(CEfiInstall::GetDefaultLoaderPath());
	QString Text = tr("The boot loader will be installed on partition %1 of disk %2.")
		.arg(m_State.Esp.iPartition).arg(m_State.Esp.iDisk);
	if (m_State.bOriginalSaved)
		Text += tr("\n\nA loader saved by an earlier install is already on this partition.\n"
			"It is kept, and Remove puts it back.");
	else if (m_State.bHasDefaultLoader)
		Text += tr("\n\nThe file currently at %1 is saved first,\nand Remove puts it back.")
			.arg(DefaultPath);
	else
		Text += tr("\n\nNothing is at %1 yet, so there is nothing to save;\nthe loader is placed there.")
			.arg(DefaultPath);

	bool bShim = ui.chkShim->isChecked();

	//
	// Both branches confirm with an OK/Cancel box carrying the same statement, so
	// the two flows stay aligned. With a shim it is a readable text box that also
	// takes the MOK password: first say what is about to happen - a shim goes in
	// front of the loader and MokManager will stop the next boot to confirm the
	// key - then ask for the password and why. It is shown, not hidden: the user
	// reads it here and types it back at the boot screen, and it is no secret from
	// the requester anyway, so concealing it would only make that step harder.
	//
	QString MokPassword;
	if (bShim)
	{
		QString Prompt = Text + "\n\n"
			+ tr("Under Secure Boot a signed shim is installed in front of the loader,\n"
				"and the shim will only run the loader once its signing key is enrolled -\n"
				"so the next boot stops at MokManager for you to confirm that enrollment.")
			+ "\n\n"
			+ tr("Set the password MokManager will ask for there to confirm it.\n"
				"It is shown here rather than hidden, so you can note it down for that step:");
		if (!AskMokEnrollPassword(Prompt, MokPassword))
			return;
	}
	else if (QMessageBox::question(this, "MajorPrivacy", Text,
		QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Ok)
		return;

	STATUS Status = CEfiInstall::Install(GetOptions());
	if (Status.IsError()) {
		ShowError(tr("The boot loader could not be installed."), Status);
		OnRefresh();
		return;
	}

	OnRefresh();

	//
	// With a shim, pre-queue the loader's signing key into the MOK list and show
	// the step-by-step MokManager walkthrough. Without a shim, the plain done
	// message.
	//
	if (bShim && EnrollSignerForShim(MokPassword)) {
		ShowDone(MokEnrollSteps(MokPassword));
		return;
	}

	ShowDone(tr("The boot loader has been installed.\n\nBefore restarting, consider writing an external "
		"loader - it is what gets this machine booted again if the firmware will not run the one on the "
		"EFI partition."));
}

void CBootloaderWnd::OnUpdate()
{
	if (!RequirePackage(eDcsPackage))
		return;

	QString UpdateText = tr("Replace the installed boot loader files with the ones shipping with this "
		"build?\n\nThe configuration, the shim and the saved original loader are left as they are.");
	if (m_State.bDiskCryptorInstalled)
		UpdateText += tr("\n\nThe DiskCryptor modules on this partition are refreshed too, so they stay "
			"in step with the loader.");
	if (!Confirm(UpdateText))
		return;

	STATUS Status = CEfiInstall::Update(CurrentDisk(), CurrentPartition());
	if (Status.IsError()) {
		ShowError(tr("The boot loader could not be updated."), Status);
		OnRefresh();
		return;
	}

	OnRefresh();
	ShowDone(tr("The boot loader has been updated."));
}

void CBootloaderWnd::OnRemove()
{
	QString Text = tr("Remove the boot loader from partition %1 of disk %2?\n\nIts files and its "
		"firmware boot entry are removed.").arg(m_State.Esp.iPartition).arg(m_State.Esp.iDisk);

	//
	// Only the Windows ESP is worth a warning: there is always a fallback
	// loader on it, so a missing backup means it was lost. On any other
	// partition there was most likely nothing to keep in the first place. And
	// even then Windows boots through its own firmware entry, so what is lost
	// is the fallback, not the next boot.
	//
	if (m_State.bOriginalSaved)
		Text += tr(" The saved original loader is put back at %1.")
			.arg(QString::fromWCharArray(CEfiInstall::GetDefaultLoaderPath()));
	else if (m_State.bIsDefaultLoader && m_State.Esp.bHasMicrosoft)
		Text += tr("\n\nThe file that was at %1 before the install was not kept, so it cannot be put "
			"back. Windows still boots through its own firmware entry, but the fallback the firmware "
			"uses when its boot entries are lost will be missing until it is repaired.")
			.arg(QString::fromWCharArray(CEfiInstall::GetDefaultLoaderPath()));

	//
	// This loader is shared with DiskCryptor when its modules are present - removing it
	// takes away DiskCryptor's boot path too.
	//
	if (m_State.bDiskCryptorInstalled)
		Text += tr("\n\nDiskCryptor modules are installed on this partition. They share this boot "
			"loader, so removing it will also stop DiskCryptor from booting.");

	if (!Confirm(Text))
		return;

	STATUS Status = CEfiInstall::Remove(CurrentDisk(), CurrentPartition());
	if (Status.IsError()) {
		ShowError(tr("The boot loader could not be removed."), Status);
		OnRefresh();
		return;
	}

	OnRefresh();
	ShowDone(tr("The boot loader has been removed."));
}

void CBootloaderWnd::OnConfigure()
{
	RESULT(CDcsConfig) Res = CEfiInstall::LoadConfig(CurrentDisk(), CurrentPartition());
	if (Res.IsError()) {
		ShowError(tr("The boot loader configuration could not be read."), Res);
		return;
	}

	CBootloaderConfigWnd Wnd(Res.GetValue(), this);
	if (Wnd.exec() != QDialog::Accepted)
		return;

	STATUS Status = CEfiInstall::SaveConfig(Wnd.GetConfig(), CurrentDisk(), CurrentPartition());
	if (Status.IsError()) {
		ShowError(tr("The boot loader configuration could not be written."), Status);
		return;
	}

	UpdateState();
}

void CBootloaderWnd::OnCreateEsp()
{
	int iDisk = CurrentDiskRow();
	if (iDisk < 0)
		return;		// the button is only offered with a disk selected

	CBootloaderEspWnd Wnd(iDisk, this);
	if (Wnd.exec() != QDialog::Accepted)
		return;

	SCreateEspOptions Options = Wnd.GetOptions();

	QString Text = tr("Create a new EFI System Partition of %1 on disk %2?")
		.arg(FormatSize(Options.uSize)).arg(Options.iDisk);
	if (Options.bAllowShrink) {
		Text += tr("\n\nIf there is not enough free space, the Windows partition will be shrunk to "
			"make room. That rewrites the partition table and moves file system data; back up "
			"anything on this disk you are not prepared to lose before continuing.");
	}
	if (!Confirm(Text))
		return;

	RESULT(SEspInfo) Res = CGptDisk::CreateEsp(Options);
	if (Res.IsError()) {
		ShowError(tr("The EFI partition could not be created."), Res);
		OnRefresh();
		return;
	}

	SEspInfo NewEsp = Res.GetValue();
	OnRefresh();

	SelectEsp(NewEsp.iDisk, NewEsp.iPartition);

	ShowDone(tr("The EFI partition has been created."));
}

//
// Put the selection on one partition, wherever under which disk it ended up.
//
void CBootloaderWnd::SelectEsp(int iDisk, int iPartition)
{
	for (int i = 0; i < ui.treeEsp->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* pDisk = ui.treeEsp->topLevelItem(i);
		for (int j = 0; j < pDisk->childCount(); j++)
		{
			QTreeWidgetItem* pItem = pDisk->child(j);
			int iIndex = pItem->data(0, ESP_ROLE_INDEX).toInt();
			if (iIndex >= 0 && iIndex < (int)m_Esps.size()
				&& m_Esps[iIndex].iDisk == iDisk && m_Esps[iIndex].iPartition == iPartition) {
				ui.treeEsp->setCurrentItem(pItem);
				return;
			}
		}
	}
}

void CBootloaderWnd::OnRemoveEsp()
{
	int iIndex = CurrentEspIndex();
	if (iIndex < 0)
		return;

	const SEspInfo& Esp = m_Esps[iIndex];

	//
	// Refused rather than merely discouraged. Deleting the partition Windows
	// boots from leaves a machine that does not start, and no confirmation
	// dialog makes that a reasonable thing to offer.
	//
	if (Esp.bHasMicrosoft) {
		QMessageBox::critical(this, "MajorPrivacy",
			tr("This is the EFI partition Windows boots from. Removing it would leave this "
			   "machine unable to start."));
		return;
	}

	QString Text = tr("Delete partition %1 on disk %2?\n\nEverything on it is lost and the "
		"partition table is rewritten.").arg(Esp.iPartition).arg(Esp.iDisk);

	if (Esp.bHasDcs)
		Text += tr("\n\nThe boot loader is installed on it. Anything in the firmware boot menu "
			"pointing at this partition will stop working.");

	if (!Confirm(Text))
		return;

	STATUS Status = CGptDisk::DeletePartition(Esp.iDisk, Esp.iPartition);
	if (Status.IsError()) {
		ShowError(tr("The EFI partition could not be removed."), Status);
		OnRefresh();
		return;
	}

	OnRefresh();
	ShowDone(tr("The EFI partition has been removed."));
}

QString CBootloaderWnd::CurrentMountPoint() const
{
	int iIndex = CurrentEspIndex();
	return iIndex < 0 ? QString() : QString::fromStdWString(m_Esps[iIndex].MountPoint);
}

void CBootloaderWnd::OnMount()
{
	if (!RequireEsp())
		return;

	QString Mounted = CurrentMountPoint();

	if (!Mounted.isEmpty())
	{
		STATUS Status = CEspLocator::Unmount(Mounted.toStdWString());
		if (Status.IsError()) {
			ShowError(tr("The drive letter could not be removed."), Status);
			return;
		}

		OnRefresh();
		return;
	}

	QString Letter;
	if (MountEsp(Letter))
		ShowDone(tr("The EFI partition is now available as %1.").arg(Letter));
}

bool CBootloaderWnd::MountEsp(QString& Letter)
{
	//
	// Offer the free letters with the first one selected, so that accepting
	// the dialog unchanged takes the next one going.
	//
	std::vector<std::wstring> Free = CEspLocator::GetFreeDriveLetters();
	if (Free.empty()) {
		QMessageBox::critical(this, "MajorPrivacy",
			tr("There is no free drive letter to mount the EFI partition on."));
		return false;
	}

	QStringList Letters;
	for (size_t i = 0; i < Free.size(); i++)
		Letters.append(QString::fromStdWString(Free[i]));

	bool bOk = false;
	QString Picked = QInputDialog::getItem(this, tr("Mount EFI Partition"),
		tr("Give the EFI partition a drive letter.\n\nWhile it has one the partition is visible "
		   "to everything on this machine, so remove it again when you are done."),
		Letters, 0, false, &bOk);

	if (!bOk || Picked.isEmpty())
		return false;

	STATUS Status = CEspLocator::Mount(CurrentDisk(), CurrentPartition(), Picked.toStdWString());
	if (Status.IsError()) {
		ShowError(tr("The EFI partition could not be mounted."), Status);
		return false;
	}

	OnRefresh();

	Letter = Picked;
	return true;
}

/* ---------------------------------------------------------------------------
 * The external loader
 *
 * The same loader on something other than this machine's EFI partition - a
 * stick, an image, a network boot directory. It is what gets a machine booted
 * when its EFI partition cannot be, and what boots one without touching its
 * disks at all. Worth writing before the loader is installed rather than
 * after, which is why it is a tab of its own rather than something to find
 * afterwards.
 * ------------------------------------------------------------------------ */

void CBootloaderWnd::LoadMediaDrives()
{
	ui.cmbDrive->clear();

	//
	// Removable drives only - that is all CRecoveryMedia will write to, and
	// offering a fixed disk here would only produce a refusal later.
	//
	QList<QStorageInfo> Drives = QStorageInfo::mountedVolumes();
	foreach(const QStorageInfo & Drive, Drives)
	{
		QString Root = Drive.rootPath();
		if (Root.length() < 2 || Root[1] != ':')
			continue;
		if (!CRecoveryMedia::IsRemovableDrive(Root.toStdWString()))
			continue;

		QString Text = Root;
		if (!Drive.displayName().isEmpty() && Drive.displayName() != Root)
			Text += QString(" (%1)").arg(Drive.displayName());
		if (Drive.bytesTotal() > 0)
			Text += QString(" - %1").arg(FormatSize(Drive.bytesTotal()));

		ui.cmbDrive->addItem(Text, Root);
	}

	if (ui.cmbDrive->count() == 0 && ui.radDrive->isChecked())
		ui.radIso->setChecked(true);
}

void CBootloaderWnd::LoadMediaConfig()
{
	//
	// Start from the installation's own configuration when there is one, so a
	// rescue disk boots the way the machine does - the keyboard layout above
	// all, since getting that wrong is exactly the situation where the password
	// cannot be typed to recover.
	//
	// Once it has been edited here it belongs to the user, and a refresh no
	// longer overwrites it.
	//
	if (m_bMediaConfigEdited)
		return;

	m_MediaConfig = CDcsConfig();

	if (m_State.bInstalled) {
		RESULT(CDcsConfig) Res = CEfiInstall::LoadConfig(CurrentDisk(), CurrentPartition());
		if (!Res.IsError())
			m_MediaConfig = Res.GetValue();
	}

	if (m_MediaConfig.IsEmpty())
		m_MediaConfig = CDcsConfig::MakeDefault();

	UpdateMediaConfigLabel();
}

void CBootloaderWnd::UpdateMediaConfigLabel()
{
	int iKeyboard = m_MediaConfig.GetInt(DCS_KEY_KEYBOARD_LAYOUT, 0);
	const wchar_t* Layouts[] = { L"QWERTY", L"QWERTZ", L"AZERTY" };

	QString Text = QString::fromWCharArray(
		iKeyboard >= 0 && iKeyboard < 3 ? Layouts[iKeyboard] : L"QWERTY");

	if (m_MediaConfig.GetInt(DCS_KEY_VERBOSE_DEBUG, 0))
		Text += tr(", verbose");

	if (m_bMediaConfigEdited)
		Text += tr(" (edited)");

	ui.lblMediaConfig->setText(Text);
}

void CBootloaderWnd::OnMediaConfigure()
{
	LoadMediaConfig();

	CBootloaderConfigWnd Wnd(m_MediaConfig, this);
	if (Wnd.exec() != QDialog::Accepted)
		return;

	m_MediaConfig = Wnd.GetConfig();
	m_bMediaConfigEdited = true;

	UpdateMediaConfigLabel();
}

void CBootloaderWnd::OnMediaTargetChanged()
{
	bool bDrive = ui.radDrive->isChecked();
	bool bHaveDrives = ui.cmbDrive->count() > 0;

	ui.cmbDrive->setEnabled(bDrive && bHaveDrives);
	ui.chkFormat->setEnabled(bDrive && bHaveDrives);
	ui.txtPath->setEnabled(!bDrive);
	ui.btnBrowse->setEnabled(!bDrive);
}

void CBootloaderWnd::OnMediaBrowse()
{
	QString Path;
	if (ui.radIso->isChecked()) {
		Path = QFileDialog::getSaveFileName(this, tr("Save Loader Image"), ui.txtPath->text(),
			tr("ISO Images (*.iso);;All Files (*.*)"));
	}
	else {
		Path = QFileDialog::getExistingDirectory(this, tr("Select a folder for the PXE files"),
			ui.txtPath->text());
	}

	if (!Path.isEmpty())
		ui.txtPath->setText(Path);
}

void CBootloaderWnd::OnWriteMedia()
{
	if (!RequirePackage(eDcsPackage))
		return;
	if (ui.chkMediaShim->isChecked() && !RequirePackage(eShimPackage))
		return;

	LoadMediaConfig();

	SRecoveryOptions Options;
	Options.bShim = ui.chkMediaShim->isChecked();
	Options.bFormat = ui.radDrive->isChecked() && ui.chkFormat->isChecked();
	Options.Config = m_MediaConfig;

	QString Path = ui.radDrive->isChecked()
		? ui.cmbDrive->currentData().toString() : ui.txtPath->text();

	if (Path.isEmpty()) {
		QMessageBox::critical(this, "MajorPrivacy", ui.radDrive->isChecked()
			? tr("There is no removable drive to write to.")
			: tr("No target path was given."));
		return;
	}

	STATUS Status;

	if (ui.radDrive->isChecked())
	{
		if (!Confirm(Options.bFormat
			? tr("Write the boot loader to %1?\n\nThe drive will be formatted first and "
				 "everything on it will be lost.").arg(Path)
			: tr("Write the boot loader to %1?").arg(Path)))
			return;

		Status = CRecoveryMedia::WriteToDrive(Path.replace("/", "\\").toStdWString(), Options);
	}
	else if (ui.radIso->isChecked())
		Status = CRecoveryMedia::WriteIso(Path.replace("/", "\\").toStdWString(), Options);
	else
		Status = CRecoveryMedia::WritePxe(Path.replace("/", "\\").toStdWString(), Options);

	if (Status.IsError()) {
		ShowError(tr("The external loader could not be written."), Status);
		return;
	}

	LoadMediaDrives();
	ShowDone(tr("The external loader has been written."));
}

/* ---------------------------------------------------------------------------
 * Secure Boot and machine owner keys
 *
 * Nothing here takes effect while Windows is running. Every one of these writes
 * a request into a firmware variable which MokManager picks up at the next
 * boot and asks the person at the keyboard to confirm - which is the whole
 * point, because otherwise anything running as administrator could enroll a key
 * that Secure Boot would then trust.
 * ------------------------------------------------------------------------ */

void CBootloaderWnd::OnEnroll()
{
	QStringList Files = QFileDialog::getOpenFileNames(this, tr("Select Certificates"), QString(),
		tr("Certificates (*.der *.cer *.crt *.pem);;All Files (*.*)"));
	if (Files.isEmpty())
		return;

	CSecurePassword Password;
	if (!AskMokPassword(tr("Enter the MOK password. This is what MokManager will ask for at the "
		"boot screen before it enrolls anything. Leave it empty if no password is set."), Password))
		return;

	std::vector<std::wstring> Paths;
	foreach(const QString & File, Files)
		Paths.push_back(QString(File).replace("/", "\\").toStdWString());

	STATUS Status = CMokManager::EnrollCertFiles(Paths, Password, CurrentMokList());
	if (Status.IsError()) {
		ShowError(tr("The keys could not be queued for enrollment."), Status);
		return;
	}

	UpdateSecureBoot();
	ShowDone(tr("The keys are queued. Restart this machine and confirm the enrollment in "
		"MokManager, which comes up before Windows."));
}

void CBootloaderWnd::OnDelete()
{
	//
	// Whatever is selected in the list, and nothing else. The list is what is
	// actually enrolled, so picking from it is both the obvious gesture and the
	// only one that cannot queue a request for a key that was never there.
	//
	QTreeWidgetItem* pItem = CurrentMokTree()->currentItem();
	if (!pItem)
		return;		// the button is disabled without a selection

	int iIndex = pItem->data(0, Qt::UserRole).toInt();
	if (iIndex < 0 || iIndex >= (int)m_MokEntries.size())
		return;

	const SMokEntry& Entry = m_MokEntries[iIndex];

	QString What = pItem->text(1);
	if (!Confirm(CurrentMokList() == eMokDeny
		? tr("Queue %1 to be unblocked?").arg(What)
		: tr("Queue %1 for deletion?").arg(What)))
		return;

	CSecurePassword Password;
	if (!AskMokPassword(tr("Enter the MOK password. Leave it empty if no password is set."), Password))
		return;

	STATUS Status = Entry.bIsCert
		? CMokManager::DeleteCert(Entry.Data, Password, CurrentMokList())
		: CMokManager::DeleteHash(Entry.Data, Password, CurrentMokList());

	if (Status.IsError()) {
		ShowError(tr("The key could not be queued for removal."), Status);
		return;
	}

	UpdateSecureBoot();
	ShowDone(tr("The change is queued. Restart this machine and confirm it in MokManager."));
}

void CBootloaderWnd::OnExport()
{
	QString Dir = QFileDialog::getExistingDirectory(this, tr("Select a folder for the exported keys"));
	if (Dir.isEmpty())
		return;

	// only the two machine owner key tabs carry an Export button
	RESULT(int) Res = CMokManager::Export(Dir.replace("/", "\\").toStdWString(), CurrentMokList());
	if (Res.IsError()) {
		ShowError(tr("The keys could not be exported."), Res);
		return;
	}

	ShowDone(tr("%1 key(s) written to %2.").arg(Res.GetValue()).arg(Dir));
}

void CBootloaderWnd::OnRevoke()
{
	if (!Confirm(tr("Withdraw the requests waiting for the next boot?")))
		return;

	EMokList List = CurrentMokList();

	STATUS Status = CMokManager::RevokeEnroll(List);
	if (Status.IsSuccess())
		Status = CMokManager::RevokeDelete(List);

	if (Status.IsError()) {
		ShowError(tr("The pending requests could not be withdrawn."), Status);
		UpdateSecureBoot();
		return;
	}

	UpdateSecureBoot();
	ShowDone(tr("The pending requests have been withdrawn."));
}

void CBootloaderWnd::OnReset()
{
	if (!Confirm(tr("Queue a reset of this key list?\n\nAt the next boot MokManager will erase "
		"every key in it. Anything signed only by one of those keys will no longer run under "
		"Secure Boot - including, if the shim is installed, the boot loader.")))
		return;

	CSecurePassword Password;
	if (!AskMokPassword(tr("Enter the MOK password. Leave it empty if no password is set."), Password))
		return;

	STATUS Status = CMokManager::Reset(Password, CurrentMokList());
	if (Status.IsError()) {
		ShowError(tr("The reset could not be queued."), Status);
		return;
	}

	UpdateSecureBoot();
	ShowDone(tr("The reset is queued. Restart this machine and confirm it in MokManager."));
}

void CBootloaderWnd::OnSetPassword()
{
	CVolumeWindow Wnd(tr("Set the MOK password. MokManager asks for this at the boot screen "
		"before it will change anything, so it has to be typed on a keyboard the firmware "
		"reads - keep it to characters that are in the same place on every layout."),
		CVolumeWindow::eSetPW, this);
	Wnd.SetNoKdf();
	if (Wnd.exec() != QDialog::Accepted)
		return;

	STATUS Status = CMokManager::SetPassword(Wnd.GetNewPassword());
	if (Status.IsError()) {
		ShowError(tr("The MOK password could not be set."), Status);
		return;
	}

	UpdateSecureBoot();
	ShowDone(tr("The MOK password has been set."));
}

void CBootloaderWnd::OnClearPassword()
{
	if (!Confirm(tr("Clear the MOK password?\n\nMokManager will then make the changes queued "
		"here without asking for anything.")))
		return;

	STATUS Status = CMokManager::ClearPassword();
	if (Status.IsError()) {
		ShowError(tr("The MOK password could not be cleared."), Status);
		return;
	}

	UpdateSecureBoot();
}

/* ---------------------------------------------------------------------------
 * The configuration sub window
 * ------------------------------------------------------------------------ */

CBootloaderConfigWnd::CBootloaderConfigWnd(const CDcsConfig& Config, QWidget* parent)
	: QDialog(parent), m_Config(Config)
{
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui.setupUi(this);
	setWindowTitle(tr("MajorPrivacy - Boot Loader Configuration"));

	// the three the loader knows, with the values DiskCryptor writes
	ui.cmbKeyboard->addItem("QWERTY", 0);
	ui.cmbKeyboard->addItem("QWERTZ", 1);
	ui.cmbKeyboard->addItem("AZERTY", 2);

	int iLayout = m_Config.GetInt(DCS_KEY_KEYBOARD_LAYOUT, 0);
	int iIndex = ui.cmbKeyboard->findData(iLayout);
	ui.cmbKeyboard->setCurrentIndex(iIndex >= 0 ? iIndex : 0);

	ui.chkVerbose->setChecked(m_Config.GetInt(DCS_KEY_VERBOSE_DEBUG, 0) != 0);

	//
	// Say what is happening to everything else in the file, because it is not
	// visible here and a configuration editor that silently drops what it does
	// not understand is worth distrusting.
	//
	int iOther = (int)m_Config.GetKeyOrder().size();
	iOther -= m_Config.Has(DCS_KEY_KEYBOARD_LAYOUT) ? 1 : 0;
	iOther -= m_Config.Has(DCS_KEY_VERBOSE_DEBUG) ? 1 : 0;

	if (iOther > 0) {
		ui.lblNote->setText(tr("The file holds %1 further setting(s). They are not shown here, "
			"and are written back exactly as they were.").arg(iOther));
	}

	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

CDcsConfig CBootloaderConfigWnd::GetConfig() const
{
	CDcsConfig Config = m_Config;
	Config.SetInt(DCS_KEY_KEYBOARD_LAYOUT, ui.cmbKeyboard->currentData().toInt());
	Config.SetInt(DCS_KEY_VERBOSE_DEBUG, ui.chkVerbose->isChecked() ? 1 : 0);
	return Config;
}

/* ---------------------------------------------------------------------------
 * Creating an EFI partition
 * ------------------------------------------------------------------------ */

CBootloaderEspWnd::CBootloaderEspWnd(int iDisk, QWidget* parent)
	: QDialog(parent)
{
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui.setupUi(this);
	setWindowTitle(tr("MajorPrivacy - Create EFI Partition"));

	ui.txtLabel->setText(QString::fromWCharArray(DCS_ESP_VOLUME_LABEL));
	ui.spinSize->setValue(DCS_ESP_DEFAULT_SIZE / (1024 * 1024));
	ui.spinSize->setMinimum(DCS_ESP_MIN_SIZE / (1024 * 1024));

	m_Disks = CEspLocator::EnumDisks();
	int iSelect = 0;
	for (size_t i = 0; i < m_Disks.size(); i++)
	{
		const SDiskInfo& Disk = m_Disks[i];

		QString Text = tr("Disk %1 - %2").arg(Disk.iDisk).arg(FormatSize(Disk.uSize));
		if (!Disk.Name.empty())
			Text += QString(" (%1)").arg(QString::fromStdWString(Disk.Name));
		if (Disk.bIsOsDisk)
			Text += tr(" [Windows]");
		if (!Disk.bGpt)
			Text += tr(" [not GPT]");

		ui.cmbDisk->addItem(Text, Disk.iDisk);

		// the disk picked in the main window, or the one Windows is on
		if (Disk.iDisk == iDisk)
			iSelect = (int)i;
		else if (iDisk < 0 && Disk.bIsOsDisk)
			iSelect = (int)i;
	}
	ui.cmbDisk->setCurrentIndex(iSelect);

	connect(ui.cmbDisk, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CBootloaderEspWnd::OnDiskChanged);
	connect(ui.spinSize, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int) { UpdateFreeSpace(); });
	connect(ui.chkShrink, &QCheckBox::clicked, this, &CBootloaderEspWnd::OnShrinkToggled);

	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	UpdateFreeSpace();
}

void CBootloaderEspWnd::OnDiskChanged()
{
	UpdateFreeSpace();
}

void CBootloaderEspWnd::OnShrinkToggled()
{
	UpdateFreeSpace();
}

void CBootloaderEspWnd::UpdateFreeSpace()
{
	int iDisk = ui.cmbDisk->currentData().toInt();
	quint64 uWanted = (quint64)ui.spinSize->value() * 1024 * 1024;

	//
	// Whether the partition can be made without touching anything that is
	// already there. This is the one question worth answering before the user
	// presses OK, because the answer decides whether the shrink checkbox is a
	// formality or the whole operation.
	//
	RESULT(SFreeSpace) Res = CGptDisk::FindFreeSpace(iDisk, uWanted);
	bool bFits = !Res.IsError() && Res.GetValue().bFound;

	if (bFits) {
		ui.lblFree->setText(tr("There is unallocated space on this disk for the new partition."));
		ui.lblWarning->setText(QString());
	}
	else {
		ui.lblFree->setText(tr("There is not enough unallocated space on this disk."));

		if (!ui.chkShrink->isChecked()) {
			ui.lblWarning->setText(tr("<b>Allow shrinking below, or choose another disk.</b>"));
		}
		else {
			quint64 uMax = CGptDisk::GetMaxShrink(iDisk, CGptDisk::GetOsPartition(iDisk));
			if (uMax < uWanted) {
				ui.lblWarning->setText(tr("<b>The Windows partition can only give up %1, which is "
					"not enough.</b>").arg(FormatSize(uMax)));
			}
			else {
				ui.lblWarning->setText(tr("The Windows partition will be shrunk by %1. Back up this "
					"disk first.").arg(FormatSize(uWanted)));
			}
		}
	}

	ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(bFits || ui.chkShrink->isChecked());
}

SCreateEspOptions CBootloaderEspWnd::GetOptions() const
{
	SCreateEspOptions Options;
	Options.iDisk = ui.cmbDisk->currentData().toInt();
	Options.uSize = (quint64)ui.spinSize->value() * 1024 * 1024;
	Options.bAllowShrink = ui.chkShrink->isChecked();
	Options.Label = ui.txtLabel->text().toStdWString();
	return Options;
}
