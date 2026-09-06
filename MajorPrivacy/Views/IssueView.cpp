#include "pch.h"
#include "IssueView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/IssueManager.h"
#include "../Core/Network/NetworkManager.h"
#include "../Core/Tweaks/TweakManager.h"
#include "../MiscHelpers/Common/CheckableMessageBox.h"
#include "../MiscHelpers/Common/Finder.h"
#include "../MajorPrivacy.h"
#include "TweakView.h"
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QCheckBox>

////////////////////////////////////////////////////////////////////////////////////////
// CIssueActionDialog - Dialog for accepting/rejecting/fixing issues
//

class CIssueActionDialog : public QDialog
{
public:
	enum EResult {
		eCancel = 0,
		eFix,
		eAccept,
		eReject,
		eIgnore
	};

	enum EMode {
		eModeAccept,		// For altered firewall rules - show Accept only
		eModeReject,		// For altered firewall rules - show Reject only
		eModeChoose,		// For altered firewall rules - show both Accept and Reject (for double-click)
		eModeFix,			// For other issues - show Fix
		eModeIgnore			// For ignoring issues
	};

protected:
	struct SIssueInfo {
		QString Id;
		QString Type;
		QIcon TypeIcon;
		QString Severity;
		QColor SeverityColor;
		QString Description;
		QString Details;
		QString DetailsTooltip;
	};

public:
	CIssueActionDialog(EMode mode, const QStringList& issueIds, bool showDontAsk = false, QWidget* parent = nullptr)
		: QDialog(parent)
		, m_Mode(mode)
		, m_IssueIds(issueIds)
		, m_Result(eCancel)
	{
		QString title;
		QString prompt;
		switch (mode) {
		case eModeAccept:
			title = tr("Accept Firewall Rule Changes");
			prompt = tr("The following firewall rule changes will be accepted:");
			break;
		case eModeReject:
			title = tr("Reject Firewall Rule Changes");
			prompt = tr("The following firewall rule changes will be rejected:");
			break;
		case eModeChoose:
			title = tr("Firewall Rule Changes");
			prompt = tr("The following firewall rule changes have been detected.\nChoose to accept or reject:");
			break;
		case eModeFix:
			title = tr("Fix Issues");
			prompt = tr("The following issues will be fixed:");
			break;
		case eModeIgnore:
			title = tr("Ignore Issues");
			prompt = tr("The following issues will be hidden:");
			break;
		}
		setWindowTitle(title);
		setMinimumSize(700, 400);
		resize(850, 500);

		QVBoxLayout* pLayout = new QVBoxLayout(this);

		QLabel* pLabel = new QLabel(prompt);
		pLayout->addWidget(pLabel);

		// Create container widget for tree and finder
		QWidget* pTreeContainer = new QWidget();
		QVBoxLayout* pTreeLayout = new QVBoxLayout(pTreeContainer);
		pTreeLayout->setContentsMargins(0, 0, 0, 0);

		m_pTree = new QTreeWidget();
		m_pTree->setHeaderLabels(QStringList() << tr("Type") << tr("Severity") << tr("Description") << tr("Details"));
		m_pTree->setRootIsDecorated(false);
		m_pTree->setAlternatingRowColors(true);
		m_pTree->header()->setStretchLastSection(true);
		m_pTree->setColumnWidth(0, 120);
		m_pTree->setColumnWidth(1, 80);
		m_pTree->setColumnWidth(2, 300);
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

		// Add "Don't ask in future" checkbox for fix mode
		if (showDontAsk && mode == eModeFix) {
			m_pDontAskCheck = new QCheckBox(tr("Don't ask in future"));
			pLayout->addWidget(m_pDontAskCheck);
		}

		m_pButtonBox = new QDialogButtonBox();
		switch (mode) {
		case eModeAccept:
			m_pAcceptBtn = m_pButtonBox->addButton(tr("Accept Changes"), QDialogButtonBox::AcceptRole);
			m_pAcceptBtn->setIcon(QIcon(":/Icons/Approve.png"));
			break;
		case eModeReject:
			m_pRejectBtn = m_pButtonBox->addButton(tr("Reject Changes"), QDialogButtonBox::AcceptRole);
			m_pRejectBtn->setIcon(QIcon(":/Icons/Recover.png"));
			break;
		case eModeChoose:
			m_pAcceptBtn = m_pButtonBox->addButton(tr("Accept Changes"), QDialogButtonBox::AcceptRole);
			m_pAcceptBtn->setIcon(QIcon(":/Icons/Approve.png"));
			m_pRejectBtn = m_pButtonBox->addButton(tr("Reject Changes"), QDialogButtonBox::ActionRole);
			m_pRejectBtn->setIcon(QIcon(":/Icons/Recover.png"));
			connect(m_pRejectBtn, &QPushButton::clicked, this, &CIssueActionDialog::OnReject);
			break;
		case eModeFix:
			m_pFixBtn = m_pButtonBox->addButton(tr("Fix Issues"), QDialogButtonBox::AcceptRole);
			m_pFixBtn->setIcon(QIcon(":/Icons/Refresh.png"));
			break;
		case eModeIgnore:
			m_pIgnoreBtn = m_pButtonBox->addButton(tr("Ignore Issues"), QDialogButtonBox::AcceptRole);
			m_pIgnoreBtn->setIcon(QIcon(":/Icons/Invisible.png"));
			break;
		}
		m_pCancelBtn = m_pButtonBox->addButton(QDialogButtonBox::Cancel);
		pLayout->addWidget(m_pButtonBox);

		connect(m_pButtonBox, &QDialogButtonBox::accepted, this, &CIssueActionDialog::OnAccept);
		connect(m_pButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

		LoadIssues();
		PopulateTree();
	}

	EResult GetResult() const { return m_Result; }
	bool HasIssues() const { return !m_Issues.isEmpty(); }
	bool IsDontAskChecked() const { return m_pDontAskCheck && m_pDontAskCheck->isChecked(); }

protected:
	void OnAccept()
	{
		switch (m_Mode) {
		case eModeAccept: m_Result = eAccept; break;
		case eModeReject: m_Result = eReject; break;
		case eModeChoose: m_Result = eAccept; break; // Accept button clicked
		case eModeFix: m_Result = eFix; break;
		case eModeIgnore: m_Result = eIgnore; break;
		}
		accept();
	}

	void OnReject()
	{
		m_Result = eReject; // Reject button clicked in eModeChoose
		accept();
	}

	void LoadIssues()
	{
		auto Issues = theCore->IssueManager()->GetIssues();
		auto FwRules = theCore->NetworkManager()->GetFwRules();
		auto Tweaks = theCore->TweakManager()->GetTweaks();

		for (const QString& id : m_IssueIds)
		{
			CIssuePtr pIssue = Issues.value(id);
			if (!pIssue)
				continue;

			SIssueInfo info;
			info.Id = id;

			// Type column with icon
			info.TypeIcon = pIssue->GetIcon();
			switch (pIssue->GetType()) {
			case CIssue::eNoUserKey:
				info.Type = tr("User Key");
				break;
			case CIssue::eFwNotApproved:
				info.Type = tr("Firewall");
				break;
			case CIssue::eFwRuleAltered:
				info.Type = tr("Firewall Rule");
				break;
			case CIssue::eTweakFailed:
				info.Type = tr("Tweak");
				break;
			default:
				info.Type = tr("Unknown");
				break;
			}

			// Severity column
			info.Severity = pIssue->GetSeverityStr();
			switch (pIssue->GetSeverity()) {
			case CIssue::eLow: info.SeverityColor = QColor(100, 180, 100); break;
			case CIssue::eMedium: info.SeverityColor = QColor(200, 180, 80); break;
			case CIssue::eHigh: info.SeverityColor = QColor(220, 140, 60); break;
			case CIssue::eCritical: info.SeverityColor = QColor(200, 80, 80); break;
			default: info.SeverityColor = QColor(150, 150, 150); break;
			}

			// Description column
			info.Description = pIssue->GetDescription();

			// Details column - add specific info based on issue type
			if (id.startsWith("backup_fw_rule_") || id.startsWith("unapproved_fw_rule_"))
			{
				QString guid = id.mid(id.indexOf("_rule_") + 6);
				CFwRulePtr pFwRule = FwRules.value(guid);
				if (pFwRule)
				{
					QStringList detailParts;

					// Direction and Action
					detailParts << QString("%1 %2").arg(pFwRule->GetDirectionStr()).arg(pFwRule->GetActionStr());

					// Protocol
					if (pFwRule->GetProtocol() != EFwKnownProtocols::Any)
						detailParts << CFwRule::ProtocolToStr(pFwRule->GetProtocol());

					// Ports
					QStringList ports;
					if (!pFwRule->GetLocalPorts().isEmpty())
						ports << tr("Local: %1").arg(pFwRule->GetLocalPorts().join(", "));
					if (!pFwRule->GetRemotePorts().isEmpty())
						ports << tr("Remote: %1").arg(pFwRule->GetRemotePorts().join(", "));
					if (!ports.isEmpty())
						detailParts << ports.join("; ");

					// Remote addresses (if not "*")
					if (!pFwRule->GetRemoteAddresses().isEmpty() &&
						!(pFwRule->GetRemoteAddresses().size() == 1 && pFwRule->GetRemoteAddresses().first() == "*"))
						detailParts << tr("Addr: %1").arg(pFwRule->GetRemoteAddresses().join(", ").left(50));

					// Program path
					QString prog = pFwRule->GetProgram();
					if (!prog.isEmpty() && prog != "*")
					{
						// Extract just the filename
						int lastSlash = prog.lastIndexOf('\\');
						if (lastSlash >= 0)
							prog = prog.mid(lastSlash + 1);
						detailParts << tr("Program: %1").arg(prog);
					}

					info.Details = detailParts.join(" | ");
					info.DetailsTooltip = pFwRule->GetProgram();
				}

				// For backup rules, check if there's an altered version
				if (id.startsWith("backup_fw_rule_"))
				{
					CFwRulePtr pBackupRule = pFwRule;
					if (pBackupRule && !pBackupRule->GetOriginalGuid().isEmpty())
					{
						CFwRulePtr pNewRule = FwRules.value(pBackupRule->GetOriginalGuid());
						if (pNewRule)
						{
							// Show that it was altered
							QString alteredInfo = tr("[Altered]");
							info.Details = alteredInfo + " " + info.Details;
						}
						else
						{
							// Rule was removed
							QString removedInfo = tr("[Removed]");
							info.Details = removedInfo + " " + info.Details;
						}
					}
				}
			}
			else if (id.startsWith("missing_tweak_"))
			{
				QString tweakId = id.mid(14); // "missing_tweak_" is 14 chars
				CTweakPtr pTweak = Tweaks.value(tweakId);
				if (pTweak)
				{
					QStringList detailParts;
					detailParts << pTweak->GetTypeStr();
					detailParts << pTweak->GetStatusStr();
					if (!pTweak->GetInfo().isEmpty())
						detailParts << pTweak->GetInfo().left(100);
					info.Details = detailParts.join(" | ");
					info.DetailsTooltip = pTweak->GetDescription();
				}
			}
			else if (id == "no_user_key")
			{
				info.Details = tr("Driver requires a user key for secure operations");
			}
			else if (id == "unapproved_fw_rules")
			{
				int count = theCore->IssueManager()->GetUnapprovedFwRuleCount();
				info.Details = tr("%1 unapproved rules detected").arg(count);
			}

			m_Issues.append(info);
		}
	}

	void PopulateTree()
	{
		m_pTree->clear();

		for (const SIssueInfo& info : m_Issues)
		{
			// Apply filter if set
			if (m_FilterExp.isValid() && !m_FilterExp.pattern().isEmpty()) {
				if (!info.Type.contains(m_FilterExp) &&
					!info.Description.contains(m_FilterExp) &&
					!info.Details.contains(m_FilterExp))
					continue;
			}

			QTreeWidgetItem* pItem = new QTreeWidgetItem();

			pItem->setText(0, info.Type);
			pItem->setIcon(0, info.TypeIcon);

			pItem->setText(1, info.Severity);
			pItem->setForeground(1, info.SeverityColor);

			pItem->setText(2, info.Description);

			pItem->setText(3, info.Details);
			if (!info.DetailsTooltip.isEmpty())
				pItem->setToolTip(3, info.DetailsTooltip);

			pItem->setData(0, Qt::UserRole, info.Id);

			m_pTree->addTopLevelItem(pItem);
		}

		// Resize columns to content
		m_pTree->resizeColumnToContents(0);
		m_pTree->resizeColumnToContents(1);
	}

private:
	EMode m_Mode;
	QStringList m_IssueIds;
	EResult m_Result;
	QTreeWidget* m_pTree;
	QCheckBox* m_pDontAskCheck = nullptr;
	QDialogButtonBox* m_pButtonBox;
	QPushButton* m_pFixBtn = nullptr;
	QPushButton* m_pAcceptBtn = nullptr;
	QPushButton* m_pRejectBtn = nullptr;
	QPushButton* m_pIgnoreBtn = nullptr;
	QPushButton* m_pCancelBtn = nullptr;
	QList<SIssueInfo> m_Issues;
	QRegularExpression m_FilterExp;
};

CIssueView::CIssueView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QGridLayout(this);
	//m_pMainLayout->setContentsMargins(9, 9, 0, 0);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pLabel = new QLabel(tr("Current Privacy related Issues requirering an Action to be taken:"));
	m_pMainLayout->addWidget(m_pLabel, 0, 0);

