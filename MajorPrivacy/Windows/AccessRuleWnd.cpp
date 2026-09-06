#include "pch.h"
#include "AccessRuleWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Enclaves/EnclaveManager.h"
#include "../Core/Access/AccessManager.h"
#include "../Core/GenericRule.h"
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
#include "../Windows/SettingsWindow.h"
#include "../Helpers/SidResolver.h"
#include <Lm.h>
#include <sddl.h>


static QString ResolveNameToSid(const QString& Name, QString* pFullName = nullptr)
{
	BYTE sidBuffer[SECURITY_MAX_SID_SIZE];
	DWORD sidSize = sizeof(sidBuffer);
	SID_NAME_USE sidType;
	WCHAR domain[256];
	DWORD domainSize = _countof(domain);

	if (LookupAccountNameW(nullptr, (LPCWSTR)Name.utf16(), sidBuffer, &sidSize, domain, &domainSize, &sidType))
	{
		if (pFullName) {
			QString domainStr = QString::fromWCharArray(domain);
			if (!domainStr.isEmpty())
				*pFullName = domainStr + "\\" + Name;
			else
				*pFullName = Name;
		}

		LPWSTR sidStr = nullptr;
		if (ConvertSidToStringSidW((PSID)sidBuffer, &sidStr))
		{
			QString sid = QString::fromWCharArray(sidStr);
			LocalFree(sidStr);
			return sid;
		}
	}
	return QString();
}

static void AddAccountsToUserComboBox(QComboBox* pComboBox)
{
	{
		LPUSER_INFO_0 pBuf = nullptr;
		DWORD entriesRead = 0, totalEntries = 0, resumeHandle = 0;
		if (NERR_Success == NetUserEnum(nullptr, 0, FILTER_NORMAL_ACCOUNT, (LPBYTE*)&pBuf, MAX_PREFERRED_LENGTH, &entriesRead, &totalEntries, &resumeHandle))
		{
			for (DWORD i = 0; i < entriesRead; ++i) {
				QString Name = QString::fromWCharArray(pBuf[i].usri0_name);
				QString FullName;
				QString Sid = ResolveNameToSid(Name, &FullName);
				pComboBox->addItem(FullName.isEmpty() ? Name : FullName, Sid.isEmpty() ? Name : Sid);
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
				QString FullName;
				QString Sid = ResolveNameToSid(Name, &FullName);
				pComboBox->addItem(QObject::tr("[%1]").arg(FullName.isEmpty() ? Name : FullName), Sid.isEmpty() ? Name : Sid);
			}
		}
		if (pBuf) NetApiBufferFree(pBuf);
	}
}


