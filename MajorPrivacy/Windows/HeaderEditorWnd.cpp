/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

#include "pch.h"
#include "HeaderEditorWnd.h"
#include "VolumeWindow.h"
#include "../MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../../MiscHelpers/Common/Common.h"
#include "../../MiscHelpers/Common/Settings.h"
#include "../../Library/Helpers/ImDiskHelpers.h"
#include "../../Library/Common/StVariant.h"

#include <QProgressDialog>
#include <QFutureWatcher>
#include <QEventLoop>
#include <QInputDialog>
#include <QMessageBox>
#include <QFile>
#include <QtConcurrent>

// last, because it undefines the min and max macros DiskCryptor's defines.h
// hands out and everything above wants the real ones
#include "../Helpers/VolumeLib.h"


/* ---------------------------------------------------------------------------
 * Small helpers
 * ------------------------------------------------------------------------ */

//
// The extended header's payload is a variant buffer, tagged the way dcapi tags
// it. Both ends have to agree on this word or the note simply is not found.
//
#define EXT_HDR_COMMENT  'CMNT'

//
// DiskCryptor reads the note into a char[57] and drops it outright if it does
// not fit, so a longer one would be there in the file and invisible in
// DiskCryptor. The limit is theirs, not ours.
//
#define MAX_VOLUME_NOTE  56

static const char* g_CipherNames[] = {
	"AES", "Twofish", "Serpent", "AES-Twofish",
	"Twofish-Serpent", "Serpent-AES", "AES-Twofish-Serpent"
};

static QString CipherName(int alg)
{
	if (alg >= 0 && alg < (int)(sizeof(g_CipherNames) / sizeof(g_CipherNames[0])))
		return QString::fromLatin1(g_CipherNames[alg]);
	return QString::number(alg);
}

static QString HeaderSizeName(quint32 uSize)
{
	if (uSize == DC_AREA_SIZE)		return CHeaderEditorWnd::tr("2 KiB (legacy)");
	if (uSize == 64 * 1024)			return CHeaderEditorWnd::tr("64 KiB (recommended)");
	if (uSize < 1024 * 1024)		return CHeaderEditorWnd::tr("%1 KiB").arg(uSize / 1024);
	return CHeaderEditorWnd::tr("%1 MiB").arg(uSize / (1024 * 1024));
}

//
// A dc_pass in library-owned secure memory. The password is never assembled
// anywhere else - not in a QString, not in a heap buffer - and the caller frees
// this with secure_free, which wipes it.
//
static dc_pass* MakeDcPass(const dcvol_api* api, const CSecurePassword& Password, int iKdf, int iSlot)
{
	dc_pass* p = (dc_pass*)api->secure_alloc(sizeof(dc_pass));
	if (!p)
		return NULL;

	memset(p, 0, sizeof(dc_pass));

	size_t uSize = Password.GetPasswordSize();
	if (uSize > sizeof(p->pass))
		uSize = sizeof(p->pass);
	if (uSize)
		memcpy(p->pass, Password.GetPassword(), uSize);

	p->size = (int)uSize;
	p->kdf = iKdf;
	p->slot = iSlot;
	return p;
}

//
// Run something slow off the UI thread behind a modal, uncancellable progress
// box. Argon2 at the recommended cost takes seconds and half a gigabyte; doing
// it inline would look exactly like a hang.
//
template <class F>
static bool RunBusy(QWidget* parent, const QString& Text, F job)
{
	QProgressDialog dlg(Text, QString(), 0, 0, parent);
	dlg.setWindowTitle("MajorPrivacy");
	dlg.setWindowModality(Qt::WindowModal);
	dlg.setCancelButton(nullptr);
	dlg.setMinimumDuration(0);
	dlg.setAutoClose(false);
	dlg.setAutoReset(false);

	QFutureWatcher<bool> watcher;
	QEventLoop loop;
	QObject::connect(&watcher, &QFutureWatcher<bool>::finished, &loop, &QEventLoop::quit);
	watcher.setFuture(QtConcurrent::run(job));

	dlg.show();
	loop.exec();
	dlg.close();

	return watcher.result();
}


/* ---------------------------------------------------------------------------
 * CHeaderEditorWnd
 * ------------------------------------------------------------------------ */

bool CHeaderEditorWnd::Edit(const QString& VolumePath, QWidget* parent)
{
	if (!CDcVolume::IsAvailable()) {
		QMessageBox::critical(parent, "MajorPrivacy",
			tr("The header editor needs the volume library, which is not available.\n\n%1")
			.arg(CDcVolume::Error()));
		return false;
	}

	CHeaderEditorWnd Wnd(VolumePath, false, parent);
	if (!Wnd.Load())
		return false;

	Wnd.exec();
	return Wnd.m_bWritten;
}

bool CHeaderEditorWnd::EditFile(const QString& FilePath, QWidget* parent)
{
	if (!CDcVolume::IsAvailable()) {
		QMessageBox::critical(parent, "MajorPrivacy",
			tr("The header editor needs the volume library, which is not available.\n\n%1")
			.arg(CDcVolume::Error()));
		return false;
	}

	CHeaderEditorWnd Wnd(FilePath, true, parent);
	if (!Wnd.Load())
		return false;

	Wnd.exec();
	return Wnd.m_bWritten;
}

