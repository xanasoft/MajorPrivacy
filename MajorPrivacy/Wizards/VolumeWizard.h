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
    enum { Page_Intro, Page_Location, Page_Encryption, Page_Size, Page_Password, Page_Cost, Page_Summary };

    CVolumeWizard(QWidget *parent = nullptr);
    ~CVolumeWizard();

    static bool ShowWizard(QWidget* parent = nullptr);

    QString GetVolumePath() const;
    QString GetPassword() const;
    quint64 GetImageSize() const;
    QString GetCipher() const;
    quint32 GetArgon2Cost() const;

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
// CVolumeEncryptionPage
//

class CVolumeEncryptionPage : public QWizardPage
{
    Q_OBJECT

public:
    CVolumeEncryptionPage(QWidget *parent = nullptr);

    int nextId() const override;

private slots:
    void OnCipherChanged(int index);

private:
    QLabel* m_pTopLabel;
    QComboBox* m_pCipherCombo;
    QLabel* m_pCipherInfo;
};

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

private:
    QLabel* m_pTopLabel;
    QLineEdit* m_pSizeEdit;
    QComboBox* m_pSizeUnit;
    QLabel* m_pSizeInfo;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CVolumePasswordPage
//

class CVolumePasswordPage : public QWizardPage
{
    Q_OBJECT

public:
    CVolumePasswordPage(QWidget *parent = nullptr);

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
};

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
    void OnEnableArgon2(bool enabled);

private:
    QLabel* m_pTopLabel;
    QCheckBox* m_pEnableArgon2;
    QSpinBox* m_pArgon2Cost;
    QLabel* m_pCostInfo;
};

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
