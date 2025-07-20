#include "pch.h"
#include "ScriptWindow.h"
#include "../MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/Finder.h"
#include "ProgramWnd.h"

CScriptWindow::CScriptWindow(QWidget* parent)
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

	//setWindowState(Qt::WindowActive);
	//SetForegroundWindow((HWND)QWidget::winId());

	ui.setupUi(this);
	this->setWindowTitle(tr("MajorPrivacy - Script Editor"));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(accept()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

	ui.txtScript->setFocus();

	restoreGeometry(theConf->GetBlob("ScriptWindow/Window_Geometry"));
}

CScriptWindow::~CScriptWindow()
{
	theConf->SetBlob("ScriptWindow/Window_Geometry", saveGeometry());
}
