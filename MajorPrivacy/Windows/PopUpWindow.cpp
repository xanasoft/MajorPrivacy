#include "pch.h"
#include "PopUpWindow.h"
#include <windows.h>
#include <QWindow>
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/Settings.h"
#include "../Core/PrivacyCore.h"
#include "../Core/HashDB/HashDB.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/NtPathMgr.h"
#include "../Core/Network/NetLogEntry.h"
#include "FirewallRuleWnd.h"
#include "AccessRuleWnd.h"
#include "ProgramRuleWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Network/NetworkManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Core/Access/ResLogEntry.h"
#include "../Core/Access/AccessManager.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Library/Helpers/NtObj.h"
#include <QFileIconProvider>
#include "../Core/Volumes/VolumeManager.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Enclaves/EnclaveManager.h"

class CPathLabel : public QLabel 
{
public:
	CPathLabel(QWidget* parent = nullptr) : QLabel(parent) {}

	void setPath(const QString& path) {
		m_FullPath = path;
		updatePath();
		setToolTip(m_FullPath);
	}

protected:

	void resizeEvent(QResizeEvent* event) override {
		updatePath() ;
		QLabel::resizeEvent(event);
	}

	void updatePath() {
		QFontMetrics metrics(font());
		setText(metrics.elidedText(m_FullPath, Qt::ElideMiddle, width()));
	}

private:
	QString m_FullPath;
};

template<typename T>
void CPopUpWindow__ReplaceWidget(T* &pOld, T* pNew)
{
	pNew->setSizePolicy(pOld->sizePolicy());
	pOld->parentWidget()->layout()->replaceWidget(pOld, pNew);
	pOld->deleteLater();
	pOld = pNew;
}

void CPopUpWindow__UpgradeTree(QTreeWidget* pOld)
{
	pOld->setIndentation(0);
	QWidget* pParent = pOld->parentWidget();
	QWidget* pDummy = new QWidget(pParent);
	pParent->layout()->replaceWidget(pOld, pDummy);
	CPanelWidgetX* pNew = new CPanelWidgetX(pOld, pParent);
	pNew->setSizePolicy(pOld->sizePolicy());
	pParent->layout()->replaceWidget(pDummy, pNew);
	pDummy->deleteLater();
}

//bool CPopUpWindow__DarkMode = false;

CPopUpWindow::CPopUpWindow(QWidget* parent) : QMainWindow(parent)
{
	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	//flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	//flags &= ~Qt::WindowMinMaxButtonsHint;
	flags &= ~Qt::WindowMaximizeButtonHint;
	//flags &= ~Qt::WindowCloseButtonHint;
	//flags |= Qt::Tool;
	setWindowFlags(flags);

	this->setWindowTitle(tr("MajorPrivacy Notifications"));

	QWidget* centralWidget = new QWidget();
	ui.setupUi(centralWidget);
	this->setCentralWidget(centralWidget);

	for(int i=0; i < eTabMax; i++)
		ui.tabs->setTabEnabled(i, false);
	
	auto PopulateTimes = [](QComboBox* pCombo, bool bEx = false){
		pCombo->addItem(tr("Permanent"), -1);
		if (bEx)
			pCombo->addItem(tr("One Time"), -3);
		pCombo->addItem(tr("Untill Restart"), -2);
#ifdef _DEBUG
		pCombo->addItem(tr("for 1 Minute"), 60);
#else
		pCombo->addItem(tr("for 5 Minutes"), 300);
#endif
		pCombo->addItem(tr("for 15 Minutes"), 900);
		pCombo->addItem(tr("for 30 Minutes"), 1800);
		pCombo->addItem(tr("for 60 Minutes"), 3600);
	};

	// Firewall
	connect(ui.btnFwPrev, SIGNAL(clicked(bool)), this, SLOT(OnFwPrev()));
	connect(ui.btnFwNext, SIGNAL(clicked(bool)), this, SLOT(OnFwNext()));

	CPopUpWindow__ReplaceWidget<QLabel>(ui.lblFwPath, new CPathLabel());
	CPopUpWindow__UpgradeTree(ui.treeFw);
	CPopUpWindow__UpgradeTree(ui.treeFwRules);
	ui.treeFwRules->setSelectionMode(QAbstractItemView::ExtendedSelection);


	connect(ui.btnFwIgnore, SIGNAL(clicked(bool)), this, SLOT(OnFwAction()));
	QMenu* pFwIgnore = new QMenu(ui.btnFwIgnore);
	m_pFwAlwaysIgnoreApp = pFwIgnore->addAction(tr("Always Ignore this Program"), this, SLOT(OnFwAction()));
	ui.btnFwIgnore->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnFwIgnore->setMenu(pFwIgnore);

	PopulateTimes(ui.cmbFwTime);

	connect(ui.btnFwAllow, SIGNAL(clicked(bool)), this, SLOT(OnFwAction()));
	QMenu* pFwAllow = new QMenu(ui.btnFwAllow);
	m_pFwCustom = pFwAllow->addAction(tr("Create Custom Rule"), this, SLOT(OnFwAction()));
	//pFwIgnore->addSeparator();
	//m_pFwLanOnly = pFwAllow->addAction(tr("Allow LAN only"), this, SLOT(OnFwAction()));
	ui.btnFwAllow->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnFwAllow->setMenu(pFwAllow);

	connect(ui.btnFwBlock, SIGNAL(clicked(bool)), this, SLOT(OnFwAction()));


	connect(ui.btnFwDiscard, SIGNAL(clicked(bool)), this, SLOT(OnFwRule()));

	connect(ui.btnFwApprove, SIGNAL(clicked(bool)), this, SLOT(OnFwRule()));
	QMenu* pFwApprove = new QMenu(ui.btnFwApprove);
	m_pFwApproveAll = pFwApprove->addAction(tr("Approve All"), this, SLOT(OnFwRule()));
	ui.btnFwApprove->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnFwApprove->setMenu(pFwApprove);

	connect(ui.btnFwReject, SIGNAL(clicked(bool)), this, SLOT(OnFwRule()));
	QMenu* pFwReject = new QMenu(ui.btnFwReject);
	m_pFwRejectAll = pFwReject->addAction(tr("Reject All"), this, SLOT(OnFwRule()));
	ui.btnFwReject->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnFwReject->setMenu(pFwReject);


	connect(ui.treeFwRules, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnFwRuleDblClick(QTreeWidgetItem*)));

	// Resources
	connect(ui.btnResPrev, SIGNAL(clicked(bool)), this, SLOT(OnResPrev()));
	connect(ui.btnResNext, SIGNAL(clicked(bool)), this, SLOT(OnResNext()));

	CPopUpWindow__ReplaceWidget<QLabel>(ui.lblResPath, new CPathLabel());
	CPopUpWindow__UpgradeTree(ui.treeRes);

	connect(ui.btnResIgnore, SIGNAL(clicked(bool)), this, SLOT(OnResAction()));
	QMenu* pResIgnore = new QMenu(ui.btnResIgnore);
	m_pResAlwaysIgnoreApp = pResIgnore->addAction(tr("Always Ignore this Program"), this, SLOT(OnResAction()));
	m_pResAlwaysIgnorePath = pResIgnore->addAction(tr("Always Ignore this Path"), this, SLOT(OnResAction()));
	ui.btnResIgnore->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnResIgnore->setMenu(pResIgnore);

	PopulateTimes(ui.cmbResTime, true);

	connect(ui.btnResAllow, SIGNAL(clicked(bool)), this, SLOT(OnResAction()));
	QMenu* pResAllow = new QMenu(ui.btnResIgnore);
	m_pResRO = pResAllow->addAction(tr("Allow Read Only Access"), this, SLOT(OnResAction()));
	m_pResEnum = pResAllow->addAction(tr("Allow Directory Listing ONLY"), this, SLOT(OnResAction()));
	m_pResCustom = pResAllow->addAction(tr("Create Custom Rule"), this, SLOT(OnResAction()));
	ui.btnResAllow->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnResAllow->setMenu(pResAllow);

	connect(ui.btnResBlock, SIGNAL(clicked(bool)), this, SLOT(OnResAction()));

	// Execution
	connect(ui.btnExecPrev, SIGNAL(clicked(bool)), this, SLOT(OnExecPrev()));
	connect(ui.btnExecNext, SIGNAL(clicked(bool)), this, SLOT(OnExecNext()));

	CPopUpWindow__ReplaceWidget<QLabel>(ui.lblExecPath, new CPathLabel());
	CPopUpWindow__UpgradeTree(ui.treeExec);
	ui.treeExec->setSelectionMode(QAbstractItemView::ExtendedSelection);

	connect(ui.btnExecIgnore, SIGNAL(clicked(bool)), this, SLOT(OnExecAction()));
	QMenu* pExecIgnore = new QMenu(ui.btnExecIgnore);
	m_pExecAlwaysIgnoreFile = pExecIgnore->addAction(tr("Always Ignore this Binary"), this, SLOT(OnExecAction()));
	ui.btnExecIgnore->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnExecIgnore->setMenu(pExecIgnore);

	connect(ui.btnExecSign, SIGNAL(clicked(bool)), this, SLOT(OnExecAction()));
	ui.btnExecSign->setText(tr("Allow File"));
	QMenu* pExecSign = new QMenu(ui.btnExecIgnore);
	//m_pExecSignAll = pExecSign->addAction(tr("Sign ALL Binaries"), this, SLOT(OnExecAction()));
	//m_pExecSignCert = pExecSign->addAction(tr("Sign Certificate"), this, SLOT(OnExecAction()));
	m_pExecSignCert = pExecSign->addAction(tr("Allow Certificate"), this, SLOT(OnExecAction()));
	m_pExecSignCA = pExecSign->addAction(tr("Allow CA Certificate"), this, SLOT(OnExecAction()));
	ui.btnExecSign->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnExecSign->setMenu(pExecSign);

	
	m_iTopMost = 0;
	SetWindowPos((HWND)this->winId(), 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	
	m_uTimerID = startTimer(1000);

	restoreGeometry(theConf->GetBlob("PopUpWindow/Window_Geometry"));

	ui.treeFw->header()->restoreState(theConf->GetBlob("PopUpWindow/FirewallView_Columns"));
	ui.treeFwRules->header()->restoreState(theConf->GetBlob("PopUpWindow/FirewallRulesView_Columns"));
	ui.treeRes->header()->restoreState(theConf->GetBlob("PopUpWindow/AccessView_Columns"));
	ui.treeExec->header()->restoreState(theConf->GetBlob("PopUpWindow/ExecutionView_Columns"));
	//ui.treeConf->header()->restoreState(theConf->GetBlob("PopUpWindow/TweakView_Columns"));

	// undid tweaks should not happen randomly and will be reviewed on the main window start page
	delete ui.tabConf;
}

