#include "pch.h"
#include "VolumeWindow.h"
#include "../Widgets/PasswordStrengthWidget.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include "../MiscHelpers/Common/Settings.h"
#include "../MiscHelpers/Common/Common.h"
#include <QStorageInfo>
#include <QFrame>
#include <QSpinBox>


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

	connect(ui.chkShow, SIGNAL(clicked(bool)), this, SLOT(OnShowPassword()));

	connect(ui.btnMount, SIGNAL(clicked()), this, SLOT(BrowseMountPoint()));
	connect(ui.txtImageSize, SIGNAL(textChanged(const QString&)), this, SLOT(OnImageSize()));

	connect(ui.chkProtect, &QCheckBox::clicked, this, [&] {
		ui.chkLockdown->setEnabled(ui.chkProtect->isChecked());
	});

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(CheckPassword()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

	// Disable OK button initially for modes that require new password
	if (Action == eNew || Action == eSetPW || Action == eChange)
		ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

	ui.lblInfo->setText(Prompt);
	ui.lblIcon->setPixmap(QPixmap::fromImage(QImage((m_Action == eMount || m_Action == eGetPW) ? ":/Icons/LockOpen.png" : ":/Icons/LockClosed.png")));

	if (m_Action == eNew || m_Action == eSetPW) 
	{
		ui.lblPassword->setVisible(false);
		ui.txtPassword->setVisible(false);

		ui.lblKdf->setVisible(false);
		ui.cmbKdf->setVisible(false);

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

		ui.lblRepeatPassword->setVisible(false);
		ui.txtRepeatPassword->setVisible(false);

		ui.lblNewKdf->setVisible(false);
		ui.cmbNewKdf->setVisible(false);
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
	ui.txtPassword->setEchoMode(ui.chkShow->isChecked() ? QLineEdit::Normal : QLineEdit::Password);
	ui.txtNewPassword->setEchoMode(ui.chkShow->isChecked() ? QLineEdit::Normal : QLineEdit::Password);
	ui.txtRepeatPassword->setEchoMode(ui.chkShow->isChecked() ? QLineEdit::Normal : QLineEdit::Password);
}

void CVolumeWindow::OnImageSize()
{
	ui.lblImageSizeKb->setText(tr("kilobytes (%1)").arg(FormatSize(GetImageSize())));
}

void CVolumeWindow::OnNewPasswordChanged()
{
	QString pw = ui.txtNewPassword->text();
	QString confirm = ui.txtRepeatPassword->text();

	if (m_pStrengthWidget) {
		int kdfValue = ui.cmbNewKdf->currentData().toInt();
		m_pStrengthWidget->SetKdfValue(kdfValue);
		m_pStrengthWidget->SetPassword(pw);
	}

	if (m_pPasswordStatus) {
		m_pPasswordStatus->setText(CPasswordStrengthWidget::GetPasswordStatusText(pw, confirm));
	}

	// Enable OK button only when passwords match and length is valid
	bool valid = !pw.isEmpty() && pw == confirm && pw.length() <= 128;
	ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid);
}

void CVolumeWindow::CheckPassword()
{
	if (m_Action == eMount || m_Action == eGetPW) {
		m_Password = ui.txtPassword->text();
	}
	else {

		if (ui.txtNewPassword->text() != ui.txtRepeatPassword->text()) {
			QMessageBox::critical(this, "MajorPrivacy", tr("Passwords don't match!!!"));
			return;
		}
		if (ui.txtNewPassword->text().length() < 20) {
			if (QMessageBox::warning(this, "MajorPrivacy", tr("WARNING: Short passwords are easy to crack using brute force techniques!\n\n"
				"It is recommended to choose a password consisting of 20 or more characters. Are you sure you want to use a short password?")
				, QMessageBox::Yes, QMessageBox::No) != QMessageBox::Yes)
				return;
		}
		if (ui.txtNewPassword->text().length() > 128) {
			QMessageBox::warning(this, "MajorPrivacy", tr("The password is constrained to a maximum length of 128 characters. \n"
				"This length permits approximately 384 bits of entropy with a passphrase composed of actual English words, \n"
				"increases to 512 bits with the application of Leet (L337) speak modifications, and exceeds 768 bits when composed of entirely random printable ASCII characters.")
				, QMessageBox::Ok);
			return;
		}

		if (m_Action == eNew || m_Action == eSetPW)
			m_Password = ui.txtNewPassword->text();
		else if (m_Action == eChange) {
			m_Password = ui.txtPassword->text();
			m_NewPassword = ui.txtNewPassword->text();
		}
	}
	
	if (m_Action == eNew) {
		if (GetImageSize() < 128 * 1024 * 1024) { // ask for 256 mb but silently accept >= 128 mb
			QMessageBox::critical(this, "MajorPrivacy", tr("The Box Disk Image must be at least 256 MB in size, 2GB are recommended."));
			SetImageSize(256 * 1024 * 1024);
			return;
		}
	}

	accept();
}

void CVolumeWindow::BrowseMountPoint()
{
	QString path = QFileDialog::getExistingDirectory(this, tr("Select Mount Point"), ui.cmbMount->currentText());
	if (!path.isEmpty())
		ui.cmbMount->setCurrentText(path);
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