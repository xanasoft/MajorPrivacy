/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

#include "pch.h"
#include "KeyFilesWnd.h"
#include "VolumeWindow.h"
#include "../Core/PrivacyCore.h"
#include "../../MiscHelpers/Common/Settings.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTreeWidgetItem>

// last: it undefines the min and max macros DiskCryptor's defines.h hands out
#include "../Helpers/VolumeLib.h"

//
// A keyfile is read whole to be hashed. Real ones are small - a photo, a key
// blob - and there is no reason to page a gigabyte through memory because
// somebody pointed the dialog at a disk image by mistake.
//
#define MAX_KEYFILE_SIZE  (16 * 1024 * 1024)


QString SKeyFile::Name() const
{
	if (!bVirtual)
		return Path;

	// the same spelling DiskCryptor uses, so a keyfile named in one program is
	// recognisable in the other
	return QString("Virtual Keyfile %1").arg(Crc, 8, 16, QChar('0')).toUpper();
}


CKeyFilesWnd::CKeyFilesWnd(const SKeyFiles& KeyFiles, QWidget* parent, bool bAllowVirtual)
	: QDialog(parent), m_KeyFiles(KeyFiles), m_bAllowVirtual(bAllowVirtual)
{
	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui.setupUi(this);
	setWindowTitle(tr("MajorPrivacy - Keyfiles"));

	ui.treeFiles->setHeaderLabels(QStringList() << tr("Keyfile") << tr("Type"));
	ui.treeFiles->setColumnWidth(0, 380);

	ui.cmbMode->addItem(tr("Canonical (recommended)"), eKeyFileCanonical);
	ui.cmbMode->addItem(tr("Additive (DiskCryptor legacy)"), eKeyFileAdditive);

	int Index = ui.cmbMode->findData(m_KeyFiles.MixMode);
	ui.cmbMode->setCurrentIndex(Index != -1 ? Index : 0);

	//
	// One level of virtual keyfiles only. This dialog is the keyfile list of a
	// virtual keyfile's own password when bAllowVirtual is false.
	//
	ui.btnAddVirtual->setEnabled(m_bAllowVirtual);
	if (!m_bAllowVirtual)
		ui.btnAddVirtual->setToolTip(tr("A virtual keyfile cannot itself contain further virtual keyfiles."));

	connect(ui.btnAddFiles, &QPushButton::clicked, this, &CKeyFilesWnd::OnAddFiles);
	connect(ui.btnAddFolder, &QPushButton::clicked, this, &CKeyFilesWnd::OnAddFolder);
	connect(ui.btnAddVirtual, &QPushButton::clicked, this, &CKeyFilesWnd::OnAddVirtual);
	connect(ui.btnRemove, &QPushButton::clicked, this, &CKeyFilesWnd::OnRemove);
	connect(ui.btnClear, &QPushButton::clicked, this, &CKeyFilesWnd::OnClear);
	connect(ui.cmbMode, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CKeyFilesWnd::OnModeChanged);
	connect(ui.treeFiles, &QTreeWidget::itemSelectionChanged, this, &CKeyFilesWnd::OnSelectionChanged);
	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	Populate();
	OnModeChanged();
	OnSelectionChanged();

	restoreGeometry(theConf->GetBlob("KeyFilesWnd/Window_Geometry"));
}

void CKeyFilesWnd::Populate()
{
	ui.treeFiles->clear();

	foreach(const SKeyFile & File, m_KeyFiles.Files)
	{
		QTreeWidgetItem* pItem = new QTreeWidgetItem(ui.treeFiles);
		pItem->setText(0, File.Name());

		// the CRC is already in a virtual keyfile's name; a column of its own
		// would just say it twice
		if (File.bVirtual)
			pItem->setText(1, tr("Virtual"));
		else
			pItem->setText(1, QFileInfo(File.Path).isDir() ? tr("Folder") : tr("File"));
	}
}

void CKeyFilesWnd::OnSelectionChanged()
{
	ui.btnRemove->setEnabled(!ui.treeFiles->selectedItems().isEmpty());
	ui.btnClear->setEnabled(ui.treeFiles->topLevelItemCount() > 0);
}

