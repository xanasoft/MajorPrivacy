#pragma once
/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

//
// Keyfiles - files whose contents are folded into the password before it is
// used, so that knowing the password is not enough on its own.
//
// This is entirely a matter for the GUI. ImBox, and DiskCryptor's driver
// underneath it, only ever see the finished password; nothing on the volume
// records that keyfiles were involved, and nothing needs to. Which is also why
// losing a keyfile is exactly as bad as forgetting the password.
//
// The mixing itself is DiskCryptor's, out of dcvolume.dll, so a volume opened
// here with keyfiles opens in DiskCryptor with the same ones.
//

#include <QDialog>
#include "ui_KeyFiles.h"
#include "../../Library/Crypto/SecurePassword.h"

//
// Which mixing to use. Not stored anywhere - it has to be remembered, the same
// way the set of keyfiles does - so the numbering is this program's own.
//
enum EKeyFileMix
{
	eKeyFileCanonical = 0,	// hash, sort, deduplicate, fold in under a domain string
	eKeyFileAdditive  = 1,	// DiskCryptor's original: add each hash into the buffer
};

//
// One entry: a file, a folder, or a virtual keyfile.
//
// A virtual keyfile is a second password used as though it were a file. It
// never touches the disk, which is the point - it is for a secret you want to
// keep in your head rather than on a stick, while still getting the property
// that the volume password alone will not open the volume.
//
// Because it is not a file, there is nothing to check it against, so its CRC-32
// is what it is named by. Retype it and the same name appears; mistype it and a
// different one does. DiskCryptor names them the same way from the same value,
// so the two programs agree on what a given virtual keyfile is called.
//
struct SKeyFile
{
	bool			bVirtual = false;
	QString			Path;		// file or folder, when not virtual
	CSecurePassword	Data;		// the bytes, when virtual
	quint32			Crc = 0;	// CRC-32 of Data; it is what the entry is named by

	QString			Name() const;
};

struct SKeyFiles
{
	QList<SKeyFile>	Files;
	int				MixMode = eKeyFileCanonical;

	bool			IsEmpty() const { return Files.isEmpty(); }
	int				Count() const { return Files.count(); }
};

class CKeyFilesWnd : public QDialog
{
	Q_OBJECT

public:
	//
	// bAllowVirtual is false for the keyfile list of a virtual keyfile's own
	// password. One level of nesting is useful - a virtual keyfile can have
	// files mixed into it - but a tree of them is a way to lose a volume, and
	// DiskCryptor draws the line in the same place.
	//
	CKeyFilesWnd(const SKeyFiles& KeyFiles, QWidget* parent = nullptr, bool bAllowVirtual = true);

	SKeyFiles			GetKeyFiles() const { return m_KeyFiles; }

	//
	// Fold the keyfiles into a password, in place. An empty set leaves the
	// password alone and succeeds; anything that goes wrong - a file that
	// cannot be read, an empty directory, a missing library - fails loudly
	// rather than quietly producing a password that opens nothing.
	//
	static bool			Mix(const SKeyFiles& KeyFiles, CSecurePassword& Password, QString* pError);

	// True when dcvolume.dll is there to do the mixing with.
	static bool			IsAvailable();

	// CRC-32 of a buffer, the same one DiskCryptor names virtual keyfiles by.
	static quint32		Crc32(const void* pData, size_t uSize);

private slots:
	void				OnAddFiles();
	void				OnAddFolder();
	void				OnAddVirtual();
	void				OnRemove();
	void				OnClear();
	void				OnModeChanged();
	void				OnSelectionChanged();

private:
	void				AddPaths(const QStringList& Paths);
	void				Populate();

	Ui::KeyFiles		ui;
	SKeyFiles			m_KeyFiles;
	bool				m_bAllowVirtual;
};