CHeaderEditorWnd::CHeaderEditorWnd(const QString& VolumePath, bool bFileMode, QWidget* parent)
	: QDialog(parent), m_Path(VolumePath), m_bFileMode(bFileMode)
{
	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui.setupUi(this);
	setWindowTitle(m_bFileMode
		? tr("MajorPrivacy - Header Backup Editor")
		: tr("MajorPrivacy - Volume Header Editor"));

	m_pApi = CDcVolume::Api();

	ui.treeSlots->setHeaderLabels(QStringList()
		<< tr("#") << tr("Name") << tr("Status") << tr("Key Derivation"));
	ui.treeSlots->setColumnWidth(0, 32);
	ui.treeSlots->setColumnWidth(1, 150);
	ui.treeSlots->setColumnWidth(2, 90);

	connect(ui.treeSlots, &QTreeWidget::itemSelectionChanged, this, &CHeaderEditorWnd::OnSlotSelected);
	connect(ui.treeSlots, &QTreeWidget::itemDoubleClicked, this, &CHeaderEditorWnd::OnSetSlotPassword);

	connect(ui.btnSetPass, &QPushButton::clicked, this, &CHeaderEditorWnd::OnSetSlotPassword);
	connect(ui.btnTestPass, &QPushButton::clicked, this, &CHeaderEditorWnd::OnTestSlotPassword);
	connect(ui.btnEditSlot, &QPushButton::clicked, this, &CHeaderEditorWnd::OnRenameSlot);
	connect(ui.btnToggleSlot, &QPushButton::clicked, this, &CHeaderEditorWnd::OnToggleSlot);
	connect(ui.btnClearSlot, &QPushButton::clicked, this, &CHeaderEditorWnd::OnClearSlot);

	connect(ui.spinSlots, QOverload<int>::of(&QSpinBox::valueChanged), this, &CHeaderEditorWnd::OnFieldChanged);
	connect(ui.chkExtHeader, &QCheckBox::clicked, this, &CHeaderEditorWnd::OnFieldChanged);
	connect(ui.chkNoHiber, &QCheckBox::clicked, this, &CHeaderEditorWnd::OnFieldChanged);
	connect(ui.radioV1, &QRadioButton::clicked, this, &CHeaderEditorWnd::OnFieldChanged);
	connect(ui.radioV2, &QRadioButton::clicked, this, &CHeaderEditorWnd::OnFieldChanged);
	connect(ui.txtNote, &QLineEdit::textEdited, this, &CHeaderEditorWnd::OnFieldChanged);
	connect(ui.txtDiskId, &QLineEdit::textEdited, this, &CHeaderEditorWnd::OnFieldChanged);

	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &CHeaderEditorWnd::OnClicked);

	restoreGeometry(theConf->GetBlob("HeaderEditorWnd/Window_Geometry"));
}

CHeaderEditorWnd::~CHeaderEditorWnd()
{
	theConf->SetBlob("HeaderEditorWnd/Window_Geometry", saveGeometry());

	//
	// The derived key and the key schedule are the two pieces of this dialog
	// that would let someone read the volume. secure_free wipes both; the
	// header itself goes with the session, which wipes its own section.
	//
	if (m_pApi) {
		if (m_pHeaderDk) m_pApi->secure_free(m_pHeaderDk);
		if (m_pHdrKey) m_pApi->secure_free(m_pHdrKey);
	}
	delete m_pSession;
}

dc_header* CHeaderEditorWnd::Header() const
{
	if (!m_pSession)
		return NULL;
	SHeaderData* pData = m_pSession->Header();
	if (!pData || !pData->size)
		return NULL;
	return (dc_header*)pData->data;
}

int CHeaderEditorWnd::HeadLen() const
{
	if (!m_pSession)
		return 0;
	SHeaderData* pData = m_pSession->Header();
	return pData ? (int)pData->head_len : 0;
}

/* ------------------------------------------------------------------ loading */

bool CHeaderEditorWnd::Load()
{
	CVolumeWindow PwWnd(tr("Enter the volume password to open the header editor."), CVolumeWindow::eGetPW, this);
	if (PwWnd.exec() != QDialog::Accepted)
		return false;

	m_Password = PwWnd.GetPassword();
	m_iKdf = PwWnd.GetKdf();

	if (!ReadHeader())
		return false;

	if (!DeriveHeaderKey())
		return false;

	m_bInitializing = true;
	PopulateSlots();
	UpdateProps();
	m_bInitializing = false;
	UpdateButtons();
	return true;
}

bool CHeaderEditorWnd::ReadHeader()
{
	return m_bFileMode ? ReadHeaderFromFile() : ReadHeaderFromVolume();
}

