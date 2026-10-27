#include "pch.h"
#include "VolumeWindow.h"
#include "KeyFilesWnd.h"
#include "../Widgets/PasswordStrengthWidget.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include "../MiscHelpers/Common/Settings.h"
#include "../MiscHelpers/Common/Common.h"
#include <QStorageInfo>
#include <QFrame>
#include <QSpinBox>
#include <QToolButton>


CVolumeWindow::CVolumeWindow(const QString& Prompt, EAction Action, QWidget *parent)
	: QDialog(parent)
{
	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	//flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	//flags &= ~Qt::WindowMinMaxButtonsHint;
	//flags |= Qt::WindowMinimizeButtonHint;
	//flags &= ~Qt::WindowCloseButtonHint;
	flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	setWindowFlags(flags);

	ui.setupUi(this);
	this->setWindowTitle("MajorPrivacy");

	m_Action = Action;

	//
	// Two buttons rather than one checkbox below the form: each sits next to the
	// field it reveals, the way the secure-entry buttons do, and the new password
	// no longer has to be shown just to check the one being typed in.
	//
	ui.btnShowPw->setIcon(QIcon(":/Icons/Invisible.png"));
	connect(ui.btnShowPw, &QToolButton::clicked, this, &CVolumeWindow::OnShowPassword);

	ui.btnShowNewPw->setIcon(QIcon(":/Icons/Invisible.png"));
	connect(ui.btnShowNewPw, &QToolButton::clicked, this, &CVolumeWindow::OnShowNewPassword);

	connect(ui.btnMount, SIGNAL(clicked()), this, SLOT(BrowseMountPoint()));
	connect(ui.txtImageSize, SIGNAL(textChanged(const QString&)), this, SLOT(OnImageSize()));

	connect(ui.chkProtect, &QCheckBox::clicked, this, [&] {
		ui.chkLockdown->setEnabled(ui.chkProtect->isChecked());
	});

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(CheckPassword()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

	// Setup secure password entry buttons
	ui.btnSecurePw->setIcon(QIcon(":/Icons/Shield.png"));
	connect(ui.btnSecurePw, &QToolButton::clicked, this, &CVolumeWindow::OnSecurePasswordEntry);

	ui.btnSecureNewPw->setIcon(QIcon(":/Icons/Shield.png"));
	connect(ui.btnSecureNewPw, &QToolButton::clicked, this, &CVolumeWindow::OnSecureNewPasswordEntry);

	//
	// Keyfile buttons, on the KDF row of the password they belong to. Added here
	// rather than in the .ui because they only exist when dcvolume.dll does -
	// without it there is nothing to mix keyfiles with, and an enabled button
	// that cannot work is worse than no button.
	//
	if (CKeyFilesWnd::IsAvailable())
	{
		QGridLayout* grid = qobject_cast<QGridLayout*>(ui.gridLayout);
		if (grid)
		{
			m_pKeyFilesBtn = new QToolButton();
			m_pKeyFilesBtn->setIcon(QIcon(":/Icons/VolumePW.png"));
			m_pKeyFilesBtn->setCheckable(true);
			m_pKeyFilesBtn->setToolTip(tr("Mix keyfiles into the password"));
			grid->addWidget(m_pKeyFilesBtn, 2, 6);
			connect(m_pKeyFilesBtn, &QToolButton::clicked, this, &CVolumeWindow::OnKeyFiles);

			m_pNewKeyFilesBtn = new QToolButton();
			m_pNewKeyFilesBtn->setIcon(QIcon(":/Icons/VolumePW.png"));
			m_pNewKeyFilesBtn->setCheckable(true);
			m_pNewKeyFilesBtn->setToolTip(tr("Mix keyfiles into the new password"));
			grid->addWidget(m_pNewKeyFilesBtn, 6, 6);
			connect(m_pNewKeyFilesBtn, &QToolButton::clicked, this, &CVolumeWindow::OnNewKeyFiles);
		}
	}

	// Disable OK button initially for modes that require new password
	if (Action == eNew || Action == eSetPW || Action == eChange)
		ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

	ui.lblInfo->setText(Prompt);
	ui.lblIcon->setPixmap(QPixmap::fromImage(QImage((m_Action == eMount || m_Action == eGetPW) ? ":/Icons/LockOpen.png" : ":/Icons/LockClosed.png")));

	if (m_Action == eNew || m_Action == eSetPW)
	{
		ui.lblPassword->setVisible(false);
		ui.txtPassword->setVisible(false);
		ui.btnSecurePw->setVisible(false);
		if (m_pKeyFilesBtn) m_pKeyFilesBtn->setVisible(false);

		ui.lblKdf->setVisible(false);
		ui.cmbKdf->setVisible(false);
		ui.btnShowPw->setVisible(false);

		ui.lblDummy->setVisible(false);

		ui.txtNewPassword->setFocus();
	}
	else
		ui.txtPassword->setFocus();

	if (m_Action == eNew)
	{
		ui.txtImageSize->setText(QString::number(2 * 1024 * 1024)); // suggest 2GB
	}

	if (m_Action != eNew)
	{
		ui.lblImageSize->setVisible(false);
		ui.txtImageSize->setVisible(false);
		ui.lblImageSizeKb->setVisible(false);
	}

	if (m_Action == eMount || m_Action == eGetPW)
	{
		ui.lblNewPassword->setVisible(false);
		ui.txtNewPassword->setVisible(false);
		ui.btnSecureNewPw->setVisible(false);
		if (m_pNewKeyFilesBtn) m_pNewKeyFilesBtn->setVisible(false);

		ui.lblRepeatPassword->setVisible(false);
		ui.txtRepeatPassword->setVisible(false);

		ui.lblNewKdf->setVisible(false);
		ui.cmbNewKdf->setVisible(false);
		ui.btnShowNewPw->setVisible(false);
	}

	// Password strength widget for actions that set a new password
	if (m_Action == eNew || m_Action == eSetPW || m_Action == eChange)
	{
		QFrame* strengthFrame = new QFrame;
		strengthFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
		QVBoxLayout* frameLayout = new QVBoxLayout(strengthFrame);
		frameLayout->setContentsMargins(0, 0, 0, 0);
		m_pStrengthWidget = new CPasswordStrengthWidget;
		frameLayout->addWidget(m_pStrengthWidget);

		// Insert after row 6 (after new KDF combo) - row 7, aligned with password fields (column 2)
		QGridLayout* grid = qobject_cast<QGridLayout*>(ui.gridLayout);

		// Password status label (match/length feedback)
		m_pPasswordStatus = new QLabel;
		m_pPasswordStatus->setTextFormat(Qt::RichText);
		grid->addWidget(m_pPasswordStatus, 8, 1, 1, 5);

		grid->addWidget(strengthFrame, 9, 1, 1, 5);

		connect(ui.txtNewPassword, &QLineEdit::textChanged, this, &CVolumeWindow::OnNewPasswordChanged);
		connect(ui.txtRepeatPassword, &QLineEdit::textChanged, this, &CVolumeWindow::OnNewPasswordChanged);
		connect(ui.cmbNewKdf, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CVolumeWindow::OnNewPasswordChanged);
	}

	ui.lblAutoLocl->setVisible(false);
	ui.cmbAutoLock->setVisible(false);

	if (m_Action == eMount)
	{
		QList<char> usedDriveLetters;
		QList<QStorageInfo> drives = QStorageInfo::mountedVolumes();
		for (const QStorageInfo &drive : drives) {
			QString driveLetter = drive.rootPath();
			if (!driveLetter.isEmpty() && driveLetter.length() >= 2 && driveLetter.at(1) == ':')
				usedDriveLetters.append(driveLetter.at(0).toUpper().toLatin1());
		}

		for (char letter = 'D'; letter <= 'Z'; ++letter) {
			if (!usedDriveLetters.contains(letter))
				ui.cmbMount->addItem(QString(1, letter) + ":\\");
		}
	}
	else
	{
		ui.lblMount->setVisible(false);
		ui.cmbMount->setVisible(false);
		ui.btnMount->setVisible(false);
	}

	//if (!bNew) {
		ui.lblCipher->setVisible(false);
		ui.cmbCipher->setVisible(false);
	//}
	ui.cmbCipher->addItem("AES", 0);
	ui.cmbCipher->addItem("Twofish", 1);
	ui.cmbCipher->addItem("Serpent", 2);
	ui.cmbCipher->addItem("AES-Twofish", 3);
	ui.cmbCipher->addItem("Twofish-Serpent", 4);
	ui.cmbCipher->addItem("Serpent-AES", 5);
	ui.cmbCipher->addItem("AES-Twofish-Serpent", 6);

	if (m_Action != eMount) {
		ui.chkProtect->setVisible(false);
		ui.chkLockdown->setVisible(false);
	}

	FillKdfCombo(ui.cmbKdf);
	FillKdfCombo(ui.cmbNewKdf, false, 5);

	if (m_Action == eMount || m_Action == eChange) {
		ui.cmbKdf->addItem(tr("Argon2id (Old 0.99.7 compatible mode)"), 1000);

		// Old KDF cost factor spinbox (shown when old Argon2id is selected)
		QGridLayout* grid = qobject_cast<QGridLayout*>(ui.gridLayout);
		if (grid) {
			m_pOldKdfLabel = new QLabel(tr("Cost Factor:"));
			m_pOldKdfLabel->setVisible(false);
			grid->addWidget(m_pOldKdfLabel, 3, 2, 1, 1);

			m_pOldKdfSpin = new QSpinBox;
			m_pOldKdfSpin->setRange(1, 100);
			m_pOldKdfSpin->setValue(12);
			m_pOldKdfSpin->setVisible(false);
			m_pOldKdfSpin->setToolTip(tr("Argon2id cost factor for old volumes created with MajorPrivacy v0.99.7"));
			grid->addWidget(m_pOldKdfSpin, 3, 3, 1, 1);
		}

		connect(ui.cmbKdf, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
			bool showOldKdf = (ui.cmbKdf->currentData().toInt() == 1000);
			if (m_pOldKdfLabel) m_pOldKdfLabel->setVisible(showOldKdf);
			if (m_pOldKdfSpin) m_pOldKdfSpin->setVisible(showOldKdf);
		});
	}

	restoreGeometry(theConf->GetBlob("VolumeWindow/Window_Geometry"));

	// Adjust the size of the dialog
	this->adjustSize();
}

