#pragma once
/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

//
// The EFI boot loader window - installing DCS on an EFI System Partition, and
// the Secure Boot / machine owner key state that decides whether the firmware
// will run it.
//
// What is installed here is less than DiskCryptor installs. DcsInt, the module
// that decrypts a boot partition, is deliberately left out: MajorPrivacy does
// not encrypt partitions. What is wanted is DcsOwner, which holds a user key in
// a firmware variable the operating system cannot read back, and the loader
// that runs it. See Library\Boot\EfiInstall.h for the rest of that reasoning.
//
// Everything in here needs administrator rights - firmware variables and raw
// partition access are not available to a normal user, and neither is writing
// to an EFI partition. The GUI is usually not elevated, so Open() starts a
// second copy of this program with -bootloader, which brings up this window and
// nothing else: no service, no driver, no main window. If the GUI is already
// elevated it just opens the window here.
//

#include <QDialog>
#include "ui_Bootloader.h"
#include "ui_BootloaderConfig.h"
#include "ui_BootloaderEsp.h"

#include "../../Library/Boot/EfiInstall.h"
#include "../../Library/Boot/MokManager.h"
#include "../../Library/Boot/GptDisk.h"
#include "../../Library/Boot/RecoveryMedia.h"

//
// The command line that brings up this window on its own. main() looks for it
// before it builds anything else.
//
#define BOOTLOADER_ARG	"-bootloader"

class CBootloaderWnd : public QDialog
{
	Q_OBJECT

public:
	//
	// Open the window, elevating first if need be. Returns straight away in the
	// elevating case - the second process owns the window from there, and this
	// one has nothing left to wait for.
	//
	static void			Open(QWidget* parent);

	//
	// The -bootloader entry point: this process is already elevated and exists
	// only to show this window. Runs its own event loop and returns the exit
	// code for main().
	//
	static int			RunStandalone();

	CBootloaderWnd(QWidget* parent = nullptr);
	~CBootloaderWnd();

private slots:
	void				OnRefresh();
	void				OnEspChanged();
	void				OnOptionChanged();
	void				OnApply();

	void				OnInstall();
	void				OnUpdate();
	void				OnRemove();
	void				OnConfigure();
	void				OnCreateEsp();
	void				OnRemoveEsp();
	void				OnMount();

	void				OnMediaTargetChanged();
	void				OnMediaBrowse();
	void				OnMediaConfigure();
	void				OnWriteMedia();

	void				OnMokRefresh();
	void				OnMokListChanged();
	void				OnMokOptionChanged();
	void				OnMokApply();
	void				OnEntryActivated();
	void				OnEnroll();
	void				OnDelete();
	void				OnExport();
	void				OnRevoke();
	void				OnReset();
	void				OnSetPassword();
	void				OnClearPassword();

private:
	void				LoadEsps();			// fill the partition list
	void				UpdateState();		// re-read and show the loader state
	void				UpdateSecureBoot();	// re-read and show the MOK state
	void				UpdateButtons();
	static void			SizeButtonColumn(class QPushButton** pButtons, int iCount);

	//
	// The recovery media tab. Its configuration is seeded from the installed
	// loader the first time the tab is looked at, so that a rescue disk boots
	// the way the machine does - see LoadMediaConfig.
	//
	void				LoadMediaDrives();
	void				LoadMediaConfig();
	void				UpdateMediaConfigLabel();

	SInstallOptions		GetOptions() const;

	//
	// Whether the two settings on the details pane differ from what is actually
	// on the partition. That is the whole of what Apply has to do, and what
	// decides whether it is offered.
	//
	bool				OptionsChanged() const;
	bool				MokOptionsChanged() const;

	int					CurrentEspIndex() const;

	//
	// The disk of the row on screen when that row is a disk rather than one of
	// its partitions, otherwise -1. Creating a partition needs a disk and
	// nothing else, so that is the selection it works from.
	//
	int					CurrentDiskRow() const;

	void				SelectEsp(int iDisk, int iPartition);
	int					CurrentDisk() const;
	int					CurrentPartition() const;
	QString				CurrentMountPoint() const;
	//
	// Which key list the Secure Boot tab is showing. The sub-tab is the whole
	// answer, which is why the buttons on each one can share a slot.
	//
	EMokList			CurrentMokList() const;
	bool				IsFirmwareList() const;	// db / dbx, which are read only here
	class QTreeWidget*	CurrentMokTree() const;

	//
	// The MOK password, asked for on the secure desktop like every other
	// password in this program. Returns false if the user cancelled; an empty
	// password is a valid answer, because a machine may have none set.
	//
	bool				AskMokPassword(const QString& Prompt, CSecurePassword& Password);

	//
	// Ask for the MOK enrollment password in a plain, readable text box (not a
	// hidden password field) prefilled with a default. This is a value the user
	// has to read and then type back at the boot screen, not a secret to
	// conceal - the secrecy does nothing here anyway (see the note on OnInstall).
	// Returns false if cancelled.
	//
	bool				AskMokEnrollPassword(const QString& Prompt, QString& OutPassword);

	//
	// Pre-queue the loader's signing certificate into shim's MOK list
	// (CEfiInstall::EnrollShimSigner) with the given password, so the next boot
	// only asks to confirm it. Returns true if a request was queued; false on
	// failure (already reported), in which case it can still be enrolled by hand
	// from the Secure Boot tab.
	//
	bool				EnrollSignerForShim(const QString& MokPassword);

	// The spelled-out MokManager steps for the finish dialog, with the password
	// the user set shown in step 5.
	QString				MokEnrollSteps(const QString& MokPassword) const;

	//
	// Pick a free drive letter and give it to the selected partition. Returns
	// false if there was none, the user cancelled, or Windows refused - all of
	// which have already been reported by then.
	//
	bool				MountEsp(QString& Letter);

	bool				RequirePackage(EBootPackage Package);
	bool				RequireEsp();

	bool				Confirm(const QString& Text);
	void				ShowError(const QString& What, const STATUS& Status);
	void				ShowDone(const QString& Text);

	Ui::Bootloader		ui;

	std::vector<SEspInfo>	m_Esps;
	SBootLoaderState		m_State;
	SSecureBootState		m_SbState;
	std::vector<SMokEntry>	m_MokEntries;	// of the sub-tab on screen

	CDcsConfig			m_MediaConfig;
	bool				m_bMediaConfigEdited = false;

	bool				m_bLoading = false;
};

//
// The loader's own settings, the ones it reads out of DcsProp before Windows
// starts. Only the two that mean anything without DcsInt are offered; every
// other key the file happens to carry is left exactly as it was - see
// Library\Boot\DcsConfig.h.
//
class CBootloaderConfigWnd : public QDialog
{
	Q_OBJECT

public:
	CBootloaderConfigWnd(const CDcsConfig& Config, QWidget* parent = nullptr);

	CDcsConfig			GetConfig() const;

private:
	Ui::BootloaderConfig	ui;
	CDcsConfig				m_Config;
};

//
// Creating a second EFI System Partition. Kept apart from the main window
// because it is the one thing here that rewrites a partition table.
//
class CBootloaderEspWnd : public QDialog
{
	Q_OBJECT

public:
	// iDisk is the disk picked in the main window, preselected here.
	CBootloaderEspWnd(int iDisk, QWidget* parent = nullptr);

	SCreateEspOptions	GetOptions() const;

private slots:
	void				OnDiskChanged();
	void				OnShrinkToggled();

private:
	void				UpdateFreeSpace();

	Ui::BootloaderEsp	ui;
	std::vector<SDiskInfo>	m_Disks;
};