CAccessRuleWnd::CAccessRuleWnd(const CAccessRulePtr& pRule, QSet<CProgramItemPtr> Items, const QString& VolumeRoot, const QString& VolumeImage, const QString& MountPoint, QWidget* parent)
	: QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);

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
	}
	else
		m_NameChanged = true;

	ui.cmbEnclave->addItem(tr("Global (Any/No Enclave)"));
	foreach(auto& pEnclave, theCore->EnclaveManager()->GetAllEnclaves()) {
		if (pEnclave->IsSystem()) continue;
		ui.cmbEnclave->addItem(pEnclave->GetName(), pEnclave->GetGuid().ToQV());
	}

	m_VolumeRoot = VolumeRoot;
	m_VolumeImage = VolumeImage;
	m_MountPoint = MountPoint;

	connect(ui.txtName, SIGNAL(textChanged(const QString&)), this, SLOT(OnNameChanged(const QString&)));
	connect(ui.btnMkName, &QToolButton::clicked, this, [&]() {
		m_NameChanged = false;
		TryMakeName();
	});

	connect(ui.cmbPath, SIGNAL(editTextChanged(const QString&)), this, SLOT(OnPathChanged()));
	connect(ui.btnBrowse, SIGNAL(clicked()), this, SLOT(BrowseFolder()));
	ui.btnBrowse->setToolTip(tr("Browse for Folder"));
	QMenu* pBrowseMenu = new QMenu(ui.btnBrowse);
	pBrowseMenu->addAction(tr("Browse for File"), this, SLOT(BrowseFile()));
	ui.btnBrowse->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnBrowse->setMenu(pBrowseMenu);

	//connect(ui.cmbUser, SIGNAL(currentIndexChanged(int)), this, SLOT(OnUserChanged()));

	connect(ui.chkAllowUsers, SIGNAL(toggled(bool)), this, SLOT(OnAllowUsersChanged()));
	connect(ui.lstApplyUsers, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnApplyUsersEdit()));
	connect(ui.btnApplyUsersSet, SIGNAL(clicked(bool)), this, SLOT(OnApplyUsersSet()));

	connect(ui.chkExcemptUsers, SIGNAL(toggled(bool)), this, SLOT(OnExemptUsersChanged()));
	connect(ui.lstExemptUsers, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(OnExemptUsersEdit()));
	connect(ui.btnExemptUsersSet, SIGNAL(clicked(bool)), this, SLOT(OnExemptUsersSet()));

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

	// Initialize User Authorization Lists
	ui.cmbApplyUsersSet->setVisible(false);
	ui.btnApplyUsersSet->setVisible(false);
	ui.cmbExemptUsersSet->setVisible(false);
	ui.btnExemptUsersSet->setVisible(false);

	AddAccountsToUserComboBox(ui.cmbApplyUsersSet);
	AddAccountsToUserComboBox(ui.cmbExemptUsersSet);


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

	// Old single-user mechanism - commented out
	//SetComboBoxValue(ui.cmbUser, m_pRule->m_User);
	//if (m_pRule->m_UserSid.IsValid())
	//{
	//	BYTE sidBuffer[SECURITY_MAX_SID_SIZE];
	//	memcpy(sidBuffer, m_pRule->m_UserSid.GetData(), Min(m_pRule->m_UserSid.GetSize(), sizeof(sidBuffer)));
	//	LPWSTR sidStr = nullptr;
	//	if (ConvertSidToStringSidW((PSID)sidBuffer, &sidStr))
	//	{
	//		ui.cmbUser->setToolTip(QString::fromWCharArray(sidStr));
	//		LocalFree(sidStr);
	//	}
	//}

	// New multi-user mechanism using SDDL
	ParsePrincipalSddl(m_pRule->m_PrincipalSddl);

	foreach(const CVolumePtr& pVolume, theCore->VolumeManager()->List())
	{
		QString Path = pVolume->GetImagePath();
		if(pVolume->IsFolder())
			Path += "*";
		else
			Path += "/*";
		ui.cmbPath->addItem(Path);
	}

	if(!m_pRule->m_AccessPath.isEmpty()) {
		QString DisplayPath = m_pRule->m_AccessPath;
		// Convert VolumeRoot to MountPoint for display
		if (!m_MountPoint.isEmpty() && !m_VolumeRoot.isEmpty() && PathStartsWith(DisplayPath, m_VolumeRoot)) {
			QString Suffix = DisplayPath.mid(m_VolumeRoot.length());
			if ((m_MountPoint.endsWith("\\") || m_MountPoint.endsWith("/")) && (Suffix.startsWith("\\") || Suffix.startsWith("/")))
				Suffix = Suffix.mid(1);
			DisplayPath = m_MountPoint + Suffix;
		}
		ui.cmbPath->setEditText(DisplayPath);
	}
	else if (m_VolumeImage.endsWith("\\")) // Protected Folder
		ui.cmbPath->setEditText(m_VolumeImage);
	else if(!m_MountPoint.isEmpty()) // Rule stored on the volume - use mount point for display
		ui.cmbPath->setEditText(m_MountPoint);
	else if(!m_VolumeRoot.isEmpty()) // Fallback to volume root
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

	OnAllowUsersChanged();
	OnExemptUsersChanged();

	m_NameHold = false;

	restoreGeometry(theConf->GetBlob("AccessRuleWindow/Window_Geometry"));
}

