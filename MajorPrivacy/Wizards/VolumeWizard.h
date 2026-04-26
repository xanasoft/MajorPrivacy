#pragma once

#include <QWizard>

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
    enum { Page_Intro, Page_Location, Page_Cipher, Page_Cost, Page_Size, Page_Encryption, Page_Password, Page_Summary };

    CVolumeWizard(QWidget *parent = nullptr);
    ~CVolumeWizard();

    static bool ShowWizard(QWidget* parent = nullptr);

    QString GetVolumePath() const;
    QString GetPassword() const;
    quint64 GetImageSize() const;
    QString GetCipher() const;
    QString GetFileSystem() const;
    int GetKdf() const;

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

private slots:
    void OnShowPassword();
    void OnPasswordChanged();

private:
    QLabel* m_pTopLabel;
    QLineEdit* m_pPassword;
    QLineEdit* m_pConfirmPassword;
    QCheckBox* m_pShowPassword;
    QLabel* m_pStrengthLabel;
    CPasswordStrengthWidget* m_pStrengthWidget;
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
