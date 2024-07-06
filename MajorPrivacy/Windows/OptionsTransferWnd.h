#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_OptionsTransferWnd.h"


class COptionsTransferWnd : public QDialog
{
	Q_OBJECT

public:

	enum EAction {
		eNone,
		eImport,
		eExport
	};

	enum EOptions {
		eGUI = 0x01,
		eDriver = 0x02,
		eUserKeys = 0x04,
		eExecRules = 0x08,
		eResRules = 0x10,
		eService = 0x20,
		eFwRules = 0x40,
		ePrograms = 0x80,
		eTraceLog = 0x100,
		eAllPermanent = 0xFF,
		eAll = 0xFFFF
	};

	COptionsTransferWnd(EAction Action, quint32 Options, QWidget *parent = Q_NULLPTR);
	~COptionsTransferWnd();

	void Disable(quint32 Options);

	quint32 GetOptions() const;

signals:
	void Closed();

protected:
	//void closeEvent(QCloseEvent* e);

private:
	Ui::OptionsTransferWnd ui;

};