	m_pInfo = new CPanelWidgetEx();
	//m_pInfo->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	m_pInfo->GetTree()->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	m_pInfo->GetTree()->setHeaderLabels(tr("Severity|Description").split("|"));
	m_pInfo->GetTree()->setIndentation(0);
	m_pInfo->GetTree()->setItemDelegate(new CTreeItemDelegate2());
	m_pInfo->GetTree()->setAlternatingRowColors(theConf->GetBool("Options/AltRowColors", false));
	m_pInfo->GetTree()->setIconSize(QSize(32, 32));
	m_pInfo->GetTree()->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pMainLayout->addWidget(m_pInfo, 1, 0);

	QAction* pFirst = m_pInfo->GetMenu()->actions().first();

	m_pFixIssue = new QAction(QIcon(":/Icons/Refresh.png"), tr("Fix Issue"), this);
	connect(m_pFixIssue, SIGNAL(triggered()), this, SLOT(OnAction()));
	m_pInfo->GetMenu()->insertAction(pFirst, m_pFixIssue);

	m_pFixAccept = new QAction(QIcon(":/Icons/Approve.png"), tr("Accept Change"), this);
	connect(m_pFixAccept, SIGNAL(triggered()), this, SLOT(OnAction()));
	m_pInfo->GetMenu()->insertAction(pFirst, m_pFixAccept);