//
// A header backup is a sealed header in a file and nothing more, so the library
// opens it directly - no container to find, no ImBox to launch. The section is
// still used to hold the result, because it is page-locked memory and what
// comes out of the decrypt is the volume's master key.
//
bool CHeaderEditorWnd::ReadHeaderFromFile()
{
	QFile File(m_Path);
	if (!File.open(QIODevice::ReadOnly)) {
		QMessageBox::critical(this, "MajorPrivacy", tr("Could not open \"%1\".").arg(m_Path));
		return false;
	}

	qint64 uFileLen = File.size();
	if (uFileLen < DC_AREA_SIZE || uFileLen > DC_AREA_MAX_SIZE_UI || (uFileLen % XTS_SECTOR_SIZE) != 0) {
		QMessageBox::critical(this, "MajorPrivacy", tr("\"%1\" is not a volume header backup.").arg(m_Path));
		return false;
	}

	unsigned char* pBlob = (unsigned char*)m_pApi->secure_alloc((unsigned long)uFileLen);
	if (!pBlob) {
		QMessageBox::critical(this, "MajorPrivacy", tr("Could not allocate secure memory."));
		return false;
	}

	bool bRead = File.read((char*)pBlob, uFileLen) == uFileLen;
	File.close();

	if (!bRead) {
		m_pApi->secure_free(pBlob);
		QMessageBox::critical(this, "MajorPrivacy", tr("Could not read \"%1\".").arg(m_Path));
		return false;
	}

	const dcvol_api* api = m_pApi;
	CSecurePassword Password = m_Password;
	int iKdf = m_iKdf;
	dc_header* pHeader = NULL;
	xts_key* pKey = NULL;
	int iFoundKdf = -1;
	int iSlot = 0;
	int iLen = (int)uFileLen;

	bool bOk = RunBusy(this, tr("Opening the header backup..."),
		[api, Password, iKdf, pBlob, iLen, &pHeader, &pKey, &iFoundKdf, &iSlot]() {
			dc_pass* p = MakeDcPass(api, Password, iKdf, SPASS_SLOT_ANY);
			if (!p) return false;
			int r = api->dc_decrypt_header(pBlob, iLen, p, &pHeader, &pKey, NULL, &iFoundKdf, &iSlot);
			api->secure_free(p);
			return r == ST_OK;
		});

	m_pApi->secure_free(pBlob);

	if (!bOk) {
		if (pHeader) m_pApi->secure_free(pHeader);
		if (pKey) m_pApi->secure_free(pKey);
		QMessageBox::critical(this, "MajorPrivacy", tr("Could not open the header backup. The password may be wrong."));
		return false;
	}

	// the key comes back too, but it is re-derived in DeriveHeaderKey along
	// with the material a key slot needs, so it is not kept here
	if (pKey) m_pApi->secure_free(pKey);

	int iHeadLen = (pHeader->version >= DC_HDR_VERSION_2) ? (int)pHeader->head_len : DC_AREA_SIZE;
	if (iHeadLen < DC_AREA_SIZE || iHeadLen > iLen || (iHeadLen % XTS_SECTOR_SIZE) != 0) {
		m_pApi->secure_free(pHeader);
		QMessageBox::critical(this, "MajorPrivacy", tr("The header backup is truncated or malformed."));
		return false;
	}

	if (iSlot != 0) {
		m_pApi->secure_free(pHeader);
		QMessageBox::critical(this, "MajorPrivacy",
			tr("This header backup was opened with a key slot password.\n\n"
			   "Editing it requires the header's own password - the one that is not in any key slot."));
		return false;
	}

	delete m_pSession;
	m_pSession = new CImBoxSession();
	if (!NT_SUCCESS(m_pSession->Create((ULONG)iHeadLen))) {
		m_pApi->secure_free(pHeader);
		QMessageBox::critical(this, "MajorPrivacy", tr("Could not allocate secure memory for the volume header."));
		return false;
	}

	SHeaderData* pData = m_pSession->Header();
	memcpy(pData->data, pHeader, iHeadLen);
	pData->size = (ULONG)iHeadLen;
	pData->head_len = (ULONG)iHeadLen;
	pData->flags = 0;

	SCryptInfo* pInfo = &m_pSession->Section()->out.info;
	pInfo->cipher_id = (unsigned char)pHeader->alg_1;
	pInfo->version = pHeader->version;
	pInfo->head_len = iHeadLen;
	pInfo->head_kdf = iFoundKdf;
	pInfo->disk_id = pHeader->disk_id;
	pInfo->used_slot = iSlot;
	pInfo->slot_count = m_pApi->cp_has_key_slots(pHeader) ? pHeader->key_slot_count : 0;

	m_pApi->secure_free(pHeader);

	m_iKdf = iFoundKdf;
	return true;
}

bool CHeaderEditorWnd::ReadHeaderFromVolume()
{
	//
	// The section has to be sized before the header is open, and head_len is
	// inside the header. So it is sized for what ImBox itself creates, and if
	// the container turns out to have a longer one, ImBox says so instead of
	// truncating and the read is repeated at the right size. In practice the
	// second pass never runs.
	//
	quint32 uHdrLen = 64 * 1024;

	for (int pass = 0; pass < 2; pass++)
	{
		delete m_pSession;
		m_pSession = new CImBoxSession();

		if (!NT_SUCCESS(m_pSession->Create(uHdrLen))) {
			QMessageBox::critical(this, "MajorPrivacy", tr("Could not allocate secure memory for the volume header."));
			return false;
		}

		m_pSession->SetPassword(m_Password.GetPassword(), m_Password.GetPasswordSize(), m_iKdf, SPASS_SLOT_ANY);

		std::wstring Path = m_Path.toStdWString();
		CImBoxSession* pSession = m_pSession;
		bool bOk = RunBusy(this, tr("Opening the volume header..."), [pSession, Path]() {
			return (bool)NT_SUCCESS(pSession->Run(Path, L"get_header"));
		});

		SHeaderData* pData = m_pSession->Header();

		if (bOk && pData && pData->size)
			break;

		//
		// A header longer than the section was still measured - ImBox fills in
		// head_len before it refuses - so one retry is enough.
		//
		if (pass == 0 && pData && pData->head_len > uHdrLen && pData->head_len <= DC_AREA_MAX_SIZE_UI) {
			uHdrLen = pData->head_len;
			continue;
		}

		QMessageBox::critical(this, "MajorPrivacy",
			tr("Could not read the volume header. The password may be wrong, or the volume may be in use."));
		return false;
	}

	//
	// A key slot password unwraps the header key but is not the header
	// password, and every slot in the header is wrapped under the latter. An
	// editor that cannot derive it cannot change a single slot, so this is
	// settled before the dialog opens rather than at the first failed apply.
	//
	if (m_pSession->Section()->out.info.used_slot != 0) {
		QMessageBox::critical(this, "MajorPrivacy",
			tr("This volume was opened with a key slot password.\n\n"
			   "Editing the header requires the volume's own password - the one that is not in any key slot."));
		return false;
	}

	// the KDF the search actually landed on; everything derived from here uses it
	m_iKdf = m_pSession->Section()->out.info.head_kdf;
	return true;
}