CAccessRuleWnd::~CAccessRuleWnd()
{
	theConf->SetBlob("AccessRuleWindow/Window_Geometry", saveGeometry());
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

	if(!g_CertInfo.active && m_pRule->m_Guid.IsNull())
		QMessageBox::warning(this, "MajorPrivacy", tr("This rule will be saved <b>but will NOT protect your system</b>.<br />"
			"MajorPrivacy is running without a valid license, so driver rule enforcement is disabled.<br />"
			"Activate a license to enable full protection."));

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

	auto Ret = theCore->AccessManager()->SetAccessRule(m_pRule);
	if (theGUI->CheckResults(QList<STATUS>() << Ret, this))
		return false;

	if(m_pRule->m_Guid.IsNull())
		m_pRule->m_Guid = Ret.GetValue();

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

	// Old single-user mechanism - commented out
	//m_pRule->m_User = ui.cmbUser->currentData().toString();
	//m_pRule->m_SidValid = false;

	// New multi-user mechanism using SDDL
	m_pRule->m_PrincipalSddl = BuildPrincipalSddl();

	QString Path = ui.cmbPath->currentText();

	// Convert MountPoint back to VolumeRoot for storage
	if (!m_MountPoint.isEmpty() && !m_VolumeRoot.isEmpty() && PathStartsWith(Path, m_MountPoint)) {
		QString Suffix = Path.mid(m_MountPoint.length());
		if (!Suffix.isEmpty() && !Suffix.startsWith("\\") && !Suffix.startsWith("/"))
			Path = m_VolumeRoot + "\\" + Suffix;
		else
			Path = m_VolumeRoot + Suffix;
	}

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

// Old single-user mechanism - commented out
//void CAccessRuleWnd::OnUserChanged()
//{
//	QString User = ui.cmbUser->currentData().toString();
//	if (User.isEmpty()) {
//		ui.cmbUser->setToolTip("");
//		return;
//	}
//
//	BYTE sidBuffer[SECURITY_MAX_SID_SIZE];
//	DWORD sidSize = sizeof(sidBuffer);
//	SID_NAME_USE sidType;
//	WCHAR domain[256];
//	DWORD domainSize = _countof(domain);
//
//	QtVariant SID;
//	if (LookupAccountNameW(nullptr, (wchar_t*)User.utf16(), sidBuffer, &sidSize, domain, &domainSize, &sidType))
//	{
//		LPWSTR sidStr = nullptr;
//		if (ConvertSidToStringSidW((PSID)sidBuffer, &sidStr))
//		{
//			ui.cmbUser->setToolTip(QString::fromWCharArray(sidStr));
//			LocalFree(sidStr);
//		}
//	}
//}

QString CAccessRuleWnd::SidToUserName(const QString& Sid)
{
	QByteArray sidBinary = CGenericRule::SidStringToBinary(Sid);
	if (sidBinary.isEmpty())
		return Sid;

	QString userName = theCore->GetSidResolver()->GetSidFullName(sidBinary, this, SLOT(OnSidResolved(const QByteArray&, const QString&)));
	return userName.isEmpty() ? Sid : userName;
}

void CAccessRuleWnd::ParsePrincipalSddl(const QString& SDDL)
{
	ui.lstApplyUsers->clear();
	ui.lstExemptUsers->clear();

	if (SDDL.isEmpty()) {
		ui.chkAllowUsers->setChecked(false);
		ui.chkExcemptUsers->setChecked(false);
	}
	else {
		// Parse SDDL format: O:LSD:(D;;CC;;;SID1)(A;;CC;;;SID2)...
		// Find the start of ACEs (after "D:")
		int dPos = SDDL.indexOf("D:");
		if (dPos == -1) {
			ui.chkAllowUsers->setChecked(false);
			ui.chkExcemptUsers->setChecked(false);
		}
		else {
			QString acesPart = SDDL.mid(dPos + 2);

			bool hasApplyUsers = false;
			bool hasExemptUsers = false;

			// Parse each ACE: (type;;rights;;;SID)
			QRegularExpression aceRx("\\(([AD]);;[^;]*;;;([^)]+)\\)");
			QRegularExpressionMatchIterator i = aceRx.globalMatch(acesPart);
			while (i.hasNext()) {
				QRegularExpressionMatch match = i.next();
				QString aceType = match.captured(1);
				QString sid = match.captured(2);
				QString userName = SidToUserName(sid);

				if (aceType == "A") {
					// Allow - rule applies to this user
					hasApplyUsers = true;
					QListWidgetItem* pItem = new QListWidgetItem(userName);
					pItem->setData(Qt::UserRole, sid);
					ui.lstApplyUsers->addItem(pItem);
				}
				else if (aceType == "D") {
					// Deny - user is exempt
					hasExemptUsers = true;
					QListWidgetItem* pItem = new QListWidgetItem(userName);
					pItem->setData(Qt::UserRole, sid);
					ui.lstExemptUsers->addItem(pItem);
				}
			}

			ui.chkAllowUsers->setChecked(hasApplyUsers);
			ui.chkExcemptUsers->setChecked(hasExemptUsers);
		}
	}

	// Add "Add User" placeholder items
	QListWidgetItem* pNewApply = new QListWidgetItem(tr("[Add User]"));
	pNewApply->setData(Qt::UserRole, "");
	ui.lstApplyUsers->addItem(pNewApply);

	QListWidgetItem* pNewExempt = new QListWidgetItem(tr("[Add User]"));
	pNewExempt->setData(Qt::UserRole, "");
	ui.lstExemptUsers->addItem(pNewExempt);
}

QString CAccessRuleWnd::BuildPrincipalSddl()
{
	if (!ui.chkAllowUsers->isChecked() && !ui.chkExcemptUsers->isChecked())
		return QString();

	QString sddl = "O:LSD:";

	// Add Deny (exempt) entries first
	if (ui.chkExcemptUsers->isChecked()) {
		for (int i = 0; i < ui.lstExemptUsers->count(); i++) {
			QListWidgetItem* pItem = ui.lstExemptUsers->item(i);
			QString sid = pItem->data(Qt::UserRole).toString();
			if (!sid.isEmpty())
				sddl += QString("(D;;CC;;;%1)").arg(sid);
		}
	}

	// Add Allow (apply) entries
	if (ui.chkAllowUsers->isChecked()) {
		for (int i = 0; i < ui.lstApplyUsers->count(); i++) {
			QListWidgetItem* pItem = ui.lstApplyUsers->item(i);
			QString sid = pItem->data(Qt::UserRole).toString();
			if (!sid.isEmpty())
				sddl += QString("(A;;CC;;;%1)").arg(sid);
		}
	}

	// If no actual entries were added, return empty
	if (sddl == "O:LSD:")
		return QString();

	return sddl;
}

void CAccessRuleWnd::OnAllowUsersChanged()
{
	ui.lstApplyUsers->setEnabled(ui.chkAllowUsers->isChecked());
}

void CAccessRuleWnd::OnApplyUsersEdit()
{
	ui.lstApplyUsers->setEnabled(false);
	QListWidgetItem* pItem = ui.lstApplyUsers->currentItem();
	if (pItem->data(Qt::UserRole).toString().isEmpty())
		ui.cmbApplyUsersSet->setCurrentText("");
	else {
		// Try to find matching item in combo by SID
		QString sid = pItem->data(Qt::UserRole).toString();
		int index = ui.cmbApplyUsersSet->findData(sid);
		if (index != -1)
			ui.cmbApplyUsersSet->setCurrentIndex(index);
		else
			ui.cmbApplyUsersSet->setCurrentText(pItem->text());
	}
	ui.cmbApplyUsersSet->setVisible(true);
	ui.btnApplyUsersSet->setVisible(true);
}

void CAccessRuleWnd::OnApplyUsersSet()
{
	QString userName = ui.cmbApplyUsersSet->currentText();

	QListWidgetItem* pItem = ui.lstApplyUsers->currentItem();
	bool bAdd = pItem->data(Qt::UserRole).toString() == "";

	if (userName.isEmpty())
	{
		if (!bAdd) delete pItem;
		pItem = NULL;
	}
	else
	{
		QString sid;

		// Check if an item is selected from the combo box (has pre-resolved SID)
		int comboIndex = ui.cmbApplyUsersSet->currentIndex();
		if (comboIndex != -1) {
			sid = ui.cmbApplyUsersSet->currentData().toString();
			// If data starts with S- it's already a SID, otherwise it's a name that needs resolution
			if (!sid.startsWith("S-"))
				sid.clear();
		}

		// If no pre-resolved SID, resolve username to SID
		if (sid.isEmpty()) {
			sid = ResolveNameToSid(userName);
		}

		if (sid.isEmpty()) {
			QMessageBox::critical(this, "MajorPrivacy", tr("Could not resolve user '%1' to SID").arg(userName));
			ui.lstApplyUsers->setEnabled(true);
			ui.cmbApplyUsersSet->setVisible(false);
			ui.btnApplyUsersSet->setVisible(false);
			return;
		}

		if (bAdd) {
			pItem = new QListWidgetItem();
			ui.lstApplyUsers->insertItem(ui.lstApplyUsers->count() - 1, pItem);
		}
		pItem->setText(userName);
		pItem->setData(Qt::UserRole, sid);
	}

	ui.lstApplyUsers->setEnabled(true);
	ui.cmbApplyUsersSet->setVisible(false);
	ui.btnApplyUsersSet->setVisible(false);
}

void CAccessRuleWnd::OnExemptUsersChanged()
{
	ui.lstExemptUsers->setEnabled(ui.chkExcemptUsers->isChecked());
}

void CAccessRuleWnd::OnExemptUsersEdit()
{
	ui.lstExemptUsers->setEnabled(false);
	QListWidgetItem* pItem = ui.lstExemptUsers->currentItem();
	if (pItem->data(Qt::UserRole).toString().isEmpty())
		ui.cmbExemptUsersSet->setCurrentText("");
	else {
		// Try to find matching item in combo by SID
		QString sid = pItem->data(Qt::UserRole).toString();
		int index = ui.cmbExemptUsersSet->findData(sid);
		if (index != -1)
			ui.cmbExemptUsersSet->setCurrentIndex(index);
		else
			ui.cmbExemptUsersSet->setCurrentText(pItem->text());
	}
	ui.cmbExemptUsersSet->setVisible(true);
	ui.btnExemptUsersSet->setVisible(true);
}

void CAccessRuleWnd::OnExemptUsersSet()
{
	QString userName = ui.cmbExemptUsersSet->currentText();

	QListWidgetItem* pItem = ui.lstExemptUsers->currentItem();
	bool bAdd = pItem->data(Qt::UserRole).toString() == "";

	if (userName.isEmpty())
	{
		if (!bAdd) delete pItem;
		pItem = NULL;
	}
	else
	{
		QString sid;

		// Check if an item is selected from the combo box (has pre-resolved SID)
		int comboIndex = ui.cmbExemptUsersSet->currentIndex();
		if (comboIndex != -1) {
			sid = ui.cmbExemptUsersSet->currentData().toString();
			// If data starts with S- it's already a SID, otherwise it's a name that needs resolution
			if (!sid.startsWith("S-"))
				sid.clear();
		}

		// If no pre-resolved SID, resolve username to SID
		if (sid.isEmpty()) {
			sid = ResolveNameToSid(userName);
		}

		if (sid.isEmpty()) {
			QMessageBox::critical(this, "MajorPrivacy", tr("Could not resolve user '%1' to SID").arg(userName));
			ui.lstExemptUsers->setEnabled(true);
			ui.cmbExemptUsersSet->setVisible(false);
			ui.btnExemptUsersSet->setVisible(false);
			return;
		}

		if (bAdd) {
			pItem = new QListWidgetItem();
			ui.lstExemptUsers->insertItem(ui.lstExemptUsers->count() - 1, pItem);
		}
		pItem->setText(userName);
		pItem->setData(Qt::UserRole, sid);
	}

	ui.lstExemptUsers->setEnabled(true);
	ui.cmbExemptUsersSet->setVisible(false);
	ui.btnExemptUsersSet->setVisible(false);
}

void CAccessRuleWnd::OnSidResolved(const QByteArray& Sid, const QString& FullName)
{
	// Convert binary SID back to string for comparison
	LPWSTR sidStr = nullptr;
	if (!ConvertSidToStringSidW((PSID)Sid.data(), &sidStr))
		return;
	QString sidString = QString::fromWCharArray(sidStr);
	LocalFree(sidStr);

	// Update any list items that have this SID
	auto UpdateList = [&](QListWidget* pList) {
		for (int i = 0; i < pList->count(); i++) {
			QListWidgetItem* pItem = pList->item(i);
			if (pItem->data(Qt::UserRole).toString() == sidString) {
				pItem->setText(FullName);
			}
		}
	};

	UpdateList(ui.lstApplyUsers);
	UpdateList(ui.lstExemptUsers);
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

	// For volume rules, shorten the path to ...\suffix
	if (!m_MountPoint.isEmpty() && PathStartsWith(Path, m_MountPoint)) {
		QString Suffix = Path.mid(m_MountPoint.length());
		if (Suffix.startsWith("\\") || Suffix.startsWith("/"))
			Suffix = Suffix.mid(1);
		Path = Split2(m_VolumeImage, "\\", true).second +  "\\" + Suffix;
	}
	else if (!m_VolumeRoot.isEmpty() && PathStartsWith(Path, m_VolumeRoot)) {
		QString Suffix = Path.mid(m_VolumeRoot.length());
		if (Suffix.startsWith("\\") || Suffix.startsWith("/"))
			Suffix = Suffix.mid(1);
		Path = Split2(m_VolumeImage, "\\", true).second +  "\\" + Suffix;
	}

	m_NameHold = true;
	ui.txtName->setText(tr("%1 %2 %3").arg(Action).arg(Path).arg(Program.isEmpty() ? "" : tr(" (%1)").arg(Program)));
	m_NameHold = false;
}

void CAccessRuleWnd::EditScript()
{
	CScriptWindow* pScriptWnd = new CScriptWindow(m_pRule->GetGuid(), EItemType::eResRule, this);
	pScriptWnd->SetScript(m_Script);
	pScriptWnd->SetSaver([&](const QString& Script, bool bApply) -> STATUS {
		m_Script = Script;
		if (bApply) {
			m_pRule->m_Script = Script;
			return theCore->AccessManager()->SetAccessRule(m_pRule);
		}
		return OK;
	});
	SafeShow(pScriptWnd);
}