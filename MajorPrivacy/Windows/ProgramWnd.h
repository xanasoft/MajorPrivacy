#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_ProgramWnd.h"
#include "../Core/Programs/ProgramItem.h"


class CProgramWnd : public QDialog
{
	Q_OBJECT

public:
	CProgramWnd(CProgramItemPtr pProgram, QWidget *parent = Q_NULLPTR);
	~CProgramWnd();

	CProgramItemPtr GetProgram() { return m_pProgram; }

signals:
	void Closed();

private slots:
	void OnSaveAndClose();

	void PickIcon();
	void BrowseImage();
	//void ResetIcon();

	void BrowseFile();
	void BrowseFolder();

protected:
	void closeEvent(QCloseEvent* e);

	CProgramItemPtr m_pProgram;
	QString m_IconFile;

private:
	Ui::ProgramWnd ui;

};
