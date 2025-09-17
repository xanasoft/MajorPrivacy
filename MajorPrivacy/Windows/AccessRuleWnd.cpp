#include "pch.h"
#include "AccessRuleWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Enclaves/EnclaveManager.h"
#include "../Core/Access/AccessManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NetUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"
#include "../Core/Volumes/VolumeManager.h"
#include "../Library/Helpers/NtPathMgr.h"
#include "../Windows/ProgramPicker.h"
#include "../Windows/ScriptWindow.h"
#include <Lm.h>
#include <sddl.h>


void AddAccountsToComboBox(QComboBox* pComboBox)
{
	pComboBox->addItem(CAccessRuleWnd::tr("Unspecified (Any)"), "");

	{
		LPUSER_INFO_0 pBuf = nullptr;
		DWORD entriesRead = 0, totalEntries = 0, resumeHandle = 0;
		if (NERR_Success == NetUserEnum(nullptr, 0, FILTER_NORMAL_ACCOUNT, (LPBYTE*)&pBuf, MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, &resumeHandle))
		{
			for (DWORD i = 0; i < entriesRead; ++i) {
				QString Name = QString::fromWCharArray(pBuf[i].usri0_name);
				pComboBox->addItem(CAccessRuleWnd::tr("%1").arg(Name), Name);
			}
		}
		if (pBuf) NetApiBufferFree(pBuf);
	}

	{
		LPLOCALGROUP_INFO_0 pBuf = nullptr;
		DWORD entriesRead = 0, totalEntries = 0;
		DWORD_PTR resumeHandle = NULL;
		if (NERR_Success == NetLocalGroupEnum(nullptr, 0, (LPBYTE*)&pBuf, MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, &resumeHandle))
		{
			for (DWORD i = 0; i < entriesRead; ++i) {
				QString Name = QString::fromWCharArray(pBuf[i].lgrpi0_name);
				pComboBox->addItem(CAccessRuleWnd::tr("[%1]").arg(Name), Name);
			}
		}
		if (pBuf) NetApiBufferFree(pBuf);
	}
}


