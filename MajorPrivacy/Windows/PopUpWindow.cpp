#include "pch.h"
#include "PopUpWindow.h"
#include <windows.h>
#include <QWindow>
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/Settings.h"
#include "../Core/PrivacyCore.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Core/Network/NetLogEntry.h"
#include "FirewallRuleWnd.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Network/NetworkManager.h"
#include "../Service/ServiceAPI.h" // todo

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

	ui.tabs->setTabEnabled(0, false);
	ui.tabs->setTabEnabled(1, false);
	ui.tabs->setTabEnabled(2, false);

	// FW
	connect(ui.btnFwPrev, SIGNAL(clicked(bool)), this, SLOT(OnFwPrev()));
	connect(ui.btnFwNext, SIGNAL(clicked(bool)), this, SLOT(OnFwNext()));

	connect(ui.btnFwIgnore, SIGNAL(clicked(bool)), this, SLOT(OnFwAction()));
	QMenu* pFwIgnore = new QMenu(ui.btnFwIgnore);
	//m_pFwAlwaysIgnore = pFwIgnore->addAction(tr("Always Ignore"), this, SLOT(OnFwAction()));
	ui.btnFwIgnore->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnFwIgnore->setMenu(pFwIgnore);

	connect(ui.btnFwAllow, SIGNAL(clicked(bool)), this, SLOT(OnFwAction()));
	QMenu* pFwAllow = new QMenu(ui.btnFwIgnore);
	m_pFwCustom = pFwAllow->addAction(tr("Create Custom Rule"), this, SLOT(OnFwAction()));
	//pFwIgnore->addSeparator();
	//m_pFwLanOnly = pFwAllow->addAction(tr("Allow LAN only"), this, SLOT(OnFwAction()));
	ui.btnFwAllow->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnFwAllow->setMenu(pFwAllow);

	connect(ui.btnFwBlock, SIGNAL(clicked(bool)), this, SLOT(OnFwAction()));


	// TODO

	
	m_iTopMost = 0;
	SetWindowPos((HWND)this->winId(), 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	
	m_uTimerID = startTimer(1000);

	m_ResetPosition = !restoreGeometry(theConf->GetBlob("PopUpWindow/Window_Geometry"));
}

CPopUpWindow::~CPopUpWindow()
{
	killTimer(m_uTimerID);

	theConf->SetBlob("PopUpWindow/Window_Geometry", saveGeometry());
}

void CPopUpWindow::Show()
{
	Poke();

	QScreen *screen = this->windowHandle()->screen();
	QRect scrRect = screen->availableGeometry();

	if (!m_ResetPosition)
	{
		QRect wndRect = this->frameGeometry();
		if (wndRect.bottom() > scrRect.height() || wndRect.right() > scrRect.width() || wndRect.top() < 0 || wndRect.left() < 0)
			m_ResetPosition = true;
	}

	if (m_ResetPosition)
	{
		m_ResetPosition = false;

		this->resize(300, 400);
		this->move(scrRect.width() - 300 - 20, scrRect.height() - 400 - 50);
	}

	SafeShow(this);
}

