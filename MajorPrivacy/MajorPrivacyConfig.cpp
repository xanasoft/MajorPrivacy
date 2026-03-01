#include "pch.h"
#include <algorithm>
#include <QTextBrowser>
#include <QMimeData>
#include <QTreeWidget>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include "MajorPrivacy.h"
#include "Core/PrivacyCore.h"
#include "Core/EventLog.h"
#include "Core/Programs/ProgramManager.h"
#include "../MiscHelpers/Common/Finder.h"
#include "../Library/Crypto/Encryption.h"
#include "../Library/Crypto/PrivateKey.h"
#include "../Library/Crypto/PublicKey.h"
#include "../Library/Crypto/HashFunction.h"


#include <Windows.h>
#include <ShellApi.h>

////////////////////////////////////////////////////////////////////////////////////////
// CConfigChangeDialog - Dialog showing pending configuration changes
//

class CConfigChangeDialog : public QDialog
{
public:
	enum EResult {
		eCancel = 0,
		eCommit,
		eDiscard
	};

protected:
	// Track state per GUID: we need to determine the net effect
	// State transitions:
	//   - Added: rule was added
	//   - Modified: rule was modified (only if not added/removed)
	//   - Removed: rule was removed
	//   - AddedThenRemoved: rule was added then removed (net zero, don't show)
	enum EChangeState { eNone = 0, eAdded, eModified, eRemoved, eAddedThenRemoved };

	struct SChangeInfo {
		EChangeState State = eNone;
		QString Type;
		QIcon TypeIcon;
		QString Name;
		QString Guid;
		ELogEventType EventType = eLogUnknown;
	};

public:

