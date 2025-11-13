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
#include "../Core/HashDB/HashDB.h"
#include "../MiscHelpers/Common/WinboxMultiCombo.h"
#include "../Windows/ScriptWindow.h"

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

	QList<QPair<QString,QVariant>> AllCollections;
	AllCollections.append({ tr("None"), QVariant()});
	foreach(const QString& Collection, theCore->HashDB()->GetCollections())
		AllCollections.append({ Collection, Collection });
	m_pCollections = new CWinboxMultiCombo(AllCollections, QList<QVariant>(), this);
	m_pCollections->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	ui.loCI->addWidget(m_pCollections, 3, 1, 1, 1);

	ui.btnAddCollection->setIcon(QIcon(":/Icons/Add.png"));
	connect(ui.btnAddCollection, SIGNAL(clicked(bool)), this, SLOT(OnAddCollection()));

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

	connect(ui.chkScript, SIGNAL(stateChanged(int)), this, SLOT(OnActionChanged()));
	connect(ui.btnScript, SIGNAL(clicked()), this, SLOT(EditScript()));

	//connect(ui.cmbDllMode, SIGNAL(currentIndexChanged(int)), this, SLOT(OnActionChanged()));

	connect(ui.cmbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(OnActionChanged()));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(OnSave()));

	AddColoredComboBoxEntry(ui.cmbAction, tr("Allow"), GetActionColor(EExecRuleType::eAllow), (int)EExecRuleType::eAllow);
	AddColoredComboBoxEntry(ui.cmbAction, tr("Block"), GetActionColor(EExecRuleType::eBlock), (int)EExecRuleType::eBlock);
	AddColoredComboBoxEntry(ui.cmbAction, tr("Protect"), GetActionColor(EExecRuleType::eProtect), (int)EExecRuleType::eProtect);
	//AddColoredComboBoxEntry(ui.cmbAction, tr("Isolate"), GetActionColor(EExecRuleType::eIsolate), (int)EExecRuleType::eIsolate); // todo
	//AddColoredComboBoxEntry(ui.cmbAction, tr("Audit"), GetActionColor(EExecRuleType::eAudit), (int)EExecRuleType::eAudit);
	ColorComboBox(ui.cmbAction);

	ui.cmbDllMode->addItem(tr("Default"), (int)EExecDllMode::eDefault);
	ui.cmbDllMode->addItem(tr("Inject Low (Exclusive)"), (int)EExecDllMode::eInjectLow);
	ui.cmbDllMode->addItem(tr("Inject High (Sbie+ Compatible)"), (int)EExecDllMode::eInjectHigh);
	ui.cmbDllMode->addItem(tr("Disabled"), (int)EExecDllMode::eDisabled);

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

	//ui.txtScript->setPlainText(m_pRule->m_Script);
	m_Script = m_pRule->m_Script;

	ui.chkScript->setChecked(m_pRule->m_bUseScript);
	if(m_pRule->m_Script.isEmpty())
		ui.btnScript->setIcon(QIcon(":/Icons/Script-Add.png"));
	else
		ui.btnScript->setIcon(QIcon(":/Icons/Script-Edit.png"));

	SetComboBoxValue(ui.cmbAction, (int)m_pRule->m_Type);

	SetComboBoxValue(ui.cmbDllMode, (int)m_pRule->m_DllMode);

	if(m_pRule->m_AllowedSignatures.Windows && m_pRule->m_AllowedSignatures.Microsoft && m_pRule->m_AllowedSignatures.Antimalware)
		ui.chkAllowMS->setCheckState(Qt::Checked);
	else if(m_pRule->m_AllowedSignatures.Windows || m_pRule->m_AllowedSignatures.Microsoft || m_pRule->m_AllowedSignatures.Antimalware)
		ui.chkAllowMS->setCheckState(Qt::PartiallyChecked);
	else
		ui.chkAllowMS->setCheckState(Qt::Unchecked);
	//if(m_pRule->m_AllowedSignatures.Authenticode && m_pRule->m_AllowedSignatures.Store)
	//	ui.chkAllowAC->setCheckState(Qt::Checked);
	//else if(m_pRule->m_AllowedSignatures.Authenticode || m_pRule->m_AllowedSignatures.Store)
	//	ui.chkAllowAC->setCheckState(Qt::PartiallyChecked);
	//else
	//	ui.chkAllowAC->setCheckState(Qt::Unchecked);
	
	if(m_pRule->m_AllowedSignatures.UserDB && m_pRule->m_AllowedSignatures.UserSign)
		ui.chkAllowUser->setCheckState(Qt::Checked);
	else if (m_pRule->m_AllowedSignatures.UserDB || m_pRule->m_AllowedSignatures.UserSign)
		ui.chkAllowUser->setCheckState(Qt::PartiallyChecked);
	else
		ui.chkAllowUser->setCheckState(Qt::Unchecked);
	ui.chkAllowEnclave->setChecked(m_pRule->m_AllowedSignatures.EnclaveDB);

	QList<QVariant> Collections;
	foreach(const QString& Collection, m_pRule->m_AllowedCollections)
		Collections.append(Collection);
	m_pCollections->setValues(Collections);

	ui.chkImageProtection->setChecked(m_pRule->m_ImageLoadProtection);
	ui.chkCoherencyChecking->setChecked(m_pRule->m_ImageCoherencyChecking);

	m_NameHold = false;


	restoreGeometry(theConf->GetBlob("ProgramRuleWindow/Window_Geometry"));
}

CProgramRuleWnd::~CProgramRuleWnd()
{
	theConf->SetBlob("ProgramRuleWindow/Window_Geometry", saveGeometry());
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
	//case EExecRuleType::eAudit:		return QColor(193, 193, 193);
	default: return QColor();
	}
}