bool CHeaderEditorWnd::DeriveHeaderKey()
{
	dc_header* pHeader = Header();
	if (!pHeader)
		return false;

	m_pHeaderDk = (unsigned char*)m_pApi->secure_alloc(PKCS_DERIVE_MAX);
	m_pHdrKey = (_xts_key*)m_pApi->secure_alloc(sizeof(xts_key));
	if (!m_pHeaderDk || !m_pHdrKey) {
		QMessageBox::critical(this, "MajorPrivacy", tr("Could not allocate secure memory."));
		return false;
	}

	//
	// The same key the volume is already sealed with, derived here rather than
	// carried over from ImBox: it is what a key slot wraps, and what the edited
	// header will be sealed under. DiskCryptor's editor caches it the same way,
	// for the same reason - it would otherwise be recomputed per slot.
	//
	const dcvol_api* api = m_pApi;
	CSecurePassword Password = m_Password;
	int iKdf = m_iKdf;
	unsigned char* pDk = m_pHeaderDk;
	_xts_key* pKey = m_pHdrKey;
	int alg = pHeader->alg_1;
	unsigned char Salt[HEADER_SALT_SIZE];
	memcpy(Salt, pHeader->salt, HEADER_SALT_SIZE);

	bool bOk = RunBusy(this, tr("Deriving the header key..."), [api, Password, iKdf, pDk, pKey, alg, &Salt]() {
		dc_pass* p = MakeDcPass(api, Password, iKdf, 0);
		if (!p) return false;
		int ok = api->dc_derive_key(p, p->kdf, Salt, pDk, NULL);
		api->secure_free(p);
		if (!ok) return false;
		return api->xts_set_key(pDk, alg, pKey) != 0;
	});

	SecureZeroMemory(Salt, sizeof(Salt));

	if (!bOk) {
		QMessageBox::critical(this, "MajorPrivacy", tr("Could not derive the header key."));
		return false;
	}
	return true;
}

/* ------------------------------------------------------------------- display */

void CHeaderEditorWnd::PopulateSlots()
{
	int iPrevious = m_SelectedSlot;

	ui.treeSlots->clear();

	dc_header* pHeader = Header();
	if (!pHeader || !m_pApi->cp_has_key_slots(pHeader)) {
		ui.tabs->setTabEnabled(0, false);
		m_SelectedSlot = -1;
		return;
	}
	ui.tabs->setTabEnabled(0, true);

	for (int i = 0; i < pHeader->key_slot_count; i++)
	{
		QTreeWidgetItem* pItem = new QTreeWidgetItem(ui.treeSlots);
		pItem->setText(0, QString::number(i + 1));

		dc_slot_info Slot;
		if (m_pApi->cp_get_slot_info(pHeader, i, &Slot) != ST_OK) {
			pItem->setText(2, tr("Invalid"));
			continue;
		}

		// an unused slot is full of random bytes, so its name is only read for
		// one that claims to be in use
		if ((Slot.flags & (SF_ACTIVE | SF_CORRUPT)) && Slot.slot_name[0])
			pItem->setText(1, QString::fromUtf8(Slot.slot_name, (int)strnlen(Slot.slot_name, SLOT_LABEL_LEN)));

		if (!(Slot.flags & SF_ACTIVE))		pItem->setText(2, tr("Empty"));
		else if (Slot.flags & SF_CORRUPT)	pItem->setText(2, tr("Corrupt"));
		else if (Slot.flags & SF_DISABLED)	pItem->setText(2, tr("Disabled"));
		else								pItem->setText(2, tr("Active"));

		if (Slot.flags & SF_ACTIVE)
			pItem->setText(3, CVolumeWindow::GetKdfName(Slot.data_0.slot_kdf));
	}

	if (iPrevious >= 0 && iPrevious < pHeader->key_slot_count) {
		ui.treeSlots->setCurrentItem(ui.treeSlots->topLevelItem(iPrevious));
		m_SelectedSlot = iPrevious;
	} else
		m_SelectedSlot = -1;
}

void CHeaderEditorWnd::UpdateProps()
{
	dc_header* pHeader = Header();
	if (!pHeader)
		return;

	bool bV2 = pHeader->version >= DC_HDR_VERSION_2;
	bool bSlots = bV2 && (pHeader->feature_flags & FF_KEY_SLOTS) != 0;
	quint32 uHeadLen = (quint32)HeadLen();

	ui.lblSummary->setText(tr("<b>%1</b> &mdash; %2 header, %3, %4 key slots")
		.arg(QFileInfo(m_Path).fileName())
		.arg(bV2 ? tr("Version 2") : tr("Version 1"))
		.arg(CipherName(pHeader->alg_1))
		.arg(bSlots ? pHeader->key_slot_count : 0));

	ui.radioV1->setChecked(!bV2);
	ui.radioV2->setChecked(bV2);

	//
	// A version 1 header has no head_len field: the volume data is fixed at
	// DC_AREA_SIZE. Downgrading a container whose data starts further in would
	// leave every byte of it at the wrong offset, so the choice is only offered
	// where the two already agree - the same test DiskCryptor's editor uses,
	// and one that a 64 KiB MajorPrivacy container never passes.
	//
	bool bCanChangeVersion = !bV2 || uHeadLen == DC_AREA_SIZE;
	ui.radioV1->setEnabled(bCanChangeVersion);
	ui.radioV2->setEnabled(bCanChangeVersion);
	if (!bCanChangeVersion)
		ui.radioV1->setToolTip(tr("Only a 2 KiB header can be downgraded; this one is %1.").arg(HeaderSizeName(uHeadLen)));

	ui.lblHeadSize->setText(HeaderSizeName(uHeadLen));
	ui.lblCipher->setText(CipherName(pHeader->alg_1));

	ui.spinSlots->setValue(bSlots ? pHeader->key_slot_count : 0);
	ui.spinSlots->setEnabled(bV2);

	bool bHasExt = false;
	QString Note;
	if (bV2 && pHeader->ext_hdr_off > 0) {
		dc_ext_header* pExt = NULL;
		if (m_pApi->get_ext_header(pHeader, &pExt) == ST_OK && pExt) {
			//
			// get_ext_header has already checked the frame and its CRC; what is
			// inside is a variant buffer, and StVariant is the same encoding
			// dcapi writes from the C side. Read in place - the block lives in
			// the locked section and there is no reason to copy it out.
			//
			CBuffer Packet(pExt->data, pExt->size - MIN_EXT_HDR_SIZE, true);
			StVariant Data;
			if (Data.FromPacket(&Packet) == StVariant::eErrNone) {
				StVariant Comment = Data.Get(EXT_HDR_COMMENT);
				if (Comment.GetSize() > 0)
					Note = QString::fromUtf8((const char*)Comment.GetData(), (int)Comment.GetSize());
				bHasExt = true;
			}
		}
	}
	ui.chkExtHeader->setChecked(bHasExt);
	ui.chkExtHeader->setEnabled(bV2);
	ui.txtNote->setText(Note);
	ui.txtNote->setEnabled(bV2 && bHasExt);

	ui.txtDiskId->setText(QString("%1").arg(pHeader->disk_id, 8, 16, QChar('0')).toUpper());
	ui.chkNoHiber->setChecked((pHeader->flags & VF_NO_HIBER) != 0);

	m_OrigDiskId = pHeader->disk_id;
	m_bOrigNoHiber = (pHeader->flags & VF_NO_HIBER) != 0;
	m_OrigSlotCount = bSlots ? pHeader->key_slot_count : 0;
	m_bOrigExtHdr = bHasExt;
	m_bOrigV2 = bV2;
	m_OrigNote = Note;
}