	CConfigChangeDialog(bool bForCommit, QWidget* parent = nullptr)
		: QDialog(parent)
		, m_bForCommit(bForCommit)
		, m_Result(eCancel)
	{
		setWindowTitle(tr("Configuration Changes"));
		setMinimumSize(600, 400);
		resize(700, 500);

		QVBoxLayout* pLayout = new QVBoxLayout(this);

		QString prompt = bForCommit
			? tr("The following configuration changes will be committed:")
			: tr("The following configuration changes will be discarded:");
		QLabel* pLabel = new QLabel(prompt);
		pLayout->addWidget(pLabel);

		// Create container widget for tree and finder
		QWidget* pTreeContainer = new QWidget();
		QVBoxLayout* pTreeLayout = new QVBoxLayout(pTreeContainer);
		pTreeLayout->setContentsMargins(0, 0, 0, 0);

		m_pTree = new QTreeWidget();
		m_pTree->setHeaderLabels(QStringList() << tr("Type") << tr("Action") << tr("Name"));
		m_pTree->setRootIsDecorated(false);
		m_pTree->setAlternatingRowColors(true);
		m_pTree->header()->setStretchLastSection(true);
		pTreeLayout->addWidget(m_pTree);

		// Add CFinder for search functionality
		CFinder* pFinder = new CFinder(nullptr, pTreeContainer, CFinder::eRegExp | CFinder::eCaseSens);
		pTreeLayout->addWidget(pFinder);
		pLayout->addWidget(pTreeContainer);

		// Connect finder's SetFilter signal to our filter method using lambda
		connect(pFinder, &CFinder::SetFilter, this, [this](const QRegularExpression& Exp, int, int) {
			m_FilterExp = Exp;
			PopulateTree();
		});

		m_pButtonBox = new QDialogButtonBox();
		if (bForCommit) {
			m_pCommitBtn = m_pButtonBox->addButton(tr("Commit"), QDialogButtonBox::AcceptRole);
			m_pCommitBtn->setIcon(QIcon(":/Icons/Approve.png"));
		} else {
			m_pDiscardBtn = m_pButtonBox->addButton(tr("Discard"), QDialogButtonBox::AcceptRole);
			m_pDiscardBtn->setIcon(QIcon(":/Icons/Revert.png"));
		}
		m_pCancelBtn = m_pButtonBox->addButton(QDialogButtonBox::Cancel);
		pLayout->addWidget(m_pButtonBox);

		connect(m_pButtonBox, &QDialogButtonBox::accepted, this, &CConfigChangeDialog::OnAccept);
		connect(m_pButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

		LoadChanges();
		PopulateTree();
	}

	EResult GetResult() const { return m_Result; }
	bool HasChanges() const { return !m_Changes.isEmpty(); }

protected:
	void OnAccept()
	{
		m_Result = m_bForCommit ? eCommit : eDiscard;
		accept();
	}

	void LoadChanges()
	{
		auto Entries = theCore->EventLog()->GetEntries();

		QMap<QString, SChangeInfo> Changes;

		// Find separate cutoff points for service and driver events
		// Service handles: eLogFw*, eLogDns*, eLogProgram*, eLogConfigPreset*
		// Driver handles: eLogRes*, eLogSecureEnclave*, eLogExec*, eLogHashDb*
		int svcCutoffIndex = -1;
		int drvCutoffIndex = -1;
		for (int i = Entries.size() - 1; i >= 0; i--) {
			ELogEventType type = (ELogEventType)Entries[i]->GetEventType();
			if (svcCutoffIndex == -1 && (type == eLogSvcConfigSaved || type == eLogSvcConfigDiscarded || type == eLogSvcStarted)) {
				svcCutoffIndex = i;
			}
			if (drvCutoffIndex == -1 && (type == eLogDrvConfigSaved || type == eLogDrvConfigDiscarded || type == eLogDrvStarted)) {
				drvCutoffIndex = i;
			}
			if (svcCutoffIndex != -1 && drvCutoffIndex != -1)
				break;
		}

		// Process entries starting from the older cutoff (to skip irrelevant entries)
		int startIndex = qMin(svcCutoffIndex, drvCutoffIndex) + 1;
		if (startIndex < 0) startIndex = 0;
		for (int i = startIndex; i < Entries.size(); i++) {
			const CEventLogEntryPtr& pEntry = Entries[i];
			ELogEventType type = (ELogEventType)pEntry->GetEventType();
			ELogLevels level = pEntry->GetEventLevel();
			QtVariant Data = pEntry->GetEventData();
			QFlexGuid Guid; Guid.FromVariant(Data[API_V_GUID]);
			QString guid = Guid.ToQS();
			QString name = Data[API_V_NAME].AsQStr();

			if (Guid.IsNull())
				continue;

			// Rule change event we caused have always level None
			if (level != ELogLevels::eNone)
				continue;

			// Determine if this is a service or driver event and check against appropriate cutoff
			bool isServiceEvent = false;
			bool isDriverEvent = false;

			QString typeStr;
			QIcon typeIcon;
			bool isAdd = false, isModify = false, isRemove = false;

			// Service events: Firewall, DNS, Program, Config Preset
			switch (type) {
			case eLogFwRuleAdded:     typeStr = tr("Firewall Rule"); typeIcon = QIcon(":/Icons/Wall3.png"); isAdd = true; isServiceEvent = true; break;
			case eLogFwRuleModified:  typeStr = tr("Firewall Rule"); typeIcon = QIcon(":/Icons/Wall3.png"); isModify = true; isServiceEvent = true; break;
			case eLogFwRuleRemoved:   typeStr = tr("Firewall Rule"); typeIcon = QIcon(":/Icons/Wall3.png"); isRemove = true; isServiceEvent = true; break;

			case eLogDnsRuleAdded:    typeStr = tr("DNS Rule"); typeIcon = QIcon(":/Icons/Network2.png"); isAdd = true; isServiceEvent = true; break;
			case eLogDnsRuleModified: typeStr = tr("DNS Rule"); typeIcon = QIcon(":/Icons/Network2.png"); isModify = true; isServiceEvent = true; break;
			case eLogDnsRuleRemoved:  typeStr = tr("DNS Rule"); typeIcon = QIcon(":/Icons/Network2.png"); isRemove = true; isServiceEvent = true; break;

			case eLogProgramAdded:    typeStr = tr("Program"); typeIcon = QIcon(":/Icons/Process.png"); isAdd = true; isServiceEvent = true; break;
			case eLogProgramModified: typeStr = tr("Program"); typeIcon = QIcon(":/Icons/Process.png"); isModify = true; isServiceEvent = true; break;
			case eLogProgramRemoved:  typeStr = tr("Program"); typeIcon = QIcon(":/Icons/Process.png"); isRemove = true; isServiceEvent = true; break;

			case eLogConfigPresetAdded:    typeStr = tr("Config Preset"); typeIcon = QIcon(":/Icons/Tweaks.png"); isAdd = true; isServiceEvent = true; break;
			case eLogConfigPresetModified: typeStr = tr("Config Preset"); typeIcon = QIcon(":/Icons/Tweaks.png"); isModify = true; isServiceEvent = true; break;
			case eLogConfigPresetRemoved:  typeStr = tr("Config Preset"); typeIcon = QIcon(":/Icons/Tweaks.png"); isRemove = true; isServiceEvent = true; break;

			// Driver events: Resource Access, Secure Enclave, Execution, HashDB
			case eLogResRuleAdded:    typeStr = tr("Resource Access Rule"); typeIcon = QIcon(":/Icons/Ampel.png"); isAdd = true; isDriverEvent = true; break;
			case eLogResRuleModified: typeStr = tr("Resource Access Rule"); typeIcon = QIcon(":/Icons/Ampel.png"); isModify = true; isDriverEvent = true; break;
			case eLogResRuleRemoved:  typeStr = tr("Resource Access Rule"); typeIcon = QIcon(":/Icons/Ampel.png"); isRemove = true; isDriverEvent = true; break;

			case eLogSecureEnclaveAdded:    typeStr = tr("Secure Enclave"); typeIcon = QIcon(":/Icons/Enclave.png"); isAdd = true; isDriverEvent = true; break;
			case eLogSecureEnclaveModified: typeStr = tr("Secure Enclave"); typeIcon = QIcon(":/Icons/Enclave.png"); isModify = true; isDriverEvent = true; break;
			case eLogSecureEnclaveRemoved:  typeStr = tr("Secure Enclave"); typeIcon = QIcon(":/Icons/Enclave.png"); isRemove = true; isDriverEvent = true; break;

			case eLogExecRuleAdded:    typeStr = tr("Execution Rule"); typeIcon = QIcon(":/Icons/Process.png"); isAdd = true; isDriverEvent = true; break;
			case eLogExecRuleModified: typeStr = tr("Execution Rule"); typeIcon = QIcon(":/Icons/Process.png"); isModify = true; isDriverEvent = true; break;
			case eLogExecRuleRemoved:  typeStr = tr("Execution Rule"); typeIcon = QIcon(":/Icons/Process.png"); isRemove = true; isDriverEvent = true; break;

			case eLogHashDbEntryAdded:    typeStr = tr("HashDB Entry"); typeIcon = QIcon(":/Icons/CertDB.png"); isAdd = true; isDriverEvent = true; break;
			case eLogHashDbEntryModified: typeStr = tr("HashDB Entry"); typeIcon = QIcon(":/Icons/CertDB.png"); isModify = true; isDriverEvent = true; break;
			case eLogHashDbEntryRemoved:  typeStr = tr("HashDB Entry"); typeIcon = QIcon(":/Icons/CertDB.png"); isRemove = true; isDriverEvent = true; break;

			default:
				continue;
			}

			// Skip events that are before their respective cutoff
			if (isServiceEvent && i <= svcCutoffIndex)
				continue;
			if (isDriverEvent && i <= drvCutoffIndex)
				continue;

			SChangeInfo& info = Changes[guid];
			info.Type = typeStr;
			info.TypeIcon = typeIcon;
			info.Name = name;
			info.Guid = guid;
			info.EventType = type;

			if (isAdd) {
				if (info.State == eRemoved) {
					// Was removed, now added again - treat as modified
					info.State = eModified;
				} else {
					info.State = eAdded;
				}
			} else if (isRemove) {
				if (info.State == eAdded) {
					// Added then removed = net zero
					info.State = eAddedThenRemoved;
				} else {
					info.State = eRemoved;
				}
			} else if (isModify) {
				// Only set to modified if not already added or removed
				if (info.State == eNone) {
					info.State = eModified;
				}
				// If already Added, keep it as Added (we show add, not the subsequent modifies)
				// If already Removed or AddedThenRemoved, keep that state
			}
		}

		// Store changes that should be displayed
		for (auto it = Changes.begin(); it != Changes.end(); ++it) {
			const SChangeInfo& info = it.value();
			if (info.State != eNone && info.State != eAddedThenRemoved)
				m_Changes.append(info);
		}
	}

	void PopulateTree()
	{
		m_pTree->clear();

		for (const SChangeInfo& info : m_Changes) {
			// Apply filter if set
			if (m_FilterExp.isValid() && !m_FilterExp.pattern().isEmpty()) {
				if (!info.Name.contains(m_FilterExp) && !info.Type.contains(m_FilterExp))
					continue;
			}

			QTreeWidgetItem* pItem = new QTreeWidgetItem();
			pItem->setIcon(0, info.TypeIcon);
			pItem->setText(0, info.Type);

			QString actionStr;
			QIcon actionIcon;
			switch (info.State) {
			case eAdded:
				actionStr = tr("Added");
				actionIcon = QIcon(":/Icons/Add.png");
				break;
			case eModified:
				actionStr = tr("Modified");
				actionIcon = QIcon(":/Icons/EditIni.png");
				break;
			case eRemoved:
				actionStr = tr("Removed");
				actionIcon = QIcon(":/Icons/Remove.png");
				break;
			default:
				continue;
			}
			pItem->setText(1, actionStr);
			pItem->setIcon(1, actionIcon);

			pItem->setText(2, info.Name);
			pItem->setToolTip(2, info.Guid);

			m_pTree->addTopLevelItem(pItem);
		}

		// Sort by type, then by name
		m_pTree->sortItems(0, Qt::AscendingOrder);
	}

private:
	bool m_bForCommit;
	EResult m_Result;
	QTreeWidget* m_pTree;
	QDialogButtonBox* m_pButtonBox;
	QPushButton* m_pCommitBtn = nullptr;
	QPushButton* m_pDiscardBtn = nullptr;
	QPushButton* m_pCancelBtn = nullptr;
	QList<SChangeInfo> m_Changes;
	QRegularExpression m_FilterExp;
};



void CMajorPrivacy::OnProtectConfig()
{
	bool bHardLock = false;
	int ret = QMessageBox::warning(this, "MajorPrivacy", tr("Would you like to lock down the user key (Yes), or only enable user key signature-based config protection (No)?"
		"\n\nLocking down the user key will prevent it from being removed or changed, and config protection cannot be disabled until the system is rebooted."
		"\n\nIf the driver is set to auto-start, it will automatically re-lock the key on reboot!"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);
	if (ret == QMessageBox::Cancel)
		return;
	bHardLock = (ret == QMessageBox::Yes);


	CPrivateKey PrivateKey;
	STATUS Status = InitSigner(ESignerPurpose::eEnableProtection, PrivateKey);
	if (!PrivateKey.IsPrivateKeySet()) { // eider error or user canceled
		CheckResults(QList<STATUS>() << Status, this);
		return;
	}

	CBuffer ConfigHash;
	Status = theCore->Driver()->GetConfigHash(ConfigHash);
	if (!Status.IsError())
	{
		CBuffer ConfigSignature;
		PrivateKey.Sign(ConfigHash, ConfigSignature);

		Status = theCore->Driver()->ProtectConfig(ConfigSignature, bHardLock);

	}
	UpdateLockStatus();
	CheckResults(QList<STATUS>() << Status, this);
}

void CMajorPrivacy::OnUnprotectConfig()
{
	CPrivateKey PrivateKey;
	STATUS Status = InitSigner(ESignerPurpose::eDisableProtection, PrivateKey);
	if (!PrivateKey.IsPrivateKeySet()) { // eider error or user canceled
		CheckResults(QList<STATUS>() << Status, this);
		return;
	}

	CBuffer ConfigHash;
	QtVariant Data;
	if (!m_pClearKeys->isEnabled()) {
		QMessageBox::warning(this, "MajorPrivacy", tr("The user key is locked. Please reboot the system to complete the removal of the config protection."));
		Data[API_S_UNLOCK] = true;
	}
	Status = theCore->Driver()->GetConfigHash(ConfigHash, Data);
	if (!Status.IsError())
	{
		CBuffer ConfigSignature;
		PrivateKey.Sign(ConfigHash, ConfigSignature);

		Status = theCore->Driver()->UnprotectConfig(ConfigSignature);

	}
	UpdateLockStatus();
	CheckResults(QList<STATUS>() << Status, this);
}

STATUS CMajorPrivacy::UnlockDrvConfig()
{
	CPrivateKey PrivateKey;
	STATUS Status = InitSigner(ESignerPurpose::eUnlockConfig, PrivateKey);
	if (!PrivateKey.IsPrivateKeySet()) // eider error or user canceled
		return Status;

	CBuffer Challenge;
	Status = theCore->Driver()->GetChallenge(Challenge);
	if (!Status.IsError())
	{
		CBuffer Hash;
		CHashFunction::Hash(Challenge, Hash);

		CBuffer ChallengeResponse;
		PrivateKey.Sign(Hash, ChallengeResponse);

		Status = theCore->Driver()->UnlockConfig(ChallengeResponse);

	}
	UpdateLockStatus();

	return Status;
}

STATUS CMajorPrivacy::CommitDrvConfig()
{
	uint32 uConfigStatus = theCore->Driver()->GetConfigStatus();
	if ((uConfigStatus & CONFIG_STATUS_DIRTY) == 0) { // nothign changed
		theCore->Driver()->DiscardConfigChanges(); // to relock the config
		return OK;
	}

	if ((uConfigStatus & CONFIG_STATUS_PROTECTED) == 0)
		return theCore->Driver()->StoreConfigChanges();

	CPrivateKey PrivateKey;
	STATUS Status = InitSigner(ESignerPurpose::eCommitConfig, PrivateKey);
	if (!PrivateKey.IsPrivateKeySet()) // eider error or user canceled
		return Status;

	CBuffer ConfigHash;
	Status = theCore->Driver()->GetConfigHash(ConfigHash);
	if (!Status.IsError())
	{
		CBuffer ConfigSignature;
		PrivateKey.Sign(ConfigHash, ConfigSignature);

		Status = theCore->Driver()->CommitConfigChanges(ConfigSignature);

	}
	return Status;
}

STATUS CMajorPrivacy::DiscardDrvConfig()
{
	uint32 uConfigStatus = theCore->Driver()->GetConfigStatus();
	bool bProtected = (uConfigStatus & CONFIG_STATUS_PROTECTED) != 0;
	bool bLocked = (uConfigStatus & CONFIG_STATUS_LOCKED) != 0;
	bool bDirty = (uConfigStatus & CONFIG_STATUS_DIRTY) != 0;

	if (!(bDirty || (bProtected && !bLocked)))
		return OK;

	return theCore->Driver()->DiscardConfigChanges();
}

void CMajorPrivacy::OnUnlockConfig()
{
	STATUS Status = UnlockDrvConfig();
	CheckResults(QList<STATUS>() << Status, this);
}

void CMajorPrivacy::OnCommitConfig()
{
	// Check if there are any changes to commit
	uint32 uDrvConfigStatus = theCore->Driver()->GetConfigStatus();
	uint32 uSvcConfigStatus = theCore->Service()->GetConfigStatus();
	bool bDrvDirty = (uDrvConfigStatus & CONFIG_STATUS_DIRTY) != 0;
	bool bSvcDirty = (uSvcConfigStatus & CONFIG_STATUS_DIRTY) != 0;

	if (!bDrvDirty && !bSvcDirty) {
		// Nothing to commit, but may need to relock
		theCore->Driver()->DiscardConfigChanges();
		return;
	}

	// Show the config change dialog
	CConfigChangeDialog dlg(true, this);
	if (dlg.HasChanges()) {
		if (dlg.exec() != QDialog::Accepted || dlg.GetResult() != CConfigChangeDialog::eCommit)
			return;
	}

	QList<STATUS> Results;

	Results.append(CommitDrvConfig());

	if (m_AutoCommitConf){
		m_AutoCommitConf = 0;
		if(!m_ForgetSignerPW)
			m_CachedPassword.clear();
	}

	if (bSvcDirty)
	{
		STATUS Status = theCore->Service()->StoreConfigChanges();
		Results.append(Status);
	}

	CheckResults(Results, this);
}

void CMajorPrivacy::OnDiscardConfig()
{
	// Check if there are any changes to discard
	uint32 uDrvConfigStatus = theCore->Driver()->GetConfigStatus();
	uint32 uSvcConfigStatus = theCore->Service()->GetConfigStatus();
	bool bDrvProtected = (uDrvConfigStatus & CONFIG_STATUS_PROTECTED) != 0;
	bool bDrvLocked = (uDrvConfigStatus & CONFIG_STATUS_LOCKED) != 0;
	bool bDrvDirty = (uDrvConfigStatus & CONFIG_STATUS_DIRTY) != 0;
	bool bSvcDirty = (uSvcConfigStatus & CONFIG_STATUS_DIRTY) != 0;

	bool bHasDrvChanges = bDrvDirty || (bDrvProtected && !bDrvLocked);

	if (!bHasDrvChanges && !bSvcDirty)
		return;

	// Show the config change dialog if commit button is enabled (meaning there are uncommitted changes)
	if (m_pCommitConfig->isEnabled()) {
		CConfigChangeDialog dlg(false, this);
		if (dlg.HasChanges()) {
			if (dlg.exec() != QDialog::Accepted || dlg.GetResult() != CConfigChangeDialog::eDiscard)
				return;
		} else {
			// No tracked changes but config is dirty - show simple confirmation
			if (QMessageBox::question(this, "MajorPrivacy", tr("Do you really want to discard all changes?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
				return;
		}
	}

	QList<STATUS> Results;

	if (m_AutoCommitConf){
		m_AutoCommitConf = 0;
		if(!m_ForgetSignerPW)
			m_CachedPassword.clear();
	}

	Results.append(DiscardDrvConfig());

	if (bSvcDirty)
	{
		STATUS Status = theCore->Service()->DiscardConfigChanges();
		Results.append(Status);
	}

	CheckResults(Results, this);
}





////////////////////////////////////////////////////////////////////////////////////////
// CCleanUpProgramsDialog - Dialog showing programs to be cleaned up
//

class CCleanUpProgramsDialog : public QDialog
{
public:
	enum EResult {
		eCancel = 0,
		eRemoveAll,        // Yes - remove all missing programs including those with rules
		eRemoveWithoutRules // No - remove only missing programs without rules
	};

protected:
	struct SProgramInfo {
		QString Type;
		QIcon TypeIcon;
		QString Name;
		QString Path;
		bool HasRules;
		int FwRuleCount;
		int ProgRuleCount;
		int ResRuleCount;
	};

public:
	CCleanUpProgramsDialog(QWidget* parent = nullptr)
		: QDialog(parent)
		, m_Result(eCancel)
	{
		setWindowTitle(tr("Clean Up Programs"));
		setMinimumSize(650, 450);
		resize(750, 550);

		QVBoxLayout* pLayout = new QVBoxLayout(this);

		QLabel* pLabel = new QLabel(tr("The following missing programs will be removed:"));
		pLayout->addWidget(pLabel);

		// Create container widget for tree and finder
		QWidget* pTreeContainer = new QWidget();
		QVBoxLayout* pTreeLayout = new QVBoxLayout(pTreeContainer);
		pTreeLayout->setContentsMargins(0, 0, 0, 0);

		m_pTree = new QTreeWidget();
		m_pTree->setHeaderLabels(QStringList() << tr("Type") << tr("Name") << tr("Rules") << tr("Path"));
		m_pTree->setRootIsDecorated(false);
		m_pTree->setAlternatingRowColors(true);
		m_pTree->header()->setStretchLastSection(true);
		m_pTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
		m_pTree->header()->setSectionResizeMode(1, QHeaderView::Interactive);
		m_pTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
		m_pTree->setColumnWidth(1, 200);
		pTreeLayout->addWidget(m_pTree);

		// Add CFinder for search functionality
		CFinder* pFinder = new CFinder(nullptr, pTreeContainer, CFinder::eRegExp | CFinder::eCaseSens);
		pTreeLayout->addWidget(pFinder);
		pLayout->addWidget(pTreeContainer);

		// Connect finder's SetFilter signal to our filter method using lambda
		connect(pFinder, &CFinder::SetFilter, this, [this](const QRegularExpression& Exp, int, int) {
			m_FilterExp = Exp;
			PopulateTree();
		});

		// Summary label
		m_pSummaryLabel = new QLabel();
		pLayout->addWidget(m_pSummaryLabel);

		m_pButtonBox = new QDialogButtonBox();
		m_pRemoveAllBtn = m_pButtonBox->addButton(tr("Remove All"), QDialogButtonBox::YesRole);
		m_pRemoveAllBtn->setIcon(QIcon(":/Icons/Remove.png"));
		m_pRemoveAllBtn->setToolTip(tr("Remove all missing programs, including those with rules"));

		m_pRemoveWithoutRulesBtn = m_pButtonBox->addButton(tr("Remove Without Rules"), QDialogButtonBox::NoRole);
		m_pRemoveWithoutRulesBtn->setIcon(QIcon(":/Icons/Clean.png"));
		m_pRemoveWithoutRulesBtn->setToolTip(tr("Remove only missing programs that have no rules"));

		m_pCancelBtn = m_pButtonBox->addButton(QDialogButtonBox::Cancel);
		pLayout->addWidget(m_pButtonBox);

		connect(m_pRemoveAllBtn, &QPushButton::clicked, this, &CCleanUpProgramsDialog::OnRemoveAll);
		connect(m_pRemoveWithoutRulesBtn, &QPushButton::clicked, this, &CCleanUpProgramsDialog::OnRemoveWithoutRules);
		connect(m_pButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

		LoadPrograms();
		PopulateTree();
	}

	EResult GetResult() const { return m_Result; }
	bool HasPrograms() const { return !m_Programs.isEmpty(); }
	bool HasProgramsWithRules() const { return m_ProgramsWithRules > 0; }
	bool HasProgramsWithoutRules() const { return m_ProgramsWithoutRules > 0; }

protected:
	void OnRemoveAll()
	{
		m_Result = eRemoveAll;
		accept();
	}

	void OnRemoveWithoutRules()
	{
		m_Result = eRemoveWithoutRules;
		accept();
	}

	void LoadPrograms()
	{
		m_ProgramsWithRules = 0;
		m_ProgramsWithoutRules = 0;

		auto Items = theCore->ProgramManager()->GetItems();

		for (auto it = Items.begin(); it != Items.end(); ++it) {
			const CProgramItemPtr& pItem = it.value();

			if (!pItem->IsMissing())
				continue;

			SProgramInfo info;

			// Get type string and icon
			info.Type = pItem->GetTypeStr();
			info.TypeIcon = pItem->GetIcon();
			if (info.TypeIcon.isNull())
				info.TypeIcon = pItem->DefaultIcon();

			info.Name = pItem->GetNameEx();
			info.Path = pItem->GetPath();

			// Check for rules
			info.FwRuleCount = pItem->GetFwRuleCount();
			info.ProgRuleCount = pItem->GetProgRuleCount();
			info.ResRuleCount = pItem->GetResRuleCount();
			info.HasRules = (info.FwRuleCount + info.ProgRuleCount + info.ResRuleCount) > 0;

			m_Programs.append(info);

			if (info.HasRules)
				m_ProgramsWithRules++;
			else
				m_ProgramsWithoutRules++;
		}

		// Sort by type, then by whether it has rules (with rules first), then by name
		std::sort(m_Programs.begin(), m_Programs.end(), [](const SProgramInfo& a, const SProgramInfo& b) {
			if (a.Type != b.Type)
				return a.Type < b.Type;
			if (a.HasRules != b.HasRules)
				return a.HasRules > b.HasRules; // with rules first
			return a.Name.toLower() < b.Name.toLower();
		});

		// Update summary (based on total, not filtered)
		m_pSummaryLabel->setText(tr("Total: %1 missing programs (%2 with rules, %3 without rules)")
			.arg(m_ProgramsWithRules + m_ProgramsWithoutRules)
			.arg(m_ProgramsWithRules)
			.arg(m_ProgramsWithoutRules));

		// Disable buttons if no relevant programs
		m_pRemoveAllBtn->setEnabled(m_ProgramsWithRules + m_ProgramsWithoutRules > 0);
		m_pRemoveWithoutRulesBtn->setEnabled(m_ProgramsWithoutRules > 0);
	}

	void PopulateTree()
	{
		m_pTree->clear();

		for (const SProgramInfo& info : m_Programs) {
			// Apply filter if set
			if (m_FilterExp.isValid() && !m_FilterExp.pattern().isEmpty()) {
				if (!info.Name.contains(m_FilterExp) && !info.Type.contains(m_FilterExp) && !info.Path.contains(m_FilterExp))
					continue;
			}

			QTreeWidgetItem* pItem = new QTreeWidgetItem();

			pItem->setIcon(0, info.TypeIcon);
			pItem->setText(0, info.Type);

			pItem->setText(1, info.Name);

			// Rules column
			if (info.HasRules) {
				QStringList rulesList;
				if (info.FwRuleCount > 0)
					rulesList << tr("FW: %1").arg(info.FwRuleCount);
				if (info.ProgRuleCount > 0)
					rulesList << tr("Prog: %1").arg(info.ProgRuleCount);
				if (info.ResRuleCount > 0)
					rulesList << tr("Res: %1").arg(info.ResRuleCount);
				pItem->setText(2, rulesList.join(", "));
				pItem->setIcon(2, QIcon(":/Icons/Wall3.png"));
				// Highlight programs with rules
				pItem->setForeground(2, QBrush(Qt::darkRed));
			} else {
				pItem->setText(2, tr("None"));
			}

			pItem->setText(3, info.Path);
			pItem->setToolTip(3, info.Path);

			m_pTree->addTopLevelItem(pItem);
		}
	}

private:
	EResult m_Result;
	int m_ProgramsWithRules = 0;
	int m_ProgramsWithoutRules = 0;
	QTreeWidget* m_pTree;
	QLabel* m_pSummaryLabel;
	QDialogButtonBox* m_pButtonBox;
	QPushButton* m_pRemoveAllBtn = nullptr;
	QPushButton* m_pRemoveWithoutRulesBtn = nullptr;
	QPushButton* m_pCancelBtn = nullptr;
	QList<SProgramInfo> m_Programs;
	QRegularExpression m_FilterExp;
};



void CMajorPrivacy::CleanUpPrograms()
{
	CCleanUpProgramsDialog dlg(this);

	if (!dlg.HasPrograms()) {
		QMessageBox::information(this, "MajorPrivacy", tr("No missing programs found."));
		return;
	}

	if (dlg.exec() != QDialog::Accepted)
		return;

	CCleanUpProgramsDialog::EResult result = dlg.GetResult();
	if (result == CCleanUpProgramsDialog::eCancel)
		return;

	bool bPurgeRules = (result == CCleanUpProgramsDialog::eRemoveAll);
	QList<STATUS> Results = QList<STATUS>() << theCore->CleanUpPrograms(bPurgeRules);
	theGUI->CheckResults(Results, this);
}

void CMajorPrivacy::ReGroupPrograms()
{
	if (QMessageBox::question(this, "MajorPrivacy", tr("Do you want to re-group all Program Items? This will remove all program items from all auto associated groups and re add it based on the default rules."), QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
		QList<STATUS> Results = QList<STATUS>() << theCore->ReGroupPrograms();
		theGUI->CheckResults(Results, this);
	}
}