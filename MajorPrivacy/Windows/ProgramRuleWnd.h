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
	//static QColor GetAuthorityColor(KPH_VERIFY_AUTHORITY Authority);
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
	bool OnSave();

	void OnAddCollection();

	void EditScript();

	void OnAddParentAllow();
	void OnAddParentBlock();
	void OnDelParent();
	void OnAddChildAllow();
	void OnAddChildBlock();
	void OnDelChild();

protected:
	void closeEvent(QCloseEvent* e);

	bool AddProgramItem(const CProgramItemPtr& pItem);

	bool Save();

	void LoadParentsList();
	void LoadChildrenList();
	void AddProgramToTree(QTreeWidget* pTree, bool bAllow);

	CProgramRulePtr m_pRule;
	QList<CProgramItemPtr> m_Items;

	QString m_Script;

private:
	Ui::ProgramRuleWnd ui;
	class CWinboxMultiCombo* m_pCollections;

	bool m_HoldProgramPath = false;

	void TryMakeName();
	bool m_NameHold = true;
	bool m_NameChanged = false;
};
