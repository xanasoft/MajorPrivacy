#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_FirewallRuleWnd.h"
#include "../Core/Network/FwRule.h"
#include "../Core/Programs/ProgramItem.h"


class CFirewallRuleWnd : public QDialog
{
	Q_OBJECT

public:
	CFirewallRuleWnd(const CFwRulePtr& pRule, QSet<CProgramItemPtr> Items, QWidget *parent = Q_NULLPTR);
	~CFirewallRuleWnd();

signals:
	void Closed();

private slots:

	void OnProgramChanged();
	void OnProfilesChanged();
	void OnInterfacesChanged();
	void OnDirectionChanged();
	void OnProtocolChanged();

	void OnLocalIPChanged();
	void OnLocalIPEdit();
	void OnLocalIPSet();

	void OnRemoteIPChanged();
	void OnRemoteIPEdit();
	void OnRemoteIPSet();

	void OnSaveAndClose();

protected:
	void closeEvent(QCloseEvent* e);

	void UpdatePorts();
	bool ValidateAddress(const QString& Address, bool bRemote);

	bool Save();

	CFwRulePtr m_pRule;
	QList<CProgramItemPtr> m_Items;

private:
	Ui::FirewallRuleWnd ui;

};