CPopUpWindow::~CPopUpWindow()
{
	killTimer(m_uTimerID);

	theConf->SetBlob("PopUpWindow/Window_Geometry", saveGeometry());

	theConf->SetBlob("PopUpWindow/FirewallView_Columns", ui.treeFw->header()->saveState());
	theConf->SetBlob("PopUpWindow/FirewallRulesView_Columns", ui.treeFwRules->header()->saveState());
	theConf->SetBlob("PopUpWindow/AccessView_Columns", ui.treeRes->header()->saveState());
	theConf->SetBlob("PopUpWindow/ExecutionView_Columns", ui.treeExec->header()->saveState());
	//theConf->SetBlob("PopUpWindow/TweakView_Columns", ui.treeConf->header()->saveState());
}

void CPopUpWindow::Show()
{
	Poke();

	QRect wndRect = this->frameGeometry();
	bool visibleOnAnyScreen = false;

	for (QScreen *screen : QGuiApplication::screens())
	{
		QRect scrRect = screen->availableGeometry();
		if (scrRect.intersects(wndRect))
		{
			visibleOnAnyScreen = true;
			break;
		}
	}

	if (!visibleOnAnyScreen)
	{
		QScreen *screen = this->windowHandle()->screen();
		QRect scrRect = screen->availableGeometry();

		this->resize(300, 400);
		this->move(scrRect.right() - 300 - 20, scrRect.bottom() - 400 - 50);
	}

	SafeShow(this);
}

void CPopUpWindow::Poke()
{
	if (!this->isVisible() || m_iTopMost <= -5) {
		SetWindowPos((HWND)this->winId(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		m_iTopMost = 5;
	}

	UpdateTimeOut((ETabs)ui.tabs->currentIndex());
}

void CPopUpWindow::closeEvent(QCloseEvent *e)
{
	e->ignore();

	this->hide();
}

void CPopUpWindow::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() != m_uTimerID)
		return;

	if (m_iTopMost > -5 && (--m_iTopMost == 0)) {
		SetWindowPos((HWND)this->winId(), HWND_NOTOPMOST , 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}

	UpdateTimeOut((ETabs)ui.tabs->currentIndex());
}

void CPopUpWindow::UpdateTimeOut(ETabs Tab)
{
	if (Tab == eTabRes && m_iResIndex != -1)
	{
		CProgramFilePtr pProgram = m_ResPrograms[m_iResIndex];

		QList<SLogEntry>& List = m_ResEvents[pProgram];

		for (int i = 0; i < ui.treeRes->topLevelItemCount(); i++) {
			const CResLogEntry* pEntry = dynamic_cast<const CResLogEntry*>(List[i].pPtr.constData());

			QTreeWidgetItem* pItem = ui.treeRes->topLevelItem(i);

			if (!List[i].Waiting.isEmpty()) {
				if (List[i].Waiting.first().TimeOut > GetCurTick())
					pItem->setText(eResStatus, tr("Block in %1").arg((List[i].Waiting.first().TimeOut - GetCurTick()) / 1000));
				else
					pItem->setText(eResStatus, pEntry->GetStatusStr());
			}
		}
	}
}

void CPopUpWindow::TryHide()
{
	bool bCanHide = true;

	// Firewall
	if (m_FwPrograms.size() == 0)
		ui.tabs->setTabEnabled(eTabFw, false);
	else
		bCanHide = false;

	// Resources
	if (m_ResPrograms.size() == 0)
		ui.tabs->setTabEnabled(eTabRes, false);
	else
		bCanHide = false;

	// Execution
	if (m_ExecPrograms.size() == 0)
		ui.tabs->setTabEnabled(eTabExec, false);
	else
		bCanHide = false;

	//Cfg
	//todo

	if(bCanHide)
		this->hide();
}

//////////////////////////////////////////////////////////////////////////
// Firewall

void CPopUpWindow::PushFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry)
{
	SFwProgram Prog = { SFwProgram::eNetworkEvent, pProgram };
	QList<SLogEntry>& List = m_FwEvents[pProgram];
	if (List.isEmpty())
		m_FwPrograms.append(Prog);
	bool bFound = false;
	for (int i = 0; i < List.size(); i++) {
		if (pEntry->Match(List[i].pPtr.constData())) {
			bFound = true;
			break;
		}
	}
	if (!bFound) List.append(pEntry);

	PushFwEvent(Prog);
}