CAccessRuleWnd::CAccessRuleWnd(const CAccessRulePtr& pRule, QSet<CProgramItemPtr> Items, const QString& VolumeRoot, const QString& VolumeImage, QWidget* parent)
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


	m_pRule = pRule;
	bool bNew = m_pRule->m_Guid.IsNull();

	setWindowTitle(bNew ? tr("Create Program Rule") : tr("Edit Program Rule"));

	foreach(auto pItem, Items) 
		AddProgramItem(pItem);

	if (bNew && m_pRule->m_Name.isEmpty()) {
		m_pRule->m_Name = tr("New Access Rule");
	} else
		m_NameChanged = true;

	ui.cmbEnclave->addItem(tr("Global (Any/No Enclave)"));
	foreach(auto& pEnclave, theCore->EnclaveManager()->GetAllEnclaves()) {
		if(pEnclave->IsSystem()) continue;
		ui.cmbEnclave->addItem(pEnclave->GetName(), pEnclave->GetGuid().ToQV());
	}

	m_VolumeRoot = VolumeRoot;
	m_VolumeImage = VolumeImage;

	connect(ui.txtName, SIGNAL(textChanged(const QString&)), this, SLOT(OnNameChanged(const QString&)));

	connect(ui.cmbPath, SIGNAL(editTextChanged(const QString&)), this, SLOT(OnPathChanged()));
	connect(ui.btnBrowse, SIGNAL(clicked()), this, SLOT(BrowseFolder()));
	ui.btnBrowse->setToolTip(tr("Browse for Folder"));
	QMenu* pBrowseMenu = new QMenu(ui.btnBrowse);
	pBrowseMenu->addAction(tr("Browse for File"), this, SLOT(BrowseFile()));
	ui.btnBrowse->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnBrowse->setMenu(pBrowseMenu);

	connect(ui.cmbUser, SIGNAL(currentIndexChanged(int)), this, SLOT(OnUserChanged()));
	connect(ui.btnProg, SIGNAL(clicked()), this, SLOT(OnPickProgram()));
	connect(ui.cmbProgram, SIGNAL(currentIndexChanged(int)), this, SLOT(OnProgramChanged()));
	connect(ui.txtProgPath, SIGNAL(textChanged(const QString&)), this, SLOT(OnProgramPathChanged()));
	connect(ui.cmbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(OnActionChanged()));
	connect(ui.chkScript, SIGNAL(stateChanged(int)), this, SLOT(OnActionChanged()));
	connect(ui.btnScript, SIGNAL(clicked()), this, SLOT(EditScript()));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(OnSave()));
	
	AddColoredComboBoxEntry(ui.cmbAction, tr("Allow"), GetActionColor(EAccessRuleType::eAllow), (int)EAccessRuleType::eAllow);
	AddColoredComboBoxEntry(ui.cmbAction, tr("Read Only"), GetActionColor(EAccessRuleType::eAllowRO), (int)EAccessRuleType::eAllowRO);
	AddColoredComboBoxEntry(ui.cmbAction, tr("Directory Listing"), GetActionColor(EAccessRuleType::eEnum), (int)EAccessRuleType::eEnum);
	AddColoredComboBoxEntry(ui.cmbAction, tr("Protect"), GetActionColor(EAccessRuleType::eProtect), (int)EAccessRuleType::eProtect);
	AddColoredComboBoxEntry(ui.cmbAction, tr("Block"), GetActionColor(EAccessRuleType::eBlock), (int)EAccessRuleType::eBlock);
	AddColoredComboBoxEntry(ui.cmbAction, tr("Don't Log"), GetActionColor(EAccessRuleType::eIgnore), (int)EAccessRuleType::eIgnore);
	ColorComboBox(ui.cmbAction);

	AddAccountsToComboBox(ui.cmbUser);


	//FixComboBoxEditing(ui.cmbGroup);


	ui.txtName->setText(m_pRule->m_Name);
	// todo: load groups
	//SetComboBoxValue(ui.cmbGroup, m_pRule->m_Grouping); // todo
	ui.txtInfo->setPlainText(m_pRule->m_Description);

	ui.cmbProgram->setEditable(true);
	ui.cmbProgram->lineEdit()->setReadOnly(true);
	CProgramItemPtr pItem;
	if (m_pRule->m_ProgramID.GetType() == EProgramType::eAllPrograms)
		pItem = theCore->ProgramManager()->GetAll();
	else if(!bNew)
		pItem = theCore->ProgramManager()->GetProgramByID(m_pRule->m_ProgramID);
	int Index = m_Items.indexOf(pItem);
	ui.cmbProgram->setCurrentIndex(Index);
	if(bNew)
		OnProgramChanged();
	else {
		m_HoldProgramPath = true;
		ui.txtProgPath->setText(m_pRule->GetProgramPath());
		m_HoldProgramPath = false;
	}

	SetComboBoxValue(ui.cmbEnclave, QFlexGuid(m_pRule->m_Enclave).ToQV());

	SetComboBoxValue(ui.cmbUser, m_pRule->m_User);
	if (m_pRule->m_UserSid.IsValid())
	{
		BYTE sidBuffer[SECURITY_MAX_SID_SIZE];
		memcpy(sidBuffer, m_pRule->m_UserSid.GetData(), Min(m_pRule->m_UserSid.GetSize(), sizeof(sidBuffer)));
		LPWSTR sidStr = nullptr;
		if (ConvertSidToStringSidW((PSID)sidBuffer, &sidStr))
		{
			ui.cmbUser->setToolTip(QString::fromWCharArray(sidStr));
			LocalFree(sidStr);
		}
	}

	foreach(const CVolumePtr& pVolume, theCore->VolumeManager()->List())
	{
		QString Path = pVolume->GetImagePath();
		if(pVolume->IsFolder())
			Path += "*";
		else
			Path += "/*";
		ui.cmbPath->addItem(Path);
	}

	if(!m_pRule->m_AccessPath.isEmpty())
		ui.cmbPath->setEditText(m_pRule->m_AccessPath);
	else if (m_VolumeImage.endsWith("\\")) // Protected Folder
		ui.cmbPath->setEditText(m_VolumeImage);
	else if(!m_VolumeRoot.isEmpty()) // Rule stored on the volume
		ui.cmbPath->setEditText(m_VolumeRoot + "\\");
	else if (!m_VolumeImage.isEmpty()) { // Rule stored globally
		m_VolumeImage += "/";
		ui.cmbPath->setEditText(m_VolumeImage);
	} else
		ui.cmbPath->setEditText("");

	SetComboBoxValue(ui.cmbAction, (int)m_pRule->m_Type);

	//ui.txtScript->setPlainText(m_pRule->m_Script);
	m_Script = m_pRule->m_Script;

	ui.chkScript->setChecked(m_pRule->m_bUseScript);
	if(m_pRule->m_Script.isEmpty())
		ui.btnScript->setIcon(QIcon(":/Icons/Script-Add.png"));
	else
		ui.btnScript->setIcon(QIcon(":/Icons/Script-Edit.png"));
	ui.chkInteractive->setChecked(m_pRule->m_bInteractive);

	m_NameHold = false;
}

