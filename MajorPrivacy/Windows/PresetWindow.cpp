#include "pch.h"
#include "PresetWindow.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Presets/PresetManager.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"
#include "../Helpers/WinHelper.h"
#include "../Windows/ScriptWindow.h"

CPresetWindow::CPresetWindow(const CPresetPtr& pPreset, QWidget* parent)
	: QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);

	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui.setupUi(this);

	connect(ui.btnIcon, SIGNAL(clicked(bool)), this, SLOT(PickIcon()));
	ui.btnIcon->setToolTip(tr("Change Icon"));
	QMenu* pIconMenu = new QMenu(ui.btnIcon);
	pIconMenu->addAction(tr("Browse for Image"), this, SLOT(BrowseImage()));
	ui.btnIcon->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnIcon->setMenu(pIconMenu);

	m_pPreset = pPreset;
	bool bNew = m_pPreset->m_Guid.IsNull();

	setWindowTitle(bNew ? tr("Create Rule Preset") : tr("Edit Rule Preset"));

	connect(ui.txtName, SIGNAL(textChanged(const QString&)), this, SLOT(OnNameChanged(const QString&)));

	connect(ui.btnScript, SIGNAL(clicked()), this, SLOT(EditScript()));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(OnSave()));

	ui.txtName->setText(m_pPreset->m_Name);
	ui.btnIcon->setIcon(m_pPreset->GetIcon());
	m_IconFile = m_pPreset->GetIconFile();
	ui.txtInfo->setPlainText(m_pPreset->m_Description);

	m_Script = m_pPreset->m_Script;

	ui.chkScript->setChecked(m_pPreset->m_bUseScript);
	if (m_pPreset->m_Script.isEmpty())
		ui.btnScript->setIcon(QIcon(":/Icons/Script-Add.png"));
	else
		ui.btnScript->setIcon(QIcon(":/Icons/Script-Edit.png"));

	restoreGeometry(theConf->GetBlob("PresetWindow/Window_Geometry"));
}

CPresetWindow::~CPresetWindow()
{
	theConf->SetBlob("PresetWindow/Window_Geometry", saveGeometry());
}

void CPresetWindow::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

bool CPresetWindow::OnSave()
{
	if (!Save()) {
		QApplication::beep();
		return false;
	}

	auto Ret = theCore->PresetManager()->SetPreset(m_pPreset);
	if (theGUI->CheckResults(QList<STATUS>() << Ret, this))
		return false;

	if (m_pPreset->m_Guid.IsNull())
		m_pPreset->m_Guid = Ret.GetValue();
	return true;
}

void CPresetWindow::OnSaveAndClose()
{
	if (OnSave())
		accept();
}

bool CPresetWindow::Save()
{
	m_pPreset->m_Name = ui.txtName->text();
	m_pPreset->SetIconFile(m_IconFile);
	m_pPreset->m_Description = ui.txtInfo->toPlainText();

	m_pPreset->m_Script = m_Script;
	m_pPreset->m_bUseScript = ui.chkScript->isChecked();

	return true;
}

void CPresetWindow::PickIcon()
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

void CPresetWindow::BrowseImage()
{
	QString Value = QFileDialog::getOpenFileName(this, tr("Select Image File"), "", tr("Image Files (*.png)")).replace("/", "\\");
	if (Value.isEmpty())
		return;

	ui.btnIcon->setIcon(QIcon(Value));
	m_IconFile = Value;
}

void CPresetWindow::OnNameChanged(const QString& Text)
{
}

void CPresetWindow::EditScript()
{
	CScriptWindow* pScriptWindow = new CScriptWindow(m_pPreset->GetGuid(), EItemType::ePreset, this);
	pScriptWindow->SetScript(m_Script);
	pScriptWindow->SetSaver([&](const QString& Script, bool bApply) -> STATUS {
		m_Script = Script;
		if (bApply) {
			m_pPreset->m_Script = Script;
			return theCore->PresetManager()->SetPreset(m_pPreset);
		}
		return OK;
		});
	SafeShow(pScriptWindow);
}
