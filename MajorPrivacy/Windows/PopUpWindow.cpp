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
#include "FwGuardEntryWnd.h"
#include "FwTemplateWnd.h"
#include "RuleLifetime.h"
#include "../MiscHelpers/Common/CheckableMessageBox.h"
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
#include <QHostAddress>
#include "../Core/Volumes/VolumeManager.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Enclaves/EnclaveManager.h"

class CPathLabel : public QLabel 
{
public:
	CPathLabel(QWidget* parent = nullptr) : QLabel(parent) {
		setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
	}

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

	if (theConf->GetBool("Options/ShowHostName", true))
		setWindowTitle(tr("MajorPrivacy - %1").arg(QHostInfo::localHostName()));
	else
		setWindowTitle(tr("MajorPrivacy Notifications"));

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
	pFwAllow->addSeparator();
	m_pFwAllowTemplate = pFwAllow->addAction(tr("Create Allow Rule Template..."), this, SLOT(OnFwAction()));
	m_pFwAllowOnly = pFwAllow->addAction(tr("Allow this Program only"), this, SLOT(OnFwAction()));
	pFwAllow->setToolTipsVisible(true);
	ui.btnFwAllow->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnFwAllow->setMenu(pFwAllow);

	connect(ui.btnFwBlock, SIGNAL(clicked(bool)), this, SLOT(OnFwAction()));
	QMenu* pFwBlock = new QMenu(ui.btnFwBlock);
	m_pFwBlockTemplate = pFwBlock->addAction(tr("Create Block Rule Template..."), this, SLOT(OnFwAction()));
	m_pFwBlockOnly = pFwBlock->addAction(tr("Block this Program only"), this, SLOT(OnFwAction()));
	pFwBlock->setToolTipsVisible(true);
	ui.btnFwBlock->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnFwBlock->setMenu(pFwBlock);


	connect(ui.btnFwDiscard, SIGNAL(clicked(bool)), this, SLOT(OnFwRule()));

	connect(ui.btnFwApprove, SIGNAL(clicked(bool)), this, SLOT(OnFwRule()));
	QMenu* pFwApprove = new QMenu(ui.btnFwApprove);
	m_pFwApproveAll = pFwApprove->addAction(tr("Approve All"), this, SLOT(OnFwRule()));
	pFwApprove->addSeparator();
	m_pFwApproveAuto = pFwApprove->addAction(tr("Add Auto Approve Entry..."), this, SLOT(OnFwRule()));
	m_pFwApproveTemplate = pFwApprove->addAction(tr("Create Rule Template..."), this, SLOT(OnFwRule()));
	pFwApprove->setToolTipsVisible(true);
	ui.btnFwApprove->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnFwApprove->setMenu(pFwApprove);

	connect(ui.btnFwReject, SIGNAL(clicked(bool)), this, SLOT(OnFwRule()));
	QMenu* pFwReject = new QMenu(ui.btnFwReject);
	m_pFwRejectAll = pFwReject->addAction(tr("Reject All"), this, SLOT(OnFwRule()));
	pFwReject->addSeparator();
	m_pFwRejectAuto = pFwReject->addAction(tr("Add Auto Reject Entry..."), this, SLOT(OnFwRule()));
	m_pFwRejectTemplate = pFwReject->addAction(tr("Create Rule Template..."), this, SLOT(OnFwRule()));
	pFwReject->setToolTipsVisible(true);
	ui.btnFwReject->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnFwReject->setMenu(pFwReject);


	connect(ui.treeFwRules, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnFwRuleDblClick(QTreeWidgetItem*)));

	// enabled by UpdateFwPattern once there is an entry to derive a path pattern from
	m_pFwAllowTemplate->setEnabled(false);
	m_pFwBlockTemplate->setEnabled(false);
	m_pFwAllowOnly->setEnabled(false);
	m_pFwBlockOnly->setEnabled(false);
	m_pFwApproveAuto->setEnabled(false);
	m_pFwRejectAuto->setEnabled(false);
	m_pFwApproveTemplate->setEnabled(false);
	m_pFwRejectTemplate->setEnabled(false);

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

void CPopUpWindow::TryShow()
{
	if (m_FwPrograms.size() == 0 && m_ResPrograms.size() == 0 && m_ExecPrograms.size() == 0) {
		QApplication::beep();
		return;
	}

	Show();
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

void CPopUpWindow::PushFwRuleEvent(EConfigEvent EventType, const CFwRulePtr& pFwRule, CProgramItemPtr pProgram)
{
	if (pProgram.isNull())
		pProgram = theCore->ProgramManager()->GetProgramByID(pFwRule->GetProgramID());

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
		if(Title.isEmpty()) Title = QFileInfo(pProgram->GetPath()).fileName();
		ui.grpFw->setTitle(Title);
		ui.lblFwProc->setText(pProgram->GetNameEx());
		ui.lblFwAux->setText(pProgram->GetPublisher());
		ui.lblFwIcon->setPixmap(pProgram->GetIcon().pixmap(32, 32));
		((CPathLabel*)ui.lblFwPath)->setPath(pProgram->GetPath());

		UpdateFwPattern(pProgram);
	}
	else
		UpdateFwOffers(pProgram); // more events came in, or a template went up, the pattern stays

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

		if (pFwRule->GetProgramID() != pFwRuleOld->GetProgramID())
			Action += tr(", moved from %1").arg(pFwRuleOld->GetProgramID().GetFilePath());

		pItem->setText(eFwAction, Action);
	}
	else
		pItem->setText(eFwAction, pFwRule->GetActionStr() + " " + pFwRule->GetDirectionStr());
	pItem->setText(eFwRuleName, CMajorPrivacy::GetResourceStr(pFwRule->GetName()));
	pItem->setToolTip(eFwRuleName, pFwRule->GetName());
}

//////////////////////////////////////////////////////////////////////////
// Firewall path patterns
//
// Windows changes binary paths on every update, store apps worst of all, and every such change
// produces the very same prompt again. Deciding the same thing over and over is what makes a user
// stop reading the prompts, which is a security problem in itself, so where a program looks like a
// new version of something which was already decided about we offer to make the decision by path
// pattern instead, as a rule template for the allow/block prompt and as a guard entry for the
// approve/reject prompt. The pattern is only ever derived from paths which already carry a
// firewall decision, never guessed, and the user gets to see and edit it before it takes effect.
//

// everything we have already made a firewall decision about, the backups of rules which were just
// replaced included, as those carry the path of the version the user did approve of
static QStringList CPopUpWindow__GetKnownFwPaths(const QString& Exclude)
{
	QStringList Paths;
	QSet<QString> Seen;
	foreach(const CFwRulePtr& pFwRule, theCore->NetworkManager()->GetFwRules())
	{
		if (pFwRule->IsTemplate())
			continue; // a pattern tells us nothing about what varies

		QString Path = pFwRule->GetProgramID().GetFilePath();
		if (Path.isEmpty())
			Path = pFwRule->GetBinaryPath();
		if (Path.isEmpty() || Path == "*" || Path.compare(Exclude, Qt::CaseInsensitive) == 0)
			continue;

		QString Key = Path.toLower();
		if (Seen.contains(Key))
			continue;
		Seen.insert(Key);
		Paths.append(Path);
	}
	return Paths;
}

