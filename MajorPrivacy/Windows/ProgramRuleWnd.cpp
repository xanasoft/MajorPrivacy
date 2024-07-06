#include "pch.h"
#include "ProgramRuleWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Network/NetworkManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NetUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"

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
	bool bNew = m_pRule->m_Guid.isEmpty();

	setWindowTitle(bNew ? tr("Create Program Rule") : tr("Edit Program Rule"));

	foreach(auto pItem, Items) 
	{
		switch (pItem->GetID().GetType())
		{
		case EProgramType::eProgramFile:
		case EProgramType::eFilePattern:
		case EProgramType::eAppInstallation:
		case EProgramType::eAllPrograms:
			break;
		default:
			continue;
		}

		m_Items.append(pItem);
		ui.cmbProgram->addItem(pItem->GetNameEx());
	}

	if (bNew && m_pRule->m_Name.isEmpty()) {
		m_pRule->m_Name = tr("New Program Rule");
		m_pRule->m_SignatureLevel = KPH_VERIFY_AUTHORITY::KphMsCodeAuthority;
	}

	connect(ui.cmbProgram, SIGNAL(currentIndexChanged(int)), this, SLOT(OnProgramChanged()));


	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

	ui.cmbAction->addItem(tr("Allow"), (int)EExecRuleType::eAllow);
	ui.cmbAction->addItem(tr("Block"), (int)EExecRuleType::eBlock);
	ui.cmbAction->addItem(tr("Protect"), (int)EExecRuleType::eProtect);
	//ui.cmbAction->addItem(tr("Isolate"), (int)EExecRuleType::eIsolate); // todo

	ui.cmbSignature->addItem(tr("User Signature ONLY"), (int)KPH_VERIFY_AUTHORITY::KphUserAuthority);
	ui.cmbSignature->addItem(tr("User + Microsoft/AV Signature"), (int)KPH_VERIFY_AUTHORITY::KphAvAuthority);
	ui.cmbSignature->addItem(tr("User + Trusted by Microsoft (Code Root)"), (int)KPH_VERIFY_AUTHORITY::KphMsCodeAuthority);
	ui.cmbSignature->addItem(tr("Any Signature (Unknown Root)"), (int)KPH_VERIFY_AUTHORITY::KphUnkAuthority);
	ui.cmbSignature->addItem(tr("No Signature"), (int)KPH_VERIFY_AUTHORITY::KphNoAuthority);

	ui.cmbOnTrustedExec->addItem(tr("Allow Execution"), (int)EProgramOnSpawn::eAllow);
	ui.cmbOnTrustedExec->addItem(tr("Eject from Secure Enclave"), (int)EProgramOnSpawn::eEject);
	ui.cmbOnTrustedExec->addItem(tr("Prevent Execution"), (int)EProgramOnSpawn::eBlock);

	ui.cmbOnUnTrustedExec->addItem(tr("Eject from Secure Enclave"), (int)EProgramOnSpawn::eEject);
	ui.cmbOnUnTrustedExec->addItem(tr("Prevent Execution"), (int)EProgramOnSpawn::eBlock);
	ui.cmbOnUnTrustedExec->addItem(tr("Allow Execution"), (int)EProgramOnSpawn::eAllow);

	//FixComboBoxEditing(ui.cmbGroup);


	ui.txtName->setText(m_pRule->m_Name);
	// todo: load groups
	//SetComboBoxValue(ui.cmbGroup, m_pRule->m_Grouping); // todo
	ui.txtInfo->setPlainText(m_pRule->m_Description);

	ui.cmbProgram->setEditable(true);
	ui.cmbProgram->lineEdit()->setReadOnly(true);
	CProgramItemPtr pItem;
	if (m_pRule->m_ProgramID.GetType() != EProgramType::eUnknown)
		pItem = theCore->ProgramManager()->GetProgramByID(m_pRule->m_ProgramID);
	else
		pItem = theCore->ProgramManager()->GetAll();
	int Index = m_Items.indexOf(pItem);
	ui.cmbProgram->setCurrentIndex(Index);
	if(bNew)
		OnProgramChanged();
	else
		ui.txtProgPath->setText(m_pRule->GetProgramPath());

	SetComboBoxValue(ui.cmbAction, (int)m_pRule->m_Type);
	SetComboBoxValue(ui.cmbSignature, (int)m_pRule->m_SignatureLevel);
	SetComboBoxValue(ui.cmbOnTrustedExec, (int)m_pRule->m_OnTrustedSpawn);
	SetComboBoxValue(ui.cmbOnUnTrustedExec, (int)m_pRule->m_OnSpawn);
	ui.chkImageProtection->setChecked(m_pRule->m_ImageLoadProtection);
}

CProgramRuleWnd::~CProgramRuleWnd()
{
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
	m_pRule->m_ProgramID.SetPath(QString::fromStdWString(DosPathToNtPath(ui.txtProgPath->text().toStdWString())));
	m_pRule->m_ProgramPath = ui.txtProgPath->text();

	m_pRule->m_Type = (EExecRuleType)GetComboBoxValue(ui.cmbAction).toInt();
	m_pRule->m_SignatureLevel = (KPH_VERIFY_AUTHORITY)GetComboBoxValue(ui.cmbSignature).toInt();
	m_pRule->m_OnTrustedSpawn = (EProgramOnSpawn)GetComboBoxValue(ui.cmbOnTrustedExec).toInt();
	m_pRule->m_OnSpawn = (EProgramOnSpawn)GetComboBoxValue(ui.cmbOnUnTrustedExec).toInt();
	m_pRule->m_ImageLoadProtection = ui.chkImageProtection->isChecked();

	return true;
}

void CProgramRuleWnd::OnProgramChanged()
{
	int Index = ui.cmbProgram->currentIndex();
	if (Index == -1) return;

	CProgramItemPtr pItem = m_Items[Index];
	//if (pItem) ui.cmbProgram->setCurrentText(pItem->GetName());
	CProgramID ID = pItem->GetID();
	ui.txtProgPath->setText(pItem->GetPath(EPathType::eWin32));
}
