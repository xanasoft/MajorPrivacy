#include "pch.h"
#include "ProgramRuleWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Enclaves/EnclaveManager.h"
#include "../Core/Network/NetworkManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NetUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"
#include "../Windows/ProgramPicker.h"

CProgramRuleWnd::CProgramRuleWnd(const CProgramRulePtr& pRule, QSet<CProgramItemPtr> Items, QWidget* parent)
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
		m_pRule->m_Name = tr("New Program Rule");
	} else
		m_NameChanged = true;

	ui.cmbEnclave->addItem(tr("Global (Any/No Enclave)"));
	foreach(auto& pEnclave, theCore->EnclaveManager()->GetAllEnclaves()) {
		if(pEnclave->IsSystem()) continue;
		ui.cmbEnclave->addItem(pEnclave->GetName(), pEnclave->GetGuid().ToQV());
	}

	connect(ui.txtName, SIGNAL(textChanged(const QString&)), this, SLOT(OnNameChanged(const QString&)));
	
	connect(ui.btnProg, SIGNAL(clicked()), this, SLOT(OnPickProgram()));
	connect(ui.cmbProgram, SIGNAL(currentIndexChanged(int)), this, SLOT(OnProgramChanged()));
	connect(ui.txtProgPath, SIGNAL(textChanged(const QString&)), this, SLOT(OnProgramPathChanged()));
	//connect(ui.cmbEnclave, SIGNAL(currentIndexChanged(int)), this, SLOT(OnActionChanged()));

	connect(ui.cmbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(OnActionChanged()));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

	AddColoredComboBoxEntry(ui.cmbAction, tr("Allow"), GetActionColor(EExecRuleType::eAllow), (int)EExecRuleType::eAllow);
	AddColoredComboBoxEntry(ui.cmbAction, tr("Block"), GetActionColor(EExecRuleType::eBlock), (int)EExecRuleType::eBlock);
	AddColoredComboBoxEntry(ui.cmbAction, tr("Protect"), GetActionColor(EExecRuleType::eProtect), (int)EExecRuleType::eProtect);
	//AddColoredComboBoxEntry(ui.cmbAction, tr("Isolate"), GetActionColor(EExecRuleType::eIsolate), (int)EExecRuleType::eIsolate); // todo
	AddColoredComboBoxEntry(ui.cmbAction, tr("Audit"), GetActionColor(EExecRuleType::eAudit), (int)EExecRuleType::eAudit);
	ColorComboBox(ui.cmbAction);

	AddColoredComboBoxEntry(ui.cmbSignature, tr("Unconfigured"), GetAuthorityColor(KPH_VERIFY_AUTHORITY::KphUntestedAuthority), (int)KPH_VERIFY_AUTHORITY::KphUntestedAuthority);
	AddColoredComboBoxEntry(ui.cmbSignature, tr("User Signature ONLY (not recommended)"), GetAuthorityColor(KPH_VERIFY_AUTHORITY::KphUserAuthority), (int)KPH_VERIFY_AUTHORITY::KphUserAuthority);
	AddColoredComboBoxEntry(ui.cmbSignature, tr("User + Microsoft/AV Signature"), GetAuthorityColor(KPH_VERIFY_AUTHORITY::KphAvAuthority), (int)KPH_VERIFY_AUTHORITY::KphAvAuthority);
	AddColoredComboBoxEntry(ui.cmbSignature, tr("User + Trusted by Microsoft (Code Root)"), GetAuthorityColor(KPH_VERIFY_AUTHORITY::KphMsCodeAuthority), (int)KPH_VERIFY_AUTHORITY::KphMsCodeAuthority);
	AddColoredComboBoxEntry(ui.cmbSignature, tr("Any Signature (Unknown Root)"), GetAuthorityColor(KPH_VERIFY_AUTHORITY::KphUnkAuthority), (int)KPH_VERIFY_AUTHORITY::KphUnkAuthority);
	AddColoredComboBoxEntry(ui.cmbSignature, tr("No Signature"), GetAuthorityColor(KPH_VERIFY_AUTHORITY::KphNoAuthority), (int)KPH_VERIFY_AUTHORITY::KphNoAuthority);
	ColorComboBox(ui.cmbSignature);

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

	SetComboBoxValue(ui.cmbAction, (int)m_pRule->m_Type);
	SetComboBoxValue(ui.cmbSignature, (int)m_pRule->m_SignatureLevel);
	ui.chkImageProtection->setChecked(m_pRule->m_ImageLoadProtection);

	m_NameHold = false;
}