static bool CPopUpWindow__CoversDirection(EFwDirections Template, EFwDirections Wanted)
{
	// the directions are bits, bidirectional being both of them
	return Wanted != EFwDirections::Unknown && ((int)Wanted & ~(int)Template) == 0;
}

// the enabled templates whose pattern takes in this path, of one action or of any
static QList<CFwRulePtr> CPopUpWindow__GetTemplates(const QString& Path, EFwActions Action = EFwActions::Undefined)
{
	QList<CFwRulePtr> Templates;
	if (Path.isEmpty())
		return Templates;
	foreach(const CFwRulePtr& pFwRule, theCore->NetworkManager()->GetFwRules()) {
		if (!pFwRule->IsTemplate() || !pFwRule->IsEnabled())
			continue;
		if (Action != EFwActions::Undefined && pFwRule->GetAction() != Action)
			continue;
		if (MatchPathPattern(pFwRule->GetProgramID().GetFilePath(), Path))
			Templates.append(pFwRule);
	}
	return Templates;
}

//
// Whether a template already decides what is on screen, and if not, whether there is one which
// would if only it pointed both ways. The service holds a template against a connection or a rule
// by direction as well as by path, so the path alone says too little: an outbound template says
// nothing about the program accepting a connection, and the prompt about that is a real one. The
// answer to it is then to extend that template, rather than to put a second one next to it.
//
enum class EFwTemplateFit
{
	eNone = 0,	// nothing for this path, a new template would be needed
	eCovered,	// a template already stands for this
	eWiden,		// one stands for this path, only not in this direction
};

static EFwTemplateFit CPopUpWindow__FitTemplate(const QString& Path, EFwActions Action, EFwDirections Direction, CFwRulePtr* ppWiden = NULL)
{
	EFwTemplateFit Fit = EFwTemplateFit::eNone;
	foreach(const CFwRulePtr& pTemplate, CPopUpWindow__GetTemplates(Path, Action)) {
		if (CPopUpWindow__CoversDirection(pTemplate->GetDirection(), Direction)) {
			if (ppWiden) ppWiden->clear(); // one seen before may have lacked the direction, this one does not
			return EFwTemplateFit::eCovered;
		}
		if (Fit == EFwTemplateFit::eNone) {
			Fit = EFwTemplateFit::eWiden;
			if (ppWiden) *ppWiden = pTemplate;
		}
	}
	return Fit;
}

static QString CPopUpWindow__WidenTip(const CFwRulePtr& pTemplate)
{
	return CPopUpWindow::tr("The %1 rule template for\n%2\nonly covers %3 connections. This extends it to both directions, so that it covers this one as well.")
		.arg(pTemplate->GetActionStr().toLower()).arg(pTemplate->GetProgramID().GetFilePath()).arg(pTemplate->GetDirectionStr().toLower());
}

// which ways the connection attempts waiting on this program go
EFwDirections CPopUpWindow::GetFwEventDirections(const CProgramItemPtr& pProgram)
{
	int Directions = 0;
	foreach(const SLogEntry& Entry, m_FwEvents.value(pProgram.objectCast<CProgramFile>())) {
		const CNetLogEntry* pEntry = dynamic_cast<const CNetLogEntry*>(Entry.pPtr.constData());
		if (pEntry)
			Directions |= (int)pEntry->GetDirection();
	}
	return (EFwDirections)Directions;
}

//
// What was observed is what is proposed: the directions the waiting connections go, both only
// where both were seen. It used to point both ways whenever something came in, on the grounds
// that a program accepting connections makes them as well, but that decides more than was asked.
// The template window shows it and the user widens it there if they want to.
//
EFwDirections CPopUpWindow::GetFwTemplateDirection(const CProgramItemPtr& pProgram, bool bAllow)
{
	Q_UNUSED(bAllow);
	EFwDirections Observed = GetFwEventDirections(pProgram);
	return Observed != EFwDirections::Unknown ? Observed : EFwDirections::Outbound;
}

void CPopUpWindow::UpdateFwPattern(const CProgramItemPtr& pProgram)
{
	m_FwPattern = SPathPattern();

	QString Path = pProgram.isNull() ? QString() : pProgram->GetPath();
	if (!Path.isEmpty() && !Path.contains('*') && !Path.contains('?'))
	{
		m_FwPattern = MakePathPattern(Path, CPopUpWindow__GetKnownFwPaths(Path));

		//
		// Nothing to compare against, the first time a store app asks for instance. The user may
		// still want to decide every version of it at once, so the dropdowns offer a pattern read
		// off the path alone. Only offer: the questions which come up by themselves stay with a
		// deduced pattern, as they say this looks like something already decided about.
		//
		if (!m_FwPattern.IsValid())
			m_FwPattern = GuessPathPattern(Path);
	}

	UpdateFwOffers(pProgram);
}

