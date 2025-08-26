#pragma once

#include <QWizard>

QT_BEGIN_NAMESPACE
class QCheckBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QPlainTextEdit;
QT_END_NAMESPACE

#define SETUP_LVL_1 1
#define SETUP_LVL_CURRENT SETUP_LVL_1

class CSetupWizard : public QWizard
{
    Q_OBJECT

public:
    enum { Page_Intro, Page_Certificate, Page_Exec, Page_Res, Page_Net, Page_Config, Page_System, Page_Update, Page_Finish };

    CSetupWizard(int iOldLevel = 0, QWidget *parent = nullptr);
    ~CSetupWizard();

    static bool ShowWizard(int iOldLevel = 0);

protected:
    friend class CConfigPage;
    class CPrivateKey* m_pPrivateKey = nullptr;

private slots:
    void showHelp();
};

//////////////////////////////////////////////////////////////////////////////////////////
// CIntroPage
// 

class CIntroPage : public QWizardPage
{
    Q_OBJECT

public:
    CIntroPage(QWidget *parent = nullptr);

    int nextId() const override;
    bool isComplete() const override;

private:
    //QLabel* m_pLabel;
    //QRadioButton *m_pPersonal;
    //QRadioButton *m_pBusiness;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CCertificatePage
// 

class CCertificatePage : public QWizardPage
{
    Q_OBJECT

public:
    CCertificatePage(int iOldLevel, QWidget *parent = nullptr);

    void initializePage() override;
    int nextId() const override;
    bool isComplete() const override;
    bool validatePage() override;

private slots:
    void OnCertData(const QByteArray& Certificate, const QVariantMap& Params);

private:
    QLabel* m_pTopLabel;
    QPlainTextEdit* m_pCertificate;
    QLineEdit* m_pSerial;
    QCheckBox* m_pEvaluate;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CExecPage
// 

class CExecPage : public QWizardPage
{
    Q_OBJECT

public:
    CExecPage(QWidget* parent = nullptr);

    int nextId() const override;

private:
    QCheckBox *m_pRecord;
    QCheckBox *m_pLog;  
};

//////////////////////////////////////////////////////////////////////////////////////////
// CResPage
// 

class CResPage : public QWizardPage
{
    Q_OBJECT

public:
    CResPage(QWidget* parent = nullptr);

    int nextId() const override;

private:
    QCheckBox *m_pCollect;
    QCheckBox *m_pRecord;
    QCheckBox *m_pLog;
    QCheckBox *m_pEnum;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CNetPage
// 

class CNetPage : public QWizardPage
{
    Q_OBJECT

public:
    CNetPage(QWidget* parent = nullptr);

    int nextId() const override;

private:
    QCheckBox *m_pEnable;
    QCheckBox *m_pRestrict;
    QCheckBox *m_pProtect;
    QCheckBox *m_pCollect;
    QCheckBox *m_pRecord;
    QCheckBox *m_pLog;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CConfigPage
// 

class CConfigPage : public QWizardPage
{
    Q_OBJECT

public:
    CConfigPage(QWidget* parent = nullptr);

    int nextId() const override;
    bool validatePage() override;

private:
    QCheckBox *m_pUnloadLock;
    QCheckBox *m_pConfigLock;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CSystemPage
// 

class CSystemPage : public QWizardPage
{
    Q_OBJECT

public:
    CSystemPage(QWidget* parent = nullptr);

    int nextId() const override;

private:
    QCheckBox *m_pEncryptPageFile;
    QCheckBox *m_pNoHibernation;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CUpdatePage
// 

class CUpdatePage : public QWizardPage
{
    Q_OBJECT

public:
    CUpdatePage(QWidget *parent = nullptr);
    
    void initializePage() override;

    int nextId() const override;

private slots:
    void UpdateOptions();

private:
    QCheckBox* m_pUpdate;
    QCheckBox* m_pVersion;
    QLabel* m_pChanelInfo;
    QRadioButton* m_pStable;
    QRadioButton* m_pPreview;
    //QRadioButton* m_pInsider;
    //QLabel* m_pUpdateInfo;
    //QLabel* m_pBottomLabel;
};

//////////////////////////////////////////////////////////////////////////////////////////
// CFinishPage
// 

class CFinishPage : public QWizardPage
{
    Q_OBJECT

public:
    CFinishPage(QWidget *parent = nullptr);

    void initializePage() override;
    int nextId() const override;
    //void setVisible(bool visible) override;

private:
    QLabel *m_pLabel;
    //QCheckBox *m_pUpdate;
};

