#include "pch.h"
#include "EnclaveWnd.h"
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
#include "../Helpers/WinHelper.h"
#include "../Core/HashDB/HashDB.h"
#include "../MiscHelpers/Common/WinboxMultiCombo.h"
#include "../Windows/ScriptWindow.h"

CEnclaveWnd::CEnclaveWnd(const CEnclavePtr& pEnclave, QWidget* parent)
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

	m_pEnclave = pEnclave;
	bool bNew = m_pEnclave->m_Guid.IsNull();

	setWindowTitle(bNew ? tr("Create Secure Enclave") : tr("Edit Secure Enclave"));

	QList<QPair<QString,QVariant>> AllCollections;
	AllCollections.append({ tr("None"), QVariant()});
	foreach(const QString& Collection, theCore->HashDB()->GetCollections())
		AllCollections.append({ Collection, Collection });
	m_pCollections = new CWinboxMultiCombo(AllCollections, QList<QVariant>(), this);
	m_pCollections->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	ui.loCI->addWidget(m_pCollections, 3, 1, 1, 1);

	ui.btnAddCollection->setIcon(QIcon(":/Icons/Add.png"));
	connect(ui.btnAddCollection, SIGNAL(clicked(bool)), this, SLOT(OnAddCollection()));

	if (bNew && m_pEnclave->m_Name.isEmpty()) {
		m_pEnclave->m_Name = tr("New Secure Enclave");
		m_pEnclave->m_AllowedSignatures.Windows = TRUE;
		m_pEnclave->m_AllowedSignatures.Antimalware = TRUE;
		m_pEnclave->m_AllowedSignatures.Enclave = TRUE;
	}

	connect(ui.txtName, SIGNAL(textChanged(const QString&)), this, SLOT(OnNameChanged(const QString&)));
	
	connect(ui.chkScript, SIGNAL(stateChanged(int)), this, SLOT(OnActionChanged()));
	connect(ui.btnScript, SIGNAL(clicked()), this, SLOT(EditScript()));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(OnSave()));

	ui.cmbOnTrustedExec->addItem(tr("Allow Execution"), (int)EProgramOnSpawn::eAllow);
	ui.cmbOnTrustedExec->addItem(tr("Eject from Secure Enclave"), (int)EProgramOnSpawn::eEject);
	ui.cmbOnTrustedExec->addItem(tr("Prevent Execution"), (int)EProgramOnSpawn::eBlock);

	ui.cmbOnUnTrustedExec->addItem(tr("Eject from Secure Enclave"), (int)EProgramOnSpawn::eEject);
	ui.cmbOnUnTrustedExec->addItem(tr("Prevent Execution"), (int)EProgramOnSpawn::eBlock);
	ui.cmbOnUnTrustedExec->addItem(tr("Allow Execution"), (int)EProgramOnSpawn::eAllow);

	ui.cmbLevel->addItem(tr("Don't change"), (int)EIntegrityLevel::eUnknown);
	ui.cmbLevel->addItem(tr("Medium +"), (int)EIntegrityLevel::eMediumPlus);
	ui.cmbLevel->addItem(tr("High"), (int)EIntegrityLevel::eHigh);
	ui.cmbLevel->addItem(tr("System"), (int)EIntegrityLevel::eSystem);

	//FixComboBoxEditing(ui.cmbGroup);


	ui.txtName->setText(m_pEnclave->m_Name);
	ui.btnIcon->setIcon(m_pEnclave->GetIcon());
	m_IconFile = m_pEnclave->GetIconFile();
	// todo: load groups
	//SetComboBoxValue(ui.cmbGroup, m_pEnclave->m_Grouping); // todo
	ui.txtInfo->setPlainText(m_pEnclave->m_Description);

	//ui.txtScript->setPlainText(m_pEnclave->m_Script);
	m_Script = m_pEnclave->m_Script;

	ui.chkScript->setChecked(m_pEnclave->m_bUseScript);
	if(m_pEnclave->m_Script.isEmpty())
		ui.btnScript->setIcon(QIcon(":/Icons/Script-Add.png"));
	else
		ui.btnScript->setIcon(QIcon(":/Icons/Script-Edit.png"));

	if(m_pEnclave->m_AllowedSignatures.Windows && m_pEnclave->m_AllowedSignatures.Microsoft && m_pEnclave->m_AllowedSignatures.Antimalware)
		ui.chkAllowMS->setCheckState(Qt::Checked);
	else if(m_pEnclave->m_AllowedSignatures.Windows ||  m_pEnclave->m_AllowedSignatures.Microsoft || m_pEnclave->m_AllowedSignatures.Antimalware)
		ui.chkAllowMS->setCheckState(Qt::PartiallyChecked);
	else
		ui.chkAllowMS->setCheckState(Qt::Unchecked);
	//if(m_pEnclave->m_AllowedSignatures.Authenticode && m_pEnclave->m_AllowedSignatures.Store)
	//	ui.chkAllowAC->setCheckState(Qt::Checked);
	//else if(m_pEnclave->m_AllowedSignatures.Authenticode || m_pEnclave->m_AllowedSignatures.Store)
	//	ui.chkAllowAC->setCheckState(Qt::PartiallyChecked);
	//else
	//	ui.chkAllowAC->setCheckState(Qt::Unchecked);
	ui.chkAllowUser->setChecked(m_pEnclave->m_AllowedSignatures.User);
	ui.chkAllowEnclave->setChecked(m_pEnclave->m_AllowedSignatures.Enclave);

	QList<QVariant> Collections;
	foreach(const QString& Collection, m_pEnclave->m_AllowedCollections)
		Collections.append(Collection);
	m_pCollections->setValues(Collections);

	SetComboBoxValue(ui.cmbOnTrustedExec, (int)m_pEnclave->m_OnTrustedSpawn);
	SetComboBoxValue(ui.cmbOnUnTrustedExec, (int)m_pEnclave->m_OnSpawn);
	ui.chkImageProtection->setChecked(m_pEnclave->m_ImageLoadProtection);
	ui.chkCoherencyChecking->setChecked(m_pEnclave->m_ImageCoherencyChecking);

	SetComboBoxValue(ui.cmbLevel, (int)m_pEnclave->m_IntegrityLevel);

	ui.chkAllowDebugging->setChecked(m_pEnclave->m_AllowDebugging);
	ui.chkKeepAlive->setChecked(m_pEnclave->m_KeepAlive);
}

