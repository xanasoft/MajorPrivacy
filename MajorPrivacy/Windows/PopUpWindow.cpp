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
	
	// Firewall
	connect(ui.btnFwPrev, SIGNAL(clicked(bool)), this, SLOT(OnFwPrev()));
	connect(ui.btnFwNext, SIGNAL(clicked(bool)), this, SLOT(OnFwNext()));

	CPopUpWindow__ReplaceWidget<QLabel>(ui.lblFwPath, new CPathLabel());
	CPopUpWindow__UpgradeTree(ui.treeFw);

	

	connect(ui.btnFwIgnore, SIGNAL(clicked(bool)), this, SLOT(OnFwAction()));
	QMenu* pFwIgnore = new QMenu(ui.btnFwIgnore);
	m_pFwAlwaysIgnoreApp = pFwIgnore->addAction(tr("Always Ignore this Program"), this, SLOT(OnFwAction()));
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

	connect(ui.btnExecIgnore, SIGNAL(clicked(bool)), this, SLOT(OnExecAction()));
	QMenu* pExecIgnore = new QMenu(ui.btnExecIgnore);
	m_pExecAlwaysIgnoreFile = pExecIgnore->addAction(tr("Always Ignore this Binary"), this, SLOT(OnExecAction()));
	ui.btnExecIgnore->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnExecIgnore->setMenu(pExecIgnore);

	connect(ui.btnExecSign, SIGNAL(clicked(bool)), this, SLOT(OnExecAction()));
	QMenu* pExecSign = new QMenu(ui.btnExecIgnore);
	m_pExecSignAll = pExecSign->addAction(tr("Sign ALL Binaries"), this, SLOT(OnExecAction()));
	ui.btnExecSign->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnExecSign->setMenu(pExecSign);

	
	m_iTopMost = 0;
	SetWindowPos((HWND)this->winId(), 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	
	m_uTimerID = startTimer(1000);

	m_ResetPosition = !restoreGeometry(theConf->GetBlob("PopUpWindow/Window_Geometry"));

	ui.treeFw->header()->restoreState(theConf->GetBlob("PopUpWindow/FirewallView_Columns"));
	ui.treeRes->header()->restoreState(theConf->GetBlob("PopUpWindow/AccessView_Columns"));
	ui.treeExec->header()->restoreState(theConf->GetBlob("PopUpWindow/ExecutionView_Columns"));
	ui.treeConf->header()->restoreState(theConf->GetBlob("PopUpWindow/TweakView_Columns"));
}

CPopUpWindow::~CPopUpWindow()
{
	killTimer(m_uTimerID);

	theConf->SetBlob("PopUpWindow/Window_Geometry", saveGeometry());

	theConf->SetBlob("PopUpWindow/FirewallView_Columns", ui.treeFw->header()->saveState());
	theConf->SetBlob("PopUpWindow/AccessView_Columns", ui.treeRes->header()->saveState());
	theConf->SetBlob("PopUpWindow/ExecutionView_Columns", ui.treeExec->header()->saveState());
	theConf->SetBlob("PopUpWindow/TweakView_Columns", ui.treeConf->header()->saveState());
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
	QList<CLogEntryPtr>& List = m_FwEvents[pProgram];
	if (List.isEmpty())
		m_FwPrograms.append(pProgram);
	List.append(pEntry);

	int oldIndex = m_iFwIndex;

	if (m_iFwIndex < 0)
		m_iFwIndex = 0;
	else if (m_iFwIndex >= m_FwPrograms.size())
		m_iFwIndex = m_FwPrograms.size() - 1;

	ui.tabs->setTabEnabled(eTabFw, true);
	UpdateFwCount();

	// don't update if the event is for a different entry
	int index = m_FwPrograms.indexOf(pProgram);
	if (m_iFwIndex == index)
		LoadFwEntry(oldIndex == m_iFwIndex);

	if (!isVisible()) {
		ui.tabs->setCurrentIndex(eTabFw);
		Show();
	} else
		Poke();
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
		QString Title = pProgram->GetName();
		QString Name = QFileInfo(pProgram->GetPath(EPathType::eWin32)).fileName();
		if(Title.isEmpty()) Title = Name;
		ui.grpFw->setTitle(Title);
		ui.lblFwProc->setText(Name);
		ui.lblFwAux->setText(""); // todo
		ui.lblFwIcon->setPixmap(pProgram->GetIcon().pixmap(32, 32));
		((CPathLabel*)ui.lblFwPath)->setPath(pProgram->GetPath(EPathType::eDisplay));
		ui.treeFw->clear();
	}

	// todo update host name delayed resolution
	
	for (int i = ui.treeFw->topLevelItemCount(); i < List.size(); i++) {
		const CNetLogEntry* pEntry = dynamic_cast<const CNetLogEntry*>(List[i].constData());
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		if (pEntry) {
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
			pItem->setText(eFwProcessId, QString::number(pEntry->GetProcessId()));
			CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pEntry->GetProcessId(), true);
			if(pProcess) pItem->setText(eFwCmdLine, pProcess->GetCmdLine());
		}
		ui.treeFw->addTopLevelItem(pItem);
	}
}