CAccessRuleWnd::~CAccessRuleWnd()
{
}

bool CAccessRuleWnd::AddProgramItem(const CProgramItemPtr& pItem)
{
	switch (pItem->GetID().GetType())
	{
	case EProgramType::eProgramFile:
	case EProgramType::eFilePattern:
	case EProgramType::eAppInstallation:
	case EProgramType::eAllPrograms:
		break;
	default:
		return false;
	}

	m_Items.append(pItem);
	ui.cmbProgram->addItem(pItem->GetNameEx());
	return true;
}

QColor CAccessRuleWnd::GetActionColor(EAccessRuleType Action)
{
	switch (Action)
	{
	case EAccessRuleType::eAllow:	return QColor(144, 238, 144);
	case EAccessRuleType::eAllowRO: return QColor(255, 255, 224);
	case EAccessRuleType::eEnum:	return QColor(255, 228, 181);
	case EAccessRuleType::eProtect:	return QColor(173, 216, 230);
	case EAccessRuleType::eBlock:	return QColor(255, 182, 193);
	case EAccessRuleType::eIgnore:	return QColor(193, 193, 193);
	default: return QColor();
	}
}

QColor CAccessRuleWnd::GetStatusColor(EEventStatus Status)
{
	switch (Status)
	{
	case EEventStatus::eAllowed:	return QColor(144, 238, 144);
	case EEventStatus::eUntrusted:	return QColor(255, 228, 181);
	case EEventStatus::eEjected:	return QColor(255, 228, 181);
	case EEventStatus::eBlocked:	return QColor(255, 182, 193);
	case EEventStatus::eProtected:	return QColor(173, 216, 230);
	default: return QColor();
	}
}

QString CAccessRuleWnd::GetStatusStr(EEventStatus Status)
{
	switch (Status) 
	{
	case EEventStatus::eAllowed:	return QObject::tr("Allowed");
	case EEventStatus::eUntrusted:	return QObject::tr("Allowed (Untrusted)");
	case EEventStatus::eEjected:	return QObject::tr("Allowed (Ejected)");
	case EEventStatus::eProtected:	return QObject::tr("Blocked (Protected)");
	case EEventStatus::eBlocked:	return QObject::tr("Blocked");
	default: return QObject::tr("Unknown");
	}
}

void CAccessRuleWnd::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

void CAccessRuleWnd::BrowseFolder()
{
	QString Value = QFileDialog::getExistingDirectory(this, tr("Select Directory")).replace("/", "\\");
	if(Value.isEmpty())
		return;
	ui.cmbPath->setEditText(Value + "\\*");
}

void CAccessRuleWnd::BrowseFile()
{
	QString Value = QFileDialog::getOpenFileName(this, tr("Select File")).replace("/", "\\");
	if (Value.isEmpty())
		return;
	ui.cmbPath->setEditText(Value);
}

