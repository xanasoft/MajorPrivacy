#include "pch.h"
#include "ProgramWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Access/AccessManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NetUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"

CProgramWnd::CProgramWnd(CProgramItemPtr pProgram, QWidget* parent)
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

	m_pProgram = pProgram;
	bool bNew = pProgram.isNull();

	setWindowTitle(bNew ? tr("Create Program Item") : tr("Edit Program Item"));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

	if (bNew)
	{
		ui.txtName->setText(tr("New Program"));

		ui.txtPath->setReadOnly(false);
	}
	else
	{
		ui.txtName->setText(pProgram->GetName());
		ui.txtInfo->setPlainText(pProgram->GetInfo());

		ui.txtPath->setText(pProgram->GetPath(EPathType::eDisplay));
		CWindowsServicePtr pService = pProgram.objectCast<CWindowsService>();
		if(pService)
			ui.txtService->setText(pService->GetSvcTag());
		CAppPackagePtr pPackage = pProgram.objectCast<CAppPackage>();
		if(pPackage)
			ui.txtApp->setText(pPackage->GetAppSid());
		CAppInstallationPtr pInstallation = pProgram.objectCast<CAppInstallation>();
		if(pInstallation)
			ui.txtApp->setText(pInstallation->GetRegKey());
	}
}

CProgramWnd::~CProgramWnd()
{
}

void CProgramWnd::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

void CProgramWnd::OnSaveAndClose()
{
	CProgramItemPtr pProgram = m_pProgram;
	if (pProgram.isNull())
	{
		QString Path = ui.txtPath->text();
		if (Path.isEmpty()) {
			QApplication::beep();
			return;
		}

		if (Path.contains("*"))
		{	
			CProgramPatternPtr pPattern = CProgramPatternPtr(new CProgramPattern());
			pPattern->SetPattern(Path, EPathType::eAuto);
			pProgram = pPattern;
		}
		else
		{
			CProgramFilePtr pFile = CProgramFilePtr(new CProgramFile());
			pFile->SetPath(Path, EPathType::eAuto);
			pProgram = pFile;
		}
		CProgramID ID(pProgram->GetType());
		ID.SetPath(Path);
		pProgram->SetID(ID);
	}

	pProgram->SetName(ui.txtName->text());
	pProgram->SetInfo(ui.txtInfo->toPlainText());

	STATUS Status = theCore->ProgramManager()->SetProgram(pProgram);
	if (theGUI->CheckResults(QList<STATUS>() << Status, this))
		return;
	accept();
}