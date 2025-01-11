#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_ProgramPicker.h"
#include "../Core/Programs/ProgramItem.h"


class CProgramPicker : public QDialog
{
	Q_OBJECT

public:
	CProgramPicker(CProgramItemPtr pProgram, const QList<CProgramItemPtr>& Items, QWidget *parent = Q_NULLPTR);
	~CProgramPicker();

	CProgramItemPtr GetProgram() { return m_pProgram; }

private slots:
	void SetFilter(const QRegularExpression& Exp, int iOptions, int Column);
	void OnAll(bool bChecked);
	void OnAdd();
	void OnPick();

protected:

	void LoadList();

	CProgramItemPtr m_pProgram;
	QList<CProgramItemPtr> m_Items;

private:
	Ui::ProgramPicker ui;
	QRegularExpression m_FilterExp;
};
