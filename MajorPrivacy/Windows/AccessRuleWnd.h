#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_AccessRuleWnd.h"
#include "../Core/Access/AccessRule.h"
#include "../Core/Programs/ProgramItem.h"


class CAccessRuleWnd : public QDialog
{
	Q_OBJECT

public:
	CAccessRuleWnd(const CAccessRulePtr& pRule, QSet<CProgramItemPtr> Items, QWidget *parent = Q_NULLPTR);
	~CAccessRuleWnd();

signals:
	void Closed();

private slots:

	void OnProgramChanged();

	void OnSaveAndClose();

protected:
	void closeEvent(QCloseEvent* e);

	bool Save();

	CAccessRulePtr m_pRule;
	QList<CProgramItemPtr> m_Items;

private:
	Ui::AccessRuleWnd ui;

};