void CPopUpWindow::PushFwRuleEvent(EConfigEvent EventType, const CFwRulePtr& pFwRule)
{
	CProgramItemPtr pProgram = theCore->ProgramManager()->GetProgramByID(pFwRule->GetProgramID());

	SFwProgram Prog = { SFwProgram::eRuleEvent, pProgram };
	QList<SFwRuleEvent>& List = m_FwRuleEvents[pProgram];
	if (List.isEmpty())
		m_FwPrograms.append(Prog);
	bool bFound = false;
	for (int i = 0; i < List.size(); i++) {
		if (pFwRule->GetGuid() == List[i].pFwRule->GetGuid()) {
			List[i].EventType = EventType;
			List[i].pFwRule = pFwRule;
			if(ui.tabs->currentIndex() == eTabFw && (m_iFwIndex >= 0 && m_iFwIndex < m_FwPrograms.size()) && m_FwPrograms[m_iFwIndex].pItem == pProgram) {
				for(int j = 0; j < ui.treeFwRules->topLevelItemCount(); j++) {
					QTreeWidgetItem* pItem = ui.treeFwRules->topLevelItem(j);
					if (pItem->data(eFwEvent, Qt::UserRole).toString() == pFwRule->GetGuid().ToQS()) {
						UpdateFwRule(List[i], pItem);
						break;
					}
				}
			}
			bFound = true;
			break;
		}
	}
	if (!bFound) List.append(SFwRuleEvent{ EventType, pFwRule });

	PushFwEvent(Prog);
}

void CPopUpWindow::PushFwEvent(const SFwProgram& Prog)
{
	int oldIndex = m_iFwIndex;

	if (m_iFwIndex < 0)
		m_iFwIndex = 0;
	else if (m_iFwIndex >= m_FwPrograms.size())
		m_iFwIndex = m_FwPrograms.size() - 1;

	ui.tabs->setTabEnabled(eTabFw, true);
	UpdateFwCount();

	// don't update if the event is for a different entry
	int index = m_FwPrograms.indexOf(Prog);
	if (m_iFwIndex == index)
		LoadFwEntry(oldIndex == m_iFwIndex);

	if (!isVisible()) {
		ui.tabs->setCurrentIndex(eTabFw);
		ui.cmbFwTime->setCurrentIndex(0);
		Show();
	} else
		Poke();
}

void CPopUpWindow::PopFwEvent()
{
	if (m_iFwIndex == -1)
		return;

	switch (m_FwPrograms[m_iFwIndex].Type)
	{
	case SFwProgram::eNetworkEvent: m_FwEvents.remove(m_FwPrograms[m_iFwIndex].pItem.objectCast<CProgramFile>()); break;
	case SFwProgram::eRuleEvent:	m_FwRuleEvents.remove(m_FwPrograms[m_iFwIndex].pItem); break;
	}
	m_FwPrograms.removeAt(m_iFwIndex);

	if (m_iFwIndex >= m_FwPrograms.size())
		m_iFwIndex = m_FwPrograms.size() - 1;
	if (m_iFwIndex < 0)
	{
		m_iFwIndex = -1;
		TryHide();
		return;
	}

	UpdateFwCount();
	LoadFwEntry();
}

void CPopUpWindow::OnFwNext()
{
	if (m_iFwIndex + 1 < m_FwPrograms.size())
		m_iFwIndex++;
	UpdateFwCount();
	LoadFwEntry();
}

void CPopUpWindow::OnFwPrev()
{
	if (m_iFwIndex > 0)
		m_iFwIndex--;
	UpdateFwCount();
	LoadFwEntry();
}

void CPopUpWindow::UpdateFwCount()
{
	ui.lblFwCount->setText(tr("%1/%2").arg(m_iFwIndex + 1).arg(m_FwPrograms.size()));
	ui.btnFwPrev->setEnabled(m_iFwIndex + 1 > 1);
	ui.btnFwNext->setEnabled(m_iFwIndex + 1 < m_FwPrograms.size());
}

void CPopUpWindow::LoadFwEntry(bool bUpdate)
{
	CProgramItemPtr pProgram = m_FwPrograms[m_iFwIndex].pItem;
	if (!bUpdate) 
	{
		QString Title = pProgram->GetName();
		QString Name = QFileInfo(pProgram->GetPath()).fileName();
		if(Title.isEmpty()) Title = Name;
		ui.grpFw->setTitle(Title);
		ui.lblFwProc->setText(pProgram->GetNameEx());
		ui.lblFwAux->setText(pProgram->GetPublisher());
		ui.lblFwIcon->setPixmap(pProgram->GetIcon().pixmap(32, 32));
		((CPathLabel*)ui.lblFwPath)->setPath(pProgram->GetPath());
	}

	switch (m_FwPrograms[m_iFwIndex].Type)
	{
	case SFwProgram::eNetworkEvent:
		ui.stkFw1->setCurrentIndex(0);
		ui.stkFw2->setCurrentIndex(0);
		LoadFwEvent(pProgram.objectCast<CProgramFile>(), bUpdate); 
		break;
	case SFwProgram::eRuleEvent:
		ui.stkFw1->setCurrentIndex(1);
		ui.stkFw2->setCurrentIndex(1);
		LoadFwRule(pProgram, bUpdate); 
		break;
	}
}

void CPopUpWindow::LoadFwEvent(const CProgramFilePtr& pProgram, bool bUpdate)
{
	const QList<SLogEntry>& List = m_FwEvents[pProgram];

	if (!bUpdate) 
		ui.treeFw->clear();

	// todo update host name delayed resolution
	
	for (int i = ui.treeFw->topLevelItemCount(); i < List.size(); i++) {
		const CNetLogEntry* pEntry = dynamic_cast<const CNetLogEntry*>(List[i].pPtr.constData());
		if(!pEntry) continue;
		QTreeWidgetItem* pItem = new QTreeWidgetItem();

		switch (pEntry->GetDirection()){
		case EFwDirections::Inbound: pItem->setIcon(eFwProtocol, QIcon(":/Icons/ArrowDown")); break;
		case EFwDirections::Outbound: pItem->setIcon(eFwProtocol, QIcon(":/Icons/ArrowUp")); break;
		case EFwDirections::Bidirectional: pItem->setIcon(eFwProtocol, QIcon(":/Icons/ArrowUpDown")); break;
		}
		pItem->setText(eFwProtocol, pEntry->GetProtocolTypeStr());
		pItem->setData(eFwProtocol, Qt::UserRole, (uint32)pEntry->GetDirection());
		QString EndPoint = tr("%1:%2").arg(pEntry->GetRemoteAddress().toString()).arg(pEntry->GetRemotePort());
		QString Host = pEntry->GetRemoteHostName();
		if(!Host.isEmpty()) EndPoint += tr(" (%1)").arg(Host);
		pItem->setText(eFwEndpoint, EndPoint);
		pItem->setToolTip(eFwEndpoint, Host);
		pItem->setText(eFwTimeStamp, QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss.zzz"));
		if(pEntry->GetOwnerService().isEmpty())
			pItem->setText(eFwProcessId, QString::number(pEntry->GetProcessId()));
		else
			pItem->setText(eFwProcessId, tr("%1 (%2)").arg(pEntry->GetProcessId()).arg(pEntry->GetOwnerService()));
		CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pEntry->GetProcessId(), true);
		if(pProcess) pItem->setText(eFwCmdLine, pProcess->GetCmdLine());
		ui.treeFw->addTopLevelItem(pItem);
	}
}