void CPopUpWindow::Poke()
{
	if (!this->isVisible() || m_iTopMost <= -5) {
		SetWindowPos((HWND)this->winId(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		m_iTopMost = 5;
	}
}

void CPopUpWindow::closeEvent(QCloseEvent *e)
{
	// todo: clean up

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
}

void CPopUpWindow::TryHide()
{
	bool bCanHide = true;

	// FW
	if (m_FwPrograms.size() == 0)
		ui.tabs->setTabEnabled(0, false);
	else
		bCanHide = false;

	//Res
	//todo

	//Cfg
	//todo

	if(bCanHide)
		this->hide();
}

//////////////////////////////////////////////////////////////////////////
// Firewall

void CPopUpWindow::PushFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry)
{
	QList<CLogEntryPtr>& List = m_FwEvents[pProgram];
	if (List.isEmpty())
		m_FwPrograms.append(pProgram);
	List.append(pEntry);

	int oldIndex = m_iFwIndex;

	if (m_iFwIndex < 0)
		m_iFwIndex = 0;
	else if (m_iFwIndex >= m_FwPrograms.size())
		m_iFwIndex = m_FwPrograms.size() - 1;

	ui.tabs->setTabEnabled(0, true);
	UpdateFwCount();

	// don't update if the event is for a different entry
	int index = m_FwPrograms.indexOf(pProgram);
	if (m_iFwIndex == index)
		LoadFwEntry(oldIndex == m_iFwIndex);

	if (!isVisible())
		Show();
	else
		Poke();
	ui.tabs->setCurrentIndex(0);
}

void CPopUpWindow::PopFwEvent()
{
	if (m_iFwIndex == -1)
		return;

	CProgramFilePtr pProgram = m_FwPrograms[m_iFwIndex];
	m_FwPrograms.removeAt(m_iFwIndex);
	m_FwEvents.remove(pProgram);

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
	CProgramFilePtr pProgram = m_FwPrograms[m_iFwIndex];
	const QList<CLogEntryPtr>& List = m_FwEvents[pProgram];

	if (!bUpdate) 
	{
		ui.grpFw->setTitle(pProgram->GetName());
		ui.lblFwProc->setText(QFileInfo(pProgram->GetPath()).fileName());
		ui.lblFwAux->setText("");
		ui.lblFwIcon->setPixmap(pProgram->GetIcon().pixmap(32, 32));
		ui.lblFwPath->setText(QString::fromStdWString(NtPathToDosPath(pProgram->GetPath().toStdWString())));
		ui.treeFw->clear();
	}

	// todo update host name delayed resolution
	
	for (int i = ui.treeFw->topLevelItemCount(); i < List.size(); i++) {
		const CNetLogEntry* pEntry = dynamic_cast<const CNetLogEntry*>(List[i].constData());
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		if (pEntry) {
			pItem->setText(eFwProtocol, QString::number(pEntry->GetProtocolType()));
			pItem->setText(eFwEndpoint, pEntry->GetRemoteAddress().toString() + tr(":%1").arg(pEntry->GetRemotePort()));
			pItem->setText(eFwHostName, pEntry->GetRemoteHostName());
			pItem->setText(eFwTimeStamp, QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss.zzz"));
			//pItem->setText(eFwProcessId, QString::number(pEntry->GetProcessId()));
		}
		ui.treeFw->addTopLevelItem(pItem);
	}
}

void CPopUpWindow::OnFwAction()
{
	QObject* pSender = sender();

	CProgramFilePtr pProgram = m_FwPrograms[m_iFwIndex];

	/*if (pSender == m_pFwAlwaysIgnore)
	{
		PopFwEvent();
		return;
	}*/

	CFwRulePtr pRule = CFwRulePtr(new CFwRule(pProgram->GetID()));
	pRule->SetEnabled(true);
	pRule->SetName(tr("%1 - MP Rule").arg(pProgram->GetNameEx()));
	//pRule->SetDirection(SVC_API_FW_BIDIRECTIONAL);

	if (pSender == m_pFwCustom)
	{
		CFirewallRuleWnd* pFirewallRuleWnd = new CFirewallRuleWnd(pRule, QSet<CProgramItemPtr>() << pProgram);
		if (!pFirewallRuleWnd->exec())
			return;
	}
	else 
	{
		if (pSender == ui.btnFwAllow)
			pRule->SetAction(SVC_API_FW_ALLOW);
		/*else if (pSender == m_pFwLanOnly)
		{
			pRule->SetAction(SVC_API_FW_ALLOW);
			// todo set remote addresses for LAN
		}*/
		else if (pSender == ui.btnFwBlock)
			pRule->SetAction(SVC_API_FW_BLOCK);

		pRule->SetDirection(SVC_API_FW_INBOUND);
		theCore->Network()->SetFwRule(pRule);
		if (theConf->GetBool("Firewall/MakeInOutRules", false)) {
			pRule->SetDirection(SVC_API_FW_OUTBOUND);
			theCore->Network()->SetFwRule(pRule);
		}
	}

	PopFwEvent();
}