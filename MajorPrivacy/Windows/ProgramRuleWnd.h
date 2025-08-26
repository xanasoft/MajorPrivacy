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

	static QColor GetActionColor(EExecRuleType Action);
	static QColor GetAuthorityColor(KPH_VERIFY_AUTHORITY Authority);
	static QColor GetRoleColor(EExecLogRole Role);

signals:
	void Closed();

private slots:

	void OnNameChanged(const QString& Text);
	void OnPickProgram();
	void OnProgramChanged();
	void OnProgramPathChanged();
	void OnActionChanged();

	void OnSaveAndClose();
	void OnAddCollection();

protected:
	void closeEvent(QCloseEvent* e);

	bool AddProgramItem(const CProgramItemPtr& pItem);

	bool Save();

	CProgramRulePtr m_pRule;
	QList<CProgramItemPtr> m_Items;

private:
	Ui::ProgramRuleWnd ui;
	class CWinboxMultiCombo* m_pCollections;

	bool m_HoldProgramPath = false;

	void TryMakeName();
	bool m_NameHold = true;
	bool m_NameChanged = false;
};