void CPopUpWindow::LoadFwRule(const CProgramItemPtr& pProgram, bool bUpdate)
{
	const QList<SFwRuleEvent>& List = m_FwRuleEvents[pProgram];

	if (!bUpdate) 
		ui.treeFwRules->clear();

	for (int i = ui.treeFwRules->topLevelItemCount(); i < List.size(); i++) {
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		pItem->setData(eFwEvent, Qt::UserRole, List[i].pFwRule->GetGuid().ToQS());
		UpdateFwRule(List[i], pItem);
		ui.treeFwRules->addTopLevelItem(pItem);
	}
}

void CPopUpWindow::UpdateFwRule(const SFwRuleEvent& Event, QTreeWidgetItem* pItem)
{
	CFwRulePtr pFwRule = Event.pFwRule;
	pItem->setData(eFwAction, Qt::UserRole, (int)Event.EventType);
	CFwRulePtr pFwRuleOld;
	if (Event.EventType == EConfigEvent::eModified)
	{
		pItem->setIcon(eFwEvent, QIcon(":/Icons/Warning.png"));
		pItem->setText(eFwEvent, tr("Modified"));

		pFwRuleOld = pFwRule;
		pFwRule = theCore->NetworkManager()->GetFwRules().value(pFwRule->GetOriginalGuid());
	}
	else
	{
		if (Event.EventType == EConfigEvent::eAdded)
		{
			pItem->setIcon(eFwEvent, QIcon(":/Icons/Add.png"));
			pItem->setText(eFwEvent, tr("Added"));
		}
		else if (Event.EventType == EConfigEvent::eRemoved)
		{
			pItem->setIcon(eFwEvent, QIcon(":/Icons/Error.png"));
			pItem->setText(eFwEvent, tr("Removed"));
		}	
	}
	if (pFwRuleOld)
	{
		QString Action;
		if (pFwRule->IsEnabled() != pFwRuleOld->IsEnabled())
			Action += tr("%1 -> %2 ").arg(pFwRuleOld->IsEnabled() ? tr("Enabled") : tr("Disabled")).arg(pFwRule->IsEnabled() ? tr("Enabled") : tr("Disabled"));

		if(pFwRule->GetAction() == pFwRuleOld->GetAction())
			Action += pFwRule->GetActionStr();
		else
			Action += tr("%1 -> %2").arg(pFwRuleOld->GetActionStr()).arg(pFwRule->GetActionStr());

		if (pFwRule->GetDirection() == pFwRuleOld->GetDirection())
			Action += " " + pFwRule->GetDirectionStr();
		else
			Action += tr(" %1 -> %2").arg(pFwRuleOld->GetDirectionStr()).arg(pFwRule->GetDirectionStr());

		pItem->setText(eFwAction, Action);
	}
	else
		pItem->setText(eFwAction, pFwRule->GetActionStr() + " " + pFwRule->GetDirectionStr());
	pItem->setText(eFwRuleName, CMajorPrivacy::GetResourceStr(pFwRule->GetName()));
	pItem->setToolTip(eFwRuleName, pFwRule->GetName());
}

void CPopUpWindow::OnFwAction()
{
	CProgramFilePtr pProgram = m_FwPrograms[m_iFwIndex].pItem.objectCast<CProgramFile>();

	QObject* pSender = sender();

	if(pSender == ui.btnFwIgnore || pSender == m_pFwAlwaysIgnoreApp)
	{
		if (pSender == m_pFwAlwaysIgnoreApp)
			theGUI->IgnoreEvent(ERuleType::eFirewall, pProgram);
		PopFwEvent();
		return;
	}

	bool bInbound = false;
	//bool bOutbound = false;
	for (int i = 0; i < ui.treeFw->topLevelItemCount(); i++) {
		QTreeWidgetItem* pItem = ui.treeFw->topLevelItem(i);
		EFwDirections Direction = (EFwDirections)pItem->data(eFwProtocol, Qt::UserRole).toUInt();
		if (Direction == EFwDirections::Inbound)
			bInbound = true;
		//else if (Direction == EFwDirections::Outbound)
		//	bOutbound = true;
	}
	
	CFwRulePtr pRule = CFwRulePtr(new CFwRule(pProgram->GetID()));
	pRule->SetApproved();
	pRule->SetSource(EFwRuleSource::eMajorPrivacy);
	pRule->SetEnabled(true);
	pRule->SetName(tr("MajorPrivacy-Rule, %1").arg(pProgram->GetNameEx()));
	//pRule->SetDirection(SVC_API_FW_BIDIRECTIONAL);
	pRule->SetProfile((int)EFwProfiles::All);
	pRule->SetProtocol(EFwKnownProtocols::Any);
	pRule->SetInterface((int)EFwInterfaces::All);

	if (ui.cmbFwTime->currentData().toInt() != -1) {
		if (ui.cmbFwTime->currentData().toInt() == -2)
			pRule->SetTemporary(true);
		else
			pRule->SetTimeOut(ui.cmbFwTime->currentData().toInt());
	}

	if (pSender == m_pFwCustom)
	{
		CFirewallRuleWnd* pFirewallRuleWnd = new CFirewallRuleWnd(pRule, QSet<CProgramItemPtr>() << pProgram);
		if (!pFirewallRuleWnd->exec())
			return;
	}
	else 
	{
		if (pSender == ui.btnFwAllow)
			pRule->SetAction(EFwActions::Allow);
		/*else if (pSender == m_pFwLanOnly)
		{
			pRule->SetAction(SVC_API_FW_ALLOW);
			// todo set remote addresses for LAN
		}*/
		else if (pSender == ui.btnFwBlock)
			pRule->SetAction(EFwActions::Block);

		QList<STATUS> Statuses;

		//if (bOutbound) {
			pRule->SetDirection(EFwDirections::Outbound);
			Statuses << theCore->NetworkManager()->SetFwRule(pRule);
		//}

		if (bInbound || theConf->GetBool("NetworkFirewall/MakeInOutRules", false)) {
			pRule->SetDirection(EFwDirections::Inbound);
			Statuses << theCore->NetworkManager()->SetFwRule(pRule);
		}

		if(theGUI->CheckResults(Statuses, this) != 0)
			return;
	}

	PopFwEvent();
}