//
// What the dropdowns offer depends on the pattern, which only changes with the program, and on
// the events waiting and the templates standing, which change while the page is shown.
//
void CPopUpWindow::UpdateFwOffers(const CProgramItemPtr& pProgram)
{
	QString Path = pProgram.isNull() ? QString() : pProgram->GetPath();

	QString Found;
	if (!m_FwPattern.IsValid())
		Found = tr("This program has no path a pattern could be made of.");
	else if (m_FwPattern.Guessed && m_FwPattern.Pattern.compare(Path, Qt::CaseInsensitive) == 0)
		Found = tr("No program with a similar path has a firewall rule yet, and no version number was found in this path, so the pattern starts out as the path itself:\n%1\n\nEdit it before saving to cover more than this one binary.").arg(m_FwPattern.Pattern);
	else if (m_FwPattern.Guessed)
		Found = tr("Covers %1\n\nNo program with a similar path has a firewall rule yet, so this was read off the path alone, with the version number wildcarded. Check it before saving.").arg(m_FwPattern.Pattern);
	else
		Found = tr("Covers %1\n\nDerived from:\n%2").arg(m_FwPattern.Pattern).arg(m_FwPattern.Similar.join("\n"));

	bool bTemplates = theCore->GetConfigBool("Service/UseFwRuleTemplates", true);
	QString OffTip = tr("Firewall rule templates are turned off in the options.");
	QString CoveredTip = tr("A rule template covering this program already exists.");

	m_pFwAllowWiden.clear();
	m_pFwBlockWiden.clear();
	m_pFwApproveWiden.clear();
	m_pFwRejectWiden.clear();

	bool bTemplate = bTemplates && m_FwPattern.IsValid(); // a new template could be made
	QString TemplateTip = bTemplates ? Found : OffTip;

	//
	// Any template covering the direction of the connections waiting decides them, whatever its
	// action. One which only lacks the direction can be widened, but only into the decision being
	// made: allowing does not extend a block template.
	//
	EFwDirections Needed = GetFwEventDirections(pProgram);
	if (bTemplates && CPopUpWindow__FitTemplate(Path, EFwActions::Undefined, Needed) == EFwTemplateFit::eCovered) {
		TemplateTip = CoveredTip;
		bTemplate = false;
	}
	else if (bTemplates) {
		CPopUpWindow__FitTemplate(Path, EFwActions::Allow, Needed, &m_pFwAllowWiden);
		CPopUpWindow__FitTemplate(Path, EFwActions::Block, Needed, &m_pFwBlockWiden);
	}

	bool bAllowTmpl = bTemplate || m_pFwAllowWiden;
	bool bBlockTmpl = bTemplate || m_pFwBlockWiden;

	// what Allow and Block may do by themselves, a pattern guessed with nothing to go on is only ever offered
	bool bDeduced = bTemplate && !m_FwPattern.Guessed;
	bool bAllowAuto = bDeduced || m_pFwAllowWiden;
	bool bBlockAuto = bDeduced || m_pFwBlockWiden;

	m_pFwAllowTemplate->setText(m_pFwAllowWiden ? tr("Extend Allow Rule Template to Both Directions") : tr("Create Allow Rule Template..."));
	m_pFwBlockTemplate->setText(m_pFwBlockWiden ? tr("Extend Block Rule Template to Both Directions") : tr("Create Block Rule Template..."));
	m_pFwAllowTemplate->setEnabled(bAllowTmpl);
	m_pFwBlockTemplate->setEnabled(bBlockTmpl);
	m_pFwAllowTemplate->setToolTip(m_pFwAllowWiden ? CPopUpWindow__WidenTip(m_pFwAllowWiden) : TemplateTip);
	m_pFwBlockTemplate->setToolTip(m_pFwBlockWiden ? CPopUpWindow__WidenTip(m_pFwBlockWiden) : TemplateTip);

	//
	// Allow and Block may end up making a template rather than a rule for this one binary, so a
	// way to say "just this one" has to be at hand wherever that can happen.
	//
	QString OnlyTip = tr("Decides this program alone and leaves the path pattern out of it.");
	QString PlainTip = tr("Allow and Block decide this program alone already, nothing similar is known to offer a template for.");
	m_pFwAllowOnly->setEnabled(bAllowAuto);
	m_pFwBlockOnly->setEnabled(bBlockAuto);
	m_pFwAllowOnly->setToolTip(bAllowAuto ? OnlyTip : bTemplate ? PlainTip : TemplateTip);
	m_pFwBlockOnly->setToolTip(bBlockAuto ? OnlyTip : bTemplate ? PlainTip : TemplateTip);

	m_bFwTemplateOffer = bDeduced;

	bool bGuard = m_FwPattern.IsValid();
	QString GuardTip = Found;
	if (bGuard && CFwGuardEntryWnd::IsCovered(Path)) {
		GuardTip = tr("The firewall guard already has an entry covering this program.");
		bGuard = false;
	}

	m_pFwApproveAuto->setEnabled(bGuard);
	m_pFwRejectAuto->setEnabled(bGuard);
	m_pFwApproveAuto->setToolTip(GuardTip);
	m_pFwRejectAuto->setToolTip(GuardTip);

	m_bFwGuardOffer = bGuard;

	//
	// The same decision offered as a rule template instead of as a guard entry. The two are not
	// interchangeable: a guard entry waves a change through whatever it is, while a template is a
	// statement about rules and is held against each one. So a template can only be offered where
	// there is a rule for it to agree or disagree with. A removal is not one, it is settled by the
	// removal option on its own, and turning a rule down needs a block template, because an allow
	// template deliberately says nothing about a block rule.
	//
	// here the rules pending give the action and the direction, so only a template agreeing on both decides them
	auto RuleOffer = [&](bool bApprove, EFwActions& Action, CFwRulePtr& pWiden, QString& Tip) -> bool
	{
		if (!bTemplates) {
			Tip = OffTip;
			return false;
		}
		EFwDirections Direction = EFwDirections::Bidirectional;
		if (!GetFwRuleTemplateAction(pProgram, bApprove, Action, Direction)) {
			Tip = bApprove
				? tr("A rule template speaks about rules which turn up or change, not about one being taken away, and it can only stand for several at once when they all say the same thing.")
				: tr("Only a block rule template can turn a rule down, which needs the rules in question to be allow rules.");
			return false;
		}
		switch (CPopUpWindow__FitTemplate(Path, Action, Direction, &pWiden))
		{
		case EFwTemplateFit::eCovered:	Tip = CoveredTip; return false;
		case EFwTemplateFit::eWiden:	Tip = CPopUpWindow__WidenTip(pWiden); return true;
		default:						Tip = Found; return m_FwPattern.IsValid();
		}
	};

	EFwActions Action = EFwActions::Undefined;
	QString ApproveTip;
	bool bApproveTmpl = RuleOffer(true, Action, m_pFwApproveWiden, ApproveTip);

	EFwActions RejectAction = EFwActions::Undefined;
	QString RejectTip;
	bool bRejectTmpl = RuleOffer(false, RejectAction, m_pFwRejectWiden, RejectTip);

	// the entry says which way the template will stand, there is no guessing left for the user
	bool bBlock = bApproveTmpl && Action == EFwActions::Block;
	if (m_pFwApproveWiden)
		m_pFwApproveTemplate->setText(bBlock ? tr("Extend Block Rule Template to Both Directions") : tr("Extend Allow Rule Template to Both Directions"));
	else
		m_pFwApproveTemplate->setText(bBlock ? tr("Create Block Rule Template...") : tr("Create Allow Rule Template..."));
	m_pFwRejectTemplate->setText(m_pFwRejectWiden ? tr("Extend Block Rule Template to Both Directions") : tr("Create Block Rule Template..."));

	m_pFwApproveTemplate->setEnabled(bApproveTmpl);
	m_pFwRejectTemplate->setEnabled(bRejectTmpl);

	m_bFwTemplateApprove = bApproveTmpl;
	m_bFwTemplateReject = bRejectTmpl;

	m_pFwApproveTemplate->setToolTip(ApproveTip);
	m_pFwRejectTemplate->setToolTip(RejectTip);
}

//
// Which action and direction a template has to carry so that a change like the one on screen is
// decided the same way the next time round. Approving means a change like this is wanted, which a
// template says by agreeing with the rule; rejecting means it is not, and only a block template
// says that. Returns false where no template can stand for what is pending.
//
bool CPopUpWindow::GetFwRuleTemplateAction(const CProgramItemPtr& pProgram, bool bApprove, EFwActions& Action, EFwDirections& Direction)
{
	bool bFound = false;
	foreach(const SFwRuleEvent& Event, m_FwRuleEvents.value(pProgram))
	{
		CFwRulePtr pFwRule = Event.pFwRule;
		if (pFwRule.isNull())
			continue;

		EConfigEvent EventType = Event.EventType;
		if (EventType == EConfigEvent::eModified) {
			// the event carries the backup, the rule as it stands now hangs off the original guid
			CFwRulePtr pLive = theCore->NetworkManager()->GetFwRules().value(pFwRule->GetOriginalGuid());
			if (pLive.isNull())
				EventType = EConfigEvent::eRemoved;
			else
				pFwRule = pLive;
		}

		if (EventType == EConfigEvent::eRemoved)
			return false; // no template has a say in a rule being taken away

		EFwActions RuleAction = pFwRule->GetAction();
		if (RuleAction != EFwActions::Allow && RuleAction != EFwActions::Block)
			return false;
		if (!bApprove && RuleAction != EFwActions::Allow)
			return false; // an allow template would not turn this down, it would say nothing at all

		EFwActions Wanted = bApprove ? RuleAction : EFwActions::Block;
		EFwDirections RuleDirection = pFwRule->GetDirection();

		if (bFound && (Action != Wanted || Direction != RuleDirection))
			return false; // what is pending does not agree, one template can not stand for all of it

		Action = Wanted;
		Direction = RuleDirection;
		bFound = true;
	}
	return bFound;
}

