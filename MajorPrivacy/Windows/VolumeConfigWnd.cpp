#include "pch.h"
#include "VolumeConfigWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Volumes/VolumeManager.h"
#include "../Core/Network/NetworkManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NetUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"
#include "../Helpers/WinHelper.h"
#include "../Core/HashDB/HashDB.h"
#include "../MiscHelpers/Common/WinboxMultiCombo.h"
#include "../Windows/ScriptWindow.h"

CVolumeConfigWnd::CVolumeConfigWnd(const CVolumePtr& pVolume, QWidget* parent)
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

	m_pVolume = pVolume;
	
	setWindowTitle(tr("Edit Secure Volume Configuration"));

	connect(ui.chkScript, SIGNAL(stateChanged(int)), this, SLOT(OnActionChanged()));
	connect(ui.btnScript, SIGNAL(clicked()), this, SLOT(EditScript()));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(OnSave()));

	//ui.txtScript->setPlainText(m_pVolume->m_Script);
	m_Script = m_pVolume->m_Script;

	ui.chkScript->setChecked(m_pVolume->m_bUseScript);
	if(m_pVolume->m_Script.isEmpty())
		ui.btnScript->setIcon(QIcon(":/Icons/Script-Add.png"));
	else
		ui.btnScript->setIcon(QIcon(":/Icons/Script-Edit.png"));


	restoreGeometry(theConf->GetBlob("VolumeConfigWindow/Window_Geometry"));
}

CVolumeConfigWnd::~CVolumeConfigWnd()
{
	theConf->SetBlob("VolumeConfigWindow/Window_Geometry", saveGeometry());
}

void CVolumeConfigWnd::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

bool CVolumeConfigWnd::OnSave()
{
	if (!Save()) {
		QApplication::beep();
		return false;
	}

	STATUS Status = theCore->VolumeManager()->SetVolume(m_pVolume);
	if (theGUI->CheckResults(QList<STATUS>() << Status, this))
		return false;
	return true;
}

void CVolumeConfigWnd::OnSaveAndClose()
{
	if(OnSave())
		accept();
}

bool CVolumeConfigWnd::Save()
{
	//m_pVolume->m_Script = ui.txtScript->toPlainText();
	m_pVolume->m_Script = m_Script;

	m_pVolume->m_bUseScript = ui.chkScript->isChecked();

	return true;
}

void CVolumeConfigWnd::EditScript()
{
	CScriptWindow* pScriptWnd = new CScriptWindow(m_pVolume->GetGuid(), EItemType::eVolume, this);
	pScriptWnd->SetScript(m_Script);
	pScriptWnd->SetSaver([&](const QString& Script, bool bApply){
		m_Script = Script;
		if (bApply) {
			m_pVolume->m_Script = Script;
			return theCore->VolumeManager()->SetVolume(m_pVolume);
		}
		return OK;
	});
	SafeShow(pScriptWnd);
}