void CPopUpWindow::OnFwRule()
{
	CProgramItemPtr pProgram = m_FwPrograms[m_iFwIndex].pItem;

	QObject* pSender = sender();

	if(pSender == ui.btnFwDiscard)
	{
		PopFwEvent();
		return;
	}

	QList<SFwRuleEvent>& List = m_FwRuleEvents[pProgram];

	QList<QTreeWidgetItem*> Items;
	if (pSender == m_pFwApproveAll || pSender == m_pFwRejectAll)
	{
		for(int i = 0; i < ui.treeFwRules->topLevelItemCount(); i++) {
			QTreeWidgetItem* pItem = ui.treeFwRules->topLevelItem(i);
			Items.append(pItem);	
		}
	}
	else
		Items = ui.treeFwRules->selectedItems();

	if (Items.isEmpty()) {
		QMessageBox::information(this, "MajorPrivacy", tr("Please select a rule."));
		return;
	}

	QList<SFwRuleEvent> Events;
	for (auto pItem : Items) {
		for (auto I = List.begin(); I != List.end(); ++I) {
			if ((int)I->EventType == pItem->data(eFwAction, Qt::UserRole).toInt() &&
				I->pFwRule->GetGuid().ToQS() == pItem->data(eFwEvent, Qt::UserRole).toString()) {
				Events.append(*I);
				break;
			}
		}
	}

	QList<STATUS> Statuses;
	foreach(const SFwRuleEvent& Event, Events)
	{
		STATUS Status;

		EConfigEvent EventType = Event.EventType;
		CFwRulePtr pFwRule = Event.pFwRule;
		CFwRulePtr pFwRuleOld;
		if (EventType == EConfigEvent::eModified)
		{
			pFwRuleOld = pFwRule;
			pFwRule = theCore->NetworkManager()->GetFwRules().value(pFwRule->GetOriginalGuid());
			if (!pFwRule) {
				pFwRule = pFwRuleOld;
				EventType = EConfigEvent::eRemoved;
			}
		}

		if (pSender == ui.btnFwApprove || pSender == m_pFwApproveAll)
		{
			switch (EventType)
			{
			case EConfigEvent::eModified:
				ASSERT(pFwRuleOld->IsBackup());
				Status = theCore->NetworkManager()->DelFwRule(pFwRuleOld);
				if(!Status) break;
			case EConfigEvent::eAdded:
				if(pFwRule->GetState() == EFwRuleState::eUnapprovedDisabled)
					pFwRule->SetEnabled(true);
				pFwRule->SetApproved();
				Status = theCore->NetworkManager()->SetFwRule(pFwRule);
				break;
			case EConfigEvent::eRemoved:
				ASSERT(pFwRule->IsBackup());
				Status = theCore->NetworkManager()->DelFwRule(pFwRule);
				break;
			}
		}
		else if (pSender == ui.btnFwReject || pSender == m_pFwRejectAll)
		{
			switch (EventType)
			{
			case EConfigEvent::eModified:
				ASSERT(pFwRuleOld->IsBackup());
				pFwRuleOld->SetApproved();
				Status = theCore->NetworkManager()->SetFwRule(pFwRuleOld);
				break;
			case EConfigEvent::eAdded:
				Status = theCore->NetworkManager()->DelFwRule(pFwRule);
				break;
			case EConfigEvent::eRemoved:
				ASSERT(pFwRule->IsBackup());
				pFwRule->SetApproved();
				Status = theCore->NetworkManager()->SetFwRule(pFwRule);
				break;
			}
		}

		if (!Status.IsError())
			List.removeOne(Event);
		Statuses.append(Status);
	}

	theGUI->CheckResults(Statuses, this);
	if(List.isEmpty())
		PopFwEvent();
	else
		LoadFwEntry();
}

void CPopUpWindow::OnFwRuleDblClick(QTreeWidgetItem* pItem)
{
	CProgramFilePtr pProgram = m_FwPrograms[m_iFwIndex].pItem.objectCast<CProgramFile>();

	EConfigEvent EventType = (EConfigEvent)pItem->data(eFwAction, Qt::UserRole).toInt();
	CFwRulePtr pFwRule = theCore->NetworkManager()->GetFwRules().value(pItem->data(eFwEvent, Qt::UserRole).toString());
	CFwRulePtr pFwRuleOld;
	if (EventType == EConfigEvent::eModified)
	{
		pFwRuleOld = pFwRule;
		pFwRule = theCore->NetworkManager()->GetFwRules().value(pFwRule->GetOriginalGuid());
		if (!pFwRule) {
			pFwRule = pFwRuleOld;
			EventType = EConfigEvent::eRemoved;
		}
	}

	QSet<CProgramItemPtr> Items;
	if (pProgram)
		Items.insert(pProgram);

	CFirewallRuleWnd* pFirewallRuleWnd = new CFirewallRuleWnd(pFwRule, Items);
	pFirewallRuleWnd->SetReadOnly(true);
	pFirewallRuleWnd->exec();
}

//////////////////////////////////////////////////////////////////////////
// Access

void CPopUpWindow::PushResEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry, uint32 TimeOut)
{
	QList<SLogEntry>& List = m_ResEvents[pProgram];
	if (List.isEmpty())
		m_ResPrograms.append(pProgram);
	int iFound = -1;
	for (int i = 0; i < List.size(); i++) {
		if (pEntry->Match(List[i].pPtr.constData())) {
			if(TimeOut) List[i].AddWaiting(pEntry, TimeOut);
			iFound = i;
			break;
		}
	}
	if (iFound == -1) List.append(pEntry);
	if (TimeOut) List.last().AddWaiting(pEntry, TimeOut);

	int oldIndex = m_iResIndex;

	if (m_iResIndex < 0)
		m_iResIndex = 0;
	else if (m_iResIndex >= m_ResPrograms.size())
		m_iResIndex = m_ResPrograms.size() - 1;

	ui.tabs->setTabEnabled(eTabRes, true);
	UpdateResCount();

	// don't update if the event is for a different entry
	int index = m_ResPrograms.indexOf(pProgram);
	if (m_iResIndex == index)
		LoadResEntry(oldIndex == m_iResIndex);

	if (!isVisible()) {
		ui.tabs->setCurrentIndex(eTabRes);
		ui.cmbResTime->setCurrentIndex(0);
		Show();
	} else
		Poke();
}

void CPopUpWindow::PopResEvent(const QString& Path, EAccessRuleType Action)
{
	if (m_iResIndex == -1)
		return;

	if (Action == EAccessRuleType::eNone)
		Action = EAccessRuleType::eBlock;

	CProgramFilePtr pProgram = m_ResPrograms[m_iResIndex];

	QString PathExp = QRegularExpression::escape(Path).replace("\\*",".*");
	QRegularExpression RegExp(PathExp, QRegularExpression::CaseInsensitiveOption);

	for (int i = 0; i < ui.treeRes->topLevelItemCount(); i++) {
		QTreeWidgetItem* pItem = ui.treeRes->topLevelItem(i);
		QString CurPath = pItem->data(eResPath, Qt::UserRole).toString();
		if (RegExp.match(CurPath).hasMatch() || RegExp.match(CurPath + "\\").hasMatch()) {
			i--;
			quint64 ref = pItem->data(eResTimeStamp, Qt::UserRole).toULongLong();
			delete pItem;

			QList<SLogEntry>& List = m_ResEvents[pProgram];
			for (auto I = List.begin(); I != List.end(); )
			{
				if ((quint64)I->pPtr.data() == ref)
				{
					foreach(const auto& pEvent, I->Waiting)
						theCore->SetAccessEventAction(pEvent.EventId, Action);
					I = List.erase(I);
				}
				else
					++I;
			}
		}
	}
	if(ui.treeRes->topLevelItemCount() > 0)
		return;

	m_ResPrograms.removeAt(m_iResIndex);
	m_ResEvents.remove(pProgram);

	if (m_iResIndex >= m_ResPrograms.size())
		m_iResIndex = m_ResPrograms.size() - 1;
	if (m_iResIndex < 0)
	{
		m_iResIndex = -1;
		TryHide();
		return;
	}

	UpdateResCount();
	LoadResEntry();
}

void CPopUpWindow::OnResNext()
{
	if (m_iResIndex + 1 < m_ResPrograms.size())
		m_iResIndex++;
	UpdateResCount();
	LoadResEntry();
}

void CPopUpWindow::OnResPrev()
{
	if (m_iResIndex > 0)
		m_iResIndex--;
	UpdateResCount();
	LoadResEntry();
}

