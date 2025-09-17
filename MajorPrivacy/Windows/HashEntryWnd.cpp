#include "pch.h"
#include "HashEntryWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Enclaves/EnclaveManager.h"
#include "../Core/HashDB/HashDB.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NetUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/WinboxMultiCombo.h"
#include "../MajorPrivacy.h"
#include "../Helpers/WinHelper.h"

CHashEntryWnd::CHashEntryWnd(const CHashPtr& pEntry, QWidget* parent)
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

	m_pEntry = pEntry;
	bool bNew = pEntry->GetHash().isEmpty();

	setWindowTitle(bNew ? tr("Create Hash DB Entry") : tr("Edit HashDB Entry"));
	
	QList<QPair<QString,QVariant>> AllEnclaves;
	AllEnclaves.append({ tr("None"), QVariant()});
	foreach(const CEnclavePtr& pEnclave, theCore->EnclaveManager()->GetAllEnclaves())
		AllEnclaves.append({pEnclave->GetName(), pEnclave->GetGuid().ToQV()});
	m_pEnclaves = new CWinboxMultiCombo(AllEnclaves, QList<QVariant>(), this);
	ui.loAffiliation->addWidget(m_pEnclaves, 0, 1, 1, 1);

	QList<QPair<QString,QVariant>> AllCollections;
	AllCollections.append({ tr("None"), QVariant()});
	foreach(const QString& Collection, theCore->HashDB()->GetCollections())
		AllCollections.append({ Collection, Collection });
	m_pCollections = new CWinboxMultiCombo(AllCollections, QList<QVariant>(), this);
	ui.loAffiliation->addWidget(m_pCollections, 1, 1, 1, 1);

	ui.btnAddCollection->setIcon(QIcon(":/Icons/Add.png"));
	connect(ui.btnAddCollection, SIGNAL(clicked(bool)), this, SLOT(OnAddCollection()));

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(OnSave()));

	ui.cmbType->setEnabled(bNew);
	ui.cmbType->addItem(tr(""), (int)EHashType::eUnknown);
	ui.cmbType->addItem(tr("File"), (int)EHashType::eFileHash);
	ui.cmbType->addItem(tr("Certificate"), (int)EHashType::eCertHash);
	if (!bNew) SetComboBoxValue(ui.cmbType, (int)pEntry->GetType());

	ui.txtHash->setEnabled(bNew);
	ui.txtHash->setText(pEntry->GetHash().toHex());

	ui.txtName->setText(pEntry->m_Name);
	ui.txtInfo->setPlainText(pEntry->m_Description);

	QList<QVariant> Enclaves;
	foreach(const QFlexGuid& EnclaveId, pEntry->m_Enclaves)
		Enclaves.append(EnclaveId.ToQV());
	m_pEnclaves->setValues(Enclaves);

	QList<QVariant> Collections;
	foreach(const QString& Collection, pEntry->m_Collections)
		Collections.append(Collection);
	m_pCollections->setValues(Collections);
}

CHashEntryWnd::~CHashEntryWnd()
{
}

void CHashEntryWnd::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

bool CHashEntryWnd::OnSave()
{
	if (!Save()) {
		QApplication::beep();
		return false;
	}

	STATUS Status = theCore->HashDB()->SetHash(m_pEntry);	
	if (theGUI->CheckResults(QList<STATUS>() << Status, this))
		return false;
	return true;
}

void CHashEntryWnd::OnSaveAndClose()
{
	if(OnSave())
		accept();
}

bool CHashEntryWnd::Save()
{
	bool bNew = m_pEntry->m_Hash.isEmpty();
	if (bNew) 
	{
		EHashType Type = (EHashType)ui.cmbType->currentData().toInt();
		if (Type == EHashType::eUnknown) {
			QMessageBox::warning(this, "MajorPrivacy", tr("Please select a type."));
			return false;
		}

		QString Hash = ui.txtHash->text();
		Hash.remove(' ');
		static const QRegularExpression re(QStringLiteral("^[0-9A-Fa-f]+$"));
		if (!re.match(Hash).hasMatch() || Hash.length() < 2 || (Hash.length() % 2) != 0) {
			QMessageBox::warning(this, "MajorPrivacy", tr("Please enter a valid hash value using hexadecimal notation."));
			return false;
		}

		m_pEntry->m_Type = Type;
		m_pEntry->m_Hash = QByteArray::fromHex(Hash.toUtf8());
	}

	m_pEntry->m_Name = ui.txtName->text();
	m_pEntry->m_Description = ui.txtInfo->toPlainText();

	int VolumeEnclaves = 0;
	m_pEntry->m_Enclaves.clear();
	foreach(QFlexGuid EnclaveId, m_pEnclaves->values()) {
		if (!EnclaveId.IsNull() && !m_pEntry->m_Enclaves.contains(EnclaveId)) {
			m_pEntry->m_Enclaves.append(EnclaveId);
			CEnclavePtr pEnclave = theCore->EnclaveManager()->GetEnclave(EnclaveId);
			if(!pEnclave->GetVolumeGuid().IsNull())
				VolumeEnclaves++;
		}
	}

	m_pEntry->m_Collections.clear();
	foreach(QVariant Collection, m_pCollections->values()) {
		if (!Collection.isNull() && !m_pEntry->m_Collections.contains(Collection.toString()))
			m_pEntry->m_Collections.append(Collection.toString());
	}

	if (bNew) {
		if(VolumeEnclaves > 0 && VolumeEnclaves == m_pEntry->m_Enclaves.count() && m_pEntry->m_Collections.isEmpty())
			m_pEntry->m_bTemporary = true;
	}

	return true;
}

void CHashEntryWnd::OnAddCollection()
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