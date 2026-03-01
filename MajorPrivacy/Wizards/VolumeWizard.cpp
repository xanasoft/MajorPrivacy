#include "pch.h"

#include "VolumeWizard.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Volumes/VolumeManager.h"
#include "../MiscHelpers/Common/CheckableMessageBox.h"
#include "../../MiscHelpers/Common/ProgressDialog.h"
#include <QtConcurrent/QtConcurrent>

CVolumeWizard::CVolumeWizard(QWidget *parent)
    : QWizard(parent)
{
    setPage(Page_Intro, new CVolumeIntroPage);
    setPage(Page_Location, new CVolumeLocationPage);
    setPage(Page_Encryption, new CVolumeEncryptionPage);
    setPage(Page_Size, new CVolumeSizePage);
    setPage(Page_Password, new CVolumePasswordPage);
    setPage(Page_Cost, new CVolumeCostPage);
    setPage(Page_Summary, new CVolumeSummaryPage);

    setStartId(Page_Intro);

    this->setMinimumWidth(550);
    this->setMinimumHeight(400);

    setWizardStyle(ModernStyle);
    setPixmap(QWizard::LogoPixmap, QPixmap(":/Icons/AddVolume.png").scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    connect(this, &QWizard::helpRequested, this, &CVolumeWizard::showHelp);

    setWindowTitle(tr("Create Encrypted Volume"));
}

CVolumeWizard::~CVolumeWizard()
{
}

void CVolumeWizard::showHelp()
{
    QString message;

    switch (currentId()) {
    case Page_Intro:
        message = tr("This wizard will guide you through the process of creating an encrypted volume.");
        break;
    case Page_Location:
        message = tr("Choose a location and filename for your encrypted volume. The file will have a .pv extension.");
        break;
    case Page_Encryption:
        message = tr("Select the encryption algorithm. AES is recommended for most users. Cascaded ciphers provide additional security at the cost of performance.");
        break;
    case Page_Size:
        message = tr("Specify the size of your encrypted volume. The minimum size is 256 MB, and 2 GB or more is recommended.");
        break;
    case Page_Password:
        message = tr("Choose a strong password. A password of 20 or more characters is recommended for maximum security.");
        break;
    case Page_Cost:
        message = tr("Cost increases the number of iterations and memory used in key derivation. Higher values increase security but slow down mounting.");
        break;
    case Page_Summary:
        message = tr("Review your settings before creating the volume.");
        break;
    default:
        message = tr("No help available for this page.");
    }

    QMessageBox::information(this, tr("Volume Creation Help"), message);
}

bool CVolumeWizard::ShowWizard(QWidget* parent)
{
    CVolumeWizard wizard(parent ? parent : theGUI);
    if (!theGUI->SafeExec(&wizard))
        return false;

    // Show warning about encryption risks
    if (theConf->GetBool("Options/WarnBoxCrypto", true)) {
        bool State = false;
        if (CCheckableMessageBox::question(parent ? parent : theGUI, "MajorPrivacy",
            QObject::tr("This volume content will be placed in an encrypted container file, "
                "please note that any corruption of the container's header will render all its content permanently inaccessible. "
                "Corruption can occur as a result of a BSOD, a storage hardware failure, or a malicious application overwriting random files. "
                "This feature is provided under a strict <b>No Backup No Mercy</b> policy, YOU the user are responsible for the data you put into an encrypted box. "
                "<br /><br />"
                "IF YOU AGREE TO TAKE FULL RESPONSIBILITY FOR YOUR DATA PRESS [YES], OTHERWISE PRESS [NO].")
            , QObject::tr("Don't show this message again."), &State, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::No, QMessageBox::Warning) != QDialogButtonBox::Yes)
            return false;

        if (State)
            theConf->SetValue("Options/WarnBoxCrypto", false);
    }

    // Create the volume
    QString Path = wizard.GetVolumePath();
    QString Password = wizard.GetPassword();
    quint64 ImageSize = wizard.GetImageSize();
    QString Cipher = wizard.GetCipher();
    int iArgon2Cost = wizard.GetArgon2Cost();

    // Show progress dialog and run operation in background
    CProgressDialog* pProgress = new CProgressDialog(QObject::tr("Creating encrypted volume..."), parent ? parent : theGUI);
    pProgress->setAttribute(Qt::WA_DeleteOnClose);

    QFutureWatcher<STATUS>* pWatcher = new QFutureWatcher<STATUS>();
    STATUS Result;
    bool bFinished = false;

    QObject::connect(pWatcher, &QFutureWatcher<STATUS>::finished, pProgress, [&]() {
        Result = pWatcher->result();
        bFinished = true;
        pProgress->close();
    });

    QFuture<STATUS> future = QtConcurrent::run([=]() {
        return theCore->CreateVolume(Path, Password, ImageSize, Cipher, iArgon2Cost);
    });
    pWatcher->setFuture(future);

    pProgress->exec();

    pWatcher->deleteLater();

    if (Result.IsSuccess())
        theCore->VolumeManager()->AddVolume(Path);
    theGUI->CheckResults(QList<STATUS>() << Result, parent ? parent : theGUI);

    return Result.IsSuccess();
}

QString CVolumeWizard::GetVolumePath() const
{
    return field("volumePath").toString();
}

QString CVolumeWizard::GetPassword() const
{
    return field("password").toString();
}

quint64 CVolumeWizard::GetImageSize() const
{
    quint64 size = field("volumeSize").toULongLong();
    int unit = field("sizeUnit").toInt();

    switch (unit) {
    case 0: // KB
        return size * 1024ULL;
    case 1: // MB
        return size * 1024ULL * 1024ULL;
    case 2: // GB
        return size * 1024ULL * 1024ULL * 1024ULL;
    default:
        return size * 1024ULL * 1024ULL; // Default to MB
    }
}

QString CVolumeWizard::GetCipher() const
{
    int cipherIndex = field("cipherIndex").toInt();
    switch (cipherIndex) {
    case 0: return "AES";
    case 1: return "Twofish";
    case 2: return "Serpent";
    case 3: return "AES-Twofish";
    case 4: return "Twofish-Serpent";
    case 5: return "Serpent-AES";
    case 6: return "AES-Twofish-Serpent";
    default: return "AES";
    }
}

quint32 CVolumeWizard::GetArgon2Cost() const
{
    if (!field("useArgon2").toBool())
        return 0;
    return field("costArgon2").toUInt();
}

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumeIntroPage
//

CVolumeIntroPage::CVolumeIntroPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Welcome to the Volume Creation Wizard"));
    setSubTitle(tr("This wizard will help you create an encrypted volume for secure data storage."));

    QVBoxLayout *layout = new QVBoxLayout;

    m_pLabel = new QLabel(tr(
        "An encrypted volume is a secure container that protects your files using strong encryption. "
        "Only those who know the correct password can access the contents.\n\n"
        "This wizard will guide you through the following steps:\n\n"
        "1. Choose a location for the volume file\n"
        "2. Select encryption settings\n"
        "3. Set the volume size\n"
        "4. Create a strong password\n"
        "5. Optionally configure advanced security settings\n\n"
        "Click Next to continue."));
    m_pLabel->setWordWrap(true);
    layout->addWidget(m_pLabel);

    layout->addStretch();

    setLayout(layout);
}