void CPopUpWindow::UpdateResCount()
{
	ui.lblResCount->setText(tr("%1/%2").arg(m_iResIndex + 1).arg(m_ResPrograms.size()));
	ui.btnResPrev->setEnabled(m_iResIndex + 1 > 1);
	ui.btnResNext->setEnabled(m_iResIndex + 1 < m_ResPrograms.size());
}

void CPopUpWindow::LoadResEntry(bool bUpdate)
{
	QFileIconProvider IconProvider;

	CProgramFilePtr pProgram = m_ResPrograms[m_iResIndex];
	const QList<SLogEntry>& List = m_ResEvents[pProgram];

	if (!bUpdate) 
	{
		QString Title = pProgram->GetName();
		QString Name = QFileInfo(pProgram->GetPath()).fileName();
		if(Title.isEmpty()) Title = Name;
		ui.grpRes->setTitle(Title);
		ui.lblResProc->setText(pProgram->GetNameEx());
		ui.lblResAux->setText(pProgram->GetPublisher());
		ui.lblResIcon->setPixmap(pProgram->GetIcon().pixmap(32, 32));
		((CPathLabel*)ui.lblResPath)->setPath(pProgram->GetPath());
		ui.treeRes->clear();
	}

	for (int i = ui.treeRes->topLevelItemCount(); i < List.size(); i++) {
		const CResLogEntry* pEntry = dynamic_cast<const CResLogEntry*>(List[i].pPtr.constData());
		if(!pEntry) continue;
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		QString DosPath = theCore->NormalizePath(pEntry->GetNtPath());
		pItem->setData(eResPath, Qt::UserRole, pEntry->GetNtPath());
		pItem->setText(eResPath, DosPath);
		pItem->setText(eResStatus, pEntry->GetStatusStr());
		FILE_BASIC_INFORMATION info = { 0 };
		SNtObject NtObject(pEntry->GetNtPath().toStdWString(), NULL);
		NtQueryAttributesFile(&NtObject.attr, &info);
		pItem->setData(eResStatus, Qt::UserRole, info.FileAttributes == FILE_ATTRIBUTE_DIRECTORY);
		if(info.FileAttributes == FILE_ATTRIBUTE_DIRECTORY)
			pItem->setIcon(eResPath, IconProvider.icon(QFileIconProvider::Folder));
		else //pItem->setIcon(eResPath, IconProvider.icon(QFileIconProvider::File));
			pItem->setIcon(eResPath, IconProvider.icon(QFileInfo(QFileInfo(DosPath).fileName()))); // prevent fiel access pass only filename to get default icon
		pItem->setText(eResTimeStamp, QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss.zzz"));
		pItem->setData(eResTimeStamp, Qt::UserRole, (quint64)pEntry);
		QFlexGuid EnclaveGuid = pEntry->GetEnclaveGuid();
		if (!EnclaveGuid.IsNull()) {
			CEnclavePtr pEnclave = theCore->EnclaveManager()->GetEnclave(EnclaveGuid);
			pItem->setText(eResEnclave, pEnclave ? pEnclave->GetName() : EnclaveGuid.ToQS());
			pItem->setData(eResEnclave, Qt::UserRole, EnclaveGuid.ToQV());
		}
		if(pEntry->GetOwnerService().isEmpty())
			pItem->setText(eResProcessId, QString::number(pEntry->GetProcessId()));
		else
			pItem->setText(eResProcessId, tr("%1 (%2)").arg(pEntry->GetProcessId()).arg(pEntry->GetOwnerService()));
		CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pEntry->GetProcessId(), true);
		if(pProcess) pItem->setText(eResCmdLine, pProcess->GetCmdLine());
		ui.treeRes->addTopLevelItem(pItem);
	}
}

void CPopUpWindow::OnResAction()
{
	QObject* pSender = sender();

	CProgramFilePtr pProgram = m_ResPrograms[m_iResIndex];

	if(pSender == ui.btnResIgnore || pSender == m_pResAlwaysIgnoreApp)
	{
		if (pSender == m_pResAlwaysIgnoreApp)
			theGUI->IgnoreEvent(ERuleType::eAccess, pProgram);
		PopResEvent("*", EAccessRuleType::eNone);
		return;
	}

	QTreeWidgetItem* pItem = ui.treeRes->currentItem();
	if (!pItem) {
		QMessageBox::information(this, "MajorPrivacy", tr("Please select a resource to create a rule for."));
		return;
	}
	QString NtPath = pItem->data(eResPath, Qt::UserRole).toString();
	bool bIsDir = pItem->data(eResStatus, Qt::UserRole).toBool();
	if (bIsDir && NtPath.right(1) != "\\")
		NtPath += "\\";
	if (NtPath.right(1) == "\\")
		NtPath += "*";

	QString Path = theCore->NormalizePath(NtPath);

	if(pSender == m_pResAlwaysIgnorePath)
	{
		theGUI->IgnoreEvent(ERuleType::eAccess, NULL, Path);
		PopResEvent(NtPath, EAccessRuleType::eNone);
		return;
	}

	bool bUseVolumeRules = theConf->GetBool("ResourceAccess/UseVolumeRules", true);

	CVolumePtr pCurVolume;
	theCore->VolumeManager()->Update();
	auto Volumes = theCore->VolumeManager()->List();
	foreach(const CVolumePtr& pVolume, Volumes) {
		if(pVolume->GetStatus() != CVolume::eMounted)
			continue;
		QString DevicePath = pVolume->GetDevicePath();
		if (NtPath.startsWith(DevicePath, Qt::CaseInsensitive)) {
			if(bUseVolumeRules)
				Path = NtPath;
			else
				Path = QString::fromStdWString(CNtPathMgr::Instance()->TranslateNtToDosPath((pVolume->GetImagePath() + "/" + NtPath.mid(DevicePath.length() + 1)).toStdWString()));
			pCurVolume = pVolume;
			break;
		}
	}

	if (theGUI->IsDrvConfigLocked() && !(bUseVolumeRules && pCurVolume)) {
		STATUS Status = theGUI->UnlockDrvConfig();
		if (Status.IsError()) {
			theGUI->CheckResults(QList<STATUS>() << Status, this);
			return;
		}
	}

	QFlexGuid EnclaveGuid;
	EnclaveGuid.FromQV(pItem->data(eResEnclave, Qt::UserRole));

	CAccessRulePtr pRule = CAccessRulePtr(new CAccessRule(pProgram->GetID()));
	pRule->SetEnabled(true);
	pRule->SetName(tr("%1 - Rule").arg(pProgram->GetNameEx()));
	pRule->SetPath(Path);
	pRule->SetEnclave(EnclaveGuid);
	if(bUseVolumeRules && pCurVolume)
		pRule->SetVolumeRule(true);

	if (ui.cmbResTime->currentData().toInt() != -1){
		if (ui.cmbResTime->currentData().toInt() == -2)
			pRule->SetTemporary(true);
		else
			pRule->SetTimeOut(ui.cmbResTime->currentData().toInt());
	}

	if (pSender == m_pResCustom)
	{
		CAccessRuleWnd* pAccessRuleWnd = new CAccessRuleWnd(pRule, QSet<CProgramItemPtr>() << pProgram);
		if (!pAccessRuleWnd->exec())
			return;

		Path = pRule->GetPath(); // could have been changed

		if (bUseVolumeRules && pCurVolume) {
			if (Path.startsWith("\\"))
				NtPath = Path;
			else
				NtPath = QString::fromStdWString(CNtPathMgr::Instance()->TranslateDosToNtPath(Path.toStdWString()));
		}
	}
	else 
	{
		if(pSender == m_pResRO)
			pRule->SetType(EAccessRuleType::eAllowRO);
		else if(pSender == m_pResEnum)
			pRule->SetType(EAccessRuleType::eEnum);
		else if (pSender == ui.btnResAllow)
			pRule->SetType(EAccessRuleType::eAllow);
		else if (pSender == ui.btnResBlock)
			pRule->SetType(EAccessRuleType::eBlock);

		if (ui.cmbResTime->currentData().toInt() != -3) {
			STATUS Status = theCore->AccessManager()->SetAccessRule(pRule);
			if (theGUI->CheckResults(QList<STATUS>() << Status, this) != 0)
				return;
		}
	}

	if (bUseVolumeRules && pCurVolume)
	{
		NtPath = QString::fromStdWString(CNtPathMgr::Instance()->TranslateDosToNtPath(Path.toStdWString()));
		
		QString ImagePath = pCurVolume->GetImagePath();
		if (Path.startsWith(ImagePath, Qt::CaseInsensitive)) {
			Path = Path.mid(ImagePath.length() + 1);
			NtPath = pCurVolume->GetDevicePath() + "\\" + Path;
		}
	}

	PopResEvent(NtPath, pRule->GetType());
}