	m_pFixReject = new QAction(QIcon(":/Icons/Recover.png"), tr("Reject Change"), this);
	connect(m_pFixReject, SIGNAL(triggered()), this, SLOT(OnAction()));
	m_pInfo->GetMenu()->insertAction(pFirst, m_pFixReject);

	m_pIgnoreIssue = new QAction(QIcon(":/Icons/Invisible.png"), tr("Ignore Issue"), this);
	connect(m_pIgnoreIssue, SIGNAL(triggered()), this, SLOT(OnAction()));
	m_pInfo->GetMenu()->insertAction(pFirst, m_pIgnoreIssue);

	disconnect(m_pInfo->GetTree(), SIGNAL(customContextMenuRequested(const QPoint&)), m_pInfo, SLOT(OnMenu(const QPoint &)));
	connect(m_pInfo->GetTree(), SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	//connect(m_pInfo->GetTree(), SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(OnIssueClicked(QTreeWidgetItem*)));
	connect(m_pInfo->GetTree(), SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)), this, SLOT(OnIssueClicked(QTreeWidgetItem*)));
	connect(m_pInfo->GetTree(), SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnIssueDoubleClicked(QTreeWidgetItem*)));


	QByteArray Columns = theConf->GetBlob("MainWindow/ProgramIssueView_Columns");
	if (Columns.isEmpty()) {
		m_pInfo->GetTree()->setColumnWidth(0, 300);
	} else
		m_pInfo->GetTree()->header()->restoreState(Columns);
}

