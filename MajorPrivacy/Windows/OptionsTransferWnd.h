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
		eGUI		= 0x0001,
		eDriver		= 0x0002,
		eUserKeys	= 0x0004,
		eEnclaves	= 0x0008,
		eExecRules	= 0x0010,
		eResRules	= 0x0020,
		eService	= 0x0040,
		eFwRules	= 0x0080,
		ePrograms	= 0x0100,
		eTraceLog	= 0x0200,
		eAllPermanent = 0x01FF,
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
