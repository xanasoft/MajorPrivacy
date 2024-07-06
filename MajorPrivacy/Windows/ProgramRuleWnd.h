#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_ProgramRuleWnd.h"
#include "../Core/Programs/ProgramRule.h"
#include "../Core/Programs/ProgramItem.h"


class CProgramRuleWnd : public QDialog
{
	Q_OBJECT

public:
	CProgramRuleWnd(const CProgramRulePtr& pRule, QSet<CProgramItemPtr> Items, QWidget *parent = Q_NULLPTR);
	~CProgramRuleWnd();

signals:
	void Closed();

private slots:

	void OnProgramChanged();

	void OnSaveAndClose();

protected:
	void closeEvent(QCloseEvent* e);

	bool Save();

	CProgramRulePtr m_pRule;
	QList<CProgramItemPtr> m_Items;

private:
	Ui::ProgramRuleWnd ui;

};