CIssueView::~CIssueView()
{
	theConf->SetBlob("MainWindow/ProgramIssueView_Columns", m_pInfo->GetTree()->header()->saveState());
}

void CIssueView::Refresh()
{
	theCore->IssueManager()->DetectIssues();
}

void CIssueView::Update()
{
	auto Issues = theCore->IssueManager()->GetIssues();

	QMap<QString, QTreeWidgetItem*> Old;
	for(int i = 0; i < m_pInfo->GetTree()->topLevelItemCount(); i++) {
		QTreeWidgetItem* pItem = m_pInfo->GetTree()->topLevelItem(i);
		QString Id = pItem->data(eName, Qt::UserRole).toString();
		Q_ASSERT(!Old.contains(Id));
		Old.insert(Id, pItem);
	}	

	for(auto I = Issues.begin(); I != Issues.end(); ++I)
	{
		QString Id = I.key();
		CIssuePtr pIssue = I.value();
		QTreeWidgetItem* pItem = Old.take(Id);

		if (!pItem) {
			pItem = new QTreeWidgetItem();
			pItem->setData(eName, Qt::UserRole, Id);
			pItem->setData(eSeverity, Qt::UserRole, (int)pIssue->GetType());
			pItem->setIcon(eSeverity, pIssue->GetIcon());
			pItem->setText(eSeverity, pIssue->GetSeverityStr());
			pItem->setText(eName, pIssue->GetDescription());
			m_pInfo->GetTree()->addTopLevelItem(pItem);
		}
	}

	foreach(QTreeWidgetItem* pItem, Old)
		delete pItem;
}