void CPopUpWindow::OnFwAction()
{
	QObject* pSender = sender();

	CProgramFilePtr pProgram = m_FwPrograms[m_iFwIndex];

	if(pSender == ui.btnFwIgnore || pSender == m_pFwAlwaysIgnoreApp)
	{
		if (pSender == m_pFwAlwaysIgnoreApp)
			theGUI->IgnoreEvent(ERuleType::eFirewall, pProgram);
		PopFwEvent();
		return;
	}

	bool bInbound = false;
	//bool bOutbound = false;
	for (int i = 0; i < ui.treeRes->topLevelItemCount(); i++) {
		QTreeWidgetItem* pItem = ui.treeRes->topLevelItem(i);
		EFwDirections Direction = (EFwDirections)pItem->data(eFwProtocol, Qt::UserRole).toUInt();
		if (Direction == EFwDirections::Inbound)
			bInbound = true;
		//else if (Direction == EFwDirections::Outbound)
		//	bOutbound = true;
	}
	
	CFwRulePtr pRule = CFwRulePtr(new CFwRule(pProgram->GetID()));
	pRule->SetEnabled(true);
	pRule->SetName(tr("%1 - Rule").arg(pProgram->GetNameEx()));
	//pRule->SetDirection(SVC_API_FW_BIDIRECTIONAL);
	pRule->SetProfile((int)EFwProfiles::All);
	pRule->SetProtocol(EFwKnownProtocols::Any);
	pRule->SetInterface((int)EFwInterfaces::All);

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

//////////////////////////////////////////////////////////////////////////
// Access

void CPopUpWindow::PushResEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry)
{
	QList<CLogEntryPtr>& List = m_ResEvents[pProgram];
	if (List.isEmpty())
		m_ResPrograms.append(pProgram);
	List.append(pEntry);

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
		Show();
	} else
		Poke();
}

