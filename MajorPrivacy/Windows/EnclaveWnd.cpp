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

	if (bNew && m_pEnclave->m_Name.isEmpty()) {
		m_pEnclave->m_Name = tr("New Secure Enclave");
		m_pEnclave->m_SignatureLevel = KPH_VERIFY_AUTHORITY::KphMsCodeAuthority;
	}

	connect(ui.txtName, SIGNAL(textChanged(const QString&)), this, SLOT(OnNameChanged(const QString&)));
	
	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));

	AddColoredComboBoxEntry(ui.cmbSignature, tr("User Signature ONLY (not recommended)"), QColor(144, 238, 144), (int)KPH_VERIFY_AUTHORITY::KphUserAuthority);
	AddColoredComboBoxEntry(ui.cmbSignature, tr("User + Microsoft/AV Signature"), QColor(173, 216, 230), (int)KPH_VERIFY_AUTHORITY::KphAvAuthority);
	AddColoredComboBoxEntry(ui.cmbSignature, tr("User + Trusted by Microsoft (Code Root)"), QColor(255, 255, 224), (int)KPH_VERIFY_AUTHORITY::KphMsCodeAuthority);
	AddColoredComboBoxEntry(ui.cmbSignature, tr("Any Signature (Unknown Root)"), QColor(255, 182, 193), (int)KPH_VERIFY_AUTHORITY::KphUnkAuthority);
	AddColoredComboBoxEntry(ui.cmbSignature, tr("No Signature"), QColor(255, 182, 193), (int)KPH_VERIFY_AUTHORITY::KphNoAuthority);
	ColorComboBox(ui.cmbSignature);

	ui.cmbOnTrustedExec->addItem(tr("Allow Execution"), (int)EProgramOnSpawn::eAllow);
	ui.cmbOnTrustedExec->addItem(tr("Eject from Secure Enclave"), (int)EProgramOnSpawn::eEject);
	ui.cmbOnTrustedExec->addItem(tr("Prevent Execution"), (int)EProgramOnSpawn::eBlock);

	ui.cmbOnUnTrustedExec->addItem(tr("Eject from Secure Enclave"), (int)EProgramOnSpawn::eEject);
	ui.cmbOnUnTrustedExec->addItem(tr("Prevent Execution"), (int)EProgramOnSpawn::eBlock);
	ui.cmbOnUnTrustedExec->addItem(tr("Allow Execution"), (int)EProgramOnSpawn::eAllow);

	//FixComboBoxEditing(ui.cmbGroup);


	ui.txtName->setText(m_pEnclave->m_Name);
	ui.btnIcon->setIcon(m_pEnclave->GetIcon());
	m_IconFile = m_pEnclave->GetIconFile();
	// todo: load groups
	//SetComboBoxValue(ui.cmbGroup, m_pEnclave->m_Grouping); // todo
	ui.txtInfo->setPlainText(m_pEnclave->m_Description);

	SetComboBoxValue(ui.cmbSignature, (int)m_pEnclave->m_SignatureLevel);
	SetComboBoxValue(ui.cmbOnTrustedExec, (int)m_pEnclave->m_OnTrustedSpawn);
	SetComboBoxValue(ui.cmbOnUnTrustedExec, (int)m_pEnclave->m_OnSpawn);
	ui.chkImageProtection->setChecked(m_pEnclave->m_ImageLoadProtection);

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

void CEnclaveWnd::OnSaveAndClose()
{
	if (!Save()) {
		QApplication::beep();
		return;
	}

	STATUS Status = theCore->EnclaveManager()->SetEnclave(m_pEnclave);	
	if (theGUI->CheckResults(QList<STATUS>() << Status, this))
		return;
	accept();
}

bool CEnclaveWnd::Save()
{
	m_pEnclave->m_Name = ui.txtName->text();
	m_pEnclave->SetIconFile(m_IconFile);
	//m_pEnclave->m_Grouping = GetComboBoxValue(ui.cmbGroup).toString(); // todo
	m_pEnclave->m_Description = ui.txtInfo->toPlainText();

	m_pEnclave->m_SignatureLevel = (KPH_VERIFY_AUTHORITY)GetComboBoxValue(ui.cmbSignature).toInt();
	m_pEnclave->m_OnTrustedSpawn = (EProgramOnSpawn)GetComboBoxValue(ui.cmbOnTrustedExec).toInt();
	m_pEnclave->m_OnSpawn = (EProgramOnSpawn)GetComboBoxValue(ui.cmbOnUnTrustedExec).toInt();
	m_pEnclave->m_ImageLoadProtection = ui.chkImageProtection->isChecked();

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
