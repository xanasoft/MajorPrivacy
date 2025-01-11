#include "pch.h"
#include "SettingsWindow.h"
#include "../MajorPrivacy.h"
#include "../core/PrivacyCore.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/CustomTheme.h"
#include "../Library/API/PrivacyDefs.h"

SCertInfo g_CertInfo = {0};
QString g_CertName;

void AddIconToLabel(QLabel* pLabel, const QPixmap& Pixmap)
{
	QWidget* pParent = pLabel->parentWidget();
	QWidget* pWidget = new QWidget(pParent);
	pParent->layout()->replaceWidget(pLabel, pWidget);
	QHBoxLayout* pLayout = new QHBoxLayout(pWidget);
	pLayout->setContentsMargins(0, 0, 0, 0);
	pLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	pLayout->setSpacing(1);
	QLabel* pIcon = new QLabel();
	pIcon = new QLabel();
	pIcon->setPixmap(Pixmap);
	pLayout->addWidget(pIcon);
	pLayout->addWidget(pLabel);
}

int CSettingsWindow__Chk2Int(Qt::CheckState state)
{
	switch (state) {
	case Qt::Unchecked: return 0;
	case Qt::Checked: return 1;
	default:
	case Qt::PartiallyChecked: return 2;
	}
}

Qt::CheckState CSettingsWindow__Int2Chk(int state)
{
	switch (state) {
	case 0: return Qt::Unchecked;
	case 1: return Qt::Checked;
	default:
	case 2: return Qt::PartiallyChecked;
	}
}