bool CAccessRuleWnd::OnSave()
{
	if (!Save()) {
		QApplication::beep();
		return false;
	}

	if (m_pRule->m_Guid.IsNull())
	{
		QString Path = ui.cmbPath->currentText();
		QString NtPath = Path.startsWith("\\") ? Path : QString::fromStdWString(CNtPathMgr::Instance()->TranslateDosToNtPath(Path.toStdWString()));
		if (!NtPath.isEmpty())
		{
			theCore->VolumeManager()->Update();
			auto Volumes = theCore->VolumeManager()->List();
			foreach(const CVolumePtr & pVolume, Volumes) {
				if (pVolume->GetStatus() != CVolume::eMounted)
					continue;
				QString DevicePath = pVolume->GetDevicePath();
				if (PathStartsWith(NtPath, DevicePath)) {
					m_pRule->SetVolumeRule(true);
					m_pRule->m_AccessPath = NtPath;
					break;
				}
			}
		}
	}

	STATUS Status = theCore->AccessManager()->SetAccessRule(m_pRule);
	if (theGUI->CheckResults(QList<STATUS>() << Status, this))
		return false;

	return true;
}

void CAccessRuleWnd::OnSaveAndClose()
{
	if(OnSave())
		accept();
}

bool CAccessRuleWnd::Save()
{
	m_pRule->m_Name = ui.txtName->text();
	//m_pRule->m_Grouping = GetComboBoxValue(ui.cmbGroup).toString(); // todo
	m_pRule->m_Description = ui.txtInfo->toPlainText();

	/*int Index = ui.cmbProgram->currentIndex();
	if (Index == -1) 
		return false;*/

	/*CProgramItemPtr pItem = m_Items[Index];
	if (m_pRule->m_ProgramID != pItem->GetID()) {
		m_pRule->m_Path = pItem->GetPath();
	}*/
	m_pRule->m_ProgramID.SetPath(ui.txtProgPath->text().toLower());
	m_pRule->m_ProgramPath = ui.txtProgPath->text();
	if (!m_pRule->m_ProgramPath.isEmpty() && m_pRule->m_ProgramPath != "*") { // is set?
		if (!CAccessRule::IsPathValid(m_pRule->m_ProgramPath)) {
			QMessageBox::warning(this, tr("Error"), tr("The Program Path is not valid."));
			return false;
		}
	}

	m_pRule->m_Enclave = QFlexGuid(GetComboBoxValue(ui.cmbEnclave));

	m_pRule->m_User = ui.cmbUser->currentData().toString();
	m_pRule->m_SidValid = false;

	QString Path = ui.cmbPath->currentText();
	if (!m_VolumeImage.isEmpty() && !PathStartsWith(Path, m_VolumeImage) && !PathStartsWith(Path, m_VolumeRoot)) {
		QMessageBox::information(this, "MajorPrivacy", tr("The path must be contained within the volume."));
		return false;
	}

	m_pRule->m_AccessPath = Path;
	if (!CAccessRule::IsPathValid(m_pRule->m_AccessPath)) {
		QMessageBox::warning(this, tr("Error"), tr("The File Path is not valid."));
		return false;
	}
	if (CAccessRule::IsUnsafePath(m_pRule->m_AccessPath)) {
		QMessageBox::warning(this, tr("Error"), tr("The File Path may break windows, please use a more specific path."));
		return false;
	}

	m_pRule->m_Type = (EAccessRuleType)GetComboBoxValue(ui.cmbAction).toInt();

	//m_pRule->m_Script = ui.txtScript->toPlainText();
	m_pRule->m_Script = m_Script;

	m_pRule->m_bUseScript = ui.chkScript->isChecked();
	m_pRule->m_bInteractive = ui.chkInteractive->isChecked();

	return true;
}

void CAccessRuleWnd::OnNameChanged(const QString& Text)
{
	if (m_NameHold) return;
	m_NameChanged = true;
}

void CAccessRuleWnd::OnPathChanged()
{
	TryMakeName();
}