/*QColor CProgramRuleWnd::GetAuthorityColor(KPH_VERIFY_AUTHORITY Authority)
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
}*/

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

bool CProgramRuleWnd::OnSave()
{
	if (!Save()) {
		QApplication::beep();
		return false;
	}

	auto Ret = theCore->ProgramManager()->SetProgramRule(m_pRule);	
	if (theGUI->CheckResults(QList<STATUS>() << Ret, this))
		return false;

	if(m_pRule->m_Guid.IsNull())
		m_pRule->m_Guid = Ret.GetValue();
}

void CProgramRuleWnd::OnSaveAndClose()
{
	if(OnSave())
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
	if (!CProgramRule::IsPathValid(m_pRule->m_ProgramPath)) {
		QMessageBox::warning(this, tr("Error"), tr("The Program Path is not valid."));
		return false;
	}
	if (CProgramRule::IsUnsafePath(m_pRule->m_ProgramPath)) {
		QMessageBox::warning(this, tr("Error"), tr("The Program Path may break windows, please use a more specific path."));
		return false;
	}

	m_pRule->m_Enclave = QFlexGuid(GetComboBoxValue(ui.cmbEnclave));

	//m_pRule->m_Script = ui.txtScript->toPlainText();
	m_pRule->m_Script = m_Script;

	m_pRule->m_bUseScript = ui.chkScript->isChecked();

	m_pRule->m_DllMode = (EExecDllMode)GetComboBoxValue(ui.cmbDllMode).toInt();

	m_pRule->m_Type = (EExecRuleType)GetComboBoxValue(ui.cmbAction).toInt();
	if (m_pRule->m_Type == EExecRuleType::eAllow) {
		
		switch(ui.chkAllowMS->checkState()) {
		case Qt::Checked:	m_pRule->m_AllowedSignatures.Windows = TRUE; m_pRule->m_AllowedSignatures.Microsoft = TRUE; m_pRule->m_AllowedSignatures.Antimalware = TRUE; break;
		case Qt::Unchecked:	m_pRule->m_AllowedSignatures.Windows = FALSE; m_pRule->m_AllowedSignatures.Microsoft = FALSE; m_pRule->m_AllowedSignatures.Antimalware = FALSE; break;
		}

		//switch(ui.chkAllowAC->checkState()) {
		//case Qt::Checked:	m_pRule->m_AllowedSignatures.Authenticode = TRUE; m_pRule->m_AllowedSignatures.Store = TRUE; break;
		//case Qt::Unchecked:	m_pRule->m_AllowedSignatures.Authenticode = FALSE; m_pRule->m_AllowedSignatures.Store = FALSE; break;
		//}

		switch(ui.chkAllowUser->checkState()) {
		case Qt::Checked:	m_pRule->m_AllowedSignatures.UserDB = TRUE; m_pRule->m_AllowedSignatures.UserSign = TRUE; break;
		case Qt::Unchecked:	m_pRule->m_AllowedSignatures.UserDB = FALSE; m_pRule->m_AllowedSignatures.UserSign = FALSE; break;
		}
		m_pRule->m_AllowedSignatures.EnclaveDB = ui.chkAllowEnclave->isChecked();

		m_pRule->m_AllowedCollections.clear();
		foreach(QVariant Collection, m_pCollections->values()) {
			if (!Collection.isNull() && !m_pRule->m_AllowedCollections.contains(Collection.toString()))
				m_pRule->m_AllowedCollections.append(Collection.toString());
		}

		m_pRule->m_ImageLoadProtection = ui.chkImageProtection->isChecked();
		m_pRule->m_ImageCoherencyChecking = ui.chkCoherencyChecking->isChecked();
	} else {
		m_pRule->m_AllowedSignatures.Value = 0;
		m_pRule->m_AllowedCollections.clear();

		m_pRule->m_ImageLoadProtection = false;
		m_pRule->m_ImageCoherencyChecking = false;
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

	ui.chkAllowMS->setEnabled(Type == EExecRuleType::eAllow);
	//ui.chkAllowAC->setEnabled(Type == EExecRuleType::eAllow);
	ui.chkAllowUser->setEnabled(Type == EExecRuleType::eAllow);
	ui.chkAllowEnclave->setEnabled(Type == EExecRuleType::eAllow);

	m_pCollections->setEnabled(Type == EExecRuleType::eAllow);

	ui.chkImageProtection->setEnabled(Type == EExecRuleType::eAllow);
	ui.chkCoherencyChecking->setEnabled(Type == EExecRuleType::eAllow);

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

void CProgramRuleWnd::OnAddCollection()
{
	QString Collection = QInputDialog::getText(this, tr("Add Collection"), tr("Enter the name of the collection:"));
	if (Collection.isEmpty())
		return;

	theCore->HashDB()->AddCollection(Collection);

	QList<QPair<QString,QVariant>> AllCollections;
	AllCollections.append({ tr("None"), QVariant()});
	foreach(const QString& Collection, theCore->HashDB()->GetCollections())
		AllCollections.append({ Collection, Collection });
	m_pCollections->setItems(AllCollections);
}

void CProgramRuleWnd::EditScript()
{
	CScriptWindow* pScriptWnd = new CScriptWindow(m_pRule->GetGuid(), EItemType::eExecRule, this);
	pScriptWnd->SetScript(m_Script);
	pScriptWnd->SetSaver([&](const QString& Script, bool bApply) -> STATUS{
		m_Script = Script;
		if (bApply) {
			m_pRule->m_Script = Script;
			return theCore->ProgramManager()->SetProgramRule(m_pRule);
		}
		return OK;
	});
	SafeShow(pScriptWnd);
}