CSettingsWindow::CSettingsWindow(QWidget* parent)
	: CConfigDialog(parent)
{
	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	//flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	//flags &= ~Qt::WindowMinMaxButtonsHint;
	flags |= Qt::WindowMinimizeButtonHint;
	//flags &= ~Qt::WindowCloseButtonHint;
	setWindowFlags(flags);

	//this->setWindowFlag(Qt::WindowStaysOnTopHint, theGUI->IsAlwaysOnTop());

	ui.setupUi(this);
	this->setWindowTitle(tr("Major Privacy - Settings"));

	//if (theConf->GetBool("Options/AltRowColors", false)) {
	//	foreach(QTreeWidget* pTree, this->findChildren<QTreeWidget*>()) 
	//		pTree->setAlternatingRowColors(true);
	//}

	FixTriStateBoxPallete(this);

	ui.tabs->setTabPosition(QTabWidget::West);

	ui.tabs->setCurrentIndex(0);
	ui.tabs->setTabIcon(0, QIcon(":/Icons/Config.png"));
	ui.tabs->setTabIcon(1, QIcon(":/Icons/Design.png"));
	ui.tabs->setTabIcon(2, QIcon(":/Icons/Notification.png"));
	ui.tabs->setTabIcon(3, QIcon(":/Icons/Process.png"));
	ui.tabs->setTabIcon(4, QIcon(":/Icons/Ampel.png"));
	ui.tabs->setTabIcon(5, QIcon(":/Icons/Wall3.png"));
	ui.tabs->setTabIcon(6, QIcon(":/Icons/Advanced.png"));
	ui.tabs->setTabIcon(7, QIcon(":/Icons/Support.png"));

	ui.tabSupport->setEnabled(false);

	ui.treeIgnore->setIndentation(0);

	int size = 16.0;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	size *= (QApplication::desktop()->logicalDpiX() / 96.0); // todo Qt6
#endif
	AddIconToLabel(ui.lblGeneral, QPixmap(":/Icons/Options.png").scaled(size, size));
	AddIconToLabel(ui.lblInterface, QPixmap(":/Icons/GUI.png").scaled(size, size));
	AddIconToLabel(ui.lblIgnore, QPixmap(":/Icons/DisableMessagePopup.png").scaled(size, size));
	AddIconToLabel(ui.lblExecInfo, QPixmap(":/Icons/Process.png").scaled(size, size));
	AddIconToLabel(ui.lblResInfo, QPixmap(":/Icons/Ampel.png").scaled(size, size));
	AddIconToLabel(ui.lblFw, QPixmap(":/Icons/Options.png").scaled(size, size));
	AddIconToLabel(ui.lblFwInfo, QPixmap(":/Icons/Wall3.png").scaled(size, size));
	AddIconToLabel(ui.lblLogging, QPixmap(":/Icons/List.png").scaled(size, size));

	ui.tabs->setCurrentIndex(0);

	{
		ui.uiLang->addItem(tr("Auto Detection"), "");
		ui.uiLang->addItem(tr("No Translation"), "native");

		QString langDir;
		/*C7zFileEngineHandler LangFS("lang", this);
		if (LangFS.Open(QApplication::applicationDirPath() + "/translations.7z"))
			langDir = LangFS.Prefix() + "/";
		else*/
			langDir = QApplication::applicationDirPath() + "/translations/";

		foreach(const QString & langFile, QDir(langDir).entryList(QStringList("majorprivacy_*.qm"), QDir::Files))
		{
			QString Code = langFile.mid(8, langFile.length() - 8 - 3);
			QLocale Locale(Code);
			QString Lang = Locale.nativeLanguageName();
			ui.uiLang->addItem(Lang, Code);
		}
		ui.uiLang->setCurrentIndex(ui.uiLang->findData(theConf->GetString("Options/UiLanguage")));
	}

	ui.cmbFwAuditPolicy->addItem(tr("Blocked && Alowed"), (uint32)FwAuditPolicy::All);
	ui.cmbFwAuditPolicy->addItem(tr("Blocked"), (uint32)FwAuditPolicy::Blocked);
	ui.cmbFwAuditPolicy->addItem(tr("Allowed"), (uint32)FwAuditPolicy::Allowed);
	ui.cmbFwAuditPolicy->addItem(tr("Off"), (uint32)FwAuditPolicy::Off);

	LoadSettings();

	ui.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

	connect(ui.uiLang, SIGNAL(currentIndexChanged(int)), this, SLOT(OnChangeGUI()));

	connect(ui.chkEnumApps, SIGNAL(clicked(bool)), this, SLOT(OnOptChanged()));

	connect(ui.btnDelIgnore, SIGNAL(clicked(bool)), this, SLOT(OnDelIgnore()));

	connect(ui.chkListOpenFiles, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkAccessTree, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkAccessLog, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkLogNotFound, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkLogRegistry, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));

	connect(ui.chkFwLog, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkReverseDNS, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkSimpleDomains, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));

	connect(ui.chkDarkTheme, SIGNAL(stateChanged(int)), this, SLOT(OnChangeGUI()));
	connect(ui.chkFusionTheme, SIGNAL(stateChanged(int)), this, SLOT(OnChangeGUI()));

	connect(ui.chkExecShowPopUp, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkExecLog, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));

	connect(ui.chkResShowPopUp, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));

	connect(ui.radFwAlowList, SIGNAL(clicked()), this, SLOT(OnFwModeChanged()));
	connect(ui.radFwBlockList, SIGNAL(clicked()), this, SLOT(OnFwModeChanged()));
	connect(ui.radFwDisable, SIGNAL(clicked()), this, SLOT(OnFwModeChanged()));

	connect(ui.cmbFwAuditPolicy, SIGNAL(currentIndexChanged(int)), this, SLOT(OnFwAuditPolicyChanged()));
	connect(ui.chkFwShowPopUp, SIGNAL(stateChanged(int)), this, SLOT(OnFwShowPopUpChanged()));
	connect(ui.chkFwInOutRules, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkLoadFwLog, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));

	connect(ui.txtLogLimit, SIGNAL(textChanged(const QString &)), this, SLOT(OnOptChanged()));
	connect(ui.txtLogRetention, SIGNAL(textChanged(const QString &)), this, SLOT(OnOptChanged()));

	connect(ui.tabs, SIGNAL(currentChanged(int)), this, SLOT(OnTab()));

	connect(ui.buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked(bool)), this, SLOT(ok()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(apply()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	this->installEventFilter(this); // prevent enter from closing the dialog

	restoreGeometry(theConf->GetBlob("SettingsWindow/Window_Geometry"));

	foreach(QTreeWidget * pTree, this->findChildren<QTreeWidget*>()) {
		QByteArray Columns = theConf->GetBlob("SettingsWindow/" + pTree->objectName() + "_Columns");
		if (!Columns.isEmpty()) 
			pTree->header()->restoreState(Columns);
	}

	int iOptionTree = theConf->GetInt("Options/OptionTree", 0);
	
	if (iOptionTree) 
		OnSetTree();
	else {
		QWidget* pSearch = AddConfigSearch(ui.tabs);
		ui.horizontalLayout->insertWidget(0, pSearch);
		QTimer::singleShot(0, [this]() {
			m_pSearch->setMaximumWidth(m_pTabs->tabBar()->width());
		});

		QAction* pSetTree = new QAction();
		connect(pSetTree, SIGNAL(triggered()), this, SLOT(OnSetTree()));
		pSetTree->setShortcut(QKeySequence("Ctrl+Alt+T"));
		pSetTree->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		this->addAction(pSetTree);
	}
	m_pSearch->setPlaceholderText(tr("Search for settings"));
}

void CSettingsWindow::OnSetTree()
{
	if (!ui.tabs) return;
	QWidget* pAltView = ConvertToTree(ui.tabs);
	ui.verticalLayout->replaceWidget(ui.tabs, pAltView);
	ui.tabs->deleteLater();
	ui.tabs = NULL;
}

CSettingsWindow::~CSettingsWindow()
{
	theConf->SetBlob("SettingsWindow/Window_Geometry",saveGeometry());

	foreach(QTreeWidget * pTree, this->findChildren<QTreeWidget*>()) 
		theConf->SetBlob("SettingsWindow/" + pTree->objectName() + "_Columns", pTree->header()->saveState());
}

void CSettingsWindow::showTab(const QString& Name, bool bExclusive)
{
	QWidget* pWidget = this->findChild<QWidget*>("tab" + Name);

	if (ui.tabs) {
		
		for (int i = 0; i < ui.tabs->count(); i++) {
			QGridLayout* pGrid = qobject_cast<QGridLayout*>(ui.tabs->widget(i)->layout());
			QTabWidget* pSubTabs = pGrid ? qobject_cast<QTabWidget*>(pGrid->itemAt(0)->widget()) : NULL;
			if(ui.tabs->widget(i) == pWidget)
				ui.tabs->setCurrentIndex(i);
			else if(pSubTabs) {
				for (int j = 0; j < pSubTabs->count(); j++) {
					if (pSubTabs->widget(j) == pWidget) {
						ui.tabs->setCurrentIndex(i);
						pSubTabs->setCurrentIndex(j);
					}
				}
			}
		}
		
	}
	else
		m_pStack->setCurrentWidget(pWidget);

	/// 

	if (bExclusive) {
		if (ui.tabs)
			ui.tabs->tabBar()->setVisible(false);
		else {
			m_pTree->setVisible(false);
			m_pSearch->setVisible(false);
		}
	}

	SafeShow(this);
}

void CSettingsWindow::closeEvent(QCloseEvent *e)
{
	emit Closed();
	this->deleteLater();
}

bool CSettingsWindow::eventFilter(QObject *source, QEvent *event)
{
	//if (event->type() == QEvent::KeyPress && ((QKeyEvent*)event)->key() == Qt::Key_Escape 
	//	&& ((QKeyEvent*)event)->modifiers() == Qt::NoModifier)
	//{
	//	return true; // cancel event
	//}

	if (event->type() == QEvent::KeyPress && (((QKeyEvent*)event)->key() == Qt::Key_Enter || ((QKeyEvent*)event)->key() == Qt::Key_Return) 
		&& (((QKeyEvent*)event)->modifiers() == Qt::NoModifier || ((QKeyEvent*)event)->modifiers() == Qt::KeypadModifier))
	{
		return true; // cancel event
	}

	return QDialog::eventFilter(source, event);
}

void CSettingsWindow::OnTab()
{
	OnTab(ui.tabs->currentWidget());
}

void CSettingsWindow::OnTab(QWidget* pTab)
{
	m_pCurrentTab = pTab;

	// ....
}

void CSettingsWindow::apply()
{
	SaveSettings();
	LoadSettings();
}

void CSettingsWindow::ok()
{
	apply();

	this->close();
}

void CSettingsWindow::reject()
{
	this->close();
}

void CSettingsWindow::OnDelIgnore()
{
	QTreeWidgetItem* pItem = ui.treeIgnore->currentItem();
	if (!pItem)
		return;

	delete pItem;
	OnIgnoreChanged();
}

void CSettingsWindow::OnFwModeChanged()
{
	m_bFwModeChanged = true;
	OnOptChanged();
}

void CSettingsWindow::OnFwAuditPolicyChanged()
{
	ui.chkFwShowPopUp->setEnabled(ui.cmbFwAuditPolicy->currentData().toUInt() != (uint32)FwAuditPolicy::Off);

	OnFwShowPopUpChanged();

	m_bFwAuditPolicyChanged = true;
}

void CSettingsWindow::OnFwShowPopUpChanged()
{
	ui.chkFwInOutRules->setEnabled(ui.chkFwShowPopUp->isEnabled() && ui.chkFwShowPopUp->isChecked());

	OnOptChanged();
}

void CSettingsWindow::OnOptChanged()
{
	if (m_HoldChange)
		return;
	ui.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
}

void CSettingsWindow::LoadSettings()
{
	m_HoldChange = true;

	ui.uiLang->setCurrentIndex(ui.uiLang->findData(theConf->GetString("Options/UiLanguage")));

	QStringList Keys = theConf->ListKeys(IGNORE_LIST_GROUP);
	foreach(const QString & Key, Keys) {	
		ERuleType Type = GetIgnoreType(Key);
		if(Type == ERuleType::eUnknown)
			continue;
		QString Value = theConf->GetString(IGNORE_LIST_GROUP "/" + Key);
		
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		switch (Type) {
		case ERuleType::eProgram:	pItem->setText(0, tr("Binary Image")); break;
		case ERuleType::eAccess:	pItem->setText(0, tr("Resource Access")); break;
		case ERuleType::eFirewall:	pItem->setText(0, tr("Network Access")); break;
		}
		pItem->setData(0, Qt::UserRole, (int)Type);
		pItem->setText(1, Value);
		ui.treeIgnore->addTopLevelItem(pItem);
	}

	ui.chkEnumApps->setChecked(theCore->GetConfigBool("Service/EnumInstallations", true));

	ui.chkListOpenFiles->setChecked(theCore->GetConfigBool("Service/EnumAllOpenFiles", false));
	ui.chkAccessTree->setChecked(theCore->GetConfigBool("Service/ResTrace", true));
	ui.chkAccessLog->setChecked(theCore->GetConfigBool("Service/ResLog", true));
	ui.chkLogNotFound->setChecked(theCore->GetConfigBool("Service/LogNotFound", false));
	ui.chkLogRegistry->setChecked(theCore->GetConfigBool("Service/LogRegistry", false));

	ui.chkFwLog->setChecked(theCore->GetConfigBool("Service/FwLog", true));
	ui.chkReverseDNS->setChecked(theCore->GetConfigBool("Service/UseReverseDns", false));
	ui.chkSimpleDomains->setChecked(theCore->GetConfigBool("Service/UseSimpleDomains", true));

	ui.chkDarkTheme->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/UseDarkTheme", 2)));
	ui.chkFusionTheme->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/UseFusionTheme", 1)));

	ui.chkExecShowPopUp->setChecked(theConf->GetBool("ProcessProtection/ShowNotifications", true));
	ui.chkExecLog->setChecked(theCore->GetConfigBool("Service/ExecLog", true));

	ui.chkResShowPopUp->setChecked(theConf->GetBool("ResourceAccess/ShowNotifications", true));

	auto FwMode = theCore->GetFwProfile();
	if (!FwMode.IsError()) {
		ui.radFwAlowList->setChecked(FwMode.GetValue() == FwFilteringModes::AllowList);
		ui.radFwBlockList->setChecked(FwMode.GetValue() == FwFilteringModes::BlockList);
		ui.radFwDisable->setChecked(FwMode.GetValue() == FwFilteringModes::NoFiltering);
	}

	auto FwAudit = theCore->GetAuditPolicy();
	if (!FwAudit.IsError())
		ui.cmbFwAuditPolicy->setCurrentIndex(ui.cmbFwAuditPolicy->findData((uint32)FwAudit.GetValue()));

	ui.chkFwShowPopUp->setChecked(theConf->GetBool("NetworkFirewall/ShowNotifications", false));
	ui.chkFwInOutRules->setChecked(theConf->GetBool("NetworkFirewall/MakeInOutRules", false));

	ui.chkLoadFwLog->setChecked(theCore->GetConfigBool("Service/LoadWindowsFirewallLog", false));

	ui.txtLogLimit->setText(QString::number(theCore->GetConfigInt64("Service/TraceLogRetentionCount", 10000)));
	ui.txtLogRetention->setText(QString::number(theCore->GetConfigInt64("Service/TraceLogRetentionMinutes", 60 * 24 * 14) / 60));
	
	OnFwAuditPolicyChanged();

	m_HoldChange = false;

	m_bFwModeChanged = false;
	m_bFwAuditPolicyChanged = false;
}

void CSettingsWindow::SaveSettings()
{
	theConf->SetValue("Options/UiLanguage", ui.uiLang->currentData());

	if (m_IgnoreChanged) {

		QStringList Keys = theConf->ListKeys(IGNORE_LIST_GROUP);
		foreach(const QString & Key, Keys)
			theConf->DelValue(IGNORE_LIST_GROUP "/" + Key);

		for (int i = 0; i < ui.treeIgnore->topLevelItemCount(); i++) {
			QTreeWidgetItem* pItem = ui.treeIgnore->topLevelItem(i);
			ERuleType Type = (ERuleType)pItem->data(0, Qt::UserRole).toUInt();
			QString Value = pItem->text(1);
			theConf->SetValue(IGNORE_LIST_GROUP "/" + GetIgnoreTypeName(Type) + "_" + QString::number(++i), Value);
		}
		
		theGUI->LoadIgnoreList();
	}

	theCore->SetConfig("Service/EnumInstallations", ui.chkEnumApps->isChecked());

	theCore->SetConfig("Service/EnumAllOpenFiles", ui.chkListOpenFiles->isChecked());
	theCore->SetConfig("Service/ResTrace", ui.chkAccessTree->isChecked());
	theCore->SetConfig("Service/ResLog", ui.chkAccessLog->isChecked());
	theCore->SetConfig("Service/LogNotFound", ui.chkLogNotFound->isChecked());
	theCore->SetConfig("Service/LogRegistry", ui.chkLogRegistry->isChecked());

	theCore->SetConfig("Service/FwLog", ui.chkFwLog->isChecked());
	theCore->SetConfig("Service/UseReverseDns", ui.chkReverseDNS->isChecked());
	theCore->SetConfig("Service/UseSimpleDomains", ui.chkSimpleDomains->isChecked());

	theConf->SetValue("Options/UseDarkTheme", CSettingsWindow__Chk2Int(ui.chkDarkTheme->checkState()));
	theConf->SetValue("Options/UseFusionTheme", CSettingsWindow__Chk2Int(ui.chkFusionTheme->checkState()));

	theConf->SetValue("ProcessProtection/ShowNotifications", ui.chkExecShowPopUp->isChecked());
	theCore->SetConfig("Service/ExecLog", ui.chkExecLog->isChecked());

	theConf->SetValue("ResourceAccess/ShowNotifications", ui.chkResShowPopUp->isChecked());

	if (m_bFwModeChanged) {
		FwFilteringModes Mode = FwFilteringModes::Unknown;
		if (ui.radFwAlowList->isChecked())		Mode = FwFilteringModes::AllowList;
		else if (ui.radFwBlockList->isChecked())Mode = FwFilteringModes::BlockList;
		else if (ui.radFwDisable->isChecked())	Mode = FwFilteringModes::NoFiltering;
		if(Mode != FwFilteringModes::Unknown)	theCore->SetFwProfile(Mode);
		m_bFwModeChanged = false;
	}

	if (m_bFwAuditPolicyChanged) {
		theCore->SetAuditPolicy((FwAuditPolicy)ui.cmbFwAuditPolicy->currentData().toUInt());
		m_bFwAuditPolicyChanged = false;
	}

	theConf->SetValue("NetworkFirewall/ShowNotifications", ui.chkFwShowPopUp->isChecked());
	theConf->SetValue("NetworkFirewall/MakeInOutRules", ui.chkFwInOutRules->isChecked());

	theCore->SetConfig("Service/LoadWindowsFirewallLog", ui.chkLoadFwLog->isChecked());

	theCore->SetConfig("Service/TraceLogRetentionCount", ui.txtLogLimit->text().toULongLong());
	theCore->SetConfig("Service/TraceLogRetentionMinutes", ui.txtLogRetention->text().toULongLong() * 60);

	emit OptionsChanged(m_bRebuildUI);
}