void CIssueView::OnMenu(const QPoint& Point)
{
	auto Items = m_pInfo->GetTree()->selectedItems();

	int FwIssueCount = 0;
	int OtherIssueCount = 0;

	for (int i = 0; i < Items.count(); i++) {
		QTreeWidgetItem* pItem = Items[i];
		if (pItem->data(eSeverity, Qt::UserRole).toInt() == (int)CIssue::eFwRuleAltered)
			FwIssueCount++;
		else
			OtherIssueCount++;
	}

	m_pFixIssue->setEnabled(OtherIssueCount > 0);
	m_pFixAccept->setEnabled(FwIssueCount > 0);
	m_pFixReject->setEnabled(FwIssueCount > 0);

	m_pInfo->OnMenu(Point);
}

void CIssueView::OnAction()
{
	QStringList IssueIds;
	auto Items = m_pInfo->GetTree()->selectedItems();

	// Determine the action mode based on sender
	CIssueActionDialog::EMode dialogMode;
	CIssue::EFixMode fixMode = CIssue::eDefault;

	if (sender() == m_pFixAccept) {
		dialogMode = CIssueActionDialog::eModeAccept;
		fixMode = CIssue::eAccept;
	} else if (sender() == m_pFixReject) {
		dialogMode = CIssueActionDialog::eModeReject;
		fixMode = CIssue::eReject;
	} else if (sender() == m_pIgnoreIssue) {
		dialogMode = CIssueActionDialog::eModeIgnore;
	} else if (sender() == m_pFixIssue) {
		dialogMode = CIssueActionDialog::eModeFix;
	} else {
		return; // unknown action
	}

	// Collect issue IDs, filtering for firewall rules if in accept/reject mode
	for (int i = 0; i < Items.count(); i++) {
		QTreeWidgetItem* pItem = Items[i];

		if (dialogMode == CIssueActionDialog::eModeAccept || dialogMode == CIssueActionDialog::eModeReject) {
			if (pItem->data(eSeverity, Qt::UserRole).toInt() != (int)CIssue::eFwRuleAltered)
				continue;
		}

		IssueIds.append(pItem->data(eName, Qt::UserRole).toString());
	}

	if (IssueIds.isEmpty())
		return;

	// Show the dialog
	CIssueActionDialog dlg(dialogMode, IssueIds, false, this);
	if (!dlg.HasIssues())
		return;

	if (dlg.exec() != QDialog::Accepted)
		return;

	CIssueActionDialog::EResult result = dlg.GetResult();

	// Handle ignore action
	if (result == CIssueActionDialog::eIgnore)
	{
		foreach(const QString& Id, IssueIds)
			theCore->IssueManager()->HideIssue(Id);
		return;
	}

	// Determine fix mode based on dialog result
	if (result == CIssueActionDialog::eAccept)
		fixMode = CIssue::eAccept;
	else if (result == CIssueActionDialog::eReject)
		fixMode = CIssue::eReject;
	else if (result == CIssueActionDialog::eFix)
		fixMode = CIssue::eDefault;
	else
		return;

	QList<STATUS> Results;
	foreach(const QString& Id, IssueIds)
		Results.append(theCore->IssueManager()->FixIssue(Id, fixMode));
	theGUI->CheckResults(Results, this);
}