void CHeaderEditorWnd::UpdateButtons()
{
	dc_header* pHeader = Header();
	bool bSlots = pHeader && m_pApi->cp_has_key_slots(pHeader) != 0;
	bool bSelected = bSlots && m_SelectedSlot >= 0 && ui.tabs->currentIndex() == 0;

	ui.btnSetPass->setEnabled(bSelected);
	ui.btnEditSlot->setEnabled(bSelected);
	ui.btnClearSlot->setEnabled(bSelected);

	bool bActive = false;
	bool bDisabled = false;
	if (bSelected) {
		dc_slot_info Slot;
		if (m_pApi->cp_get_slot_info(pHeader, m_SelectedSlot, &Slot) == ST_OK) {
			bActive = (Slot.flags & SF_ACTIVE) != 0;
			bDisabled = (Slot.flags & SF_DISABLED) != 0;
		}
	}
	ui.btnTestPass->setEnabled(bSelected && bActive);
	ui.btnToggleSlot->setEnabled(bSelected && bActive);
	ui.btnToggleSlot->setText(bDisabled ? tr("&Enable Slot") : tr("&Disable Slot"));

	ui.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(m_bModified);
}

void CHeaderEditorWnd::SetModified()
{
	if (m_bInitializing)
		return;
	m_bModified = true;
	ui.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
}

void CHeaderEditorWnd::OnSlotSelected()
{
	QTreeWidgetItem* pItem = ui.treeSlots->currentItem();
	m_SelectedSlot = pItem ? ui.treeSlots->indexOfTopLevelItem(pItem) : -1;
	UpdateButtons();
}

void CHeaderEditorWnd::OnFieldChanged()
{
	// the note only means anything when there is an extended header to put it in
	ui.txtNote->setEnabled(ui.radioV2->isChecked() && ui.chkExtHeader->isChecked());
	SetModified();
}

/* --------------------------------------------------------------- slot edits */

bool CHeaderEditorWnd::GetSlotPassword(const QString& Prompt, bool bNew, CSecurePassword& Password, int& iKdf)
{
	CVolumeWindow Wnd(Prompt, bNew ? CVolumeWindow::eSetPW : CVolumeWindow::eGetPW, this);
	if (!bNew)
		Wnd.SetNoAutoKdf(); // a slot names its own KDF; there is nothing to search
	if (Wnd.exec() != QDialog::Accepted)
		return false;

	Password = bNew ? Wnd.GetNewPassword() : Wnd.GetPassword();
	iKdf = bNew ? Wnd.GetNewKdf() : Wnd.GetKdf();
	return !Password.IsEmpty();
}

bool CHeaderEditorWnd::DeriveKey(const CSecurePassword& Password, int iKdf, int iSlot, unsigned char* pDk)
{
	dc_header* pHeader = Header();
	if (!pHeader)
		return false;

	const dcvol_api* api = m_pApi;
	unsigned char Salt[HEADER_SALT_SIZE];
	memcpy(Salt, pHeader->salt, HEADER_SALT_SIZE);

	bool bOk = RunBusy(this, tr("Deriving the key..."), [api, Password, iKdf, iSlot, pDk, &Salt]() {
		dc_pass* p = MakeDcPass(api, Password, iKdf, iSlot);
		if (!p) return false;
		int ok = api->dc_derive_key(p, p->kdf, Salt, pDk, NULL);
		api->secure_free(p);
		return ok != 0;
	});

	SecureZeroMemory(Salt, sizeof(Salt));
	return bOk;
}

void CHeaderEditorWnd::OnSetSlotPassword()
{
	dc_header* pHeader = Header();
	if (!pHeader || m_SelectedSlot < 0)
		return;

	CSecurePassword Password;
	int iKdf = KDF_ARGON_DEFAULT;
	if (!GetSlotPassword(tr("Enter the password for key slot %1.").arg(m_SelectedSlot + 1), true, Password, iKdf))
		return;

	unsigned char* pSlotDk = (unsigned char*)m_pApi->secure_alloc(PKCS_DERIVE_MAX);
	unsigned char* pPayload = (unsigned char*)m_pApi->secure_alloc(PKCS_DERIVE_MAX);
	if (!pSlotDk || !pPayload) {
		if (pSlotDk) m_pApi->secure_free(pSlotDk);
		if (pPayload) m_pApi->secure_free(pPayload);
		return;
	}

	bool bOk = DeriveKey(Password, iKdf, 0, pSlotDk);

	if (bOk)
	{
		dc_slot_info Slot;
		if (m_pApi->cp_get_slot_info(pHeader, m_SelectedSlot, &Slot) == ST_OK)
		{
			Slot.flags |= SF_ACTIVE;
			Slot.flags &= ~SF_CORRUPT;
			Slot.type = DC_SLOT_TYPE_0;
			Slot.data_0.slot_kdf = (unsigned char)iKdf;

			//
			// The slot holds the header key XOR the slot password's derived
			// key. That is what lets the slot password recover the header key
			// later, and it is why setting a slot needs the header password to
			// have been given first.
			//
			if (m_pApi->cp_wrap_header_key(pPayload, pSlotDk, m_pHeaderDk, Slot.type) &&
				m_pApi->cp_set_slot(pHeader, m_SelectedSlot, &Slot, pPayload, PKCS_DERIVE_MAX) == ST_OK)
			{
				PopulateSlots();
				UpdateButtons();
				SetModified();
			}
			else
				bOk = false;
		}
		else
			bOk = false;
	}

	m_pApi->secure_free(pSlotDk);
	m_pApi->secure_free(pPayload);

	if (!bOk)
		QMessageBox::critical(this, "MajorPrivacy", tr("Failed to set the key slot password."));
}