CVolumeWindow::~CVolumeWindow()
{
	theConf->SetBlob("VolumeWindow/Window_Geometry", saveGeometry());
}

QStringList g_KdfOptions = {
	CVolumeWindow::tr("Pkcs5.2  (0 - Legacy)"),
	CVolumeWindow::tr("Argon2id (1 - Minimum)"),
	CVolumeWindow::tr("Argon2id (2 - Low)"),
	CVolumeWindow::tr("Argon2id (3 - Moderate)"),
	CVolumeWindow::tr("Argon2id (4 - Standard)"),
	CVolumeWindow::tr("Argon2id (5 - Recommended)"),
	CVolumeWindow::tr("Argon2id (6 - Enhanced)"),
	CVolumeWindow::tr("Argon2id (7 - Hardened)"),
	CVolumeWindow::tr("Argon2id (8 - High Cost)"),
	CVolumeWindow::tr("Argon2id (9 - Very High Cost)"),
	CVolumeWindow::tr("Argon2id (10 - Extreme Cost)")
};

void CVolumeWindow::FillKdfCombo(QComboBox* Combo, bool bAuto, int Selected)
{
	if (bAuto) Combo->addItem(tr("Standard Pkcs5.2 & Default Argon2id"), -2);
	if (bAuto) Combo->addItem(tr("Try all KDF's >>> Very Slow <<<"), -3);

	for(int i = 0; i < g_KdfOptions.size(); i++) {
		Combo->addItem(g_KdfOptions[i], i);
	}

	int Index = Combo->findData(Selected);
	if (Index != -1) Combo->setCurrentIndex(Index);
}