void CIssueView::OnIssueClicked(QTreeWidgetItem* pItem)
{
}

/*void CIssueView::OnIssueDoubleClicked(QTreeWidgetItem* pItem)
{
	QString Id = pItem->data(eName, Qt::UserRole).toString();
	CIssue::EIssueType Type = (CIssue::EIssueType)pItem->data(eSeverity, Qt::UserRole).toInt();
	CIssue::EFixMode Mode = CIssue::eDefault;

	QStringList IssueIds;
	IssueIds << Id;

	if (Type == CIssue::eFwRuleAltered)
	{
		// Show the accept/reject dialog for firewall rule changes - user can choose
		CIssueActionDialog dlg(CIssueActionDialog::eModeChoose, IssueIds, false, this);
		if (!dlg.HasIssues())
			return;

		if (dlg.exec() != QDialog::Accepted)
			return;

		CIssueActionDialog::EResult result = dlg.GetResult();
		if (result == CIssueActionDialog::eAccept)
			Mode = CIssue::eAccept;
		else if (result == CIssueActionDialog::eReject)
			Mode = CIssue::eReject;
		else
			return;
	}
	else if (theConf->GetInt("Options/DblClickFixQuick", -1) != 1)
	{
		// Show the fix dialog for other issues with "Don't ask" option
		CIssueActionDialog dlg(CIssueActionDialog::eModeFix, IssueIds, true, this);
		if (!dlg.HasIssues())
			return;

		if (dlg.exec() != QDialog::Accepted)
			return;

		if (dlg.GetResult() != CIssueActionDialog::eFix)
			return;

		// Save the "Don't ask in future" preference
		if (dlg.IsDontAskChecked())
			theConf->SetValue("Options/DblClickFixQuick", 1);
	}

	STATUS Status = theCore->IssueManager()->FixIssue(Id, Mode);

	theGUI->CheckResults(QList<STATUS>() << Status, this);
}*/

void CIssueView::OnIssueDoubleClicked(QTreeWidgetItem* pItem)
{
	QString Id = pItem->data(eName, Qt::UserRole).toString();
	CIssue::EIssueType Type = (CIssue::EIssueType)pItem->data(eSeverity, Qt::UserRole).toInt();
	CIssue::EFixMode Mode = CIssue::eDefault;

	if (Type == CIssue::eFwRuleAltered)
	{
		QString message = tr("Do you want to accept the changes to the Firewall Rule (Yes) or restore the previouse state (No)?<br /><br />%1")
			.arg(QString::fromWCharArray(L"\u2022 ") + "<b>" + pItem->text(eName) + "</b>");

		switch (QMessageBox::question(theGUI, "MajorPrivacy", message, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes))
		{
		case QMessageBox::Yes: Mode = CIssue::eAccept; break;
		case QMessageBox::No: Mode = CIssue::eReject; break;
		case QMessageBox::Cancel: return; // cancel
		}
	}
	else if (theConf->GetInt("Options/DblClickFixQuick", -1) != 1)
	{
		QString message = tr("Do you want to fix the issue?<br /><br />%1")
			.arg(QString::fromWCharArray(L"\u2022 ") + "<b>" + pItem->text(eName) + "</b>");

		bool State = false;
		if (CCheckableMessageBox::question(theGUI, "MajorPrivacy", message, tr("Don't ask in future"), &State, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes, QMessageBox::Question) == QDialogButtonBox::No)
			return;
		if (State)
			theConf->SetValue("Options/DblClickFixQuick", 1);
	}

	STATUS Status = theCore->IssueManager()->FixIssue(Id, Mode);

	theGUI->CheckResults(QList<STATUS>() << Status, this);
}