void CPopUpWindow::PopResEvent(const QString& Path)
{
	if (m_iResIndex == -1)
		return;

	QString PathExp = QRegularExpression::escape(Path).replace("\\*",".*");
	QRegularExpression RegExp(PathExp, QRegularExpression::CaseInsensitiveOption);

	for (int i = 0; i < ui.treeRes->topLevelItemCount(); i++) {
		QTreeWidgetItem* pItem = ui.treeRes->topLevelItem(i);
		QString CurPath = pItem->data(eResPath, Qt::UserRole).toString();
		if (RegExp.match(CurPath).hasMatch() || RegExp.match(CurPath + "\\").hasMatch()) {
			i--;
			delete pItem;
		}
	}
	if(ui.treeRes->topLevelItemCount() > 0)
		return;

	CProgramFilePtr pProgram = m_ResPrograms[m_iResIndex];
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
	const QList<CLogEntryPtr>& List = m_ResEvents[pProgram];

	if (!bUpdate) 
	{
		QString Title = pProgram->GetName();
		QString Name = QFileInfo(pProgram->GetPath(EPathType::eWin32)).fileName();
		if(Title.isEmpty()) Title = Name;
		ui.grpRes->setTitle(Title);
		ui.lblResProc->setText(QFileInfo(pProgram->GetPath(EPathType::eWin32)).fileName());
		ui.lblResAux->setText(""); // todo
		ui.lblResIcon->setPixmap(pProgram->GetIcon().pixmap(32, 32));
		((CPathLabel*)ui.lblResPath)->setPath(pProgram->GetPath(EPathType::eDisplay));
		ui.treeRes->clear();
	}

	for (int i = ui.treeRes->topLevelItemCount(); i < List.size(); i++) {
		const CResLogEntry* pEntry = dynamic_cast<const CResLogEntry*>(List[i].constData());
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		if (pEntry) {
			pItem->setData(eResPath, Qt::UserRole, pEntry->GetPath(EPathType::eNative));
			pItem->setText(eResPath, pEntry->GetPath(EPathType::eDisplay));
			pItem->setText(eResStatus, pEntry->GetStatusStr());
			FILE_BASIC_INFORMATION info = { 0 };
			NtQueryAttributesFile(&SNtObject(pEntry->GetPath(EPathType::eNative).toStdWString(), NULL).attr, &info);
			pItem->setData(eResStatus, Qt::UserRole, info.FileAttributes == FILE_ATTRIBUTE_DIRECTORY);
			if(info.FileAttributes == FILE_ATTRIBUTE_DIRECTORY)
				pItem->setIcon(eResPath, IconProvider.icon(QFileIconProvider::Folder));
			else //pItem->setIcon(eResPath, IconProvider.icon(QFileIconProvider::File));
				pItem->setIcon(eResPath, IconProvider.icon(QFileInfo(QFileInfo(pEntry->GetPath(EPathType::eWin32)).fileName()))); // prevent fiel access pass only filename to get default icon
			pItem->setText(eResTimeStamp, QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss.zzz"));
			pItem->setText(eResProcessId, QString::number(pEntry->GetProcessId()));
			CProcessPtr pProcess = theCore->ProcessList()->GetProcess(pEntry->GetProcessId(), true);
			if(pProcess) pItem->setText(eResCmdLine, pProcess->GetCmdLine());
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
			theGUI->IgnoreEvent(ERuleType::eAccess, pProgram);
		PopResEvent("*");
		return;
	}

	QTreeWidgetItem* pItem = ui.treeRes->currentItem();
	if (!pItem) {
		QMessageBox::information(this, tr("MajorPrivacy"), tr("Please select a resource to create a rule for."));
		return;
	}
	QString NtPath = pItem->data(eResPath, Qt::UserRole).toString();
	bool bIsDir = pItem->data(eResStatus, Qt::UserRole).toBool();
	if (bIsDir && NtPath.right(1) != "\\")
		NtPath += "\\";
	if (NtPath.right(1) == "\\")
		NtPath += "*";

	QString Path = QString::fromStdWString(NtPathToDosPath(NtPath.toStdWString()));

	if(pSender == m_pResAlwaysIgnorePath)
	{
		theGUI->IgnoreEvent(ERuleType::eAccess, NULL, Path);
		PopResEvent(NtPath);
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
				Path = QString::fromStdWString(NtPathToDosPath((pVolume->GetImagePath() + "/" + NtPath.mid(DevicePath.length() + 1)).toStdWString()));
			pCurVolume = pVolume;
			break;
		}
	}

	CAccessRulePtr pRule = CAccessRulePtr(new CAccessRule(pProgram->GetID()));
	pRule->SetEnabled(true);
	pRule->SetName(tr("%1 - Rule").arg(pProgram->GetNameEx()));
	pRule->SetPath(Path);
	if(bUseVolumeRules)
		pRule->SetVolumeRule(true);

	if (pSender == m_pResCustom)
	{
		CAccessRuleWnd* pAccessRuleWnd = new CAccessRuleWnd(pRule, QSet<CProgramItemPtr>() << pProgram);
		if (!pAccessRuleWnd->exec())
			return;

		Path = pRule->GetPath(); // could have been changed

		if (bUseVolumeRules) {
			if (Path.startsWith("\\"))
				NtPath = Path;
			else
				NtPath = QString::fromStdWString(DosPathToNtPath(Path.toStdWString()));
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

		STATUS Status = theCore->AccessManager()->SetAccessRule(pRule);
		if(theGUI->CheckResults(QList<STATUS>() << Status, this) != 0)
			return;
	}

	if (bUseVolumeRules)
	{
		NtPath = QString::fromStdWString(DosPathToNtPath(Path.toStdWString()));
		
		if(pCurVolume) {
			QString ImagePath = pCurVolume->GetImagePath();
			if (Path.startsWith(ImagePath, Qt::CaseInsensitive)) {
				Path = Path.mid(ImagePath.length() + 1);
				NtPath = pCurVolume->GetDevicePath() + "\\" + Path;
			}
		}
	}

	PopResEvent(NtPath);
}

//////////////////////////////////////////////////////////////////////////
// Execution

void CPopUpWindow::PushExecEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry)
{
	QList<CLogEntryPtr>& List = m_ExecEvents[pProgram];
	if (List.isEmpty())
		m_ExecPrograms.append(pProgram);
	List.append(pEntry);

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
	const QList<CLogEntryPtr>& List = m_ExecEvents[pProgram];

	if (!bUpdate) 
	{
		QString Title = pProgram->GetName();
		QString Name = QFileInfo(pProgram->GetPath(EPathType::eWin32)).fileName();
		if(Title.isEmpty()) Title = Name;
		ui.grpExec->setTitle(Title);
		ui.lblExecProc->setText(QFileInfo(pProgram->GetPath(EPathType::eWin32)).fileName());
		ui.lblExecAux->setText(""); // todo
		ui.lblExecIcon->setPixmap(pProgram->GetIcon().pixmap(32, 32));
		((CPathLabel*)ui.lblExecPath)->setPath(pProgram->GetPath(EPathType::eDisplay));
		ui.treeExec->clear();
	}

	for (int i = ui.treeExec->topLevelItemCount(); i < List.size(); i++) {
		const CExecLogEntry* pEntry = dynamic_cast<const CExecLogEntry*>(List[i].constData());
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		if (pEntry) {

			pItem->setData(eExecName, Qt::UserRole, (uint32)pEntry->GetType());

			if (pEntry->GetType() == EExecLogType::eImageLoad)
			{
				uint64 uLibRef = pEntry->GetMiscID();
				CProgramLibraryPtr pLibrary = theCore->ProgramManager()->GetLibraryByUID(uLibRef);
				if (pLibrary)
				{
					QMap<quint64, SLibraryInfo> Log = pProgram->GetLibraries();

					pItem->setData(eExecPath, Qt::UserRole, pLibrary->GetPath(EPathType::eDisplay));
					pItem->setText(eExecName, pLibrary->GetName());
					pItem->setIcon(eExecName, CProgramLibrary::DefaultIcon());
					pItem->setText(eExecStatus, pEntry->GetStatusStr());
					pItem->setText(eExecSign, CProgramFile::GetSignatureInfoStr(Log[uLibRef].Sign));
					pItem->setText(eExecTimeStamp, QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss.zzz"));
					pItem->setText(eExecProcessId, QString::number(pEntry->GetProcessId()));
					pItem->setText(eExecPath, pLibrary->GetPath(EPathType::eDisplay));
				}
			}
			else if (pEntry->GetType() == EExecLogType::eProcessStarted)
			{
				uint64 uID = pEntry->GetMiscID();
				CProgramFilePtr pExecutable = theCore->ProgramManager()->GetProgramByUID(uID).objectCast<CProgramFile>();
				if (pExecutable) {
					pItem->setData(eExecPath, Qt::UserRole, pExecutable->GetPath(EPathType::eDisplay));
					pItem->setText(eExecName, pExecutable->GetNameEx());
					pItem->setIcon(eExecName, pExecutable->GetIcon());
					pItem->setText(eExecStatus, pEntry->GetStatusStr());
					pItem->setText(eExecSign, CProgramFile::GetSignatureInfoStr(pExecutable->GetSignInfo()));
					pItem->setText(eExecTimeStamp, QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss.zzz"));
					//pItem->setText(eExecProcessId, ); // we would need the target PID but we have the actor event
					pItem->setText(eExecPath, pExecutable->GetPath(EPathType::eDisplay));
				}
			}
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
	
	QStringList Paths;
	if (pSender == m_pExecSignAll)
	{
		for (int i = 0; i < ui.treeExec->topLevelItemCount(); i++) {
			QTreeWidgetItem* pItem = ui.treeExec->topLevelItem(i);
			QString Path = pItem->data(eExecPath, Qt::UserRole).toString();
			if(!Paths.contains(Path, Qt::CaseInsensitive))
				Paths.append(Path);
		}
	}
	else if (pSender == ui.btnExecSign || pSender == m_pExecAlwaysIgnoreFile)
	{
		QTreeWidgetItem* pItem = ui.treeExec->currentItem();
		if (!pItem) {
			QMessageBox::information(this, tr("MajorPrivacy"), tr("Please select a binnary file to sign with the your User Key."));
			return;
		}

		QString Path = pItem->data(eExecPath, Qt::UserRole).toString();

		if (pSender == m_pExecAlwaysIgnoreFile)
		{
			theGUI->IgnoreEvent(ERuleType::eProgram, NULL, Path);
			PopResEvent(Path);
			return;
		}

		Paths.append(Path);
	}

	STATUS Status = theGUI->SignFiles(Paths);
	if(theGUI->CheckResults(QList<STATUS>() << Status, this) != 0)
		return;

	PopExecEvent(Paths);
}