void CKeyFilesWnd::OnModeChanged()
{
	m_KeyFiles.MixMode = ui.cmbMode->currentData().toInt();

	if (m_KeyFiles.MixMode == eKeyFileAdditive)
		ui.lblModeHint->setText(tr("<i>The original DiskCryptor scheme, kept so that volumes created with it still open. "
			"Adding the same keyfile twice is not the same as adding it once, and one keyfile can cancel another.</i>"));
	else
		ui.lblModeHint->setText(tr("<i>Order-independent and duplicate-proof by construction. "
			"A single 64-byte keyfile is taken as a ready-made key set.</i>"));
}

void CKeyFilesWnd::AddPaths(const QStringList& Paths)
{
	foreach(const QString & Path, Paths)
	{
		QString Native = QDir::toNativeSeparators(Path);

		bool bDuplicate = false;
		foreach(const SKeyFile & File, m_KeyFiles.Files) {
			if (!File.bVirtual && File.Path.compare(Native, Qt::CaseInsensitive) == 0) {
				bDuplicate = true;
				break;
			}
		}
		if (bDuplicate)
			continue;

		SKeyFile File;
		File.bVirtual = false;
		File.Path = Native;
		m_KeyFiles.Files.append(File);
	}
	Populate();
	OnSelectionChanged();
}

void CKeyFilesWnd::OnAddFiles()
{
	QStringList Files = QFileDialog::getOpenFileNames(this, tr("Select Keyfiles"));
	if (!Files.isEmpty())
		AddPaths(Files);
}

void CKeyFilesWnd::OnAddFolder()
{
	QString Dir = QFileDialog::getExistingDirectory(this, tr("Select a Keyfile Folder"));
	if (!Dir.isEmpty())
		AddPaths(QStringList() << Dir);
}

void CKeyFilesWnd::OnAddVirtual()
{
	if (!m_bAllowVirtual)
		return;

	//
	// A virtual keyfile is just a second password. It gets the full password
	// dialog - strength meter, secure desktop entry, and its own keyfile button
	// - so the thing being used as a keyfile can be as well protected as the
	// volume password itself.
	//
	CVolumeWindow Wnd(tr("Enter the secret to use as a virtual keyfile.\n"
		"It is never written anywhere; you will have to type it again to open the volume."),
		CVolumeWindow::eSetPW, this);
	Wnd.SetNestedKeyFiles();

	if (Wnd.exec() != QDialog::Accepted)
		return;

	CSecurePassword Secret = Wnd.GetNewPassword();
	if (Secret.IsEmpty())
		return;

	SKeyFile File;
	File.bVirtual = true;
	File.Data = Secret;
	File.Crc = Crc32(Secret.GetPassword(), Secret.GetPasswordSize());

	// the same secret twice is the same keyfile, and in canonical mode the
	// mixer would drop the duplicate anyway
	foreach(const SKeyFile & Existing, m_KeyFiles.Files) {
		if (Existing.bVirtual && Existing.Crc == File.Crc) {
			QMessageBox::information(this, "MajorPrivacy",
				tr("That virtual keyfile is already in the list."));
			return;
		}
	}

	m_KeyFiles.Files.append(File);
	Populate();
	OnSelectionChanged();
}

void CKeyFilesWnd::OnRemove()
{
	QList<int> Rows;
	foreach(QTreeWidgetItem * pItem, ui.treeFiles->selectedItems())
		Rows.append(ui.treeFiles->indexOfTopLevelItem(pItem));

	std::sort(Rows.begin(), Rows.end(), std::greater<int>());
	foreach(int Row, Rows) {
		if (Row >= 0 && Row < m_KeyFiles.Files.count())
			m_KeyFiles.Files.removeAt(Row);
	}

	Populate();
	OnSelectionChanged();
}

void CKeyFilesWnd::OnClear()
{
	m_KeyFiles.Files.clear();
	Populate();
	OnSelectionChanged();
}

/* ---------------------------------------------------------------------------
 * Mixing
 * ------------------------------------------------------------------------ */

bool CKeyFilesWnd::IsAvailable()
{
	return CDcVolume::IsAvailable();
}