void CHeaderEditorWnd::OnTestSlotPassword()
{
	dc_header* pHeader = Header();
	if (!pHeader || m_SelectedSlot < 0)
		return;

	CSecurePassword Password;
	int iKdf = 0;
	if (!GetSlotPassword(tr("Enter the password to test against key slot %1.").arg(m_SelectedSlot + 1), false, Password, iKdf))
		return;

	unsigned char* pTestDk = (unsigned char*)m_pApi->secure_alloc(PKCS_DERIVE_MAX);
	unsigned char* pPayload = (unsigned char*)m_pApi->secure_alloc(PKCS_DERIVE_MAX);
	unsigned char* pRecovered = (unsigned char*)m_pApi->secure_alloc(PKCS_DERIVE_MAX);
	if (!pTestDk || !pPayload || !pRecovered) {
		if (pTestDk) m_pApi->secure_free(pTestDk);
		if (pPayload) m_pApi->secure_free(pPayload);
		if (pRecovered) m_pApi->secure_free(pRecovered);
		return;
	}

	bool bMatch = false;
	if (DeriveKey(Password, iKdf, 0, pTestDk) &&
		m_pApi->cp_get_slot_payload(pHeader, m_SelectedSlot, pPayload, PKCS_DERIVE_MAX) == ST_OK)
	{
		//
		// Unwrapping is the same XOR as wrapping, so the wrap call serves for
		// both. The password is right when what comes out is the header key we
		// already hold.
		//
		m_pApi->cp_wrap_header_key(pRecovered, pPayload, pTestDk, DC_SLOT_TYPE_0);
		bMatch = memcmp(pRecovered, m_pHeaderDk, PKCS_DERIVE_MAX) == 0;
	}

	m_pApi->secure_free(pTestDk);
	m_pApi->secure_free(pPayload);
	m_pApi->secure_free(pRecovered);

	if (bMatch)
		QMessageBox::information(this, "MajorPrivacy", tr("The password opens key slot %1.").arg(m_SelectedSlot + 1));
	else
		QMessageBox::warning(this, "MajorPrivacy", tr("The password does not open key slot %1.").arg(m_SelectedSlot + 1));
}

void CHeaderEditorWnd::OnRenameSlot()
{
	dc_header* pHeader = Header();
	if (!pHeader || m_SelectedSlot < 0)
		return;

	dc_slot_info Slot;
	if (m_pApi->cp_get_slot_info(pHeader, m_SelectedSlot, &Slot) != ST_OK)
		return;

	QString Current;
	if ((Slot.flags & (SF_ACTIVE | SF_CORRUPT)) && Slot.slot_name[0])
		Current = QString::fromUtf8(Slot.slot_name, (int)strnlen(Slot.slot_name, SLOT_LABEL_LEN));

	bool bOk = false;
	QString Name = QInputDialog::getText(this, tr("MajorPrivacy - Key Slot %1").arg(m_SelectedSlot + 1),
		tr("Slot name:"), QLineEdit::Normal, Current, &bOk);
	if (!bOk)
		return;

	QByteArray Utf8 = Name.toUtf8();
	if (Utf8.size() > SLOT_LABEL_LEN) {
		QMessageBox::warning(this, "MajorPrivacy", tr("The slot name must fit into %1 bytes of UTF-8.").arg(SLOT_LABEL_LEN));
		return;
	}

	// zero padded rather than terminated - the field has no room for a NUL
	memset(Slot.slot_name, 0, sizeof(Slot.slot_name));
	memcpy(Slot.slot_name, Utf8.constData(), Utf8.size());

	if (m_pApi->cp_set_slot(pHeader, m_SelectedSlot, &Slot, NULL, 0) == ST_OK) {
		PopulateSlots();
		SetModified();
	}
}

void CHeaderEditorWnd::OnToggleSlot()
{
	dc_header* pHeader = Header();
	if (!pHeader || m_SelectedSlot < 0)
		return;

	dc_slot_info Slot;
	if (m_pApi->cp_get_slot_info(pHeader, m_SelectedSlot, &Slot) != ST_OK)
		return;

	if (Slot.flags & SF_DISABLED)
		Slot.flags &= ~SF_DISABLED;
	else
		Slot.flags |= SF_DISABLED;

	if (m_pApi->cp_set_slot(pHeader, m_SelectedSlot, &Slot, NULL, 0) == ST_OK) {
		PopulateSlots();
		UpdateButtons();
		SetModified();
	}
}