int CVolumeIntroPage::nextId() const
{
    return CVolumeWizard::Page_Location;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumeLocationPage
//

CVolumeLocationPage::CVolumeLocationPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Volume Location"));
    setSubTitle(tr("Select where to create the encrypted volume file."));

    QVBoxLayout *mainLayout = new QVBoxLayout;

    m_pTopLabel = new QLabel(tr(
        "The volume will be stored as a single encrypted file. Choose a location and filename for this file.\n\n"
        "Note: The volume file should be stored on a reliable storage device. Data loss can occur if the file becomes corrupted."));
    m_pTopLabel->setWordWrap(true);
    mainLayout->addWidget(m_pTopLabel);

    mainLayout->addSpacing(20);

    QHBoxLayout *pathLayout = new QHBoxLayout;

    QLabel* pathLabel = new QLabel(tr("Volume Location:"));
    pathLayout->addWidget(pathLabel);

    m_pVolumePath = new QLineEdit;
    m_pVolumePath->setPlaceholderText(tr("Select or enter volume file path..."));
    pathLayout->addWidget(m_pVolumePath, 1);
    registerField("volumePath*", m_pVolumePath);

    m_pBrowseBtn = new QToolButton;
    m_pBrowseBtn->setText("...");
    m_pBrowseBtn->setToolTip(tr("Browse for location"));
    connect(m_pBrowseBtn, &QToolButton::clicked, this, &CVolumeLocationPage::OnBrowse);
    pathLayout->addWidget(m_pBrowseBtn);

    mainLayout->addLayout(pathLayout);

    QLabel* noteLabel = new QLabel(tr("<i>The file will be created with a .pv (Private Volume) extension if not specified.</i>"));
    noteLabel->setWordWrap(true);
    mainLayout->addWidget(noteLabel);

    mainLayout->addStretch();

    setLayout(mainLayout);
}