quint32 CKeyFilesWnd::Crc32(const void* pData, size_t uSize)
{
	const dcvol_api* api = CDcVolume::Api();
	if (!api || !pData || !uSize)
		return 0;
	return (quint32)api->crc32((const unsigned char*)pData, (unsigned long)uSize);
}

//
// What a list entry actually contributes: either bytes we already hold, or a
// set of files on disk. Directories expand to the files they hold, sorted by
// name and not recursive - the same shape dcapi's dc_add_keyfiles has, and a
// rule the user can keep in their head when they have to reproduce the set.
//
static QStringList ExpandFolder(const QString& Path, QString* pError)
{
	QStringList Files;

	QFileInfo Info(Path);
	if (Info.isDir()) {
		QDir Dir(Path);
		QFileInfoList Entries = Dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System, QDir::Name);
		if (Entries.isEmpty()) {
			*pError = CKeyFilesWnd::tr("The keyfile folder \"%1\" is empty.").arg(Path);
			return QStringList();
		}
		foreach(const QFileInfo & Entry, Entries)
			Files.append(Entry.absoluteFilePath());
	}
	else if (Info.isFile())
		Files.append(Info.absoluteFilePath());
	else
		*pError = CKeyFilesWnd::tr("The keyfile \"%1\" was not found.").arg(Path);

	return Files;
}

//
// Read a keyfile and hash it. The contents are wiped on the way out - a
// keyfile is a secret, and a copy of one left in the heap is a copy of the key.
//
static bool HashKeyFile(const dcvol_api* api, const QString& Path, unsigned char* pHash, QString* pError)
{
	QFile File(Path);
	if (!File.open(QIODevice::ReadOnly)) {
		*pError = CKeyFilesWnd::tr("The keyfile \"%1\" could not be opened.").arg(Path);
		return false;
	}

	if (File.size() == 0 || File.size() > MAX_KEYFILE_SIZE) {
		*pError = CKeyFilesWnd::tr("The keyfile \"%1\" is empty or larger than %2 MiB.")
			.arg(Path).arg(MAX_KEYFILE_SIZE / (1024 * 1024));
		return false;
	}

	QByteArray Data = File.readAll();
	File.close();

	if (Data.size() != (int)File.size()) {
		SecureZeroMemory(Data.data(), Data.size());
		*pError = CKeyFilesWnd::tr("The keyfile \"%1\" could not be read.").arg(Path);
		return false;
	}

	bool bOk = api->cp_kf_hash_data(Data.constData(), (u32)Data.size(), pHash) == ST_OK;
	SecureZeroMemory(Data.data(), Data.size());

	if (!bOk)
		*pError = CKeyFilesWnd::tr("The keyfile \"%1\" could not be hashed.").arg(Path);
	return bOk;
}