void CAccessRuleWnd::OnUserChanged()
{
	QString User = ui.cmbUser->currentData().toString();
	if (User.isEmpty()) {
		ui.cmbUser->setToolTip("");
		return;
	}

	BYTE sidBuffer[SECURITY_MAX_SID_SIZE];
	DWORD sidSize = sizeof(sidBuffer);
	SID_NAME_USE sidType;
	WCHAR domain[256];
	DWORD domainSize = _countof(domain);

	QtVariant SID;
	if (LookupAccountNameW(nullptr, (wchar_t*)User.utf16(), sidBuffer, &sidSize, domain, &domainSize, &sidType))
	{
		LPWSTR sidStr = nullptr;
		if (ConvertSidToStringSidW((PSID)sidBuffer, &sidStr))
		{
			ui.cmbUser->setToolTip(QString::fromWCharArray(sidStr));
			LocalFree(sidStr);
		}
	}
}

void CAccessRuleWnd::OnPickProgram()
{
	int Index = ui.cmbProgram->currentIndex();
	CProgramItemPtr pItem = Index != -1 ? m_Items[Index] : nullptr;
	CProgramPicker Picker(pItem, m_Items, this);
	if (theGUI->SafeExec(&Picker)) {
		pItem = Picker.GetProgram();
		Index = m_Items.indexOf(pItem);
		if (Index == -1) {
			if(!AddProgramItem(pItem))
				QMessageBox::warning(this, "MajorPrivacy", tr("The selected program type is not supported for this rule type"));
			else
				Index = m_Items.indexOf(pItem);
		}
		if (Index != -1) {
			ui.cmbProgram->setCurrentIndex(Index);
			OnProgramChanged();
		}
	}
}

void CAccessRuleWnd::OnProgramChanged()
{
	int Index = ui.cmbProgram->currentIndex();
	if (Index == -1) return;

	CProgramItemPtr pItem = m_Items[Index];
	//if (pItem) ui.cmbProgram->setCurrentText(pItem->GetName());
	CProgramID ID = pItem->GetID();
	m_HoldProgramPath = true;
	ui.txtProgPath->setText(pItem->GetPath());
	m_HoldProgramPath = false;
	
	TryMakeName();
}

void CAccessRuleWnd::OnProgramPathChanged()
{
	if(m_HoldProgramPath) return;
	ui.cmbProgram->setCurrentIndex(-1);
}

void CAccessRuleWnd::OnActionChanged()
{
	TryMakeName();

	//ui.chkScript->setEnabled(ui.cmbAction->currentData() == (int)EAccessRuleType::eProtect);
	//ui.btnScript->setEnabled(ui.chkScript->isChecked());

	ui.chkInteractive->setEnabled(ui.cmbAction->currentData() == (int)EAccessRuleType::eProtect);
}

void CAccessRuleWnd::TryMakeName()
{
	if (ui.txtName->text().isEmpty())
		m_NameChanged = false;
	if (m_NameHold || m_NameChanged)
		return;

	QString Action = ui.cmbAction->currentText();
	QString Path = ui.cmbPath->currentText();
	QString Program = ui.cmbProgram->currentText();
	if (Action.isEmpty() && Path.isEmpty())
		return;

	m_NameHold = true;
	ui.txtName->setText(tr("%1 %2 %3").arg(Action).arg(Path).arg(Program.isEmpty() ? "" : tr(" (%1)").arg(Program)));
	m_NameHold = false;
}

void CAccessRuleWnd::EditScript()
{
	CScriptWindow* pScriptWnd = new CScriptWindow(m_pRule->GetGuid(), EScriptTypes::eResRule, this);
	pScriptWnd->SetScript(m_Script);
	pScriptWnd->SetSaver([&](const QString& Script, bool bApply){
		m_Script = Script;
		if (bApply) {
			m_pRule->m_Script = Script;
			return theCore->AccessManager()->SetAccessRule(m_pRule);
		}
		return OK;
	});
	SafeShow(pScriptWnd);
}