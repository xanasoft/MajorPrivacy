#pragma once
/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

//
// The volume header editor - key slots and header properties, on an unmounted
// container. Modelled on DiskCryptor's own editor (gui\prc_header.c) and using
// the same library underneath, so a container edited here and one edited there
// end up byte-identical.
//
// How it works is the part worth knowing:
//
//   ImBox.exe opens the container and puts the *decrypted* header into a
//   page-locked shared section. This dialog edits it in place - there is no
//   copy of it on the heap, because the header carries the volume's master key.
//   On apply, the edited header is sealed again under the volume's current
//   header key and handed back for ImBox to check and write.
//
//   That sealing is the whole authentication. Only something holding the header
//   key can produce a blob which comes out of ImBox's decrypt as a valid header,
//   which is the same bargain DiskCryptor's GUI has with its driver.
//
// Nothing here links dcvolume.dll; it is resolved at run time through
// CDcVolume, and the editor refuses to open if it is not installed.
//

#include <QDialog>
#include "ui_HeaderEditor.h"
#include "../../Library/Crypto/SecurePassword.h"

class CImBoxSession;
struct _dcvol_api;
struct _dc_header;
struct _xts_key;

class CHeaderEditorWnd : public QDialog
{
	Q_OBJECT

public:
	//
	// Ask for the password, read the header, and run the editor. Returns true
	// if anything was written. Everything is reported to the user on the way,
	// so a false return needs no further message.
	//
	static bool				Edit(const QString& VolumePath, QWidget* parent);

	//
	// The same editor on a header backup file. There is no volume and no ImBox
	// here - a .pvh is a sealed header and nothing else, so the library opens
	// it, this edits it, and the library seals it again. Handy for repairing a
	// backup before restoring it, and for putting a slot password on one.
	//
	static bool				EditFile(const QString& FilePath, QWidget* parent);

	~CHeaderEditorWnd();

protected:
	CHeaderEditorWnd(const QString& VolumePath, bool bFileMode, QWidget* parent);

	bool					Load();			// password prompt, read, derive
	bool					ReadHeader();	// whichever of the two below applies
	bool					ReadHeaderFromVolume();
	bool					ReadHeaderFromFile();
	bool					DeriveHeaderKey();
	bool					Apply();
	bool					SaveHeader(const void* pSealed, quint32 uLen, quint32 uFlags);
	bool					SaveHeaderToFile(const void* pSealed, quint32 uLen);

	struct _dc_header*		Header() const;
	int						HeadLen() const;

	void					PopulateSlots();
	void					UpdateProps();
	void					UpdateButtons();
	void					SetModified();

	bool					GetSlotPassword(const QString& Prompt, bool bNew, CSecurePassword& Password, int& iKdf);
	bool					DeriveKey(const CSecurePassword& Password, int iKdf, int iSlot, unsigned char* pDk);

private slots:
	void					OnSlotSelected();
	void					OnSetSlotPassword();
	void					OnTestSlotPassword();
	void					OnRenameSlot();
	void					OnToggleSlot();
	void					OnClearSlot();
	void					OnFieldChanged();
	void					OnClicked(QAbstractButton* pButton);

private:
	Ui::HeaderEditor		ui;

	QString					m_Path;
	bool					m_bFileMode = false;	// a .pvh rather than a container
	const struct _dcvol_api* m_pApi = nullptr;
	CImBoxSession*			m_pSession = nullptr;

	CSecurePassword			m_Password;		// the header password, as entered
	int						m_iKdf = 0;		// the one that actually opened it

	//
	// Both are secure_alloc'd by the library and freed in the destructor. The
	// derived key is what a key slot wraps, so it is needed for the whole life
	// of the dialog rather than for one operation.
	//
	unsigned char*			m_pHeaderDk = nullptr;
	struct _xts_key*		m_pHdrKey = nullptr;

	bool					m_bInitializing = false;
	bool					m_bModified = false;
	bool					m_bWritten = false;
	int						m_SelectedSlot = -1;

	// what the header said when it was read, for change detection
	quint32					m_OrigDiskId = 0;
	bool					m_bOrigNoHiber = false;
	int						m_OrigSlotCount = 0;
	bool					m_bOrigExtHdr = false;
	bool					m_bOrigV2 = true;
	QString					m_OrigNote;
};