bool CKeyFilesWnd::Mix(const SKeyFiles& KeyFiles, CSecurePassword& Password, QString* pError)
{
	QString Dummy;
	if (!pError) pError = &Dummy;

	if (KeyFiles.IsEmpty())
		return true;

	const dcvol_api* api = CDcVolume::Api();
	if (!api) {
		*pError = CDcVolume::Error();
		return false;
	}

	//
	// Flatten the list into hashes first, so that a missing file is reported
	// before the password has been touched.
	//
	QList<QByteArray> Hashes;		// DC_KF_HASH_SIZE each
	bool bOk = true;

	foreach(const SKeyFile & File, KeyFiles.Files)
	{
		unsigned char Hash[DC_KF_HASH_SIZE];

		if (File.bVirtual)
		{
			//
			// A virtual keyfile is hashed exactly as a file's contents would
			// be - dcapi's dc_add_virtual_keyfile does the same - so the two
			// kinds are indistinguishable from here on.
			//
			if (File.Data.IsEmpty()) {
				*pError = tr("A virtual keyfile has no content.");
				bOk = false;
			}
			else if (api->cp_kf_hash_data(File.Data.GetPassword(), (u32)File.Data.GetPasswordSize(), Hash) != ST_OK) {
				*pError = tr("A virtual keyfile could not be hashed.");
				bOk = false;
			}
		}
		else
		{
			QStringList Expanded = ExpandFolder(File.Path, pError);
			if (Expanded.isEmpty()) {
				bOk = false;
			}
			else {
				foreach(const QString & One, Expanded) {
					if (!HashKeyFile(api, One, Hash, pError)) { bOk = false; break; }
					Hashes.append(QByteArray((const char*)Hash, DC_KF_HASH_SIZE));
				}
				SecureZeroMemory(Hash, sizeof(Hash));
				if (!bOk) break;
				continue;	// already appended, one per file in the folder
			}
		}

		if (!bOk) {
			SecureZeroMemory(Hash, sizeof(Hash));
			break;
		}

		Hashes.append(QByteArray((const char*)Hash, DC_KF_HASH_SIZE));
		SecureZeroMemory(Hash, sizeof(Hash));
	}

	if (!bOk) {
		for (int i = 0; i < Hashes.count(); i++)
			SecureZeroMemory(Hashes[i].data(), Hashes[i].size());
		return false;
	}

	if (Hashes.isEmpty()) {
		*pError = tr("No keyfiles were found.");
		return false;
	}

	//
	// The password is assembled into library-owned secure memory, mixed there,
	// and read back out. Nothing else ever holds the combined value.
	//
	dc_pass* pPass = (dc_pass*)api->secure_alloc(sizeof(dc_pass));
	if (!pPass) {
		*pError = tr("Could not allocate secure memory.");
		bOk = false;
	}
	else
	{
		memset(pPass, 0, sizeof(dc_pass));

		size_t uSize = Password.GetPasswordSize();
		if (uSize > sizeof(pPass->pass))
			uSize = sizeof(pPass->pass);
		if (uSize)
			memcpy(pPass->pass, Password.GetPassword(), uSize);
		pPass->size = (int)uSize;
		pPass->flags |= PF_KEYFILE_MIXED;

		if (KeyFiles.MixMode == eKeyFileAdditive)
		{
			foreach(const QByteArray & Hash, Hashes)
				api->cp_kf_mix_additive(pPass, (const unsigned char*)Hash.constData());
		}
		else
		{
			//
			// One keyfile of exactly a hash's length is taken as a ready-made
			// key set rather than hashed again, which is how a key handed over
			// by another tool - or a TPM - is used. DiskCryptor does the same.
			// It cannot apply to a virtual keyfile, whose hash is always a hash.
			//
			bool bRaw = false;
			if (Hashes.count() == 1 && KeyFiles.Files.count() == 1 && !KeyFiles.Files.first().bVirtual) {
				QFile File(KeyFiles.Files.first().Path);
				if (File.open(QIODevice::ReadOnly) && File.size() == DC_KF_HASH_SIZE) {
					QByteArray Data = File.readAll();
					if (Data.size() == DC_KF_HASH_SIZE) {
						api->cp_kf_mixer_combine(pPass, (const unsigned char*)Data.constData());
						bRaw = true;
					}
					SecureZeroMemory(Data.data(), Data.size());
				}
			}

			if (!bRaw)
			{
				dc_kf_mixer Mixer;
				if (api->cp_kf_mixer_init(&Mixer) != ST_OK) {
					*pError = tr("Could not start the keyfile mixer.");
					bOk = false;
				}
				else {
					foreach(const QByteArray & Hash, Hashes) {
						if (api->cp_kf_mixer_add_hash(&Mixer, (const unsigned char*)Hash.constData()) != ST_OK) {
							*pError = tr("Could not add a keyfile to the mixer.");
							bOk = false;
							break;
						}
					}

					if (bOk)
						bOk = api->cp_kf_mixer_finish(&Mixer, pPass) == ST_OK;
					else
						api->cp_kf_mixer_free(&Mixer);

					if (!bOk && pError->isEmpty())
						*pError = tr("The keyfiles could not be mixed into the password.");
				}
			}
		}

		if (bOk)
			Password.SetPassword(pPass->pass, (size_t)pPass->size);

		api->secure_free(pPass);
	}

	for (int i = 0; i < Hashes.count(); i++)
		SecureZeroMemory(Hashes[i].data(), Hashes[i].size());

	return bOk;
}
