#include "pch.h"
#include "ProgramWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Access/AccessManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NetUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/MiscHelpers.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"
#include "../Helpers/WinHelper.h"

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

	connect(ui.btnIcon, SIGNAL(clicked(bool)), this, SLOT(PickIcon()));
	ui.btnIcon->setToolTip(tr("Change Icon"));
	QMenu* pIconMenu = new QMenu(ui.btnIcon);
	pIconMenu->addAction(tr("Browse for Image"), this, SLOT(BrowseImage()));
	//QAction* pResetIcon = pIconMenu->addAction(tr("Reset Icon"), this, SLOT(ResetIcon()));
	//pResetIcon->setEnabled(!pProgram.isNull());
	ui.btnIcon->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnIcon->setMenu(pIconMenu);

	connect(ui.btnBrowse, SIGNAL(clicked(bool)), this, SLOT(BrowseFile()));
	ui.btnBrowse->setToolTip(tr("Browse for File"));
	QMenu* pBrowseMenu = new QMenu(ui.btnBrowse);
	pBrowseMenu->addAction(tr("Browse for Folder"), this, SLOT(BrowseFolder()));
	ui.btnBrowse->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnBrowse->setMenu(pBrowseMenu);

	m_pProgram = pProgram;
	bool bNew = pProgram.isNull();

	setWindowTitle(bNew ? tr("Create Program Item") : tr("Edit Program Item"));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

	if (bNew)
	{
		ui.txtName->setText(tr("New Program Item"));

		ui.txtPath->setReadOnly(false);
	}
	else
	{
		ui.btnBrowse->setEnabled(false);

		ui.txtName->setText(pProgram->GetName());
		ui.txtInfo->setPlainText(pProgram->GetInfo());
		ui.btnIcon->setIcon(pProgram->GetIcon());
		m_IconFile = pProgram->GetIconFile();

		QString Path = pProgram->GetPath();
		ui.txtPath->setText(Path);
		
		CWindowsServicePtr pService = pProgram.objectCast<CWindowsService>();
		if(pService)
			ui.txtService->setText(pService->GetServiceTag());
		
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

void CProgramWnd::PickIcon()
{
	QString Path = m_IconFile;
	quint32 Index = 0;

	StrPair PathIndex = Split2(Path, ",", true);
	if (!PathIndex.second.isEmpty() && !PathIndex.second.contains(".")) {
		Path = PathIndex.first;
		Index = PathIndex.second.toInt();
	}

	if (!PickWindowsIcon(this, Path, Index))
		return;

	ui.btnIcon->setIcon(LoadWindowsIcon(Path, Index));
	m_IconFile = QString("%1,%2").arg(Path).arg(Index);
}

void CProgramWnd::BrowseImage()
{
	QString Value = QFileDialog::getOpenFileName(this, tr("Select Image File"), "", tr("Image Files (*.png)")).replace("/", "\\");
	if (Value.isEmpty())
		return;

	ui.btnIcon->setIcon(QIcon(Value));
	m_IconFile = Value;
}

/*void CProgramWnd::ResetIcon()
{
	
}*/

void CProgramWnd::BrowseFile()
{
	QString Value = QFileDialog::getOpenFileName(this, tr("Select Program File"), "", tr("Executable Files (*.exe)")).replace("/", "\\");
	if (Value.isEmpty())
		return;

	ui.txtPath->setText(Value);
}

void CProgramWnd::BrowseFolder()
{
	QString Value = QFileDialog::getExistingDirectory(this, tr("Select Directory")).replace("/", "\\");
	if (Value.isEmpty())
		return;

	if (!Value.endsWith("\\"))
		Value.append("\\*");

	ui.txtPath->setText(Value);
}

void CProgramWnd::OnSaveAndClose()
{
	CProgramItemPtr pProgram = m_pProgram;
	if (pProgram.isNull())
	{
		QString Path = ui.txtPath->text();
		if (Path.isEmpty()) 
		{
			pProgram = CProgramGroupPtr(new CProgramGroup());
		}
		else if (Path.contains("*"))
		{	
			CProgramPatternPtr pPattern = CProgramPatternPtr(new CProgramPattern());
			pPattern->SetPattern(Path);
			pProgram = pPattern;
		}
		else
		{
			CProgramFilePtr pFile = CProgramFilePtr(new CProgramFile());
			pFile->SetPath(Path);
			pProgram = pFile;
		}
		CProgramID ID(pProgram->GetType());
		if(Path.isEmpty()) 
			ID.SetGuid(QString::fromStdWString(MkGuid()));
		else
			ID.SetPath(Path);
		pProgram->SetID(ID);
	}

	pProgram->SetName(ui.txtName->text());
	pProgram->SetInfo(ui.txtInfo->toPlainText());
	pProgram->SetIconFile(m_IconFile);

	RESULT(quint64) Ret = theCore->ProgramManager()->SetProgram(pProgram);
	if (theGUI->CheckResults(QList<STATUS>() << Ret, this))
		return;

	if (!m_pProgram) {
		pProgram->SetUID(Ret.GetValue());
		m_pProgram = pProgram;
	}
	accept();
}