//////////////////////////////////////////////////////////////////////////
// Execution

void CPopUpWindow::PushExecEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry)
{
	QList<SLogEntry>& List = m_ExecEvents[pProgram];
	if (List.isEmpty())
		m_ExecPrograms.append(pProgram);
	bool bFound = false;
	for (int i = 0; i < List.size(); i++) {
		if (pEntry->Match(List[i].pPtr.constData())) {
			bFound = true;
			break;
		}
	}
	if (!bFound) List.append(pEntry);

	int oldIndex = m_iExecIndex;

	if (m_iExecIndex < 0)
		m_iExecIndex = 0;
	else if (m_iExecIndex >= m_ExecPrograms.size())
		m_iExecIndex = m_ExecPrograms.size() - 1;

	ui.tabs->setTabEnabled(eTabExec, true);
	UpdateExecCount();

	// don't update if the event is for a different entry
	int index = m_ExecPrograms.indexOf(pProgram);
	if (m_iExecIndex == index)
		LoadExecEntry(oldIndex == m_iExecIndex);

	if (!isVisible()) {
		ui.tabs->setCurrentIndex(eTabExec);
		Show();
	} else
		Poke();
}

void CPopUpWindow::PopExecEvent(const QStringList& Paths)
{
	if (m_iExecIndex == -1)
		return;

	for (int i = 0; i < ui.treeExec->topLevelItemCount(); i++) {
		QTreeWidgetItem* pItem = ui.treeExec->topLevelItem(i);
		if (Paths.isEmpty() || Paths.contains(pItem->data(eExecPath, Qt::UserRole).toString(), Qt::CaseInsensitive)) {
			i--;
			delete pItem;
		}
	}
	if(ui.treeExec->topLevelItemCount() > 0)
		return;

	CProgramFilePtr pProgram = m_ExecPrograms[m_iExecIndex];
	m_ExecPrograms.removeAt(m_iExecIndex);
	m_ExecEvents.remove(pProgram);

	if (m_iExecIndex >= m_ExecPrograms.size())
		m_iExecIndex = m_ExecPrograms.size() - 1;
	if (m_iExecIndex < 0)
	{
		m_iExecIndex = -1;
		TryHide();
		return;
	}

	UpdateExecCount();
	LoadExecEntry();
}

void CPopUpWindow::OnExecNext()
{
	if (m_iExecIndex + 1 < m_ExecPrograms.size())
		m_iExecIndex++;
	UpdateExecCount();
	LoadExecEntry();
}

void CPopUpWindow::OnExecPrev()
{
	if (m_iExecIndex > 0)
		m_iExecIndex--;
	UpdateExecCount();
	LoadExecEntry();
}

void CPopUpWindow::UpdateExecCount()
{
	ui.lblExecCount->setText(tr("%1/%2").arg(m_iExecIndex + 1).arg(m_ExecPrograms.size()));
	ui.btnExecPrev->setEnabled(m_iExecIndex + 1 > 1);
	ui.btnExecNext->setEnabled(m_iExecIndex + 1 < m_ExecPrograms.size());
}

