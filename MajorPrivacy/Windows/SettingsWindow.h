#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_SettingsWindow.h"
#include "../../MiscHelpers/Common/SettingsWidgets.h"
#include "../../Driver/Isolator/Support.h"


class CSettingsWindow : public CConfigDialog
{
	Q_OBJECT

public:
	CSettingsWindow(QWidget *parent = Q_NULLPTR);
	~CSettingsWindow();

	virtual void accept() {}
	virtual void reject();


signals:
	void OptionsChanged(bool bRebuildUI = false);
	void Closed();

public slots:
	void ok();
	void apply();

	void showTab(const QString& Name, bool bExclusive = false);

private slots:

	void OnDelIgnore();
	void OnFwModeChanged();
	void OnFwAuditPolicyChanged();
	void OnFwShowPopUpChanged();

	void OnChangeGUI()		{ if (!m_HoldChange) m_bRebuildUI = true; OnOptChanged(); }
	void OnIgnoreChanged()	{ if (!m_HoldChange) m_IgnoreChanged = true; OnOptChanged(); }

	void OnOptChanged();

	void OnDnsChanged();
	void OnDnsChanged2();
	void OnDnsBlockListClicked(QTreeWidgetItem* pItem, int Column);
	void OnDnsBlockDoubleClicked(QTreeWidgetItem* pItem, int Column);
	void OnAddBlockList();
	void OnDelBlockList();
	void OnDnsChanged3();

	void OnTab();

	void OnSetTree();

protected:
	void closeEvent(QCloseEvent *e);

	bool eventFilter(QObject *watched, QEvent *e);

	void	timerEvent(QTimerEvent* pEvent);
	int		m_uTimerID;

	void	OnTab(QWidget* pTab);

	void	LoadSettings();
	void	SaveSettings();

	bool	m_HoldChange = false;
	bool	m_bRebuildUI = false;

	bool	m_IgnoreChanged = false;
	bool	m_ResolverChanged = false;
	bool	m_BlockListChanged = false;

	bool	m_bFwModeChanged = false;
	bool	m_bFwAuditPolicyChanged = false;

// Support
public:
	static bool ApplyCertificate(const QByteArray &Certificate, QWidget* widget);
	static bool	SetCertificate(const QByteArray& Certificate);
	static void LoadCertificate(QString CertPath = QString());
	static bool	TryRefreshCert(QWidget* parent, QObject* receiver, const char* member);
	static bool	CertRefreshRequired();

	static QString GetCertType();
	static QColor GetCertColor();
	static QString GetCertLevel();

	static void StartEval(QWidget* parent, QObject* receiver, const char* member);


public slots:
	void CertChanged();
	void KeyChanged();
	void UpdateCert();
	void OnGetCert();
	void OnCertData(const QByteArray& Certificate, const QVariantMap& Params);
	void ApplyCert();
	void OnStartEval();

protected:
	void	InitSupport();

	bool	m_CertChanged = false;

private:

	Ui::SettingsWindow ui;
};

extern QByteArray g_Certificate;

//#include "..\Driver\Isolator\Support.h"

extern SCertInfo g_CertInfo;
extern QString g_CertName;
extern QString g_SystemHwid;

#define EVAL_MAX 3		// for UI only actual limits enforced on server
#define EVAL_DAYS 10	