QString CVolumeWindow::GetKdfName(int iKdf)
{
	//if (iKdf == -2) return tr("Standard Pkcs5.2 & Default Argon2id");
	//if (iKdf == -3) return tr("Try all KDF's >>> Very Slow <<<");
	//if (iKdf == 1000) return tr("Argon2id (Old 0.99.7 compatible mode)");
	if (iKdf >= 0 && iKdf < g_KdfOptions.size())
		return g_KdfOptions[iKdf];
	return QString::number(iKdf);
}

void CVolumeWindow::OnShowPassword()
{
	ui.txtPassword->setEchoMode(ui.btnShowPw->isChecked() ? QLineEdit::Normal : QLineEdit::Password);
}

void CVolumeWindow::OnShowNewPassword()
{
	QLineEdit::EchoMode Mode = ui.btnShowNewPw->isChecked() ? QLineEdit::Normal : QLineEdit::Password;
	ui.txtNewPassword->setEchoMode(Mode);
	ui.txtRepeatPassword->setEchoMode(Mode);
}

void CVolumeWindow::OnImageSize()
{
	ui.lblImageSizeKb->setText(tr("kilobytes (%1)").arg(FormatSize(GetImageSize())));
}

void CVolumeWindow::OnNewPasswordChanged()
{
	CSecurePassword pw;
	bool bConfirm = true;
	bool bToLong = false;
	if (!m_SecureNewPassword.IsEmpty())
		pw = m_SecureNewPassword;
	else {
		// Get the new password (from edit field)
		QString sPassword = ui.txtNewPassword->text();
		if (sPassword.length() > MAX_SEC_PASSWORD)
			bToLong = true;
		else {
			QString sConfirm = ui.txtRepeatPassword->text();
			bConfirm = sPassword == sConfirm;

			pw.SetPassword((wchar_t*)sPassword.utf16(), sPassword.length() * sizeof(wchar_t));
		}
	}

	// Enable OK button only when passwords match
	bool valid = !pw.IsEmpty() && bConfirm;
	ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);

	if (m_pStrengthWidget) {
		int kdfValue = ui.cmbNewKdf->currentData().toInt();
		m_pStrengthWidget->SetKdfValue(kdfValue);
		m_pStrengthWidget->SetPassword(pw);
	}

	if (m_pPasswordStatus) {
		if (bToLong)
			m_pPasswordStatus->setText(tr("<span style='color:red;'>Password exceeds maximum length of %1 characters!</span>").arg(MAX_SEC_PASSWORD));
		else if (!bConfirm)
			m_pPasswordStatus->setText(tr("<span style='color:red;'>Passwords do not match!</span>"));
		else
			m_pPasswordStatus->setText(CPasswordStrengthWidget::GetPasswordStatusText(pw));
	}
}

