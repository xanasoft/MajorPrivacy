#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_ScriptWindow.h"
#include "../Core/Programs/ProgramItem.h"
#include "../../Library/Status.h"
#include "./Common/QtFlexGuid.h"


class CScriptWindow : public QDialog
{
	Q_OBJECT

public:
	CScriptWindow(const QFlexGuid& Guid, EScriptTypes Type, QWidget *parent = Q_NULLPTR);
	~CScriptWindow();

	template<typename T>
	void SetSaver(T Saver) { m_Save = std::bind(Saver, std::placeholders::_1, std::placeholders::_2); }

	void SetScript(const QString& Script);
	QString GetScript() const;

private slots:

	void OnImport();
	void OnExport();
	void OnTest();
	void OnCleanUp();

	void OnScriptChanged();
	void OnApply();
	void OnSave();

protected:
	void timerEvent(QTimerEvent* pEvent);
	int	m_uTimerID;

	void UpdateLog();

	std::function<STATUS(const QString& Script, bool bApply)> m_Save;

	QFlexGuid m_Guid; 
	EScriptTypes m_Type = EScriptTypes::eNone;
	quint32 m_LastId = 0;

private:
	Ui::ScriptWindow ui;
	QToolBar* m_pToolBar;
	class CCodeEdit* m_pCodeEdit;
};
