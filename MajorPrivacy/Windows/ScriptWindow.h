#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_ScriptWindow.h"
#include "../Core/Programs/ProgramItem.h"


class CScriptWindow : public QDialog
{
	Q_OBJECT

public:
	CScriptWindow(QWidget *parent = Q_NULLPTR);
	~CScriptWindow();

	void SetScript(const QString& Script) { ui.txtScript->setPlainText(Script); }
	QString GetScript() const { return ui.txtScript->toPlainText(); }

private slots:

protected:

private:
	Ui::ScriptWindow ui;
};
