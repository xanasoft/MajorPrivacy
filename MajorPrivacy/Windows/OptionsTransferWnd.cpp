#include "pch.h"
#include "OptionsTransferWnd.h"

COptionsTransferWnd::COptionsTransferWnd(EAction Action, quint32 Options, QWidget* parent)
	: QDialog(parent)
{
	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	//flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	//flags &= ~Qt::WindowMinMaxButtonsHint;
	//flags |= Qt::WindowMinimizeButtonHint;
	//flags &= ~Qt::WindowCloseButtonHint;
	flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	setWindowFlags(flags);

	ui.setupUi(this);

	ui.lblInfo->setText(Action == eImport ? tr("Select the data you want to import") : tr("Select the data you want to export"));

	if (Options & eGUI)
		ui.chkGUI->setChecked(true);

	if (Options & eDriver)
		ui.chkDriver->setChecked(true);

	if (Options & eUserKeys)
		ui.chkUserKey->setChecked(true);

	if (Options & eEnclaves)
		ui.chkEnclaves->setChecked(true);

	if (Options & eExecRules)
		ui.chkExecRules->setChecked(true);

	if (Options & eResRules)
		ui.chkResRules->setChecked(true);

	if (Options & eService)
		ui.chkService->setChecked(true);

	if (Options & eFwRules)
		ui.chkFwRules->setChecked(true);

	if (Options & ePrograms)
		ui.chkPrograms->setChecked(true);

	ui.chkTraceLog->setVisible(false);
	//if (Options & eTraceLog)
	//	ui.chkTraceLog->setChecked(true);

	if (Action == eImport)
		Disable(~Options);
}

COptionsTransferWnd::~COptionsTransferWnd()
{
}

//void COptionsTransferWnd::closeEvent(QCloseEvent *e)
//{
//	emit Closed();
//	this->deleteLater();
//}

void COptionsTransferWnd::Disable(quint32 Options)
{
	if (Options & eGUI)
		ui.chkGUI->setEnabled(false);

	if (Options & eDriver)
		ui.chkDriver->setEnabled(false);

	if (Options & eUserKeys)
		ui.chkUserKey->setEnabled(false);

	if (Options & eEnclaves)
		ui.chkEnclaves->setEnabled(false);

	if (Options & eExecRules)
		ui.chkExecRules->setEnabled(false);

	if (Options & eResRules)
		ui.chkResRules->setEnabled(false);

	if (Options & eService)
		ui.chkService->setEnabled(false);

	if (Options & eFwRules)
		ui.chkFwRules->setEnabled(false);

	if (Options & ePrograms)
		ui.chkPrograms->setEnabled(false);

	//if (Options & eTraceLog)
	//	ui.chkTraceLog->setEnabled(false);
}

quint32 COptionsTransferWnd::GetOptions() const
{
	quint32 Options = 0;

	if (ui.chkGUI->isChecked() && ui.chkGUI->isEnabled())
		Options |= eGUI;

	if (ui.chkDriver->isChecked() && ui.chkDriver->isEnabled())
		Options |= eDriver;

	if (ui.chkUserKey->isChecked() && ui.chkUserKey->isEnabled())
		Options |= eUserKeys;

	if (ui.chkEnclaves->isChecked() && ui.chkEnclaves->isEnabled())
		Options |= eEnclaves;

	if (ui.chkExecRules->isChecked() && ui.chkExecRules->isEnabled())
		Options |= eExecRules;

	if (ui.chkResRules->isChecked() && ui.chkResRules->isEnabled())
		Options |= eResRules;

	if (ui.chkService->isChecked() && ui.chkService->isEnabled())
		Options |= eService;

	if (ui.chkFwRules->isChecked() && ui.chkFwRules->isEnabled())
		Options |= eFwRules;

	if (ui.chkPrograms->isChecked() && ui.chkPrograms->isEnabled())
		Options |= ePrograms;

	//if (ui.chkTraceLog->isChecked() && ui.chkTraceLog->isEnabled())
	//	Options |= eTraceLog;

	return Options;
}