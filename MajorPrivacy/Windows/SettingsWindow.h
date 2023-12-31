#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_SettingsWindow.h"
#include "../../MiscHelpers/Common/SettingsWidgets.h"


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
	void OnTab();

	void OnSetTree();

protected:
	void closeEvent(QCloseEvent *e);

	bool eventFilter(QObject *watched, QEvent *e);

	void OnTab(QWidget* pTab);

	void	LoadSettings();
	void	SaveSettings();

	bool	m_bRebuildUI;
	bool	m_HoldChange;

private:

	Ui::SettingsWindow ui;
};
