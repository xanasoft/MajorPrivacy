#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_AccessRuleWnd.h"
#include "../Core/Access/AccessRule.h"
#include "../Core/Programs/ProgramItem.h"


class CAccessRuleWnd : public QDialog
{
	Q_OBJECT

public:
	CAccessRuleWnd(const CAccessRulePtr& pRule, QSet<CProgramItemPtr> Items, const QString& VolumeRoot = QString(), const QString& VolumeImage = QString(), QWidget *parent = Q_NULLPTR);
	~CAccessRuleWnd();

	static QColor GetActionColor(EAccessRuleType Action);
	static QColor GetStatusColor(EEventStatus Status);
	static QString GetStatusStr(EEventStatus Status);

signals:
	void Closed();

private slots:

	void OnNameChanged(const QString& Text);
	void OnPickProgram();
	void OnPathChanged();
	void OnUserChanged();
	void OnProgramChanged();
	void OnProgramPathChanged();
	void OnActionChanged();

	void BrowseFolder();
	void BrowseFile();
	void EditScript();

	void OnSaveAndClose();
	bool OnSave();

protected:
	void closeEvent(QCloseEvent* e);

	bool AddProgramItem(const CProgramItemPtr& pItem);

	bool Save();

	CAccessRulePtr m_pRule;
	QList<CProgramItemPtr> m_Items;

	QString m_VolumeRoot;
	QString m_VolumeImage;

	QString m_Script;

private:
	Ui::AccessRuleWnd ui;

	bool m_HoldProgramPath = false;

	void TryMakeName();
	bool m_NameHold = true;
	bool m_NameChanged = false;
};