void CHeaderEditorWnd::OnClearSlot()
{
	dc_header* pHeader = Header();
	if (!pHeader || m_SelectedSlot < 0)
		return;

	if (QMessageBox::question(this, "MajorPrivacy",
		tr("Clear key slot %1?\n\nThe password stored in it will no longer open this volume.").arg(m_SelectedSlot + 1),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		return;

	//
	// CP_SLOT_CLEAR zeroes the descriptor and CP_SLOT_RANDOM fills the slot
	// with random bytes rather than zeroes: a cleared slot has to be
	// indistinguishable from one that was never used.
	//
	if (m_pApi->cp_set_slot(pHeader, m_SelectedSlot, CP_SLOT_CLEAR, CP_SLOT_RANDOM, 0) == ST_OK) {
		PopulateSlots();
		UpdateButtons();
		SetModified();
	}
}

/* ------------------------------------------------------------------ applying */

bool CHeaderEditorWnd::Apply()
{
	dc_header* pHeader = Header();
	int iHeadLen = HeadLen();
	if (!pHeader || !iHeadLen)
		return false;

	//
	// The layout arithmetic, following DiskCryptor's _apply_header_changes.
	// The header length is fixed - everything has to fit in the space that is
	// already there.
	//
	bool bOldV2 = pHeader->version >= DC_HDR_VERSION_2;
	int iOldSlotCount = (bOldV2 && (pHeader->feature_flags & FF_KEY_SLOTS)) ? pHeader->key_slot_count : 0;
	int iOldSlotArea = iOldSlotCount ? pHeader->slot_area_len : 0;
	int iOldInfoSize = iOldSlotCount ? pHeader->slot_info_size : 0;

	int iOldExtOff = 0, iOldExtSize = 0;	// read back only to describe the old layout
	if (bOldV2 && pHeader->ext_hdr_off > 0) {
		dc_ext_header* pExt = NULL;
		if (m_pApi->get_ext_header(pHeader, &pExt) == ST_OK && pExt) {
			iOldExtOff = pHeader->ext_hdr_off;
			iOldExtSize = pExt->size;
		}
	}

	bool bNewV2 = ui.radioV2->isChecked();
	int iNewSlotCount = bNewV2 ? ui.spinSlots->value() : 0;
	int iNewInfoSize = iNewSlotCount ? (iOldInfoSize ? iOldInfoSize : (int)sizeof(dc_slot_info)) : 0;
	int iSlotSize = m_pApi->cp_get_key_slot_size(DC_SLOT_TYPE_0);
	int iNewSlotArea = iNewSlotCount * iSlotSize;

	// shrinking must not drop a slot somebody is using
	for (int i = iNewSlotCount; i < iOldSlotCount; i++) {
		dc_slot_info Slot;
		if (m_pApi->cp_get_slot_info(pHeader, i, &Slot) == ST_OK && (Slot.flags & SF_ACTIVE)) {
			QMessageBox::critical(this, "MajorPrivacy",
				tr("Key slot %1 is in use and would be removed.\n\nClear it first, or keep at least %2 key slots.")
				.arg(i + 1).arg(iOldSlotCount));
			return false;
		}
	}

	bool bNewExt = bNewV2 && ui.chkExtHeader->isChecked();
	CBuffer ExtPacket;
	int iNewExtSize = 0;

	if (bNewExt)
	{
		QByteArray Note = ui.txtNote->text().toUtf8();
		if (Note.size() > MAX_VOLUME_NOTE) {
			QMessageBox::critical(this, "MajorPrivacy",
				tr("The volume note must fit into %1 bytes of UTF-8.").arg(MAX_VOLUME_NOTE));
			return false;
		}

		//
		// An index variant with the note under 'CMNT' - the same shape dcapi
		// writes. The two encoders choose different length-field widths for a
		// short payload, which is legal and which each of them reads back, so
		// the note survives a round trip through either program.
		//
		StVariant Data(nullptr, VAR_TYPE_INDEX);
		Data.Insert(EXT_HDR_COMMENT, StVariant(Note.constData(), Note.size()));

		if (Data.ToPacket(&ExtPacket) != StVariant::eErrNone) {
			QMessageBox::critical(this, "MajorPrivacy", tr("The extended header could not be built."));
			return false;
		}

		iNewExtSize = (int)ExtPacket.GetSize() + MIN_EXT_HDR_SIZE;

		int iSpace = iHeadLen - (DC_BASE_SIZE + iNewSlotCount * (iSlotSize + iNewInfoSize));
		if (iNewExtSize > iSpace) {
			QMessageBox::critical(this, "MajorPrivacy", tr("There is not enough room in the header for the extended header."));
			return false;
		}
	}

	int iNewExtOff = DC_BASE_SIZE + iNewSlotArea + iNewSlotCount * iNewInfoSize;
	if (iNewExtOff + iNewExtSize > iHeadLen) {
		QMessageBox::critical(this, "MajorPrivacy",
			tr("The requested layout needs more than the %1 this header has.").arg(HeaderSizeName(iHeadLen)));
		return false;
	}
	if (!bNewExt)
		iNewExtOff = 0;

	//
	// Build the replacement. Secure memory, because it is a copy of the header
	// and therefore a copy of the master key.
	//
	dc_header* pUpd = (dc_header*)m_pApi->secure_alloc(iHeadLen);
	unsigned char* pSealed = (unsigned char*)m_pApi->secure_alloc(iHeadLen);
	if (!pUpd || !pSealed) {
		if (pUpd) m_pApi->secure_free(pUpd);
		if (pSealed) m_pApi->secure_free(pSealed);
		QMessageBox::critical(this, "MajorPrivacy", tr("Could not allocate secure memory."));
		return false;
	}

	memset(pUpd, 0, iHeadLen);
	memcpy(pUpd, pHeader, DC_BASE_SIZE);

	if (bOldV2 != bNewV2) {
		if (bNewV2) {
			pUpd->version = DC_HDR_VERSION_2;
			pUpd->head_len = iHeadLen;
			pUpd->stor_len = iHeadLen;
			pUpd->head_kdf = (unsigned char)m_iKdf;
		} else {
			pUpd->version = DC_HDR_VERSION;
			pUpd->head_len = 0;
			pUpd->stor_len = 0;
		}
	}

	if (bNewV2) {
		pUpd->ext_hdr_off = (unsigned short)iNewExtOff;
		if (iNewSlotCount > 0)
			pUpd->feature_flags |= FF_KEY_SLOTS;
		else
			pUpd->feature_flags &= ~FF_KEY_SLOTS;
		pUpd->slot_area_len = (unsigned short)iNewSlotArea;
		pUpd->key_slot_count = (unsigned char)iNewSlotCount;
		pUpd->slot_info_size = (unsigned short)iNewInfoSize;
	} else {
		// a v1 header has nothing past the base
		memset(&pUpd->footer_cnt, 0, sizeof(dc_header) - offsetof(dc_header, footer_cnt));
	}

	bool bDiskIdOk = false;
	quint32 uDiskId = ui.txtDiskId->text().toUInt(&bDiskIdOk, 16);
	pUpd->disk_id = (bDiskIdOk && uDiskId) ? uDiskId : m_OrigDiskId;

	if (ui.chkNoHiber->isChecked())
		pUpd->flags |= VF_NO_HIBER;
	else
		pUpd->flags &= ~VF_NO_HIBER;

	pUpd->hdr_crc = m_pApi->calculate_header_crc(pUpd);

	//
	// Carry the key slots across. Anything the new layout adds is filled with
	// random bytes - a slot that has never been used has to look like one that
	// has.
	//
	if (iNewSlotCount > 0)
	{
		if (iOldSlotArea) {
			memcpy(((unsigned char*)pUpd) + DC_BASE_SIZE, ((unsigned char*)pHeader) + DC_BASE_SIZE,
				qMin(iOldSlotArea, iNewSlotArea));
			memcpy(((unsigned char*)pUpd) + DC_BASE_SIZE + iNewSlotArea,
				((unsigned char*)pHeader) + DC_BASE_SIZE + iOldSlotArea,
				qMin(iOldSlotCount * iOldInfoSize, iNewSlotCount * iNewInfoSize));
		}
		if (iNewSlotArea > iOldSlotArea)
			m_pApi->make_rand(((unsigned char*)pUpd) + DC_BASE_SIZE + iOldSlotArea, iNewSlotArea - iOldSlotArea);
	}

	if (bNewExt && iNewExtOff && iNewExtSize > MIN_EXT_HDR_SIZE)
	{
		//
		// Written out fresh rather than carried over, even when the note has not
		// changed. The block is a serialised buffer, so rebuilding it from the
		// field is the same thing as copying it, with one fewer case to get
		// wrong when the slot area has moved underneath it.
		//
		dc_ext_header* pExt = (dc_ext_header*)(((unsigned char*)pUpd) + iNewExtOff);
		pExt->size = iNewExtSize;
		pExt->version = DC_EXT_VERSION;
		memcpy(pExt->data, ExtPacket.GetBuffer(), iNewExtSize - MIN_EXT_HDR_SIZE);
		pExt->crc = m_pApi->cp_ext_header_crc(pExt);
	}

	//
	// Seal under the key the volume is already using. That keeps the salt, and
	// with it every key slot, and it is what proves to ImBox that this update
	// came from something holding the header key.
	//
	bool bOk = m_pApi->cp_encrypt_header(pUpd, iHeadLen, m_pHdrKey, pSealed) == ST_OK;

	m_pApi->secure_free(pUpd);

	if (bOk)
		bOk = SaveHeader(pSealed, iHeadLen, HF_UPDATE_ALL);

	m_pApi->secure_free(pSealed);

	if (!bOk)
		return false;

	//
	// Read it back rather than assuming. A round trip through ImBox is the only
	// thing that says the volume really is in the state the dialog shows.
	//
	m_bWritten = true;
	if (!ReadHeader())
		return false;

	m_bInitializing = true;
	PopulateSlots();
	UpdateProps();
	m_bInitializing = false;
	m_bModified = false;
	UpdateButtons();
	return true;
}

bool CHeaderEditorWnd::SaveHeaderToFile(const void* pSealed, quint32 uLen)
{
	QFile File(m_Path);
	if (!File.open(QIODevice::ReadWrite)) {
		QMessageBox::critical(this, "MajorPrivacy", tr("Could not open \"%1\" for writing.").arg(m_Path));
		return false;
	}

	bool bOk = File.write((const char*)pSealed, uLen) == (qint64)uLen;
	// a backup file is exactly one header long; anything past it was never part
	// of the header and has no business surviving the edit
	if (bOk) bOk = File.resize(uLen);
	File.close();

	if (!bOk) {
		QMessageBox::critical(this, "MajorPrivacy", tr("Could not write \"%1\".").arg(m_Path));
		return false;
	}
	return true;
}

bool CHeaderEditorWnd::SaveHeader(const void* pSealed, quint32 uLen, quint32 uFlags)
{
	if (m_bFileMode)
		return SaveHeaderToFile(pSealed, uLen);

	SHeaderData* pData = m_pSession->Header();
	memcpy(pData->data, pSealed, uLen);
	pData->size = uLen;
	pData->flags = uFlags;

	//
	// The derived key stands in for the password, so ImBox does not pay for
	// Argon2 a second time. KDF_NONE with a full-length dc_pass means "this is
	// already the derived key" - DiskCryptor's GUI does the same when it hands
	// an update to its driver.
	//
	m_pSession->SetRawKey(m_pHeaderDk, PKCS_DERIVE_MAX);

	std::wstring Path = m_Path.toStdWString();
	CImBoxSession* pSession = m_pSession;
	bool bOk = RunBusy(this, tr("Writing the volume header..."), [pSession, Path]() {
		return (bool)NT_SUCCESS(pSession->Run(Path, L"set_header"));
	});

	if (!bOk) {
		QMessageBox::critical(this, "MajorPrivacy",
			tr("Failed to write the volume header.\n\nThe volume has not been changed."));
		return false;
	}
	return true;
}

void CHeaderEditorWnd::OnClicked(QAbstractButton* pButton)
{
	switch (ui.buttonBox->standardButton(pButton))
	{
	case QDialogButtonBox::Apply:
		Apply();
		break;

	case QDialogButtonBox::Ok:
		if (m_bModified && !Apply())
			return;
		accept();
		break;

	case QDialogButtonBox::Cancel:
		if (m_bModified && QMessageBox::question(this, "MajorPrivacy",
			tr("Discard the changes to the volume header?"),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			return;
		reject();
		break;

	default:
		break;
	}
}