CProgramRuleWnd::~CProgramRuleWnd()
{
}

bool CProgramRuleWnd::AddProgramItem(const CProgramItemPtr& pItem)
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

QColor CProgramRuleWnd::GetActionColor(EExecRuleType Action)
{
	switch (Action)
	{
	case EExecRuleType::eAllow:		return QColor(144, 238, 144);
	case EExecRuleType::eBlock:		return QColor(255, 182, 193);
	case EExecRuleType::eProtect:	return QColor(173, 216, 230);
	//case EExecRuleType::eIsolate: return QColor(, , ); // todo
	case EExecRuleType::eAudit:		return QColor(193, 193, 193);
	default: return QColor();
	}
}

QColor CProgramRuleWnd::GetAuthorityColor(KPH_VERIFY_AUTHORITY Authority)
{
	switch (Authority)
	{
	case KPH_VERIFY_AUTHORITY::KphUserAuthority: return QColor(144, 238, 144);
	case KPH_VERIFY_AUTHORITY::KphAvAuthority: return QColor(173, 216, 230);
	case KPH_VERIFY_AUTHORITY::KphMsCodeAuthority: return QColor(255, 255, 224);
	case KPH_VERIFY_AUTHORITY::KphUnkAuthority: return QColor(255, 182, 193);
	case KPH_VERIFY_AUTHORITY::KphNoAuthority: return QColor(255, 182, 193);
	default: return QColor();
	}
}

QColor CProgramRuleWnd::GetRoleColor(EExecLogRole Role)
{
	switch (Role)
	{
	case EExecLogRole::eActor:	/*light blue */ return QColor(173, 216, 230);
	case EExecLogRole::eTarget: /*light red*/ return QColor(255, 182, 193);
	case EExecLogRole::eBoth:  /*light green*/ return QColor(144, 238, 144);
	default: return QColor();
	}
}

void CProgramRuleWnd::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

void CProgramRuleWnd::OnSaveAndClose()
{
	if (!Save()) {
		QApplication::beep();
		return;
	}

	STATUS Status = theCore->ProgramManager()->SetProgramRule(m_pRule);	
	if (theGUI->CheckResults(QList<STATUS>() << Status, this))
		return;
	accept();
}

bool CProgramRuleWnd::Save()
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

	m_pRule->m_Enclave = QFlexGuid(GetComboBoxValue(ui.cmbEnclave));

	m_pRule->m_Type = (EExecRuleType)GetComboBoxValue(ui.cmbAction).toInt();
	if (m_pRule->m_Type == EExecRuleType::eAllow) {
		m_pRule->m_SignatureLevel = (KPH_VERIFY_AUTHORITY)GetComboBoxValue(ui.cmbSignature).toInt();
		m_pRule->m_ImageLoadProtection = ui.chkImageProtection->isChecked();
	} else {
		m_pRule->m_SignatureLevel = KphUntestedAuthority;
		m_pRule->m_ImageLoadProtection = false;
	}

	if (m_pRule->m_Type == EExecRuleType::eProtect && m_pRule->m_Enclave.IsNull()) {
		QMessageBox::warning(this, tr("Error"), tr("Please select an enclave for a protection rule."));
		return false;
	}

	return true;
}

void CProgramRuleWnd::OnNameChanged(const QString& Text)
{
	if (m_NameHold) return;
	m_NameChanged = true;
}

void CProgramRuleWnd::OnPickProgram()
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

void CProgramRuleWnd::OnProgramChanged()
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

void CProgramRuleWnd::OnProgramPathChanged()
{
	if(m_HoldProgramPath) return;
	ui.cmbProgram->setCurrentIndex(-1);
}

void CProgramRuleWnd::OnActionChanged()
{
	EExecRuleType Type = (EExecRuleType)GetComboBoxValue(ui.cmbAction).toInt();

	ui.cmbSignature->setEnabled(Type == EExecRuleType::eAllow);
	ui.chkImageProtection->setEnabled(Type == EExecRuleType::eAllow);

	TryMakeName();
}

void CProgramRuleWnd::TryMakeName()
{
	if (ui.txtName->text().isEmpty())
		m_NameChanged = false;
	if (m_NameHold || m_NameChanged)
		return;

	QString Action = ui.cmbAction->currentText();
	QString Program = ui.cmbProgram->currentText();
	if (Action.isEmpty() && Program.isEmpty())
		return;

	m_NameHold = true;
	ui.txtName->setText(tr("%1 %2").arg(Action).arg(Program));
	m_NameHold = false;
}