void CPopUpWindow::LoadExecEntry(bool bUpdate)
{
	CProgramFilePtr pProgram = m_ExecPrograms[m_iExecIndex];
	const QList<SLogEntry>& List = m_ExecEvents[pProgram];

	if (!bUpdate) 
	{
		QString Title = pProgram->GetName();
		QString Name = QFileInfo(pProgram->GetPath()).fileName();
		if(Title.isEmpty()) Title = Name;
		ui.grpExec->setTitle(Title);
		ui.lblExecProc->setText(pProgram->GetNameEx());
		ui.lblExecAux->setText(pProgram->GetPublisher());
		ui.lblExecIcon->setPixmap(pProgram->GetIcon().pixmap(32, 32));
		((CPathLabel*)ui.lblExecPath)->setPath(pProgram->GetPath());
		ui.treeExec->clear();
	}

	for (int i = ui.treeExec->topLevelItemCount(); i < List.size(); i++) {
		const CExecLogEntry* pEntry = dynamic_cast<const CExecLogEntry*>(List[i].pPtr.constData());
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		if (pEntry) {

			pItem->setData(eExecName, Qt::UserRole, (uint32)pEntry->GetType());

			if (pEntry->GetType() == EExecLogType::eImageLoad)
			{
				uint64 uLibRef = pEntry->GetMiscID();
				CProgramLibraryPtr pLibrary = theCore->ProgramManager()->GetLibraryByUID(uLibRef, true);
				if (pLibrary)
				{
					QMultiMap<quint64, SLibraryInfo> Libs = pProgram->GetLibraries();
					SLibraryInfo pLibInfo = Libs.value(uLibRef);

					pItem->setData(eExecPath, Qt::UserRole, pLibrary->GetPath());
					pItem->setText(eExecName, pLibrary->GetName());
					pItem->setIcon(eExecName, CProgramLibrary::DefaultIcon());
					pItem->setText(eExecStatus, pEntry->GetStatusStr());
					pItem->setText(eExecTrust, CProgramFile::GetSignatureInfoStr(pLibInfo.SignInfo));
					
					pItem->setText(eExecSigner, pLibInfo.SignInfo.GetSignerName());
					pItem->setToolTip(eExecSigner, pLibInfo.SignInfo.GetSignerHash().toHex().toUpper());
					pItem->setData(eExecSigner, Qt::UserRole, pLibInfo.SignInfo.GetSignerHash());

					pItem->setText(eExecIssuer, pLibInfo.SignInfo.GetIssuerName());
					pItem->setToolTip(eExecIssuer, pLibInfo.SignInfo.GetIssuerHash().toHex().toUpper());
					pItem->setData(eExecIssuer, Qt::UserRole, pLibInfo.SignInfo.GetIssuerHash());

					pItem->setText(eExecTimeStamp, QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss.zzz"));
					QFlexGuid EnclaveGuid = pEntry->GetEnclaveGuid();
					if (!EnclaveGuid.IsNull()) {
						CEnclavePtr pEnclave = theCore->EnclaveManager()->GetEnclave(EnclaveGuid);
						pItem->setText(eExecEnclave, pEnclave ? pEnclave->GetName() : EnclaveGuid.ToQS());
						pItem->setData(eExecEnclave, Qt::UserRole, EnclaveGuid.ToQV());
					}
					if(pEntry->GetOwnerService().isEmpty())
						pItem->setText(eExecProcessId, QString::number(pEntry->GetProcessId()));
					else
						pItem->setText(eExecProcessId, tr("%1 (%2)").arg(pEntry->GetProcessId()).arg(pEntry->GetOwnerService()));
					pItem->setText(eExecPath, pLibrary->GetPath());
				}
			}
			else if (pEntry->GetType() == EExecLogType::eProcessStarted)
			{
				uint64 uID = pEntry->GetMiscID();
				//CProgramFilePtr pExecutable = theCore->ProgramManager()->GetProgramByUID(uID, true).objectCast<CProgramFile>();
				CProgramFilePtr pExecutable = pProgram;
				CProgramFilePtr pParent = theCore->ProgramManager()->GetProgramByUID(uID, true).objectCast<CProgramFile>();
				if (pExecutable) {
					pItem->setData(eExecPath, Qt::UserRole, pExecutable->GetPath());
					pItem->setText(eExecName, pExecutable->GetNameEx());
					pItem->setIcon(eExecName, pExecutable->GetIcon());
					pItem->setText(eExecStatus, pEntry->GetStatusStr());
					pItem->setText(eExecTrust, CProgramFile::GetSignatureInfoStr(pExecutable->GetSignInfo()));
					
					pItem->setText(eExecSigner, pExecutable->GetSignInfo().GetSignerName());
					pItem->setToolTip(eExecSigner, pExecutable->GetSignInfo().GetSignerHash().toHex().toUpper());
					pItem->setData(eExecSigner, Qt::UserRole, pExecutable->GetSignInfo().GetSignerHash());
					
					pItem->setText(eExecIssuer, pExecutable->GetSignInfo().GetIssuerName());
					pItem->setToolTip(eExecIssuer, pExecutable->GetSignInfo().GetIssuerHash().toHex().toUpper());
					pItem->setData(eExecIssuer, Qt::UserRole, pExecutable->GetSignInfo().GetIssuerHash());
					
					pItem->setText(eExecTimeStamp, QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss.zzz"));
					QFlexGuid EnclaveGuid = pEntry->GetEnclaveGuid();
					if (!EnclaveGuid.IsNull()) {
						CEnclavePtr pEnclave = theCore->EnclaveManager()->GetEnclave(EnclaveGuid);
						pItem->setText(eExecEnclave, pEnclave ? pEnclave->GetName() : EnclaveGuid.ToQS());
						pItem->setData(eExecEnclave, Qt::UserRole, EnclaveGuid.ToQV());
					}
					//pItem->setText(eExecProcessId, ); // we would need the target PID but we have the actor event
					pItem->setText(eExecPath, pExecutable->GetPath());
				}
			}

			QTreeWidgetItem* pSubItem = new QTreeWidgetItem();
			pSubItem->setText(0, "test");
			pItem->addChild(pSubItem);
		}
		ui.treeExec->addTopLevelItem(pItem);
	}
}

void CPopUpWindow::OnExecAction()
{
	QObject* pSender = sender();

	CProgramFilePtr pProgram = m_ExecPrograms[m_iExecIndex];

	if(pSender == ui.btnExecIgnore)
	{
		PopExecEvent(QStringList());
		return;
	}

	//EExecLogType Type = (EExecLogType)pItem->data(eExecName, Qt::UserRole).toUInt();
	

	if (pSender == m_pExecAlwaysIgnoreFile)
	{
		auto Items = ui.treeExec->selectedItems();
		QStringList Paths;
		foreach(auto pItem, Items) 
		{
			QString Path = pItem->data(eExecPath, Qt::UserRole).toString();
			if (pSender == m_pExecAlwaysIgnoreFile) {
				theGUI->IgnoreEvent(ERuleType::eProgram, NULL, Path);
				Paths.append(Path);
			}
		}
		PopExecEvent(Paths);
		return;
	}

	struct SSignTask
	{
		QString Path;
		QByteArray Cert;
		QString CertName;
		QFlexGuid EnclaveId;
		QString Collection;
	};

	QMap<QString, SSignTask> FileTasks;
	QMap<QByteArray, SSignTask> CertTasks;

	/*if (pSender == m_pExecSignAll)
	{
		for (int i = 0; i < ui.treeExec->topLevelItemCount(); i++) {
			QTreeWidgetItem* pItem = ui.treeExec->topLevelItem(i);
			QString Path = pItem->data(eExecPath, Qt::UserRole).toString();
			if(!Paths.contains(Path, Qt::CaseInsensitive))
				Paths.append(Path);
		}
	} else*/
	if (pSender == ui.btnExecSign || pSender == m_pExecSignCert || pSender == m_pExecSignCA)
	{
		auto Items = ui.treeExec->selectedItems();
		if (Items.isEmpty()) {
			QMessageBox::information(this, "MajorPrivacy", tr("Please select a file to add to Allowed Hash Database."));
			return;
		}

		if (pSender == m_pExecSignCA)
		{
			if (QMessageBox::question(this, "MajorPrivacy", tr("Do you really want to allow the Issuing Certificate Authoricy Certificate? This will allow all Signign Certificate issued by this CA."), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
				return;
		}

		foreach(auto pItem, Items) 
		{
			QVariant Guid = pItem->data(eExecEnclave, Qt::UserRole);
			QFlexGuid EnclaveId(Guid);

			QString Path = pItem->data(eExecPath, Qt::UserRole).toString();
			if (!FileTasks.contains(Path)) {
				SSignTask& FileTask = FileTasks[Path];
				FileTask.Path = Path;
				FileTask.EnclaveId = EnclaveId;
				//FileTask.Collection = 
			}

			QByteArray Cert = pItem->data(pSender == m_pExecSignCA ? eExecIssuer : eExecSigner, Qt::UserRole).toByteArray();
			QString CertName = pItem->text(pSender == m_pExecSignCA ? eExecIssuer : eExecSigner);
			if (!Cert.isEmpty() && !CertTasks.contains(Cert)) {
				SSignTask& CertTask = CertTasks[Cert];
				CertTask.Path = Path;
				CertTask.Cert = Cert;
				CertTask.CertName = CertName;
				CertTask.EnclaveId = EnclaveId;
				//CertTask.Collection = 
			}
		}
	}

	if (theGUI->IsDrvConfigLocked()) {
		STATUS Status = theGUI->UnlockDrvConfig();
		if (Status.IsError()) {
			theGUI->CheckResults(QList<STATUS>() << Status, this);
			return;
		}
	}

	STATUS Status;
	if (pSender == m_pExecSignCert)
	{
		if (CertTasks.isEmpty()) {
			QMessageBox::critical(this, "MajorPrivacy", tr("Selected Items do not have Certificates!"));
			return;
		}
		//Status = theGUI->SignCerts(Certs);
		foreach(const SSignTask& Task, CertTasks) {
			Status = theCore->HashDB()->AllowCert(Task.Cert, Task.CertName, Task.EnclaveId, Task.Collection);
			if (Status.IsError())
				break;
		}
	}
	else
	{
		//Status = theGUI->SignFiles(Paths);
		foreach(const SSignTask& Task, FileTasks) {
			Status = theCore->HashDB()->AllowFile(Task.Path, Task.EnclaveId, Task.Collection);
			if (Status.IsError())
				break;
		}
	}
	if(theGUI->CheckResults(QList<STATUS>() << Status, this) != 0)
		return;

	PopExecEvent(FileTasks.keys());
}