//
// Puts the deduced pattern in front of the user as a rule template, pre filled and open for
// editing, the same way the allow and block prompts do it.
//
bool CPopUpWindow::MakeFwRuleTemplate(bool bApprove, CFwRulePtr* ppRule)
{
	CProgramItemPtr pProgram = m_FwPrograms[m_iFwIndex].pItem;

	CFwRulePtr pWiden = bApprove ? m_pFwApproveWiden : m_pFwRejectWiden;
	if (pWiden)
		return WidenFwTemplate(pWiden, ppRule);

	EFwActions Action = EFwActions::Undefined;
	EFwDirections Direction = EFwDirections::Bidirectional;
	if (!GetFwRuleTemplateAction(pProgram, bApprove, Action, Direction))
		return false;

	return MakeFwTemplate(Action == EFwActions::Allow, Direction, pProgram, ppRule);
}

CProgramItemPtr CPopUpWindow::GetFwPatternItem(const QString& Pattern)
{
	return CFwTemplateWnd::GetPatternItem(Pattern, this);
}

//
// Puts the deduced pattern in front of the user before it takes effect. A template is a path
// pattern and an action and not much besides, so it gets a window of that size; the rest of the
// rule editor is a button away for anyone who wants to set ports or addresses as well.
//
bool CPopUpWindow::MakeFwTemplate(bool bAllow, EFwDirections Direction, const CProgramItemPtr& pProgram, CFwRulePtr* ppRule, int Lifetime)
{
	if (!m_FwPattern.IsValid())
		return false;

	CFwTemplateWnd Wnd(m_FwPattern.Pattern, bAllow, Direction, m_FwPattern.Similar, this, Lifetime);
	if (theGUI->SafeExec(&Wnd) != QDialog::Accepted)
		return false;

	CProgramItemPtr pPattern = GetFwPatternItem(Wnd.GetPattern());
	if (pPattern.isNull())
		return false;

	CFwRulePtr pRule = CFwRulePtr(new CFwRule(pPattern->GetID()));
	pRule->SetApproved();
	pRule->SetSource(EFwRuleSource::eMajorPrivacy);
	pRule->SetEnabled(true);
	pRule->SetName(QString("MajorPrivacy-Template, %1").arg(pProgram->GetNameEx()));
	pRule->SetProfile((int)EFwProfiles::All);
	pRule->SetProtocol(EFwKnownProtocols::Any);
	pRule->SetInterface((int)EFwInterfaces::All);
	pRule->SetAction(Wnd.IsAllow() ? EFwActions::Allow : EFwActions::Block);
	pRule->SetDirection(Wnd.GetDirection());
	ApplyRuleLifetime(pRule.data(), Wnd.GetLifetime());

	if (Wnd.WantsFullEditor())
	{
		QSet<CProgramItemPtr> Items;
		Items.insert(pProgram);
		Items.insert(pPattern); // preselected, the program itself stays available as a fallback
		CFirewallRuleWnd* pFirewallRuleWnd = new CFirewallRuleWnd(pRule, Items);
		if (!pFirewallRuleWnd->exec())
			return false;
	}
	else
	{
		STATUS Status = theCore->NetworkManager()->SetFwRule(pRule);
		if (theGUI->CheckResults(QList<STATUS>() << Status, this) != 0)
			return false;
	}

	if (ppRule) *ppRule = pRule;
	return true;
}

//
// The template stays what it was, pattern, action and all, it only stops saying nothing about
// the other direction. No editor for that: the pattern is not new, and the entry or question
// which led here already said what is going to change.
//
bool CPopUpWindow::WidenFwTemplate(const CFwRulePtr& pTemplate, CFwRulePtr* ppRule)
{
	CFwRulePtr pRule = CFwRulePtr(pTemplate->Clone(true)); // with its guid, so it replaces the one standing
	pRule->SetDirection(EFwDirections::Bidirectional);

	STATUS Status = theCore->NetworkManager()->SetFwRule(pRule);
	if (theGUI->CheckResults(QList<STATUS>() << Status, this) != 0)
		return false;

	if (ppRule) *ppRule = pRule;
	return true;
}

//
// Pressing allow or block settles one binary. Where that binary looks like a new version of one
// already decided about, the same decision can be made by path pattern instead and the version
// after this one will not ask at all. That is the whole point of the templates, and a feature
// nobody opens a dropdown to find is a feature nobody has, so it is put in front of the user.
//
bool CPopUpWindow::WantFwTemplate(const CProgramItemPtr& pProgram, bool bAllow)
{
	CFwRulePtr pWiden = bAllow ? m_pFwAllowWiden : m_pFwBlockWiden;
	if (!m_bFwTemplateOffer && !pWiden)
		return false; // there is nothing to derive a pattern from, or a template already covers it

	if (theCore->GetConfigBool("Service/FwAlwaysMakeTemplate", false))
		return true;
	if (theConf->GetBool("NetworkFirewall/NeverOfferTemplate", false))
		return false;

	QString Name = pProgram.isNull() ? QString() : pProgram->GetNameEx();
	QString Text;
	if (pWiden)
		Text = tr("A %1 rule template already covers <b>%2</b>, but only for %3 connections, so it does not decide this one.<br /><br />"
			"Shall it be extended to both directions, covering<br /><b>%4</b><br />"
			"so that this program and the next versions of it do not ask about this again?")
			.arg(pWiden->GetActionStr().toLower()).arg(Name).arg(pWiden->GetDirectionStr().toLower()).arg(pWiden->GetProgramID().GetFilePath());
	else
		Text = tr("<b>%1</b> looks like a new version of a program you have already decided about.<br /><br />"
			"Shall this be remembered as a rule template covering<br /><b>%2</b><br />"
			"so that the next version of it is covered too and does not ask again?<br /><br />"
			"You get to see and change the pattern before it takes effect.")
			.arg(Name).arg(m_FwPattern.Pattern);

	bool bRemember = false;
	QDialogButtonBox::StandardButton Ret = CCheckableMessageBox::question(this, "MajorPrivacy", Text,
		tr("Don't ask again"), &bRemember,
		QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes);

	bool bWant = (Ret == QDialogButtonBox::Yes);
	if (bRemember)
	{
		// the same answer from here on, said once rather than asked every time
		if (bWant)
			theCore->SetConfig("Service/FwAlwaysMakeTemplate", true);
		else
			theConf->SetValue("NetworkFirewall/NeverOfferTemplate", true);
	}
	return bWant;
}