void CVolumeLocationPage::OnBrowse()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Select Volume Location"),
        m_pVolumePath->text().isEmpty() ? QString() : m_pVolumePath->text(),
        tr("Private Volume Files (*.pv);;All Files (*)"));

    if (!path.isEmpty()) {
        path = path.replace("/", "\\");
        if (!path.endsWith(".pv", Qt::CaseInsensitive))
            path += ".pv";
        m_pVolumePath->setText(path);
    }
}

int CVolumeLocationPage::nextId() const
{
    return CVolumeWizard::Page_Encryption;
}

bool CVolumeLocationPage::isComplete() const
{
    return !m_pVolumePath->text().trimmed().isEmpty();
}

bool CVolumeLocationPage::validatePage()
{
    QString path = m_pVolumePath->text().trimmed().replace("/", "\\");

    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("Volume Creation"), tr("Please specify a location for the volume file."));
        return false;
    }

    // Add .pv extension if not present
    if (!path.endsWith(".pv", Qt::CaseInsensitive))
        path += ".pv";

    m_pVolumePath->setText(path);

    // Check if file already exists
    if (QFile::exists(path)) {
        if (QMessageBox::question(this, tr("Volume Creation"),
            tr("A file already exists at the specified location. Do you want to overwrite it?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return false;
    }

    // Check if parent directory exists
    QFileInfo fileInfo(path);
    QDir parentDir = fileInfo.dir();
    if (!parentDir.exists()) {
        QMessageBox::warning(this, tr("Volume Creation"),
            tr("The specified directory does not exist. Please choose a valid location."));
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumeEncryptionPage
//

CVolumeEncryptionPage::CVolumeEncryptionPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Encryption Options"));
    setSubTitle(tr("Select the encryption algorithm for your volume."));

    QVBoxLayout *layout = new QVBoxLayout;

    m_pTopLabel = new QLabel(tr(
        "Choose the encryption algorithm to protect your data. All available algorithms provide strong security.\n\n"
        "Cascaded ciphers encrypt data multiple times with different algorithms, providing additional security "
        "at the cost of slower read/write performance."));
    m_pTopLabel->setWordWrap(true);
    layout->addWidget(m_pTopLabel);

    layout->addSpacing(20);

    QHBoxLayout *cipherLayout = new QHBoxLayout;
    QLabel* cipherLabel = new QLabel(tr("Encryption Algorithm:"));
    cipherLayout->addWidget(cipherLabel);

    m_pCipherCombo = new QComboBox;
    m_pCipherCombo->addItem(tr("AES"), 0);
    m_pCipherCombo->addItem(tr("Twofish"), 1);
    m_pCipherCombo->addItem(tr("Serpent"), 2);
    m_pCipherCombo->addItem(tr("AES-Twofish (Cascaded)"), 3);
    m_pCipherCombo->addItem(tr("Twofish-Serpent (Cascaded)"), 4);
    m_pCipherCombo->addItem(tr("Serpent-AES (Cascaded)"), 5);
    m_pCipherCombo->addItem(tr("AES-Twofish-Serpent (Cascaded)"), 6);
    m_pCipherCombo->setCurrentIndex(0);
    connect(m_pCipherCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CVolumeEncryptionPage::OnCipherChanged);
    cipherLayout->addWidget(m_pCipherCombo, 1);

    layout->addLayout(cipherLayout);
    registerField("cipherIndex", m_pCipherCombo, "currentIndex", "currentIndexChanged");

    layout->addSpacing(10);

    m_pCipherInfo = new QLabel;
    m_pCipherInfo->setWordWrap(true);
    m_pCipherInfo->setStyleSheet("QLabel { background-color: palette(alternate-base); padding: 10px; border-radius: 5px; }");
    layout->addWidget(m_pCipherInfo);

    OnCipherChanged(0);

    layout->addStretch();

    setLayout(layout);
}

void CVolumeEncryptionPage::OnCipherChanged(int index)
{
    QString info;
    switch (index) {
    case 0:
        info = tr("<b>AES (Advanced Encryption Standard)</b><br/>"
                  "256-bit key, widely adopted and thoroughly analyzed. "
                  "Recommended for most users due to excellent performance and proven security.");
        break;
    case 1:
        info = tr("<b>Twofish</b><br/>"
                  "256-bit key, finalist in the AES competition. "
                  "Highly secure with no known practical attacks.");
        break;
    case 2:
        info = tr("<b>Serpent</b><br/>"
                  "256-bit key, finalist in the AES competition. "
                  "Designed with a large security margin, slower but very conservative design.");
        break;
    case 3:
        info = tr("<b>AES-Twofish (Cascaded)</b><br/>"
                  "Data is first encrypted with Twofish, then with AES. "
                  "Provides defense-in-depth if one cipher is compromised.");
        break;
    case 4:
        info = tr("<b>Twofish-Serpent (Cascaded)</b><br/>"
                  "Data is first encrypted with Serpent, then with Twofish. "
                  "Combines two AES competition finalists.");
        break;
    case 5:
        info = tr("<b>Serpent-AES (Cascaded)</b><br/>"
                  "Data is first encrypted with AES, then with Serpent. "
                  "Combines proven security with conservative design.");
        break;
    case 6:
        info = tr("<b>AES-Twofish-Serpent (Cascaded)</b><br/>"
                  "Triple encryption using all three ciphers. "
                  "Maximum security at the cost of performance.");
        break;
    }
    m_pCipherInfo->setText(info);
}

int CVolumeEncryptionPage::nextId() const
{
    return CVolumeWizard::Page_Size;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumeSizePage
//

CVolumeSizePage::CVolumeSizePage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Volume Size"));
    setSubTitle(tr("Specify the size of the encrypted volume."));

    QVBoxLayout *layout = new QVBoxLayout;

    m_pTopLabel = new QLabel(tr(
        "Enter the desired size for your encrypted volume. The volume file will be created with this exact size.\n\n"
        "The minimum size is 256 MB. For general use, 2 GB or more is recommended."));
    m_pTopLabel->setWordWrap(true);
    layout->addWidget(m_pTopLabel);

    layout->addSpacing(20);

    QHBoxLayout *sizeLayout = new QHBoxLayout;
    QLabel* sizeLabel = new QLabel(tr("Volume Size:"));
    sizeLayout->addWidget(sizeLabel);

    m_pSizeEdit = new QLineEdit;
    m_pSizeEdit->setText("2048");
    m_pSizeEdit->setMaximumWidth(150);
    connect(m_pSizeEdit, &QLineEdit::textChanged, this, &CVolumeSizePage::OnSizeChanged);
    sizeLayout->addWidget(m_pSizeEdit);
    registerField("volumeSize", m_pSizeEdit);

    m_pSizeUnit = new QComboBox;
    m_pSizeUnit->addItem(tr("KB"), 0);
    m_pSizeUnit->addItem(tr("MB"), 1);
    m_pSizeUnit->addItem(tr("GB"), 2);
    m_pSizeUnit->setCurrentIndex(1); // Default to MB
    connect(m_pSizeUnit, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &CVolumeSizePage::OnSizeChanged);
    sizeLayout->addWidget(m_pSizeUnit);
    registerField("sizeUnit", m_pSizeUnit, "currentIndex", "currentIndexChanged");

    sizeLayout->addStretch();
    layout->addLayout(sizeLayout);

    m_pSizeInfo = new QLabel;
    m_pSizeInfo->setWordWrap(true);
    layout->addWidget(m_pSizeInfo);

    OnSizeChanged();

    layout->addStretch();

    setLayout(layout);
}

void CVolumeSizePage::initializePage()
{
    OnSizeChanged();
}

void CVolumeSizePage::OnSizeChanged()
{
    bool ok;
    quint64 size = m_pSizeEdit->text().toULongLong(&ok);
    int unit = m_pSizeUnit->currentIndex();

    if (!ok || size == 0) {
        m_pSizeInfo->setText(tr("<span style='color:red;'>Please enter a valid size.</span>"));
        return;
    }

    quint64 sizeInBytes;
    switch (unit) {
    case 0: // KB
        sizeInBytes = size * 1024ULL;
        break;
    case 1: // MB
        sizeInBytes = size * 1024ULL * 1024ULL;
        break;
    case 2: // GB
        sizeInBytes = size * 1024ULL * 1024ULL * 1024ULL;
        break;
    default:
        sizeInBytes = size * 1024ULL * 1024ULL;
    }

    QString sizeStr = FormatSize(sizeInBytes);

    if (sizeInBytes < 128ULL * 1024ULL * 1024ULL) {
        m_pSizeInfo->setText(tr("<span style='color:red;'>Size too small. Minimum size is 256 MB.</span>"));
    } else if (sizeInBytes < 256ULL * 1024ULL * 1024ULL) {
        m_pSizeInfo->setText(tr("<span style='color:orange;'>Warning: Size is below recommended minimum of 256 MB.</span><br/>Volume size: %1").arg(sizeStr));
    } else if (sizeInBytes < 2ULL * 1024ULL * 1024ULL * 1024ULL) {
        m_pSizeInfo->setText(tr("Volume size: %1<br/><i>Tip: 2 GB or more is recommended for general use.</i>").arg(sizeStr));
    } else {
        m_pSizeInfo->setText(tr("Volume size: %1").arg(sizeStr));
    }

    emit completeChanged();
}

int CVolumeSizePage::nextId() const
{
    return CVolumeWizard::Page_Password;
}

bool CVolumeSizePage::isComplete() const
{
    bool ok;
    quint64 size = m_pSizeEdit->text().toULongLong(&ok);
    if (!ok || size == 0)
        return false;

    int unit = m_pSizeUnit->currentIndex();
    quint64 sizeInBytes;
    switch (unit) {
    case 0: sizeInBytes = size * 1024ULL; break;
    case 1: sizeInBytes = size * 1024ULL * 1024ULL; break;
    case 2: sizeInBytes = size * 1024ULL * 1024ULL * 1024ULL; break;
    default: sizeInBytes = size * 1024ULL * 1024ULL;
    }

    return sizeInBytes >= 128ULL * 1024ULL * 1024ULL;
}

bool CVolumeSizePage::validatePage()
{
    bool ok;
    quint64 size = m_pSizeEdit->text().toULongLong(&ok);
    int unit = m_pSizeUnit->currentIndex();

    quint64 sizeInBytes;
    switch (unit) {
    case 0: sizeInBytes = size * 1024ULL; break;
    case 1: sizeInBytes = size * 1024ULL * 1024ULL; break;
    case 2: sizeInBytes = size * 1024ULL * 1024ULL * 1024ULL; break;
    default: sizeInBytes = size * 1024ULL * 1024ULL;
    }

    if (sizeInBytes < 128ULL * 1024ULL * 1024ULL) {
        QMessageBox::warning(this, tr("Volume Creation"),
            tr("The volume size must be at least 256 MB."));
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumePasswordPage
//

CVolumePasswordPage::CVolumePasswordPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Volume Password"));
    setSubTitle(tr("Create a strong password to protect your volume."));

    QVBoxLayout *layout = new QVBoxLayout;

    m_pTopLabel = new QLabel(tr(
        "Choose a strong password for your encrypted volume. The security of your data depends on "
        "the strength of this password.\n\n"
        "A good password should:\n"
        "- Be at least 20 characters long\n"
        "- Include a mix of letters, numbers, and symbols\n"
        "- Not be based on dictionary words or personal information"));
    m_pTopLabel->setWordWrap(true);
    layout->addWidget(m_pTopLabel);

    layout->addSpacing(15);

    QGridLayout *pwLayout = new QGridLayout;

    pwLayout->addWidget(new QLabel(tr("Password:")), 0, 0);
    m_pPassword = new QLineEdit;
    m_pPassword->setEchoMode(QLineEdit::Password);
    connect(m_pPassword, &QLineEdit::textChanged, this, &CVolumePasswordPage::OnPasswordChanged);
    pwLayout->addWidget(m_pPassword, 0, 1);
    registerField("password", m_pPassword);

    pwLayout->addWidget(new QLabel(tr("Confirm:")), 1, 0);
    m_pConfirmPassword = new QLineEdit;
    m_pConfirmPassword->setEchoMode(QLineEdit::Password);
    connect(m_pConfirmPassword, &QLineEdit::textChanged, this, &CVolumePasswordPage::OnPasswordChanged);
    pwLayout->addWidget(m_pConfirmPassword, 1, 1);

    m_pShowPassword = new QCheckBox(tr("Show password"));
    connect(m_pShowPassword, &QCheckBox::toggled, this, &CVolumePasswordPage::OnShowPassword);
    pwLayout->addWidget(m_pShowPassword, 2, 1);

    layout->addLayout(pwLayout);

    m_pStrengthLabel = new QLabel;
    m_pStrengthLabel->setWordWrap(true);
    layout->addWidget(m_pStrengthLabel);

    OnPasswordChanged();

    layout->addStretch();

    setLayout(layout);
}

void CVolumePasswordPage::OnShowPassword()
{
    QLineEdit::EchoMode mode = m_pShowPassword->isChecked() ? QLineEdit::Normal : QLineEdit::Password;
    m_pPassword->setEchoMode(mode);
    m_pConfirmPassword->setEchoMode(mode);
}

void CVolumePasswordPage::OnPasswordChanged()
{
    QString pw = m_pPassword->text();
    QString confirm = m_pConfirmPassword->text();

    if (pw.isEmpty()) {
        m_pStrengthLabel->setText("");
        emit completeChanged();
        return;
    }

    QString info;

    // Check password strength
    int length = pw.length();
    if (length > 128) {
        info = tr("<span style='color:red;'>Password is too long. Maximum length is 128 characters.</span>");
    } else if (length < 8) {
        info = tr("<span style='color:red;'>Password is very weak. Please use at least 8 characters.</span>");
    } else if (length < 12) {
        info = tr("<span style='color:orange;'>Password is weak. Consider using more characters.</span>");
    } else if (length < 20) {
        info = tr("<span style='color:orange;'>Password strength: Fair. 20+ characters recommended.</span>");
    } else {
        info = tr("<span style='color:green;'>Password strength: Good</span>");
    }

    if (!confirm.isEmpty() && pw != confirm) {
        info += tr("<br/><span style='color:red;'>Passwords do not match!</span>");
    } else if (!confirm.isEmpty() && pw == confirm) {
        info += tr("<br/><span style='color:green;'>Passwords match.</span>");
    }

    m_pStrengthLabel->setText(info);
    emit completeChanged();
}

int CVolumePasswordPage::nextId() const
{
    return CVolumeWizard::Page_Cost;
}

bool CVolumePasswordPage::isComplete() const
{
    QString pw = m_pPassword->text();
    QString confirm = m_pConfirmPassword->text();

    if (pw.isEmpty() || confirm.isEmpty())
        return false;
    if (pw != confirm)
        return false;
    if (pw.length() > 128)
        return false;

    return true;
}

bool CVolumePasswordPage::validatePage()
{
    QString pw = m_pPassword->text();
    QString confirm = m_pConfirmPassword->text();

    if (pw != confirm) {
        QMessageBox::warning(this, tr("Volume Creation"), tr("Passwords do not match!"));
        return false;
    }

    if (pw.length() > 128) {
        QMessageBox::warning(this, tr("Volume Creation"),
            tr("The password is constrained to a maximum length of 128 characters.\n"
               "This length permits approximately 384 bits of entropy with a passphrase composed of actual English words, "
               "increases to 512 bits with the application of Leet (L337) speak modifications, "
               "and exceeds 768 bits when composed of entirely random printable ASCII characters."));
        return false;
    }

    if (pw.length() < 20) {
        if (QMessageBox::warning(this, tr("Volume Creation"),
            tr("WARNING: Short passwords are easy to crack using brute force techniques!\n\n"
               "It is recommended to choose a password consisting of 20 or more characters. "
               "Are you sure you want to use a short password?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumeCostPage
//

CVolumeCostPage::CVolumeCostPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Argon2 Key Derivation"));
    setSubTitle(tr("Optionally configure advanced key derivation settings."));

    QVBoxLayout *layout = new QVBoxLayout;

    m_pTopLabel = new QLabel(tr(
        "Argon2 is a modern, memory-hard key derivation function that provides strong protection against "
        "brute-force and specialized hardware attacks. The cost parameter controls the computational "
        "resources required for key derivation.\n"
        "A higher cost value increases security by making attacks more expensive, but also increases "
        "the time required to mount the volume.\n"
        "For most users, the default setting (custom cost disabled) provides adequate security. "
        "Enable custom cost only if you need additional protection or have specific security requirements."));
    m_pTopLabel->setWordWrap(true);
    layout->addWidget(m_pTopLabel);

    layout->addSpacing(20);

    m_pEnableArgon2 = new QCheckBox(tr("Use custom Argon2 cost parameter"));
    m_pEnableArgon2->setChecked(false);
    connect(m_pEnableArgon2, &QCheckBox::toggled, this, &CVolumeCostPage::OnEnableArgon2);
    layout->addWidget(m_pEnableArgon2);
    registerField("useArgon2", m_pEnableArgon2);

    QHBoxLayout *pimLayout = new QHBoxLayout;
    pimLayout->addSpacing(20);
    QLabel* pimLabel = new QLabel(tr("Cost Parameter:"));
    pimLayout->addWidget(pimLabel);

    m_pArgon2Cost = new QSpinBox;
    m_pArgon2Cost->setMinimum(1);
    m_pArgon2Cost->setMaximum(100);
    m_pArgon2Cost->setValue(12);
    m_pArgon2Cost->setEnabled(false);
    pimLayout->addWidget(m_pArgon2Cost);
    registerField("costArgon2", m_pArgon2Cost);

    pimLayout->addStretch();
    layout->addLayout(pimLayout);

    m_pCostInfo = new QLabel(tr(
        "<i>Note: Higher cost values significantly increase mount time. The cost parameter determines "
        "memory usage and iterations for the Argon2 algorithm. Remember your cost setting - "
        "without it, you cannot access your data!</i>"));
    m_pCostInfo->setWordWrap(true);
    m_pCostInfo->setEnabled(false);
    layout->addWidget(m_pCostInfo);

    layout->addStretch();
    setLayout(layout);
}

void CVolumeCostPage::OnEnableArgon2(bool enabled)
{
    m_pArgon2Cost->setEnabled(enabled);
    m_pCostInfo->setEnabled(enabled);
}

int CVolumeCostPage::nextId() const
{
    return CVolumeWizard::Page_Summary;
}

bool CVolumeCostPage::isComplete() const
{
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumeSummaryPage
//

CVolumeSummaryPage::CVolumeSummaryPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Summary"));
    setSubTitle(tr("Review your settings before creating the encrypted volume."));

    QVBoxLayout *layout = new QVBoxLayout;

    m_pSummaryLabel = new QLabel;
    m_pSummaryLabel->setWordWrap(true);
    m_pSummaryLabel->setStyleSheet("QLabel { background-color: palette(alternate-base); padding: 15px; border-radius: 5px; }");
    layout->addWidget(m_pSummaryLabel);

    layout->addSpacing(20);

    QLabel* noteLabel = new QLabel(tr(
        "<b>Important:</b> After clicking Finish, the volume will be created. "
        "This may take some time depending on the volume size.\n\n"
        "Make sure to remember your password (and PIM if enabled). "
        "Without the correct password, your data cannot be recovered!"));
    noteLabel->setWordWrap(true);
    layout->addWidget(noteLabel);

    layout->addStretch();

    setLayout(layout);
}

void CVolumeSummaryPage::initializePage()
{
    QString volumePath = field("volumePath").toString();

    int cipherIndex = field("cipherIndex").toInt();
    QStringList ciphers = { "AES", "Twofish", "Serpent", "AES-Twofish", "Twofish-Serpent", "Serpent-AES", "AES-Twofish-Serpent" };
    QString cipher = cipherIndex >= 0 && cipherIndex < ciphers.size() ? ciphers[cipherIndex] : "AES";

    quint64 size = field("volumeSize").toULongLong();
    int unit = field("sizeUnit").toInt();
    quint64 sizeInBytes;
    switch (unit) {
    case 0: sizeInBytes = size * 1024ULL; break;
    case 1: sizeInBytes = size * 1024ULL * 1024ULL; break;
    case 2: sizeInBytes = size * 1024ULL * 1024ULL * 1024ULL; break;
    default: sizeInBytes = size * 1024ULL * 1024ULL;
    }
    QString sizeStr = FormatSize(sizeInBytes);

    bool enablePIM = field("enablePIM").toBool();
    int pimValue = field("pimValue").toInt();

    QString summary = tr(
        "<table>"
        "<tr><td><b>Volume Location:</b></td><td>%1</td></tr>"
        "<tr><td><b>Encryption Algorithm:</b></td><td>%2</td></tr>"
        "<tr><td><b>Volume Size:</b></td><td>%3</td></tr>"
        "<tr><td><b>PIM:</b></td><td>%4</td></tr>"
        "</table>")
        .arg(volumePath)
        .arg(cipher)
        .arg(sizeStr)
        .arg(enablePIM ? QString::number(pimValue) : tr("Default (disabled)"));

    m_pSummaryLabel->setText(summary);
}

int CVolumeSummaryPage::nextId() const
{
    return -1;
}