CEnclaveWnd::~CEnclaveWnd()
{
}

void CEnclaveWnd::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

bool CEnclaveWnd::OnSave()
{
	if (!Save()) {
		QApplication::beep();
		return false;
	}

	STATUS Status = theCore->EnclaveManager()->SetEnclave(m_pEnclave);	
	if (theGUI->CheckResults(QList<STATUS>() << Status, this))
		return false;
	return true;
}

void CEnclaveWnd::OnSaveAndClose()
{
	if(OnSave())
		accept();
}

bool CEnclaveWnd::Save()
{
	m_pEnclave->m_Name = ui.txtName->text();
	m_pEnclave->SetIconFile(m_IconFile);
	//m_pEnclave->m_Grouping = GetComboBoxValue(ui.cmbGroup).toString(); // todo
	m_pEnclave->m_Description = ui.txtInfo->toPlainText();
	
	//m_pEnclave->m_Script = ui.txtScript->toPlainText();
	m_pEnclave->m_Script = m_Script;

	m_pEnclave->m_bUseScript = ui.chkScript->isChecked();

	switch(ui.chkAllowMS->checkState()) {
		case Qt::Checked:	m_pEnclave->m_AllowedSignatures.Windows = TRUE; m_pEnclave->m_AllowedSignatures.Microsoft = TRUE; m_pEnclave->m_AllowedSignatures.Antimalware = TRUE; break;
		case Qt::Unchecked:	m_pEnclave->m_AllowedSignatures.Windows = FALSE; m_pEnclave->m_AllowedSignatures.Microsoft = FALSE; m_pEnclave->m_AllowedSignatures.Antimalware = FALSE; break;
	}
	//switch(ui.chkAllowAC->checkState()) {
	//case Qt::Checked:	m_pEnclave->m_AllowedSignatures.Authenticode = TRUE; m_pEnclave->m_AllowedSignatures.Store = TRUE; break;
	//case Qt::Unchecked:	m_pEnclave->m_AllowedSignatures.Authenticode = FALSE; m_pEnclave->m_AllowedSignatures.Store = FALSE; break;
	//}
	m_pEnclave->m_AllowedSignatures.User = ui.chkAllowUser->isChecked();
	m_pEnclave->m_AllowedSignatures.Enclave = ui.chkAllowEnclave->isChecked();

	m_pEnclave->m_AllowedCollections.clear();
	foreach(QVariant Collection, m_pCollections->values()) {
		if (!Collection.isNull() && !m_pEnclave->m_AllowedCollections.contains(Collection.toString()))
			m_pEnclave->m_AllowedCollections.append(Collection.toString());
	}

	m_pEnclave->m_OnTrustedSpawn = (EProgramOnSpawn)GetComboBoxValue(ui.cmbOnTrustedExec).toInt();
	m_pEnclave->m_OnSpawn = (EProgramOnSpawn)GetComboBoxValue(ui.cmbOnUnTrustedExec).toInt();
	m_pEnclave->m_ImageLoadProtection = ui.chkImageProtection->isChecked();
	m_pEnclave->m_ImageCoherencyChecking = ui.chkCoherencyChecking->isChecked();

	m_pEnclave->m_IntegrityLevel = (EIntegrityLevel)GetComboBoxValue(ui.cmbLevel).toInt();

	m_pEnclave->m_AllowDebugging = ui.chkAllowDebugging->isChecked();
	m_pEnclave->m_KeepAlive = ui.chkKeepAlive->isChecked();

	return true;
}

void CEnclaveWnd::PickIcon()
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

void CEnclaveWnd::BrowseImage()
{
	QString Value = QFileDialog::getOpenFileName(this, tr("Select Image File"), "", tr("Image Files (*.png)")).replace("/", "\\");
	if (Value.isEmpty())
		return;

	ui.btnIcon->setIcon(QIcon(Value));
	m_IconFile = Value;
}

void CEnclaveWnd::OnNameChanged(const QString& Text)
{
}

void CEnclaveWnd::OnAddCollection()
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

void CEnclaveWnd::EditScript()
{
	CScriptWindow* pScriptWnd = new CScriptWindow(m_pEnclave->GetGuid(), EScriptTypes::eEnclave, this);
	pScriptWnd->SetScript(m_Script);
	pScriptWnd->SetSaver([&](const QString& Script, bool bApply){
		m_Script = Script;
		if (bApply) {
			m_pEnclave->m_Script = Script;
			return theCore->EnclaveManager()->SetEnclave(m_pEnclave);
		}
		return OK;
	});
	SafeShow(pScriptWnd);
}