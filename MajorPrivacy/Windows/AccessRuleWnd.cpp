#include "pch.h"
#include "AccessRuleWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Access/AccessManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NetUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MajorPrivacy.h"

CAccessRuleWnd::CAccessRuleWnd(const CAccessRulePtr& pRule, QSet<CProgramItemPtr> Items, QWidget* parent)
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
		m_pRule->m_Name = tr("New Access Rule");
	}

	connect(ui.cmbProgram, SIGNAL(currentIndexChanged(int)), this, SLOT(OnProgramChanged()));


	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

	ui.cmbAction->addItem(tr("Allow"), (int)EAccessRuleType::eAllow);
	ui.cmbAction->addItem(tr("Read Only"), (int)EAccessRuleType::eAllowRO);
	ui.cmbAction->addItem(tr("Directory Listing"), (int)EAccessRuleType::eEnum);
	ui.cmbAction->addItem(tr("Protect"), (int)EAccessRuleType::eProtect);
	ui.cmbAction->addItem(tr("Block"), (int)EAccessRuleType::eBlock);
	

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

	ui.txtPath->setText(m_pRule->m_AccessPath);

	SetComboBoxValue(ui.cmbAction, (int)m_pRule->m_Type);
}

CAccessRuleWnd::~CAccessRuleWnd()
{
}

void CAccessRuleWnd::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

void CAccessRuleWnd::OnSaveAndClose()
{
	if (!Save()) {
		QApplication::beep();
		return;
	}

	STATUS Status = theCore->AccessManager()->SetAccessRule(m_pRule);	
	if (theGUI->CheckResults(QList<STATUS>() << Status, this))
		return;
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
	m_pRule->m_ProgramID.SetPath(QString::fromStdWString(DosPathToNtPath(ui.txtProgPath->text().toStdWString())));
	m_pRule->m_ProgramPath = ui.txtProgPath->text();

	m_pRule->m_AccessPath = ui.txtPath->text();

	m_pRule->m_Type = (EAccessRuleType)GetComboBoxValue(ui.cmbAction).toInt();

	return true;
}

void CAccessRuleWnd::OnProgramChanged()
{
	int Index = ui.cmbProgram->currentIndex();
	if (Index == -1) return;

	CProgramItemPtr pItem = m_Items[Index];
	//if (pItem) ui.cmbProgram->setCurrentText(pItem->GetName());
	CProgramID ID = pItem->GetID();
	ui.txtProgPath->setText(pItem->GetPath(EPathType::eWin32));
}