//
// Nothing in the list is picked out, so the question is what to do with all of it. Where the
// program also happens to be one a standing answer could be written for, that is worth asking in
// the same breath rather than leaving it to a dropdown.
//
// What is offered is a rule template rather than a guard entry, although either could carry the
// decision. A template is itself a rule: it sits in the rule list where it can be seen, asked
// about and changed. A guard entry lives in a settings page and nowhere else, so a firewall that
// grew one every time somebody clicked yes would be deciding things somewhere nobody looks. The
// entries stay as they were, they are simply asked for on purpose instead of arrived at by default.
//
bool CPopUpWindow::AskAboutAllFwRules(bool bApprove, bool& bAlsoTemplate)
{
	bAlsoTemplate = false;

	QString Question = tr("No rules selected, do you want to apply the action to all rules?");

	// a guessed pattern is in the dropdown, it is not put in front of the user unasked
	CFwRulePtr pWiden = bApprove ? m_pFwApproveWiden : m_pFwRejectWiden;
	if (!(bApprove ? m_bFwTemplateApprove : m_bFwTemplateReject) || (m_FwPattern.Guessed && !pWiden))
		return QMessageBox::question(this, "MajorPrivacy", Question, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;

	//
	// Three answers are wanted here, not two: write the standing answer, do not write it, or step
	// back from the whole thing. A checkbox can only carry two of those, and reading "no" as
	// "leave it all alone" is not what anyone means by it, so each answer gets its own button.
	//
	QString Text = Question;
	if (pWiden)
		Text += tr("<br /><br />The %1 rule template for<br /><b>%2</b><br />only covers %3 rules, so it does not "
			"answer for these. Extended to both directions it can answer for rules like these from now on.")
			.arg(pWiden->GetActionStr().toLower()).arg(pWiden->GetProgramID().GetFilePath()).arg(pWiden->GetDirectionStr().toLower());
	else
		Text += tr("<br /><br />This program looks like a new version of one already decided about, so a "
			"rule template covering<br /><b>%1</b><br />can answer for rules like these from now on. "
			"A template is a rule itself, listed with the others, where you can see and change it.")
			.arg(m_FwPattern.Pattern);
	Text += tr("<br /><br /><b>Yes</b> - %2 and %1 the listed changes"
			"<br /><b>No</b> - %1 the listed changes and nothing further"
			"<br /><b>Cancel</b> - leave everything as it is")
			.arg(bApprove ? tr("approve") : tr("reject"))
			.arg(pWiden ? tr("extend that template") : tr("make that template"));

	switch (QMessageBox::question(this, "MajorPrivacy", Text,
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes))
	{
	case QMessageBox::Yes:	bAlsoTemplate = true; return true;
	case QMessageBox::No:	return true;
	default:				return false;
	}
}

bool CPopUpWindow::MakeFwGuardEntry(bool bApprove)
{
	if (!m_FwPattern.IsValid())
		return false;

	CFwGuardEntryWnd Wnd(m_FwPattern.Pattern, bApprove, m_FwPattern.Similar, this);
	if (theGUI->SafeExec(&Wnd) != QDialog::Accepted)
		return false;

	STATUS Status = CFwGuardEntryWnd::StoreEntry(Wnd.GetPattern(), Wnd.IsApprove());
	return theGUI->CheckResults(QList<STATUS>() << Status, this) == 0;
}

//////////////////////////////////////////////////////////////////////////
// Firewall rule matching
//
// Once a rule has been approved, or a new one has been created, all pending connection attempts which
// are covered by it are already decided and no longer require the users attention, hence they can be
// dropped silently instead of asking the user again about something which has just been ruled on.
//
// Note: this is a simplified variant of CFirewall::MatchRulesWithEvent, the GUI has no adapter
// information at hand, hence everything which depends on it is treated as inconclusive and never matches.
//

enum class EFwMatch
{
	eNo = 0,	// the rule definitively does not apply
	eYes,		// the rule definitively applies
	eUnknown	// the rule contains something which can not be evaluated here
};

static bool CPopUpWindow__IsEmptyOrStar(const QStringList& List)
{
	return List.isEmpty() || (List.size() == 1 && (List.first().isEmpty() || List.first() == "*"));
}

static QHostAddress CPopUpWindow__NormAddress(const QHostAddress& Address)
{
	bool bIsIPv4 = false;
	quint32 uIPv4 = Address.toIPv4Address(&bIsIPv4);
	return bIsIPv4 ? QHostAddress(uIPv4) : Address; // unify IPv4 mapped IPv6 addresses with plain IPv4 ones
}

// returns -1, 0 or 1 like memcmp does, or 2 when the two addresses can not be compared at all
static int CPopUpWindow__CmpAddress(const QHostAddress& Left, const QHostAddress& Right)
{
	if (Left.isNull() || Right.isNull() || Left.protocol() != Right.protocol())
		return 2;
	if (Left.protocol() == QAbstractSocket::IPv4Protocol) {
		quint32 uLeft = Left.toIPv4Address();
		quint32 uRight = Right.toIPv4Address();
		return uLeft < uRight ? -1 : (uLeft > uRight ? 1 : 0);
	}
	if (Left.protocol() == QAbstractSocket::IPv6Protocol) {
		Q_IPV6ADDR uLeft = Left.toIPv6Address();
		Q_IPV6ADDR uRight = Right.toIPv6Address();
		int iCmp = memcmp(&uLeft, &uRight, sizeof(Q_IPV6ADDR));
		return iCmp < 0 ? -1 : (iCmp > 0 ? 1 : 0);
	}
	return 2;
}

// returns true when Name is a windows address keyword, Ranges stays empty when it depends on the current adapter configuration
static bool CPopUpWindow__GetSpecialNet(const QString& Name, QStringList& Ranges)
{
	if (Name.compare(QString("LocalSubnet"), Qt::CaseInsensitive) == 0 || Name.compare(QString("LocalIntranet"), Qt::CaseInsensitive) == 0)
	{
		Ranges << "10.0.0.0-10.255.255.255" << "127.0.0.0-127.255.255.255" << "172.16.0.0-172.31.255.255"
			<< "192.168.0.0-192.168.255.255" << "224.0.0.0-239.255.255.255";
		Ranges << "::1" << "fc00::-fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff" << "fe80::-fe80::ffff:ffff:ffff:ffff"
			<< "ff00::-ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff";
		return true;
	}

	if (Name.compare(QString("Internet"), Qt::CaseInsensitive) == 0)
	{
		Ranges << "0.0.0.0-9.255.255.255" << "11.0.0.0-126.255.255.255" << "128.0.0.0-172.15.255.255"
			<< "172.32.0.0-192.167.255.255" << "192.169.0.0-223.255.255.255" << "240.0.0.0-255.255.255.255";
		Ranges << "::2-fc00::" << "fc00::ffff:ffff:ffff:ffff-fe80::" << "fe80::ffff:ffff:ffff:ffff-ff00::";
		return true;
	}

	static const QStringList Keywords = QStringList() << "DNS" << "DHCP" << "WINS" << "DefaultGateway"
		<< "RemoteIntranet" << "Ply2Renders" << "CaptivePortal";
	foreach(const QString& Keyword, Keywords) {
		if (Name.compare(Keyword, Qt::CaseInsensitive) == 0)
			return true; // a known keyword but one we can not resolve here
	}
	return false;
}

static EFwMatch CPopUpWindow__MatchAddress(const QHostAddress& Address, const QStringList& Addresses)
{
	bool bUnknown = false;
	foreach(const QString& Entry, Addresses)
	{
		QString Range = Entry.trimmed();
		if (Range.isEmpty())
			continue;

		int iSep = (int)Range.indexOf('-');
		if (iSep != -1) // begin-end
		{
			QHostAddress Begin = CPopUpWindow__NormAddress(QHostAddress(Range.left(iSep).trimmed()));
			QHostAddress End = CPopUpWindow__NormAddress(QHostAddress(Range.mid(iSep + 1).trimmed()));
			if (Begin.isNull() || End.isNull())
				bUnknown = true;
			else if (CPopUpWindow__CmpAddress(Begin, Address) <= 0 && CPopUpWindow__CmpAddress(Address, End) <= 0)
				return EFwMatch::eYes;
		}
		else if (Range.contains('/')) // ip/net
		{
			QPair<QHostAddress, int> SubNet = QHostAddress::parseSubnet(Range);
			if (SubNet.second == -1)
				bUnknown = true;
			else if (Address.isInSubnet(SubNet))
				return EFwMatch::eYes;
		}
		else
		{
			QStringList Special;
			if (CPopUpWindow__GetSpecialNet(Range, Special))
			{
				if (Special.isEmpty())
					bUnknown = true;
				else {
					EFwMatch Match = CPopUpWindow__MatchAddress(Address, Special);
					if (Match == EFwMatch::eYes)
						return EFwMatch::eYes;
					if (Match == EFwMatch::eUnknown)
						bUnknown = true;
				}
			}
			else
			{
				QHostAddress Single = CPopUpWindow__NormAddress(QHostAddress(Range));
				if (Single.isNull())
					bUnknown = true;
				else if (CPopUpWindow__CmpAddress(Single, Address) == 0)
					return EFwMatch::eYes;
			}
		}
	}
	return bUnknown ? EFwMatch::eUnknown : EFwMatch::eNo;
}

static EFwMatch CPopUpWindow__MatchPort(quint16 Port, const QStringList& Ports)
{
	bool bUnknown = false;
	foreach(const QString& Entry, Ports)
	{
		QString Range = Entry.trimmed();
		if (Range.isEmpty())
			continue;

		int iSep = (int)Range.indexOf('-');
		if (iSep == -1)
		{
			bool bOk = false;
			uint uPort = Range.toUInt(&bOk);
			if (!bOk || uPort > 0xFFFF) // some rules use keywords like RPC, RPC-EPMap, Teredo or IPHTTPS instead of numbers
				bUnknown = true;
			else if (uPort == Port)
				return EFwMatch::eYes;
		}
		else
		{
			bool bOkBegin = false, bOkEnd = false;
			uint uBegin = Range.left(iSep).trimmed().toUInt(&bOkBegin);
			uint uEnd = Range.mid(iSep + 1).trimmed().toUInt(&bOkEnd);
			if (!bOkBegin || !bOkEnd || uBegin > 0xFFFF || uEnd > 0xFFFF)
				bUnknown = true;
			else if (uBegin <= Port && Port <= uEnd)
				return EFwMatch::eYes;
		}
	}
	return bUnknown ? EFwMatch::eUnknown : EFwMatch::eNo;
}

static EFwMatch CPopUpWindow__MatchEndpoint(const QStringList& Addresses, const QStringList& Ports, const QHostAddress& Address, quint16 Port)
{
	if (!CPopUpWindow__IsEmptyOrStar(Ports)) {
		EFwMatch Match = CPopUpWindow__MatchPort(Port, Ports);
		if (Match != EFwMatch::eYes)
			return Match;
	}
	if (!Address.isNull() && !CPopUpWindow__IsEmptyOrStar(Addresses)) {
		EFwMatch Match = CPopUpWindow__MatchAddress(CPopUpWindow__NormAddress(Address), Addresses);
		if (Match != EFwMatch::eYes)
			return Match;
	}
	return EFwMatch::eYes;
}

// note: CProgramManager::GetProgramByID does not handle all program types, hence we resolve the item ourselves
static CProgramItemPtr CPopUpWindow__GetRuleItem(const CProgramID& ID)
{
	switch (ID.GetType())
	{
	case EProgramType::eAllPrograms:
	case EProgramType::eProgramFile:
	case EProgramType::eWindowsService:
	case EProgramType::eAppPackage:		return theCore->ProgramManager()->GetProgramByID(ID);
	case EProgramType::eProgramGroup:	return theCore->ProgramManager()->GetGroups().value(ID.GetGuid());
	default:							return CProgramItemPtr(); // patterns and the like are expanded by the service
	}
}

//
// A rule written for a path pattern has no program item of its own, the service expands it into
// one rule per binary it matches. Which binaries those are is no mystery though, it is the very
// same match the pattern was offered on, so a template can be held against a pending connection
// here as well. That matters because answering several prompts at once is what a template is for.
//
static bool CPopUpWindow__PatternCoversProgram(const CFwRulePtr& pFwRule, const CProgramFilePtr& pProgram)
{
	if (!pFwRule->IsTemplate() || pProgram.isNull())
		return false;
	QString Path = pProgram->GetPath();
	return !Path.isEmpty() && MatchPathPattern(pFwRule->GetProgramID().GetFilePath(), Path);
}

// checks if a rule which was made for pRuleItem covers a connection of pProgram
static bool CPopUpWindow__RuleAppliesTo(const CProgramItemPtr& pRuleItem, const CProgramFilePtr& pProgram, const CNetLogEntry* pEntry)
{
	if (pRuleItem.isNull() || pProgram.isNull())
		return false;
	if (pRuleItem == pProgram)
		return true;
	if (pRuleItem->GetType() == EProgramType::eAllPrograms)
		return true;

	// a rule made for a service covers only the traffic of that very service
	CWindowsServicePtr pService = pRuleItem.objectCast<CWindowsService>();
	if (pService)
		return !pEntry->GetOwnerService().isEmpty() && pService->GetServiceTag().compare(pEntry->GetOwnerService(), Qt::CaseInsensitive) == 0;

	// walk up the group hierarchy and look if the rules item is one of the programs ancestors
	QSet<const CProgramItem*> Visited;
	QList<CProgramItemPtr> Todo;
	Todo.append(pProgram);
	while (!Todo.isEmpty())
	{
		CProgramItemPtr pCurrent = Todo.takeFirst();
		if (Visited.contains(pCurrent.data()))
			continue;
		Visited.insert(pCurrent.data());

		foreach(const QWeakPointer<QObject>& Group, pCurrent->GetGroups()) {
			CProgramItemPtr pGroup = Group.lock().objectCast<CProgramItem>();
			if (pGroup.isNull())
				continue;
			if (pGroup == pRuleItem)
				return true;
			Todo.append(pGroup);
		}
	}
	return false;
}

// checks everything about a rule which does not depend on a particular connection attempt
static bool CPopUpWindow__IsRuleConclusive(const CFwRulePtr& pFwRule)
{
	if (pFwRule.isNull() || !pFwRule->IsEnabled() || pFwRule->IsUnapproved())
		return false;
	if (pFwRule->GetAction() != EFwActions::Allow && pFwRule->GetAction() != EFwActions::Block)
		return false;
	if (!pFwRule->GetPrincipalSddl().isEmpty()) // user restrictions can not be evaluated here
		return false;

	// without adapter information we can only be sure about rules which apply to all profiles and interfaces
	const int AllProfiles = (int)EFwProfiles::Domain | (int)EFwProfiles::Private | (int)EFwProfiles::Public;
	if ((pFwRule->GetProfile() & AllProfiles) != AllProfiles)
		return false;
	if (pFwRule->GetInterface() != (int)EFwInterfaces::All)
		return false;
	return true;
}

static bool CPopUpWindow__RuleCoversEvent(const CFwRulePtr& pFwRule, const CProgramItemPtr& pRuleItem, const CProgramFilePtr& pProgram, const CNetLogEntry* pEntry)
{
	if (pFwRule->GetDirection() != EFwDirections::Bidirectional && pFwRule->GetDirection() != pEntry->GetDirection())
		return false;
	if (pFwRule->GetProtocol() != EFwKnownProtocols::Any && (quint32)pFwRule->GetProtocol() != pEntry->GetProtocolType())
		return false;
	if (CPopUpWindow__MatchEndpoint(pFwRule->GetRemoteAddresses(), pFwRule->GetRemotePorts(), pEntry->GetRemoteAddress(), pEntry->GetRemotePort()) != EFwMatch::eYes)
		return false;
	if (CPopUpWindow__MatchEndpoint(pFwRule->GetLocalAddresses(), pFwRule->GetLocalPorts(), pEntry->GetLocalAddress(), pEntry->GetLocalPort()) != EFwMatch::eYes)
		return false;
	if (pRuleItem.isNull()) // a template, the pattern itself says what it covers
		return CPopUpWindow__PatternCoversProgram(pFwRule, pProgram);
	return CPopUpWindow__RuleAppliesTo(pRuleItem, pProgram, pEntry);
}

int CPopUpWindow::ClearCoveredFwEvents(const QList<CFwRulePtr>& Rules)
{
	if (m_FwEvents.isEmpty())
		return 0;

	// resolve the program items only once, the lookup may have to ask the service
	QList<QPair<CFwRulePtr, CProgramItemPtr>> Active;
	foreach(const CFwRulePtr& pFwRule, Rules) {
		if (!CPopUpWindow__IsRuleConclusive(pFwRule))
			continue;
		CProgramItemPtr pRuleItem = CPopUpWindow__GetRuleItem(pFwRule->GetProgramID());
		if (!pRuleItem.isNull() || pFwRule->IsTemplate()) // a template stays, it is matched by path
			Active.append(qMakePair(pFwRule, pRuleItem));
	}
	if (Active.isEmpty())
		return 0;

	int Cleared = 0;
	QList<CProgramFilePtr> Obsolete;

	for (auto I = m_FwEvents.begin(); I != m_FwEvents.end(); ++I)
	{
		QList<SLogEntry>& List = I.value();
		for (auto J = List.begin(); J != List.end(); )
		{
			const CNetLogEntry* pEntry = dynamic_cast<const CNetLogEntry*>(J->pPtr.constData());
			bool bCovered = false;
			for (auto K = Active.begin(); pEntry && K != Active.end() && !bCovered; ++K)
				bCovered = CPopUpWindow__RuleCoversEvent(K->first, K->second, I.key(), pEntry);

			if (!bCovered) {
				++J;
				continue;
			}
			J = List.erase(J);
			Cleared++;
		}
		if (List.isEmpty())
			Obsolete.append(I.key());
	}

	if (Cleared == 0)
		return 0;

	// remember what is currently on screen so that we can keep showing it if it is still around
	SFwProgram Current;
	bool bHasCurrent = (m_iFwIndex >= 0 && m_iFwIndex < m_FwPrograms.size());
	if (bHasCurrent) Current = m_FwPrograms[m_iFwIndex];

	foreach(const CProgramFilePtr& pProgram, Obsolete) {
		m_FwEvents.remove(pProgram);
		SFwProgram Prog = { SFwProgram::eNetworkEvent, pProgram };
		m_FwPrograms.removeOne(Prog);
	}

	if (m_FwPrograms.isEmpty()) {
		m_iFwIndex = -1;
		ui.treeFw->clear();
		UpdateFwCount();
		TryHide();
		return Cleared;
	}

	int index = (int)(bHasCurrent ? m_FwPrograms.indexOf(Current) : -1);
	if (index != -1)
		m_iFwIndex = index;
	else if (m_iFwIndex >= m_FwPrograms.size())
		m_iFwIndex = (int)m_FwPrograms.size() - 1;
	else if (m_iFwIndex < 0)
		m_iFwIndex = 0;

	UpdateFwCount();
	LoadFwEntry(); // the event list of the shown program may have changed, reload it entirely
	return Cleared;
}

void CPopUpWindow::OnFwAction()
{
	CProgramFilePtr pProgram = m_FwPrograms[m_iFwIndex].pItem.objectCast<CProgramFile>();

	QObject* pSender = sender();

	if(pSender == ui.btnFwIgnore || pSender == m_pFwAlwaysIgnoreApp)
	{
		if (pSender == m_pFwAlwaysIgnoreApp)
			theGUI->IgnoreEvent(EItemType::eFwRule, pProgram);
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
	
	//
	// A template rule is bound to a path pattern rather than to one single binary, the service
	// expands it into a real rule for every program the pattern covers, which is what keeps the
	// next update of this very program from asking all over again.
	//
	bool bTemplate = (pSender == m_pFwAllowTemplate || pSender == m_pFwBlockTemplate);
	if (pSender == ui.btnFwAllow || pSender == ui.btnFwBlock)
		bTemplate = WantFwTemplate(pProgram, pSender == ui.btnFwAllow);

	QList<CFwRulePtr> NewRules;

	if (bTemplate)
	{
		//
		// A template stands on its own: one pattern, one action, saved from its own small window.
		// Nothing below applies to it, as it is not a rule about this one binary.
		//
		CFwRulePtr pTemplateRule;
		bool bAllow = (pSender == m_pFwAllowTemplate || pSender == ui.btnFwAllow);
		CFwRulePtr pWiden = bAllow ? m_pFwAllowWiden : m_pFwBlockWiden;
		if (pWiden ? !WidenFwTemplate(pWiden, &pTemplateRule)
				   : !MakeFwTemplate(bAllow, GetFwTemplateDirection(pProgram, bAllow), pProgram, &pTemplateRule, ui.cmbFwTime->currentData().toInt()))
			return;
		NewRules.append(pTemplateRule);

		PopFwEvent();

		// other pending connection attempts may be covered by it as well, there is no point in still asking about those
		ClearCoveredFwEvents(NewRules);
		return;
	}

	CFwRulePtr pRule = CFwRulePtr(new CFwRule(pProgram->GetID()));
	pRule->SetApproved();
	pRule->SetSource(EFwRuleSource::eMajorPrivacy);
	pRule->SetEnabled(true);
	pRule->SetName(QString("MajorPrivacy-Rule, %1").arg(pProgram->GetNameEx()));
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
		QSet<CProgramItemPtr> Items;
		Items.insert(pProgram);

		CFirewallRuleWnd* pFirewallRuleWnd = new CFirewallRuleWnd(pRule, Items);
		if (!pFirewallRuleWnd->exec())
			return;
		NewRules.append(pRule);
	}
	else 
	{
		if (pSender == ui.btnFwAllow || pSender == m_pFwAllowOnly)
			pRule->SetAction(EFwActions::Allow);
		/*else if (pSender == m_pFwLanOnly)
		{
			pRule->SetAction(SVC_API_FW_ALLOW);
			// todo set remote addresses for LAN
		}*/
		else if (pSender == ui.btnFwBlock || pSender == m_pFwBlockOnly)
			pRule->SetAction(EFwActions::Block);

		QList<STATUS> Statuses;

		//if (bOutbound) {
			pRule->SetDirection(EFwDirections::Outbound);
			NewRules.append(CFwRulePtr(pRule->Clone(true))); // the rule is reused for the other direction, keep a snapshot
			Statuses << theCore->NetworkManager()->SetFwRule(pRule);
		//}

		if (bInbound || theConf->GetBool("NetworkFirewall/MakeInOutRules", false)) {
			pRule->SetDirection(EFwDirections::Inbound);
			NewRules.append(CFwRulePtr(pRule->Clone(true)));
			Statuses << theCore->NetworkManager()->SetFwRule(pRule);
		}

		if(theGUI->CheckResults(Statuses, this) != 0)
			return;
	}

	PopFwEvent();

	// other pending connection attempts may be covered by the new rules as well, there is no point in still asking about those
	ClearCoveredFwEvents(NewRules);
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

	bool bApprove = (pSender == ui.btnFwApprove || pSender == m_pFwApproveAll || pSender == m_pFwApproveAuto || pSender == m_pFwApproveTemplate);
	bool bReject = (pSender == ui.btnFwReject || pSender == m_pFwRejectAll || pSender == m_pFwRejectAuto || pSender == m_pFwRejectTemplate);
	bool bAuto = (pSender == m_pFwApproveAuto || pSender == m_pFwRejectAuto);
	bool bTemplate = (pSender == m_pFwApproveTemplate || pSender == m_pFwRejectTemplate);
	if (!bApprove && !bReject)
		return;

	//
	// Which of the listed changes this applies to, everything when nothing is picked out. That
	// has to be settled before any editor opens, because the list is rebuilt once one has run.
	//
	bool bAll = (pSender == m_pFwApproveAll || pSender == m_pFwRejectAll || bAuto || bTemplate);
	bool bAlsoTemplate = false;
	QList<QTreeWidgetItem*> Items;
	if (!bAll)
	{
		Items = ui.treeFwRules->selectedItems();
		if (Items.isEmpty() && !AskAboutAllFwRules(bApprove, bAlsoTemplate))
			return;
	}

	CFwRulePtr pNewTemplate;
	if (bAuto || bTemplate || bAlsoTemplate)
	{
		//
		// Neither a guard entry nor a template reaches back to the change which is on screen right
		// now, both only speak for what comes after it, so the standing answer is put in place
		// first and the change at hand is then carried out as it would have been without it.
		//
		// only the dropdown asks for a guard entry, everything else makes a template
		SFwProgram Current = m_FwPrograms[m_iFwIndex];
		if (!(bAuto ? MakeFwGuardEntry(bApprove) : MakeFwRuleTemplate(bApprove, &pNewTemplate)))
			return;

		// the editor ran an event loop of its own, make sure we are still on the same entry
		int index = (int)m_FwPrograms.indexOf(Current);
		if (index == -1)
			return;
		m_iFwIndex = index;
		pProgram = m_FwPrograms[m_iFwIndex].pItem;

		Items.clear(); // the list was rebuilt along with it, what was picked out before is gone
	}

	QList<SFwRuleEvent>& List = m_FwRuleEvents[pProgram];

	if (Items.isEmpty())
	{
		for (int i = 0; i < ui.treeFwRules->topLevelItemCount(); i++) {
			QTreeWidgetItem* pItem = ui.treeFwRules->topLevelItem(i);
			Items.append(pItem);
		}
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
	QList<CFwRulePtr> ActiveRules; // rules which are in effect once the user made their choice
	if (!pNewTemplate.isNull())
		ActiveRules.append(pNewTemplate); // including a template just written, it covers whole paths
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

		if (bApprove)
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
				if(!Status.IsError()) ActiveRules.append(pFwRule);
				break;
			case EConfigEvent::eRemoved:
				ASSERT(pFwRule->IsBackup());
				Status = theCore->NetworkManager()->DelFwRule(pFwRule);
				break;
			}
		}
		else
		{
			switch (EventType)
			{
			case EConfigEvent::eModified:
				ASSERT(pFwRuleOld->IsBackup());
				pFwRuleOld->SetApproved();
				Status = theCore->NetworkManager()->SetFwRule(pFwRuleOld);
				if(!Status.IsError()) ActiveRules.append(pFwRuleOld); // the previous version has been restored
				break;
			case EConfigEvent::eAdded:
				Status = theCore->NetworkManager()->DelFwRule(pFwRule);
				break;
			case EConfigEvent::eRemoved:
				ASSERT(pFwRule->IsBackup());
				pFwRule->SetApproved();
				Status = theCore->NetworkManager()->SetFwRule(pFwRule);
				if(!Status.IsError()) ActiveRules.append(pFwRule); // the removed rule has been restored
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

	// pending connection attempts which are covered by the now active rules are already decided, drop them
	ClearCoveredFwEvents(ActiveRules);
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
		if(Title.isEmpty()) Title = QFileInfo(pProgram->GetPath()).fileName();
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
		if (pProcess) {
			pItem->setText(eResUser, pProcess->GetUser());
			pItem->setText(eResCmdLine, pProcess->GetCmdLine());
		}
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
			theGUI->IgnoreEvent(EItemType::eResRule, pProgram);
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
		theGUI->IgnoreEvent(EItemType::eResRule, NULL, Path);
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
		if(Title.isEmpty()) Title = QFileInfo(pProgram->GetPath()).fileName();
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
				theGUI->IgnoreEvent(EItemType::eExecRule, NULL, Path);
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

	// keyed by hash/path + enclave, the same file or cert may show up in multiple rows with different enclaves
	// and each of them must be approved for its own enclave, not just for the enclave of the first row encountered
	QMap<QPair<QString, QString>, SSignTask> FileTasks;
	QMap<QPair<QByteArray, QString>, SSignTask> CertTasks;

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
			if (Path.isEmpty())
				continue;
			auto FileKey = qMakePair(Path.toLower(), EnclaveId.ToQS());
			if (!FileTasks.contains(FileKey)) {
				SSignTask& FileTask = FileTasks[FileKey];
				FileTask.Path = Path;
				FileTask.EnclaveId = EnclaveId;
				//FileTask.Collection =
			}

			QByteArray Cert = pItem->data(pSender == m_pExecSignCA ? eExecIssuer : eExecSigner, Qt::UserRole).toByteArray();
			QString CertName = pItem->text(pSender == m_pExecSignCA ? eExecIssuer : eExecSigner);
			auto CertKey = qMakePair(Cert, EnclaveId.ToQS());
			if (!Cert.isEmpty() && !CertTasks.contains(CertKey)) {
				SSignTask& CertTask = CertTasks[CertKey];
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
	if (pSender == m_pExecSignCert || pSender == m_pExecSignCA)
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

	QStringList Paths;
	foreach(const SSignTask& Task, FileTasks) {
		if (!Paths.contains(Task.Path, Qt::CaseInsensitive))
			Paths.append(Task.Path);
	}
	PopExecEvent(Paths);
}