void CVolumeWindow::CheckPassword()
{
	if (m_Action != eMount && m_Action != eGetPW) 
	{
		int PasswordLength = 0;
		if (!m_SecureNewPassword.IsEmpty())
			PasswordLength = m_SecureNewPassword.GetPasswordLength();
		else {
			// Get the new password (from secure entry or edit field)
			QString sPassword = ui.txtNewPassword->text();
			QString sConfirm = ui.txtRepeatPassword->text();

			if (sPassword != sConfirm) {
				QMessageBox::critical(this, "MajorPrivacy", tr("Passwords don't match!!!"));
				return;
			}
			PasswordLength = sPassword.length();
			if (sPassword.length() > MAX_SEC_PASSWORD) {
				QMessageBox::warning(this, "MajorPrivacy", tr("The password is constrained to a maximum length of %1 characters. \n"
					"This length permits approximately 384 bits of entropy with a passphrase composed of actual English words, \n"
					"increases to 512 bits with the application of Leet (L337) speak modifications, and exceeds 768 bits when composed of entirely random printable ASCII characters.").arg(MAX_SEC_PASSWORD)
					, QMessageBox::Ok);
				return;
			}
		}

		if (PasswordLength < 20) {
			if (QMessageBox::warning(this, "MajorPrivacy", tr("WARNING: Short passwords are easy to crack using brute force techniques!\n\n"
				"It is recommended to choose a password consisting of 20 or more characters. Are you sure you want to use a short password?")
				, QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
				return;
		}
	}
	
	if (!MixKeyFiles())
		return;

	if (m_Action == eNew) {
		if (GetImageSize() < 128 * 1024 * 1024) { // ask for 256 mb but silently accept >= 128 mb
			QMessageBox::critical(this, "MajorPrivacy", tr("The Box Disk Image must be at least 128 MB in size, 2GB are recommended."));
			SetImageSize(128 * 1024 * 1024);
			return;
		}
	}

	accept();
}

void CVolumeWindow::BrowseMountPoint()
{
	QString path = QFileDialog::getExistingDirectory(this, tr("Select Mount Point"), ui.cmbMount->currentText());
	if (!path.isEmpty())
		ui.cmbMount->setCurrentText(path.replace("/", "\\"));
}

void CVolumeWindow::SetAutoLock(int iSeconds, const QString& Text) const 
{ 
	ui.lblAutoLocl->setVisible(true);
	ui.lblAutoLocl->setText(Text);

	ui.cmbAutoLock->setVisible(true);
	ui.cmbAutoLock->addItem(tr("Never"), 0);
	ui.cmbAutoLock->addItem(tr("5 minutes"), 5*60);
	ui.cmbAutoLock->addItem(tr("15 minutes"), 15*60);
	ui.cmbAutoLock->addItem(tr("30 minutes"), 30*60);
	ui.cmbAutoLock->addItem(tr("1 hour"), 60*60);

	int Index = ui.cmbAutoLock->findData(iSeconds);
	if (Index == -1) {
		if(iSeconds >= 60*60)
			ui.cmbAutoLock->addItem(tr("%1 hours").arg(double(iSeconds)/3600.0, 2), iSeconds);
		else
			ui.cmbAutoLock->addItem(tr("%1 minutes").arg(iSeconds/60), iSeconds);
		ui.cmbAutoLock->setCurrentIndex(ui.cmbAutoLock->count() - 1);
	} else
		ui.cmbAutoLock->setCurrentIndex(Index); 
}

int CVolumeWindow::GetKdf() const
{
	int iKdf = ui.cmbKdf->currentData().toInt();
	if (iKdf == 1000 && m_pOldKdfSpin)
	{
		iKdf = 1000 + m_pOldKdfSpin->value();
	}
	return iKdf;
}

void CVolumeWindow::SetKdf(int iKdf) const
{
	int Index = ui.cmbKdf->findData(iKdf);
	if (Index != -1) ui.cmbKdf->setCurrentIndex(Index);
}

void CVolumeWindow::SetNoAutoKdf()
{
	ui.cmbKdf->clear();

	for(int i = 0; i < g_KdfOptions.size(); i++) {
		ui.cmbKdf->addItem(g_KdfOptions[i], i);
	}

	SetKdf(5);
}

void CVolumeWindow::SetNoKdf()
{
	ui.lblKdf->setVisible(false);
	ui.cmbKdf->setVisible(false);
	ui.lblNewKdf->setVisible(false);
	ui.cmbNewKdf->setVisible(false);

	if (m_pKeyFilesBtn) m_pKeyFilesBtn->setVisible(false);
	if (m_pNewKeyFilesBtn) m_pNewKeyFilesBtn->setVisible(false);
}

CSecurePassword CVolumeWindow::GetPassword() const
{
	if (m_Action == eNew || m_Action == eSetPW)
		return GetNewPassword();

	// what MixKeyFiles built on accept, if keyfiles were named
	if (!m_MixedPassword.IsEmpty())
		return m_MixedPassword;

	if (!m_SecurePassword.IsEmpty())
		return m_SecurePassword;
	QString sPassword = ui.txtPassword->text();
	return CSecurePassword((wchar_t*)sPassword.utf16(), sPassword.length() * sizeof(wchar_t));
}

CSecurePassword CVolumeWindow::GetNewPassword() const
{
	if (!m_MixedNewPassword.IsEmpty())
		return m_MixedNewPassword;

	if (!m_SecureNewPassword.IsEmpty())
		return m_SecureNewPassword;
	QString sPassword = ui.txtNewPassword->text();
	return CSecurePassword((wchar_t*)sPassword.utf16(), sPassword.length() * sizeof(wchar_t));
}

CSecurePassword	 CVolumeWindow::RequestSecurePassword(bool bConfirm)
{
	QString prompt = bConfirm
		? tr("Enter and confirm your new password:")
		: tr("Enter your password:");

	auto Ret = theCore->RequestSecurePassword(prompt, tr("MajorPrivacy - Secure Password Entry"), bConfirm);
	if (Ret.IsError())
	{
		if (Ret.GetStatus() == STATUS_CANCELLED)
			return CSecurePassword(); // User cancelled - return empty

		const wchar_t* msg = Ret.GetMessageText();
		QMessageBox::warning(this, "MajorPrivacy", tr("Secure password entry failed: %1").arg(msg ? QString::fromWCharArray(msg) : tr("Unknown error")));
		return CSecurePassword();
	}

	return Ret.GetValue();
}

void CVolumeWindow::SetKeyFileSet(const SKeyFiles& KeyFiles)
{
	m_KeyFiles = KeyFiles;
	UpdateKeyFileButtons();
}

void CVolumeWindow::UpdateKeyFileButtons()
{
	if (m_pKeyFilesBtn) {
		m_pKeyFilesBtn->setChecked(!m_KeyFiles.IsEmpty());
		m_pKeyFilesBtn->setToolTip(m_KeyFiles.IsEmpty()
			? tr("Mix keyfiles into the password")
			: tr("%1 keyfile(s) will be mixed into the password").arg(m_KeyFiles.Count()));
	}
	if (m_pNewKeyFilesBtn) {
		m_pNewKeyFilesBtn->setChecked(!m_NewKeyFiles.IsEmpty());
		m_pNewKeyFilesBtn->setToolTip(m_NewKeyFiles.IsEmpty()
			? tr("Mix keyfiles into the new password")
			: tr("%1 keyfile(s) will be mixed into the new password").arg(m_NewKeyFiles.Count()));
	}
}

void CVolumeWindow::OnKeyFiles()
{
	CKeyFilesWnd Wnd(m_KeyFiles, this, m_bAllowVirtualKeyFiles);
	if (Wnd.exec() == QDialog::Accepted)
		m_KeyFiles = Wnd.GetKeyFiles();
	UpdateKeyFileButtons();
}

void CVolumeWindow::OnNewKeyFiles()
{
	CKeyFilesWnd Wnd(m_NewKeyFiles, this, m_bAllowVirtualKeyFiles);
	if (Wnd.exec() == QDialog::Accepted)
		m_NewKeyFiles = Wnd.GetKeyFiles();
	UpdateKeyFileButtons();
}

//
// Fold the keyfiles in, once, as the dialog is accepted.
//
// Doing it here rather than in the accessors means a missing or unreadable
// keyfile is a message in front of the user with the dialog still open, instead
// of a wrong password reported by whatever ran next.
//
bool CVolumeWindow::MixKeyFiles()
{
	m_MixedPassword.ClearPassword();
	m_MixedNewPassword.ClearPassword();

	QString Error;

	if (!m_KeyFiles.IsEmpty() && m_Action != eNew && m_Action != eSetPW)
	{
		CSecurePassword Password = m_SecurePassword;
		if (Password.IsEmpty()) {
			QString sPassword = ui.txtPassword->text();
			Password.SetPassword((wchar_t*)sPassword.utf16(), sPassword.length() * sizeof(wchar_t));
		}

		if (!CKeyFilesWnd::Mix(m_KeyFiles, Password, &Error)) {
			QMessageBox::critical(this, "MajorPrivacy", tr("The keyfiles could not be applied.\n\n%1").arg(Error));
			return false;
		}
		m_MixedPassword = Password;
	}

	if (!m_NewKeyFiles.IsEmpty() && m_Action != eMount && m_Action != eGetPW)
	{
		CSecurePassword Password = m_SecureNewPassword;
		if (Password.IsEmpty()) {
			QString sPassword = ui.txtNewPassword->text();
			Password.SetPassword((wchar_t*)sPassword.utf16(), sPassword.length() * sizeof(wchar_t));
		}

		if (!CKeyFilesWnd::Mix(m_NewKeyFiles, Password, &Error)) {
			QMessageBox::critical(this, "MajorPrivacy", tr("The keyfiles could not be applied to the new password.\n\n%1").arg(Error));
			return false;
		}
		m_MixedNewPassword = Password;
	}

	return true;
}

void CVolumeWindow::OnSecurePasswordEntry()
{
	if (ui.btnSecurePw->isChecked())
	{
		// Button is now checked - request secure password entry
		CSecurePassword password = RequestSecurePassword(false);
		if (password.IsEmpty())
		{
			// User cancelled or error - uncheck button
			ui.btnSecurePw->setChecked(false);
			return;
		}

		// Store cached password and disable edit
		m_SecurePassword = password;
		ui.txtPassword->setEnabled(false);
		ui.txtPassword->setText(QString(password.GetPasswordLength(), QChar(0x2022))); // Show bullets
	}
	else
	{
		// Button unchecked - clear cached password and re-enable edit
		m_SecurePassword.ClearPassword();
		ui.txtPassword->setEnabled(true);
		ui.txtPassword->clear();
		ui.txtPassword->setFocus();
	}
}

void CVolumeWindow::OnSecureNewPasswordEntry()
{
	if (ui.btnSecureNewPw->isChecked())
	{
		// Button is now checked - request secure password entry with confirmation
		CSecurePassword password = RequestSecurePassword(true);
		if (password.IsEmpty())
		{
			// User cancelled or error - uncheck button
			ui.btnSecureNewPw->setChecked(false);
			return;
		}

		// Store cached password and disable edits
		m_SecureNewPassword = password;
		ui.txtNewPassword->setEnabled(false);
		ui.txtNewPassword->setText(QString(password.GetPasswordLength(), QChar(0x2022))); // Show bullets
		ui.txtRepeatPassword->setEnabled(false);
		ui.txtRepeatPassword->setText(QString(password.GetPasswordLength(), QChar(0x2022)));

		// Update password strength display
		OnNewPasswordChanged();
	}
	else
	{
		// Button unchecked - clear cached password and re-enable edits
		m_SecureNewPassword.ClearPassword();
		ui.txtNewPassword->setEnabled(true);
		ui.txtNewPassword->clear();
		ui.txtRepeatPassword->setEnabled(true);
		ui.txtRepeatPassword->clear();
		ui.txtNewPassword->setFocus();

		// Update password strength display
		OnNewPasswordChanged();
	}
}