#include "pch.h"
#include "IssueView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/IssueManager.h"
#include "../MiscHelpers/Common/CheckableMessageBox.h"
#include "../MajorPrivacy.h"
#include "TweakView.h"

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
	CIssue::EFixMode Mode = CIssue::eDefault;
	if(sender() == m_pFixAccept)
		Mode = CIssue::eAccept;
	else if (sender() == m_pFixReject)
		Mode = CIssue::eReject;

	QString IssueNames;
	QStringList IssueIds;

	int MaxDisplay = 10;

	auto Items = m_pInfo->GetTree()->selectedItems();

	for (int i = 0; i < Items.count(); i++) {
		QTreeWidgetItem* pItem = Items[i];

		if(Mode != CIssue::eDefault) {
			if (pItem->data(eSeverity, Qt::UserRole).toInt() != (int)CIssue::eFwRuleAltered)
				continue;
		}

		IssueIds.append(pItem->data(eName, Qt::UserRole).toString());

		if (i < MaxDisplay) {
			if (i != 0) IssueNames.append("<br />");
			IssueNames.append(QString::fromWCharArray(L"\u2022 ")); // Unicode bullet
			IssueNames.append("<b>" + pItem->text(eName) + "</b>");
		}
	}
	if (IssueIds.count() > MaxDisplay) 
	{
		IssueNames.append(tr("<br />"));
		IssueNames.append(QString::fromWCharArray(L"\u2022 ")); // Unicode bullet
		IssueNames.append(tr("... and %1 more").arg(IssueIds.count() - MaxDisplay));
	}

	if (sender() == m_pIgnoreIssue)
	{
		if (QMessageBox::question(theGUI, "MajorPrivacy", tr("Do you really want to hide the following issue?<br /><br />%1").arg(IssueNames), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::No)
			return;

		foreach(QString Id, IssueIds)
			theCore->IssueManager()->HideIssue(Id);

		return;
	}
	
	if (sender() == m_pFixIssue)
	{
		if (QMessageBox::question(theGUI, "MajorPrivacy",  tr("Do you want to fix the following issue?<br /><br />%1").arg(IssueNames), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::No)
			return;
	}
	else if (sender() == m_pFixAccept || sender() == m_pFixReject)
	{
		if (QMessageBox::question(theGUI, "MajorPrivacy", tr("Do you want to %1 the following firewall rule changes?<br /><br />%2").arg(sender() == m_pFixAccept ? tr("accept") : tr("reject")).arg(IssueNames), QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::No)
			return;
	}
	else
		return; // unknown action
	
	QList<STATUS> Results;
	foreach(QString Id, IssueIds)
		Results.append(theCore->IssueManager()->FixIssue(Id, Mode));
	theGUI->CheckResults(Results, this);
}

void CIssueView::OnIssueClicked(QTreeWidgetItem* pItem)
{
}

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
