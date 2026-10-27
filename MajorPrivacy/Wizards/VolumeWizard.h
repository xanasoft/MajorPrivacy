#pragma once

#include <QWizard>

#include "../../Library/Crypto/SecurePassword.h"
#include "../Windows/KeyFilesWnd.h"
// IMBOX_SLOTS_DEFAULT / IMBOX_SLOTS_MAX, the header format's own limits
#include "../../ImBox/ImBox.h"

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QComboBox;
class QSpinBox;
class QToolButton;
QT_END_NAMESPACE

class CVolumeWizard : public QWizard
{
    Q_OBJECT

public:
    //
    // Page_Properties is last so that the existing ids do not move; the order
    // the pages are shown in is decided by nextId(), not by this list.
    //
    enum { Page_Intro, Page_Location, Page_Cipher, Page_Cost, Page_Size, Page_Encryption, Page_Password, Page_Summary, Page_Properties };

    CVolumeWizard(QWidget *parent = nullptr);
    ~CVolumeWizard();

    static bool ShowWizard(QWidget* parent = nullptr);

    QString GetVolumePath() const;
    CSecurePassword GetPassword() const;
    quint64 GetImageSize() const;
    QString GetCipher() const;
    QString GetFileSystem() const;
    int GetKdf() const;

    // How many key slots to lay out in the header, 0 for none.
    int GetSlotCount() const;

private slots:
    void showHelp();
};

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumeIntroPage
//

class CVolumeIntroPage : public QWizardPage
{
    Q_OBJECT

public:
    CVolumeIntroPage(QWidget *parent = nullptr);

    int nextId() const override;

private:
    QLabel* m_pLabel;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumeLocationPage
//

class CVolumeLocationPage : public QWizardPage
{
    Q_OBJECT

public:
    CVolumeLocationPage(QWidget *parent = nullptr);

    int nextId() const override;
    bool isComplete() const override;
    bool validatePage() override;

private slots:
    void OnBrowse();

private:
    QLabel* m_pTopLabel;
    QLineEdit* m_pVolumePath;
    QToolButton* m_pBrowseBtn;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumeEncryptionPage (combined cipher + KDF)
//

class CVolumeEncryptionPage : public QWizardPage
{
    Q_OBJECT

public:
    CVolumeEncryptionPage(QWidget *parent = nullptr);

    int nextId() const override;

private slots:
    void OnCipherChanged(int index);
    void OnKdfChanged(int index);

private:
    QComboBox* m_pCipherCombo;
    QLabel* m_pCipherInfo;
    QComboBox* m_pKdfCombo;
    QLabel* m_pKdfInfo;
};

#if 0
//////////////////////////////////////////////////////////////////////////////////////////
// CVolumeCipherPage
//

class CVolumeCipherPage : public QWizardPage
{
    Q_OBJECT

public:
    CVolumeCipherPage(QWidget *parent = nullptr);

    int nextId() const override;

private slots:
    void OnCipherChanged(int index);

private:
    QLabel* m_pTopLabel;
    QComboBox* m_pCipherCombo;
    QLabel* m_pCipherInfo;
};
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumeSizePage
//

class CVolumeSizePage : public QWizardPage
{
    Q_OBJECT

public:
    CVolumeSizePage(QWidget *parent = nullptr);

    void initializePage() override;
    int nextId() const override;
    bool isComplete() const override;
    bool validatePage() override;

private slots:
    void OnSizeChanged();
    void OnFileSystemChanged(int index);

private:
    QLabel* m_pTopLabel;
    QLineEdit* m_pSizeEdit;
    QComboBox* m_pSizeUnit;
    QComboBox* m_pFileSystem;
    QLabel* m_pSizeInfo;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumePasswordPage
//

class CPasswordStrengthWidget;

class CVolumePasswordPage : public QWizardPage
{
    Q_OBJECT

public:
    CVolumePasswordPage(QWidget *parent = nullptr);

    void initializePage() override;
    void cleanupPage() override;
    int nextId() const override;
    bool isComplete() const override;
    bool validatePage() override;

    CSecurePassword GetPassword() const;

private slots:
    void OnShowPassword();
    void OnPasswordChanged();
    void OnSecurePasswordEntry();
    void OnKeyFiles();

private:
    void UpdateKeyFilesButton();

    QLabel* m_pTopLabel;
    QLineEdit* m_pPassword;
    QLineEdit* m_pConfirmPassword;
    QToolButton* m_pShowPasswordBtn;
    QToolButton* m_pSecureEntryBtn;
    QToolButton* m_pKeyFilesBtn;
    QLabel* m_pStrengthLabel;
    CPasswordStrengthWidget* m_pStrengthWidget;

    // Secure password entry state
    CSecurePassword m_SecurePassword;

    // Keyfiles, and the finished password validatePage builds from them
    SKeyFiles m_KeyFiles;
    CSecurePassword m_MixedPassword;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumePropertiesPage
//
// Things about the volume itself rather than about the encryption. Only the key
// slot count so far; the page exists because that list is going to grow.
//

class CVolumePropertiesPage : public QWizardPage
{
    Q_OBJECT

public:
    CVolumePropertiesPage(QWidget *parent = nullptr);

    int nextId() const override;
    int GetSlotCount() const;

private slots:
    void OnSlotCountChanged(int value);

private:
    QLabel* m_pTopLabel;
    QSpinBox* m_pSlotCount;
    QLabel* m_pSlotInfo;
};

#if 0
//////////////////////////////////////////////////////////////////////////////////////////
// CVolumeCostPage
//

class CVolumeCostPage : public QWizardPage
{
    Q_OBJECT

public:
    CVolumeCostPage(QWidget *parent = nullptr);

    int nextId() const override;
    bool isComplete() const override;

private slots:
    void OnKdfChanged(int index);

private:
    QLabel* m_pTopLabel;
	QComboBox* m_pKdfCombo;
    QLabel* m_pKdfInfo;
};
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumeSummaryPage
//

class CVolumeSummaryPage : public QWizardPage
{
    Q_OBJECT

public:
    CVolumeSummaryPage(QWidget *parent = nullptr);

    void initializePage() override;
    int nextId() const override;

private:
    QLabel* m_pSummaryLabel;
};
