#include "pch.h"
#include "SettingsWindow.h"
#include "../MajorPrivacy.h"
#include "../core/PrivacyCore.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/CustomTheme.h"
#include "../Library/API/PrivacyDefs.h"
#include "../MiscHelpers/Archive/ArchiveFS.h"
#include "../MiscHelpers/Common/CheckableMessageBox.h"
#include "../OnlineUpdater.h"
#include <QFontDialog>
#include "Helpers/WinAdmin.h"

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
	this->setWindowTitle(tr("MajorPrivacy - Settings"));

	if (theConf->GetBool("Options/AltRowColors", false)) {
		foreach(QTreeWidget* pTree, this->findChildren<QTreeWidget*>()) 
			pTree->setAlternatingRowColors(true);
	}

	FixTriStateBoxPallete(this);

	ui.tabs->setTabPosition(QTabWidget::West);

	ui.tabs->setCurrentIndex(0);
	ui.tabs->setTabIcon(0, QIcon(":/Icons/Config.png"));
	ui.tabs->setTabIcon(1, QIcon(":/Icons/Design.png"));
	ui.tabs->setTabIcon(2, QIcon(":/Icons/Notification.png"));
	ui.tabs->setTabIcon(3, QIcon(":/Icons/Process.png"));
	ui.tabs->setTabIcon(4, QIcon(":/Icons/Ampel.png"));
	ui.tabs->setTabIcon(5, QIcon(":/Icons/Wall3.png"));
	ui.tabs->setTabIcon(6, QIcon(":/Icons/Network2.png"));
	ui.tabs->setTabIcon(7, QIcon(":/Icons/Advanced.png"));
	ui.tabs->setTabIcon(8, QIcon(":/Icons/Support.png"));

	ui.tabsFw->setTabIcon(0, QIcon(":/Icons/Windows.png"));
	ui.tabsFw->setTabIcon(1, QIcon(":/Icons/Shield12.png"));

	ui.tabsSupport->setTabIcon(0, QIcon(":/Icons/Cert.png"));
	ui.tabsSupport->setTabIcon(1, QIcon(":/Icons/ReloadIni.png"));

	ui.tabs->setCurrentIndex(0);
	ui.tabsFw->setCurrentIndex(0);
	ui.tabsSupport->setCurrentIndex(0);

	ui.treeIgnore->setIndentation(0);

	int size = 16.0;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	size *= (QApplication::desktop()->logicalDpiX() / 96.0); // todo Qt6
#endif
	AddIconToLabel(ui.lblGeneral, QPixmap(":/Icons/Options.png").scaled(size, size));
	AddIconToLabel(ui.lblOptions, QPixmap(":/Icons/Advanced.png").scaled(size, size));
	AddIconToLabel(ui.lblInterface, QPixmap(":/Icons/GUI.png").scaled(size, size));
	AddIconToLabel(ui.lblDisplay, QPixmap(":/Icons/Advanced.png").scaled(size, size));
	AddIconToLabel(ui.lblIgnore, QPixmap(":/Icons/DisableMessagePopup.png").scaled(size, size));
	AddIconToLabel(ui.lblExecInfo, QPixmap(":/Icons/Process.png").scaled(size, size));
	AddIconToLabel(ui.lblResInfo, QPixmap(":/Icons/Ampel.png").scaled(size, size));
	AddIconToLabel(ui.lblFw, QPixmap(":/Icons/Options.png").scaled(size, size));
	AddIconToLabel(ui.lblFwGuard, QPixmap(":/Icons/Shield8.png").scaled(size, size));
	AddIconToLabel(ui.lblFwAutoGuard, QPixmap(":/Icons/Security2.png").scaled(size, size));
	AddIconToLabel(ui.lblFwInfo, QPixmap(":/Icons/Wall3.png").scaled(size, size));
	AddIconToLabel(ui.lblLogging, QPixmap(":/Icons/List.png").scaled(size, size));
	AddIconToLabel(ui.lblDns, QPixmap(":/Icons/DNS.png").scaled(size, size));
	AddIconToLabel(ui.lblUpdates, QPixmap(":/Icons/Update.png").scaled(size,size));

	{
		ui.uiLang->addItem(tr("Auto Detection"), "");
		ui.uiLang->addItem("No Translation", "native"); // do not translate

		QString langDir;
		C7zFileEngineHandler LangFS("lang", this);
		if (LangFS.Open(QApplication::applicationDirPath() + "/translations.7z"))
			langDir = LangFS.Prefix() + "/";
		else
			langDir = QApplication::applicationDirPath() + "/translations/";

		foreach(const QString & langFile, QDir(langDir).entryList(QStringList("MajorPrivacy_*.qm"), QDir::Files))
		{
			QString Code = langFile.mid(13, langFile.length() - 13 - 3);
			QLocale Locale(Code);
			QString Lang = Locale.nativeLanguageName();
			ui.uiLang->addItem(Lang, Code);
		}
		ui.uiLang->setCurrentIndex(ui.uiLang->findData(theConf->GetString("Options/UiLanguage")));
	}

	ui.cmbDPI->addItem(tr("None"), 0);
	ui.cmbDPI->addItem(tr("Native"), 1);
	ui.cmbDPI->addItem(tr("Qt"), 2);

	int FontScales[] = { 75,100,125,150,175,200,225,250,275,300,350,400, 0 };
	for (int* pFontScales = FontScales; *pFontScales != 0; pFontScales++)
		ui.cmbFontScale->addItem(tr("%1").arg(*pFontScales), *pFontScales);

	// UI Font
	ui.btnSelectUiFont->setIcon(QPixmap(":/Icons/Font.png").scaled(size, size));
	ui.btnSelectUiFont->setToolTip(tr("Select font"));
	ui.btnResetUiFont->setIcon(QPixmap(":/Icons/ResetFont.png").scaled(size, size));
	ui.btnResetUiFont->setToolTip(tr("Reset font"));

	connect(ui.btnSelectUiFont, SIGNAL(clicked(bool)), this, SLOT(OnSelectUiFont()));
	connect(ui.btnResetUiFont, SIGNAL(clicked(bool)), this, SLOT(OnResetUiFont()));
	ui.lblUiFont->setText(QApplication::font().family());

	ui.cmbFwAuditPolicy->addItem(tr("All"), (uint32)FwAuditPolicy::All);
	ui.cmbFwAuditPolicy->addItem(tr("Blocked"), (uint32)FwAuditPolicy::Blocked);
	ui.cmbFwAuditPolicy->addItem(tr("Allowed"), (uint32)FwAuditPolicy::Allowed);
	ui.cmbFwAuditPolicy->addItem(tr("Off"), (uint32)FwAuditPolicy::Off);

	// Updater
	ui.cmbInterval->addItem(tr("Every Day"), 1 * 24 * 60 * 60);
	ui.cmbInterval->addItem(tr("Every Week"), 7 * 24 * 60 * 60);
	ui.cmbInterval->addItem(tr("Every 2 Weeks"), 14 * 24 * 60 * 60);
	ui.cmbInterval->addItem(tr("Every 30 days"), 30 * 24 * 60 * 60);

	ui.cmbUpdate->addItem(tr("Ignore"), "ignore");
	ui.cmbUpdate->addItem(tr("Notify"), "notify");
	ui.cmbUpdate->addItem(tr("Download & Notify"), "download");
	ui.cmbUpdate->addItem(tr("Download & Install"), "install");

	//ui.cmbRelease->addItem(tr("Ignore"), "ignore");
	ui.cmbRelease->addItem(tr("Notify"), "notify");
	ui.cmbRelease->addItem(tr("Download & Notify"), "download");
	ui.cmbRelease->addItem(tr("Download & Install"), "install");
	//

	LoadSettings();

	ui.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

	connect(ui.uiLang, SIGNAL(currentIndexChanged(int)), this, SLOT(OnChangeGUI()));

	connect(ui.chkEnumApps, SIGNAL(clicked(bool)), this, SLOT(OnOptChanged()));

	connect(ui.chkEncryptSwap, SIGNAL(clicked(bool)), this, SLOT(OnOptChanged()));
	connect(ui.chkGuardSuspend, SIGNAL(clicked(bool)), this, SLOT(OnOptChanged()));

	connect(ui.btnDelIgnore, SIGNAL(clicked(bool)), this, SLOT(OnDelIgnore()));

	connect(ui.chkListOpenFiles, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkAccessTrace, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkAccessRecord, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkAccessLog, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkLogNotFound, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkLogRegistry, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));

	connect(ui.chkFwPlus, SIGNAL(stateChanged(int)), this, SLOT(OnOptFwChanged()));
	connect(ui.chkFwPlusNotify, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkNetTrace, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkTrafficRecord, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkFwLog, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkReverseDNS, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkSimpleDomains, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));

	connect(ui.chkDarkTheme, SIGNAL(stateChanged(int)), this, SLOT(OnChangeGUI()));
	connect(ui.chkFusionTheme, SIGNAL(stateChanged(int)), this, SLOT(OnChangeGUI()));
	connect(ui.chkUseW11Style, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	QOperatingSystemVersion current = QOperatingSystemVersion::current();
	ui.chkUseW11Style->setEnabled(current.majorVersion() == 10 && current.microVersion() >= 22000); // Windows 10 22000+ (Windows 11)
	connect(ui.chkAltRows, SIGNAL(stateChanged(int)), this, SLOT(OnChangeGUI()));

	connect(ui.chkMinimize, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkSingleShow, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	
	connect(ui.cmbDPI, SIGNAL(currentIndexChanged(int)), this, SLOT(OnChangeGUI()));
	connect(ui.cmbFontScale, SIGNAL(currentIndexChanged(int)), this, SLOT(OnChangeGUI()));
	connect(ui.cmbFontScale, SIGNAL(currentTextChanged(const QString&)), this, SLOT(OnChangeGUI()));

	connect(ui.chkExecShowPopUp, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkExecLog, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkExecRecord, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));

	connect(ui.chkResShowPopUp, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));

	connect(ui.radFwAllowList, SIGNAL(clicked()), this, SLOT(OnFwModeChanged()));
	connect(ui.radFwBlockList, SIGNAL(clicked()), this, SLOT(OnFwModeChanged()));
	connect(ui.radFwDisable, SIGNAL(clicked()), this, SLOT(OnFwModeChanged()));

	connect(ui.cmbFwAuditPolicy, SIGNAL(currentIndexChanged(int)), this, SLOT(OnFwAuditPolicyChanged()));
	connect(ui.chkFwShowPopUp, SIGNAL(stateChanged(int)), this, SLOT(OnOptFwChanged()));
	connect(ui.chkFwInOutRules, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkFwShowAlerts, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkGuardFwRules, SIGNAL(stateChanged(int)), this, SLOT(OnFwGuardChanged()));
	connect(ui.chkFwDelRogue, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.chkLoadFwLog, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));

	connect(ui.btnFwGuardApprove, SIGNAL(clicked(bool)), this, SLOT(OnGuardAddApprove()));
	connect(ui.btnFwGuardReject, SIGNAL(clicked(bool)), this, SLOT(OnGuardAddReject()));
	connect(ui.btnFwGuardRemove, SIGNAL(clicked(bool)), this, SLOT(OnGuardRemove()));

	connect(ui.treeFwGuard, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(OnFwGuardListClicked(QTreeWidgetItem*, int)));
	connect(ui.treeFwGuard, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnFwGuardDoubleClicked(QTreeWidgetItem*, int)));

	connect(ui.btnFwGuardNotify, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));

	connect(ui.chkDnsFilter, SIGNAL(stateChanged(int)), this, SLOT(OnDnsChanged()));
	connect(ui.chkDnsInstall, SIGNAL(stateChanged(int)), this, SLOT(OnDnsChanged()));
	connect(ui.txtDnsResolvers, SIGNAL(textChanged(const QString &)), this, SLOT(OnDnsChanged2()));
	connect(ui.treeDnsBlockLists, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(OnDnsBlockListClicked(QTreeWidgetItem*, int)));
	connect(ui.treeDnsBlockLists, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnDnsBlockListDoubleClicked(QTreeWidgetItem*, int)));
	connect(ui.btnAddBlockList, SIGNAL(clicked(bool)), this, SLOT(OnAddBlockList()));
	connect(ui.btnDelBlockList, SIGNAL(clicked(bool)), this, SLOT(OnDelBlockList()));

	connect(ui.txtLogLimit, SIGNAL(textChanged(const QString &)), this, SLOT(OnOptChanged()));
	connect(ui.txtLogRetention, SIGNAL(textChanged(const QString &)), this, SLOT(OnOptChanged()));

	// Updater
	connect(ui.lblCurrent, SIGNAL(linkActivated(const QString&)), this, SLOT(OnUpdate(const QString&)));
	connect(ui.lblStable, SIGNAL(linkActivated(const QString&)), this, SLOT(OnUpdate(const QString&)));
	connect(ui.lblPreview, SIGNAL(linkActivated(const QString&)), this, SLOT(OnUpdate(const QString&)));
	
	connect(ui.chkAutoUpdate, SIGNAL(toggled(bool)), this, SLOT(UpdateUpdater()));

	connect(ui.cmbInterval, SIGNAL(currentIndexChanged(int)), this, SLOT(OnOptChanged()));

	connect(ui.radStable, SIGNAL(toggled(bool)), this, SLOT(UpdateUpdater()));
	connect(ui.radPreview, SIGNAL(toggled(bool)), this, SLOT(UpdateUpdater()));

	connect(ui.cmbUpdate, SIGNAL(currentIndexChanged(int)), this, SLOT(OnOptChanged()));
	connect(ui.cmbRelease, SIGNAL(currentIndexChanged(int)), this, SLOT(OnOptChanged()));

	connect(ui.chkUpdateTweaks, SIGNAL(toggled(bool)), this, SLOT(OnOptChanged()));
	//

	connect(ui.tabs, SIGNAL(currentChanged(int)), this, SLOT(OnTab()));

	connect(ui.buttonBox->button(QDialogButtonBox::Ok), SIGNAL(clicked(bool)), this, SLOT(ok()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(apply()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	InitSupport();

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

	m_uTimerID = startTimer(1000);
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
	killTimer(m_uTimerID);

	theConf->SetBlob("SettingsWindow/Window_Geometry",saveGeometry());

	foreach(QTreeWidget * pTree, this->findChildren<QTreeWidget*>()) 
		theConf->SetBlob("SettingsWindow/" + pTree->objectName() + "_Columns", pTree->header()->saveState());
}

void CSettingsWindow::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() != m_uTimerID)
		return;

	if (m_pCurrentTab == ui.tabDns) 
	{
		auto ret = theCore->GetBlockListInfo();
		if (!ret.IsError())
		{
			QMap<QString, QTreeWidgetItem*> Lists;
			for (int i = 0; i < ui.treeDnsBlockLists->topLevelItemCount(); i++)
			{
				QTreeWidgetItem* pItem = ui.treeDnsBlockLists->topLevelItem(i);
				Lists.insert(pItem->text(0).toLower(), pItem);
			}

			auto var = ret.GetValue();
			for (int i = 0; i < var.Count(); i++)
			{
				QtVariant Item = var[i];
				QString Url = Item[API_V_URL].AsQStr();
				if(QTreeWidgetItem* pItem = Lists.value(Url.toLower()))
					pItem->setText(1, QString::number(Item[API_V_COUNT].To<int>()));
			}
		}
	}
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

	if (source == ui.txtCertificate)
	{
		static bool m_bRightButtonPressed = false;

		if (event->type() == QEvent::FocusIn && ui.txtCertificate->property("hidden").toBool())	{
			ui.txtCertificate->setProperty("hidden", false);
			ui.txtCertificate->setPlainText(g_Certificate);
			ui.txtCertificate->setProperty("modified", false);
		}
		else if (event->type() == QEvent::MouseButtonPress && ((QMouseEvent*)event)->button() == Qt::RightButton) {
			m_bRightButtonPressed = true;
		}
		else if (event->type() == QEvent::FocusOut && !ui.txtCertificate->property("hidden").toBool()) {
			if (!ui.txtCertificate->property("modified").toBool() && !m_bRightButtonPressed) {
				ui.txtCertificate->setProperty("hidden", true);
				int Pos = g_Certificate.indexOf("HWID:");
				if (Pos == -1)
					Pos = g_Certificate.indexOf("UPDATEKEY:");

				QByteArray truncatedCert = (g_Certificate.left(Pos) + "...");
				int namePos = truncatedCert.indexOf("NAME:");
				int datePos = truncatedCert.indexOf("DATE:");
				if (namePos != -1 && datePos != -1 && datePos > namePos)
					truncatedCert = truncatedCert.mid(0, namePos + 5) + " ...\n" + truncatedCert.mid(datePos);
				ui.txtCertificate->setPlainText(truncatedCert);
			}
		}

		if (event->type() == QEvent::FocusOut) {
			m_bRightButtonPressed = false;
		}
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

	if (pTab == ui.tabSupport)
	{
		if (ui.lblCurrent->text().isEmpty()) {
			if (ui.chkAutoUpdate->checkState())
				GetUpdates();
			else
				ui.lblCurrent->setText(tr("<a href=\"check\">Check Now</a>"));
		}
	}
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

void CSettingsWindow::OnFwGuardChanged()
{
	ui.chkFwDelRogue->setEnabled(ui.chkGuardFwRules->isChecked());

	OnOptChanged();
}

void CSettingsWindow::OnFwAuditPolicyChanged()
{
	ui.chkFwShowPopUp->setEnabled(ui.cmbFwAuditPolicy->currentData().toUInt() != (uint32)FwAuditPolicy::Off);

	OnOptFwChanged();

	m_bFwAuditPolicyChanged = true;
}

void CSettingsWindow::OnOptFwChanged()
{
	ui.chkFwInOutRules->setEnabled(ui.chkFwShowPopUp->isEnabled() && ui.chkFwShowPopUp->isChecked());
	ui.chkFwPlusNotify->setEnabled(ui.chkFwPlus->isChecked());

	OnOptChanged();
}

void CSettingsWindow::AddAutoEntry(QString Path, bool bApprove)
{
	if (Path.isEmpty())
		return;

	QTreeWidgetItem* pItem = new QTreeWidgetItem();
	pItem->setText(0, bApprove ? tr("Auto Approve") : tr("Auto Reject"));
	pItem->setData(0, Qt::UserRole, bApprove);
	if (Path.startsWith("-")) {
		Path.remove(0, 1);
		pItem->setCheckState(0, Qt::Unchecked);
	}
	else
		pItem->setCheckState(0, Qt::Checked);
	pItem->setText(1, Path);
	//pItem->setFlags(pItem->flags() | Qt::ItemIsEditable);
	ui.treeFwGuard->addTopLevelItem(pItem);
}

void CSettingsWindow::OnGuardAddApprove()
{
	QString Value = QInputDialog::getText(this, "MajorPrivacy", tr("Enter file path pattern for the entry"), QLineEdit::Normal);
	
	AddAutoEntry(Value, true);

	if (!m_HoldChange) m_AutoGuardChanged = true;
}

void CSettingsWindow::OnGuardAddReject()
{
	QString Value = QInputDialog::getText(this, "MajorPrivacy", tr("Enter file path pattern for the entry"), QLineEdit::Normal);

	AddAutoEntry(Value, false);

	if (!m_HoldChange) m_AutoGuardChanged = true;
	OnOptChanged();
}

void CSettingsWindow::OnGuardRemove()
{
	QTreeWidgetItem* pItem = ui.treeFwGuard->currentItem();
	if (!pItem)
		return;

	delete pItem;
	
	if (!m_HoldChange) m_AutoGuardChanged = true;
	OnOptChanged();
}

void CSettingsWindow::OnFwGuardListClicked(QTreeWidgetItem* pItem, int Column)
{
	/*QTreeWidgetItem* pItem = ui.treeFwGuard->currentItem();
	if (!pItem)
	return;

	pItem->setCheckState(0, Qt::Unchecked);*/
	if (!m_HoldChange) m_AutoGuardChanged = true;
	OnOptChanged();
}

void CSettingsWindow::OnFwGuardDoubleClicked(QTreeWidgetItem* pItem, int Column)
{
}

void CSettingsWindow::OnOptChanged()
{
	if (m_HoldChange)
		return;
	ui.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
}

void CSettingsWindow::OnDnsChanged()
{
	ui.chkDnsInstall->setEnabled(ui.chkDnsFilter->isChecked());
	ui.txtDnsResolvers->setEnabled(ui.chkDnsFilter->isChecked());
	ui.treeDnsBlockLists->setEnabled(ui.chkDnsFilter->isChecked());
	ui.btnAddBlockList->setEnabled(ui.chkDnsFilter->isChecked());
	ui.btnDelBlockList->setEnabled(ui.chkDnsFilter->isChecked());
	OnOptChanged();
}

void CSettingsWindow::OnDnsChanged2()
{ 
	if (!m_HoldChange) m_ResolverChanged = true; 
	OnOptChanged(); 
}

void CSettingsWindow::OnDnsBlockListClicked(QTreeWidgetItem* pItem, int Column)
{
	/*QTreeWidgetItem* pItem = ui.treeDnsBlockLists->currentItem();
	if (!pItem)
		return;

	pItem->setCheckState(0, Qt::Unchecked);*/
	if (!m_HoldChange) m_BlockListChanged = true;
	OnOptChanged();
}

void CSettingsWindow::OnDnsBlockListDoubleClicked(QTreeWidgetItem* pItem, int Column)
{
}

void CSettingsWindow::OnAddBlockList()
{
	QString Value = QInputDialog::getText(this, "MajorPrivacy", tr("Enter Block Lisz URL to be added"), QLineEdit::Normal);
	if (Value.isEmpty())
		return;
	QTreeWidgetItem* pItem = new QTreeWidgetItem();
	pItem->setText(0, Value);
	pItem->setCheckState(0, Qt::Checked);
	//pItem->setFlags(pItem->flags() | Qt::ItemIsEditable);
	ui.treeDnsBlockLists->addTopLevelItem(pItem);

	if (!m_HoldChange) m_BlockListChanged = true;
}

void CSettingsWindow::OnDelBlockList()
{
	QTreeWidgetItem* pItem = ui.treeDnsBlockLists->currentItem();
	if (!pItem)
		return;

	delete pItem;
	if (!m_HoldChange) m_BlockListChanged = true;
}

void CSettingsWindow::LoadSettings()
{
	m_HoldChange = true;

	ui.chkAutoStart->setChecked(IsAutorunEnabled());

	ui.uiLang->setCurrentIndex(ui.uiLang->findData(theConf->GetString("Options/UiLanguage")));

	QStringList Keys = theConf->ListKeys(IGNORE_LIST_GROUP);
	foreach(const QString & Key, Keys) {	
		EItemType Type = GetIgnoreType(Key);
		if(Type == EItemType::eUnknown)
			continue;
		QString Value = theConf->GetString(IGNORE_LIST_GROUP "/" + Key);
		
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		switch (Type) {
		case EItemType::eExecRule:	pItem->setText(0, tr("Binary Image")); break;
		case EItemType::eResRule:	pItem->setText(0, tr("Resource Access")); break;
		case EItemType::eFwRule:	pItem->setText(0, tr("Network Access")); break;
		}
		pItem->setData(0, Qt::UserRole, (int)Type);
		pItem->setText(1, Value);
		ui.treeIgnore->addTopLevelItem(pItem);
	}

	ui.chkEnumApps->setChecked(theCore->GetConfigBool("Service/EnumInstallations", true));

	ui.chkEncryptSwap->setChecked(theCore->GetConfigBool("Service/EncryptPageFile", false));
	ui.chkGuardSuspend->setChecked(theCore->GetConfigBool("Service/GuardHibernation", false));

	ui.chkListOpenFiles->setChecked(theCore->GetConfigBool("Service/EnumAllOpenFiles", false));
	ui.chkAccessTrace->setChecked(theCore->GetConfigBool("Service/ResTrace", true));
	ui.chkAccessRecord->setChecked(theCore->GetConfigBool("Service/SaveAccessRecord", false));
	ui.chkAccessLog->setChecked(theCore->GetConfigBool("Service/ResLog", false));
	ui.chkLogNotFound->setChecked(theCore->GetConfigBool("Service/LogNotFound", false));
	ui.chkLogRegistry->setChecked(theCore->GetConfigBool("Service/LogRegistry", false));

	ui.chkFwPlus->setChecked(theCore->GetConfigBool("Service/UseFwRuleTemplates", true));
	ui.chkFwPlusNotify->setChecked(theConf->GetBool("Options/NotifyFwRuleTemplates", true));
	ui.chkFwAutoCleanup->setChecked(theCore->GetConfigBool("Service/AutoCleanUpFwTemplateRules", false));
	ui.chkNetTrace->setChecked(theCore->GetConfigBool("Service/NetTrace", true));
	ui.chkTrafficRecord->setChecked(theCore->GetConfigBool("Service/SaveTrafficRecord", false));
	ui.chkFwLog->setChecked(theCore->GetConfigBool("Service/NetLog", false));
	ui.chkReverseDNS->setChecked(theCore->GetConfigBool("Service/UseReverseDns", false));
	ui.chkSimpleDomains->setChecked(theCore->GetConfigBool("Service/UseSimpleDomains", true));

	ui.chkDarkTheme->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/UseDarkTheme", 2)));
	ui.chkFusionTheme->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/UseFusionTheme", 1)));
	ui.chkUseW11Style->setChecked(theConf->GetBool("Options/UseW11Style", false));
	ui.chkAltRows->setChecked(theConf->GetBool("Options/AltRowColors", false));

	ui.chkMinimize->setChecked(theConf->GetBool("Options/MinimizeToTray", false));
	ui.chkSingleShow->setChecked(theConf->GetBool("Options/TraySingleClick", false));

	ui.cmbDPI->setCurrentIndex(theConf->GetInt("Options/DPIScaling", 1));
	//ui.cmbFontScale->setCurrentIndex(ui.cmbFontScale->findData(theConf->GetInt("Options/FontScaling", 100)));
	ui.cmbFontScale->setCurrentText(QString::number(theConf->GetInt("Options/FontScaling", 100)));

	ui.chkExecShowPopUp->setChecked(theConf->GetBool("ProcessProtection/ShowNotifications", true));
	ui.chkExecRecord->setChecked(theCore->GetConfigBool("Service/SaveIngressRecord", false));
	ui.chkExecLog->setChecked(theCore->GetConfigBool("Service/ExecLog", false));

	ui.chkResShowPopUp->setChecked(theConf->GetBool("ResourceAccess/ShowNotifications", true));

	auto FwMode = theCore->GetFwProfile();
	if (!FwMode.IsError()) {
		ui.radFwAllowList->setChecked(FwMode.GetValue() == FwFilteringModes::AllowList);
		ui.radFwBlockList->setChecked(FwMode.GetValue() == FwFilteringModes::BlockList);
		ui.radFwDisable->setChecked(FwMode.GetValue() == FwFilteringModes::NoFiltering);
	}

	auto FwAudit = theCore->GetAuditPolicy();
	if (!FwAudit.IsError())
		ui.cmbFwAuditPolicy->setCurrentIndex(ui.cmbFwAuditPolicy->findData((uint32)FwAudit.GetValue()));

	ui.chkFwShowPopUp->setChecked(theConf->GetBool("NetworkFirewall/ShowNotifications", false));
	ui.chkFwInOutRules->setChecked(theConf->GetBool("NetworkFirewall/MakeInOutRules", false));
	ui.chkFwShowAlerts->setChecked(theConf->GetBool("NetworkFirewall/ShowChangeAlerts", false));
	ui.chkGuardFwRules->setChecked(theCore->GetConfigBool("Service/GuardFwRules", false));
	ui.chkFwDelRogue->setChecked(theCore->GetConfigBool("Service/DeleteRogueFwRules", false));

	ui.chkLoadFwLog->setChecked(theCore->GetConfigBool("Service/LoadWindowsFirewallLog", false));

	ui.treeFwGuard->clear();

	QStringList AutoApproveList = theCore->GetConfigStr("Service/FwAutoApprove").split("|");
	foreach(QString AutoApprove, AutoApproveList)
		AddAutoEntry(AutoApprove, true);

	QStringList AutoRejectList = theCore->GetConfigStr("Service/FwAutoReject").split("|");
	foreach(QString AutoReject, AutoRejectList)
		AddAutoEntry(AutoReject, false);

	m_AutoGuardChanged = false;

	ui.btnFwGuardNotify->setChecked(theConf->GetBool("Options/NotifyFwGuardAction", true));

	ui.chkDnsFilter->setChecked(theCore->GetConfigBool("Service/DnsEnableFilter", false));
	ui.chkDnsInstall->setChecked(theCore->GetConfigBool("Service/DnsInstallFilter", true));
	ui.txtDnsResolvers->setText(theCore->GetConfigStr("Service/DnsResolvers"));
	QStringList BlockLists = theCore->GetConfigStr("Service/DnsBlockLists").split(";");
	ui.treeDnsBlockLists->clear();
	foreach(QString BlockList, BlockLists) {
		if(BlockList.isEmpty()) continue;
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		if (BlockList.left(1) == "-") {
			BlockList.remove(0, 1);
			pItem->setCheckState(0, Qt::Unchecked);
		}
		else
			pItem->setCheckState(0, Qt::Checked);
		pItem->setText(0, BlockList);
		//pItem->setFlags(pItem->flags() | Qt::ItemIsEditable);
		ui.treeDnsBlockLists->addTopLevelItem(pItem);
	}
	m_BlockListChanged = false;

	OnDnsChanged();

	ui.txtLogLimit->setText(QString::number(theCore->GetConfigInt64("Service/TraceLogRetentionCount", 10000)));
	ui.txtLogRetention->setText(QString::number(theCore->GetConfigInt64("Service/TraceLogRetentionMinutes", 60 * 24 * 14) / 60));
	
	OnFwAuditPolicyChanged();

	UpdateCert();

	ui.chkNoCheck->setChecked(theConf->GetBool("Options/NoSupportCheck", false));
	if(ui.chkNoCheck->isCheckable() && !g_CertInfo.expired)
		ui.chkNoCheck->setVisible(false); // hide if not relevant

	// Updater
	ui.chkAutoUpdate->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/CheckForUpdates", 2)));

	int UpdateInterval = theConf->GetInt("Options/UpdateInterval", UPDATE_INTERVAL);
	int pos = ui.cmbInterval->findData(UpdateInterval);
	if (pos == -1)
		ui.cmbInterval->setCurrentText(QString::number(UpdateInterval));
	else
		ui.cmbInterval->setCurrentIndex(pos);

	QString ReleaseChannel = theConf->GetString("Options/ReleaseChannel", "stable");
	ui.radStable->setChecked(ReleaseChannel == "stable");
	ui.radPreview->setChecked(ReleaseChannel == "preview");

	m_HoldChange = true;
	UpdateUpdater();
	m_HoldChange = false;

	ui.cmbUpdate->setCurrentIndex(ui.cmbUpdate->findData(theConf->GetString("Options/OnNewUpdate", "ignore")));
	ui.cmbRelease->setCurrentIndex(ui.cmbRelease->findData(theConf->GetString("Options/OnNewRelease", "download")));

	ui.chkUpdateTweaks->setCheckState(CSettingsWindow__Int2Chk(theConf->GetInt("Options/UpdateTweaks", 2)));
	//

	m_HoldChange = false;

	m_bFwModeChanged = false;
	m_bFwAuditPolicyChanged = false;
}

void CSettingsWindow::SaveSettings()
{
	AutorunEnable(ui.chkAutoStart->isChecked());

	theConf->SetValue("Options/UiLanguage", ui.uiLang->currentData());

	if (m_IgnoreChanged) {

		QStringList Keys = theConf->ListKeys(IGNORE_LIST_GROUP);
		foreach(const QString & Key, Keys)
			theConf->DelValue(IGNORE_LIST_GROUP "/" + Key);

		for (int i = 0; i < ui.treeIgnore->topLevelItemCount(); i++) {
			QTreeWidgetItem* pItem = ui.treeIgnore->topLevelItem(i);
			EItemType Type = (EItemType)pItem->data(0, Qt::UserRole).toUInt();
			QString Value = pItem->text(1);
			theConf->SetValue(IGNORE_LIST_GROUP "/" + GetIgnoreTypeName(Type) + "_" + QString::number(++i), Value);
		}
		
		theGUI->LoadIgnoreList();
	}

	theCore->SetConfig("Service/EnumInstallations", ui.chkEnumApps->isChecked());

	theCore->SetConfig("Service/EncryptPageFile", ui.chkEncryptSwap->isChecked());
	theCore->SetConfig("Service/GuardHibernation", ui.chkGuardSuspend->isChecked());

	theCore->SetConfig("Service/EnumAllOpenFiles", ui.chkListOpenFiles->isChecked());
	theCore->SetConfig("Service/ResTrace", ui.chkAccessTrace->isChecked());
	theCore->SetConfig("Service/SaveAccessRecord", ui.chkAccessRecord->isChecked());
	theCore->SetConfig("Service/ResLog", ui.chkAccessLog->isChecked());
	theCore->SetConfig("Service/LogNotFound", ui.chkLogNotFound->isChecked());
	theCore->SetConfig("Service/LogRegistry", ui.chkLogRegistry->isChecked());

	theCore->SetConfig("Service/UseFwRuleTemplates", ui.chkFwPlus->isChecked());
	theConf->SetValue("Options/NotifyFwRuleTemplates", ui.chkFwPlusNotify->isChecked());
	theCore->SetConfig("Service/AutoCleanUpFwTemplateRules", ui.chkFwAutoCleanup->isChecked());
	theCore->SetConfig("Service/NetTrace", ui.chkNetTrace->isChecked());
	theCore->SetConfig("Service/SaveTrafficRecord", ui.chkTrafficRecord->isChecked());
	theCore->SetConfig("Service/NetLog", ui.chkFwLog->isChecked());
	theCore->SetConfig("Service/UseReverseDns", ui.chkReverseDNS->isChecked());
	theCore->SetConfig("Service/UseSimpleDomains", ui.chkSimpleDomains->isChecked());

	theConf->SetValue("Options/UseDarkTheme", CSettingsWindow__Chk2Int(ui.chkDarkTheme->checkState()));
	theConf->SetValue("Options/UseFusionTheme", CSettingsWindow__Chk2Int(ui.chkFusionTheme->checkState()));
	theConf->SetValue("Options/UseW11Style", ui.chkUseW11Style->isChecked());
	theConf->SetValue("Options/AltRowColors", ui.chkAltRows->isChecked());

	theConf->SetValue("Options/MinimizeToTray", ui.chkMinimize->isChecked());
	theConf->SetValue("Options/TraySingleClick", ui.chkSingleShow->isChecked());

	theConf->SetValue("UIConfig/UIFont", ui.lblUiFont->text());
	theConf->SetValue("Options/DPIScaling", ui.cmbDPI->currentData());
	int Scaling = ui.cmbFontScale->currentText().toInt();
	if (Scaling < 75)
		Scaling = 75;
	else if (Scaling > 500)
		Scaling = 500;
	theConf->SetValue("Options/FontScaling", Scaling);

	theConf->SetValue("ProcessProtection/ShowNotifications", ui.chkExecShowPopUp->isChecked());
	theCore->SetConfig("Service/SaveIngressRecord", ui.chkExecRecord->isChecked());
	theCore->SetConfig("Service/ExecLog", ui.chkExecLog->isChecked());

	theConf->SetValue("ResourceAccess/ShowNotifications", ui.chkResShowPopUp->isChecked());

	if (m_bFwModeChanged) {
		FwFilteringModes Mode = FwFilteringModes::Unknown;
		if (ui.radFwAllowList->isChecked())		Mode = FwFilteringModes::AllowList;
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
	theConf->SetValue("NetworkFirewall/ShowChangeAlerts", ui.chkFwShowAlerts->isChecked());
	theCore->SetConfig("Service/GuardFwRules", ui.chkGuardFwRules->isChecked());
	theCore->SetConfig("Service/DeleteRogueFwRules", ui.chkFwDelRogue->isChecked());

	theCore->SetConfig("Service/LoadWindowsFirewallLog", ui.chkLoadFwLog->isChecked());

	if (m_AutoGuardChanged)
	{
		QStringList AutoApproveList;
		QStringList AutoRejectList;
		for (int i = 0; i < ui.treeFwGuard->topLevelItemCount(); i++) {
			QTreeWidgetItem* pItem = ui.treeFwGuard->topLevelItem(i);
			QString Value = pItem->text(1);
			if (pItem->checkState(0) == Qt::Unchecked)
				Value = "-" + Value;
			if(pItem->data(0, Qt::UserRole).toBool())
				AutoApproveList.append(Value);
			else
				AutoRejectList.append(Value);
		}
		theCore->SetConfig("Service/FwAutoApprove", AutoApproveList.join("|"));
		theCore->SetConfig("Service/FwAutoReject", AutoRejectList.join("|"));

		m_AutoGuardChanged = false;
	}

	theConf->SetValue("Options/NotifyFwGuardAction", ui.btnFwGuardNotify->isChecked());

	theCore->SetConfig("Service/DnsEnableFilter", ui.chkDnsFilter->isChecked());
	theCore->SetConfig("Service/DnsInstallFilter", ui.chkDnsInstall->isChecked());
	if (m_ResolverChanged)
		theCore->SetConfig("Service/DnsResolvers", ui.txtDnsResolvers->text());
	if (m_BlockListChanged) {
		QStringList BlockLists;
		for (int i = 0; i < ui.treeDnsBlockLists->topLevelItemCount(); i++) {
			QTreeWidgetItem* pItem = ui.treeDnsBlockLists->topLevelItem(i);
			QString Value = pItem->text(0);
			if (pItem->checkState(0) == Qt::Unchecked)
				Value = "-" + Value;
			BlockLists.append(Value);
		}
		theCore->SetConfig("Service/DnsBlockLists", BlockLists.join(";"));
		m_BlockListChanged = false;
	}

	theCore->SetConfig("Service/TraceLogRetentionCount", ui.txtLogLimit->text().toULongLong());
	theCore->SetConfig("Service/TraceLogRetentionMinutes", ui.txtLogRetention->text().toULongLong() * 60);

	if(m_CertChanged)
		ApplyCert();

	theConf->SetValue("Options/NoSupportCheck", ui.chkNoCheck->isChecked());

	// Updater
	theConf->SetValue("Options/CheckForUpdates", CSettingsWindow__Chk2Int(ui.chkAutoUpdate->checkState()));

	int UpdateInterval = ui.cmbInterval->currentData().toInt();
	if (!UpdateInterval)
		UpdateInterval = ui.cmbInterval->currentText().toInt();
	if (!UpdateInterval)
		UpdateInterval = UPDATE_INTERVAL;
	theConf->SetValue("Options/UpdateInterval", UpdateInterval);

	QString ReleaseChannel;
	if (ui.radStable->isChecked())
		ReleaseChannel = "stable";
	else if (ui.radPreview->isChecked())
		ReleaseChannel = "preview";
	if(!ReleaseChannel.isEmpty()) theConf->SetValue("Options/ReleaseChannel", ReleaseChannel);

	theConf->SetValue("Options/OnNewUpdate", ui.cmbUpdate->currentData());
	theConf->SetValue("Options/OnNewRelease", ui.cmbRelease->currentData());

	theConf->SetValue("Options/UpdateTweaks", CSettingsWindow__Chk2Int(ui.chkUpdateTweaks->checkState()));
	//

	emit OptionsChanged(m_bRebuildUI);
}

void CSettingsWindow::OnSelectUiFont()
{
	bool ok;
	auto newFont = QFontDialog::getFont(&ok, QApplication::font(), this);
	if (!ok) return;
	ui.lblUiFont->setText(newFont.family());
	OnChangeGUI();
}

void CSettingsWindow::OnResetUiFont()
{
	QFont defaultFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
	ui.lblUiFont->setText(defaultFont.family());
	OnChangeGUI();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support
//

SCertInfo g_CertInfo = {0};
QString g_CertName;
QString g_SystemHwid;
QByteArray g_Certificate;

void CSettingsWindow::InitSupport()
{
	//connect(ui.lblSupport, SIGNAL(linkActivated(const QString&)), theGUI, SLOT(OpenUrl(const QString&)));
	connect(ui.lblSupportCert, SIGNAL(linkActivated(const QString&)), theGUI, SLOT(OpenUrl(const QString&)));
	connect(ui.lblCert, SIGNAL(linkActivated(const QString&)), theGUI, SLOT(OpenUrl(const QString&)));
	connect(ui.lblCertExp, SIGNAL(linkActivated(const QString&)), theGUI, SLOT(OpenUrl(const QString&)));
	connect(ui.lblCertGuide, SIGNAL(linkActivated(const QString&)), theGUI, SLOT(OpenUrl(const QString&)));

	connect(ui.txtCertificate, SIGNAL(textChanged()), this, SLOT(CertChanged()));
	ui.txtCertificate->installEventFilter(this);
	connect(ui.txtSerial, SIGNAL(textChanged(const QString&)), this, SLOT(KeyChanged()));
	ui.btnGetCert->setEnabled(false);
	connect(theGUI, SIGNAL(CertUpdated()), this, SLOT(UpdateCert()));

	ui.txtCertificate->setPlaceholderText(
		"NAME: User Name\n"
		"DATE: dd.mm.yyyy\n"
		"TYPE: ULTIMATE\n"
		"UPDATEKEY: 00000000000000000000000000000000\n"
		"SIGNATURE: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="
	);

	if (!g_SystemHwid.isEmpty()) {
		QString clickToR = tr("Click to reveal");
		QString clickToH = tr("Click to hide");

		ui.lblHwId->setText(tr("HwId: <a href=\"show\">[%1]</a>").arg(clickToR));
		ui.lblHwId->setToolTip(clickToR);

		connect(ui.lblHwId, &QLabel::linkActivated, this, [=](const QString& Link) {
			if (Link == "show") {
				ui.lblHwId->setText(tr("HwId: <a href=\"hide\" style=\"text-decoration:none; color:inherit;\">%1</a> <a href=\"copy\">(copy)</a>").arg(g_SystemHwid));
				ui.lblHwId->setToolTip(clickToH);
			}
			else if (Link == "hide") {
				ui.lblHwId->setText(tr("HwId: <a href=\"show\">[%1]</a>").arg(clickToR));
				ui.lblHwId->setToolTip(clickToR);
			}
			else if (Link == "copy") {
				QApplication::clipboard()->setText(g_SystemHwid);
			}
			});
	}

	ui.lblVersion->setText(tr("MajorPrivacy Version: %1").arg(theGUI->GetVersion()));

	connect(ui.lblEvalCert, SIGNAL(linkActivated(const QString&)), this, SLOT(OnStartEval()));

	connect(ui.btnGetCert, SIGNAL(clicked(bool)), this, SLOT(OnGetCert()));

	connect(ui.chkNoCheck, SIGNAL(stateChanged(int)), this, SLOT(OnOptChanged()));
}

void CSettingsWindow::CertChanged()
{ 
	m_CertChanged = true; 
	QPalette palette = QApplication::palette();
	ui.txtCertificate->setPalette(palette);
	ui.txtCertificate->setProperty("modified", true);
	OnOptChanged();
}

void CSettingsWindow::KeyChanged()
{
	ui.btnGetCert->setEnabled(ui.txtSerial->text().length() > 5);
}

bool CSettingsWindow::SetCertificate(const QByteArray& Certificate)
{
	g_Certificate = Certificate;
	STATUS Status = theCore->SetDatFile("Certificate.dat", Certificate);
	return Status;
}

void CSettingsWindow::LoadCertificate(QString CertPath)
{
	if (theCore->Driver()->IsConnected())
		CertPath = theCore->GetAppDir() + "\\Certificate.dat";

	QFile CertFile(CertPath);
	if (CertFile.open(QFile::ReadOnly)) {
		g_Certificate = CertFile.readAll();
		CertFile.close();
	}
}

void CSettingsWindow::UpdateCert()
{
	ui.lblCertExp->setVisible(false);
	ui.lblEvalCert->setVisible(g_Certificate.isEmpty());

	//ui.lblCertLevel->setVisible(!g_Certificate.isEmpty());
	if (!g_Certificate.isEmpty())
	{
		ui.txtCertificate->setProperty("hidden", true);
		int Pos = g_Certificate.indexOf("HWID:");
		if (Pos == -1)
			Pos = g_Certificate.indexOf("UPDATEKEY:");

		QByteArray truncatedCert = (g_Certificate.left(Pos) + "...");
		int namePos = truncatedCert.indexOf("NAME:");
		int datePos = truncatedCert.indexOf("DATE:");
		if (namePos != -1 && datePos != -1 && datePos > namePos)
			truncatedCert = truncatedCert.mid(0, namePos + 5) + " ...\n" + truncatedCert.mid(datePos);
		ui.txtCertificate->setPlainText(truncatedCert);
		//ui.lblSupport->setVisible(false);

		QString ReNewUrl = "https://xanasoft.com/go.php?to=priv-renew-cert";
		if (CERT_IS_TYPE(g_CertInfo, eCertPatreon))
			ReNewUrl = "https://xanasoft.com/get-supporter-certificate/";

		QPalette palette = QApplication::palette();
		if (CCustomTheme::IsDarkTheme())
			palette.setColor(QPalette::Text, Qt::black);
		if (g_CertInfo.expired) {
			palette.setColor(QPalette::Base, QColor(255, 255, 192));
			QString infoMsg = tr("This license has expired, please <a href=\"%1\">get new license</a>.").arg(ReNewUrl);
			if (g_CertInfo.active) {
				if (g_CertInfo.grace_period)
					infoMsg.append(tr("<br /><font color='red'>Plus features will be disabled in %1 days.</font>").arg((g_CertInfo.expirers_in_sec + 30*60*60*24) / (60*60*24)));
				else if (!g_CertInfo.outdated) // must be an expiren medium or large cert on an old build
					infoMsg.append(tr("<br /><font color='red'>For the current build Plus features remain enabled</font>, but you no longer have access updates."));
			} else
				infoMsg.append(tr("<br />Plus features are no longer enabled."));
			ui.lblCertExp->setText(infoMsg);
			ui.lblCertExp->setVisible(true);
		}
		else {
			if (g_CertInfo.expirers_in_sec > 0 && g_CertInfo.expirers_in_sec < (60 * 60 * 24 * 30)) {
				ui.lblCertExp->setText(tr("This license will <font color='red'>expire in %1 days</font>, please <a href=\"%2\">get new license</a>.").arg(g_CertInfo.expirers_in_sec / (60*60*24)).arg(ReNewUrl));
				ui.lblCertExp->setVisible(true);
			}
			/*#ifdef _DEBUG
			else {
			ui.lblCertExp->setText(tr("This license is valid, <a href=\"%1\">check for new license</a>.").arg(ReNewUrl));
			ui.lblCertExp->setVisible(true);
			}
			#endif*/
			palette.setColor(QPalette::Base, QColor(192, 255, 192));
		}
		ui.txtCertificate->setPalette(palette);

		//ui.lblCertLevel->setText(tr("Feature Level: %1").arg(GetCertLevel()));
		//
		//QStringList Infos;
		//Infos += tr("Type: %1").arg(GetCertType());
		//if (CERT_IS_INSIDER(g_CertInfo))
		//	Infos += tr("Insider release capable");
		//ui.lblCertLevel->setToolTip(Infos.join("\n"));

		//if (CERT_IS_TYPE(g_CertInfo, eCertBusiness)) {
		//	ScanForSeats();
		//	QTimer::singleShot(1000, this, [=]() {
		//		QString CntInfo = QString::number(CountSeats());
		//		QString Amount = GetArguments(g_Certificate, L'\n', L':').value("AMOUNT");
		//		if (!Amount.isEmpty())
		//			CntInfo += "/" + Amount;
		//		ui.lblCertCount->setText(CntInfo);
		//		ui.lblCertCount->setToolTip(tr("Count of certificates in use"));
		//	});
		//}

		QString ExpInfo;
		if(g_CertInfo.expirers_in_sec > 0)
			ExpInfo = tr("Expires in: %1 days").arg(g_CertInfo.expirers_in_sec / (60*60*24));
		else if(g_CertInfo.expirers_in_sec < 0)
			ExpInfo = tr("Expired: %1 days ago").arg(-g_CertInfo.expirers_in_sec / (60*60*24));
		if (CERT_IS_TYPE(g_CertInfo, eCertPatreon))
			ExpInfo += tr("; eligible Patreons can always <a href=\"https://xanasoft.com/get-supporter-certificate/\">obtain an updated certificate</a> from xanasoft.com");
		ui.lblCert->setText(ExpInfo);

		/*QStringList Options;
		if (g_CertInfo.opt_sec) Options.append("SBox");
		else Options.append(QString("<font color='gray'>SBox</font>"));
		if (g_CertInfo.opt_enc) Options.append("EBox");
		else Options.append(QString("<font color='gray'>EBox</font>"));
		if (g_CertInfo.opt_net) Options.append("NetI");
		else Options.append(QString("<font color='gray'>NetI</font>"));
		if (g_CertInfo.opt_desk) Options.append("Desk");
		else Options.append(QString("<font color='gray'>Desk</font>"));
		ui.lblCertOpt->setText(tr("Options: %1").arg(Options.join(", ")));*/
		ui.lblCertOpt->setVisible(false);

		/*QStringList OptionsEx;
		OptionsEx.append(tr("Security/Privacy Enhanced & App Boxes (SBox): %1").arg(g_CertInfo.opt_sec ? tr("Enabled") : tr("Disabled")));
		OptionsEx.append(tr("Encrypted Sandboxes (EBox): %1").arg(g_CertInfo.opt_enc ? tr("Enabled") : tr("Disabled")));
		OptionsEx.append(tr("Network Interception (NetI): %1").arg(g_CertInfo.opt_net ? tr("Enabled") : tr("Disabled")));
		OptionsEx.append(tr("Sandboxie Desktop (Desk): %1").arg(g_CertInfo.opt_desk ? tr("Enabled") : tr("Disabled")));
		ui.lblCertOpt->setToolTip(OptionsEx.join("\n"));*/
	}
	else
	{
		ui.lblCert->clear();
		ui.lblCertOpt->clear();

		int EvalCount = theConf->GetInt("User/EvalCount", 0);
		if(EvalCount >= EVAL_MAX)
			ui.lblEvalCert->setText(tr("<b>You have used %1/%2 evaluation licenses. No more free licenses can be generated.</b>").arg(EvalCount).arg(EVAL_MAX));
		else
			ui.lblEvalCert->setText(tr("<b><a href=\"_\">Get a free evaluation license</a> and enjoy all premium features for %1 days.</b>").arg(EVAL_DAYS));
		ui.lblEvalCert->setToolTip(tr("You can request a free %1-day evaluation license up to %2 times per hardware ID.").arg(EVAL_DAYS).arg(EVAL_MAX));
	}
}

void CSettingsWindow::OnGetCert()
{
	QByteArray Certificate;
	if (!ui.txtCertificate->property("hidden").toBool())
		Certificate = ui.txtCertificate->toPlainText().toUtf8();
	else
		Certificate = g_Certificate;
	QString Serial = ui.txtSerial->text();

	QString Message;

	if (Serial.length() < 4 || Serial.left(4).compare("PRIVS", Qt::CaseInsensitive) != 0) {
		Message = tr("This does not look like a MajorPrivacy Serial Number.<br />"
			"If you have attempted to enter the UpdateKey or the Signature from license certificate, "
			"that is not correct, please enter the entire license certificate into the text area above instead.");
	}
	else if(Certificate.isEmpty())
	{
		if (Serial.length() > 5 && Serial.at(4).toUpper() == 'U') {
			Message = tr("You are attempting to use a feature Upgrade-Key without having entered a pre-existing license. "
				"Please note that this type of key (<b>as it is clearly stated in bold on the website</b) requires you to have a pre-existing valid base license; it is useless without one."
				"<br />If you want to use the advanced features, you need to obtain both a base license and the feature upgrade key to unlock advanced functionality.");
		}

		else if (Serial.length() > 5 && Serial.at(4).toUpper() == 'R') {
			Message = tr("You are attempting to use a Renew-Key without having entered a pre-existing license. "
				"Please note that this type of key (<b>as it is clearly stated in bold on the website</b) requires you to have a pre-existing valid license; it is useless without one.");
		}

		if (!Message.isEmpty()) 
			Message += tr("<br /><br /><u>If you have not read the product description and obtained this key by mistake, please contact us via email (provided on our website) to resolve this issue.</u>");
	}

	if (!Message.isEmpty()) {
		CMajorPrivacy::ShowMessageBox(this, QMessageBox::Critical, Message);
		return;
	}

	QVariantMap Params;
	if(!Certificate.isEmpty())
		Params["key"] = GetArguments(Certificate, L'\n', L':').value("UPDATEKEY");

	PROGRESS Status = theGUI->m_pUpdater->GetSupportCert(Serial, this, SLOT(OnCertData(const QByteArray&, const QVariantMap&)), Params);
	if (Status.GetStatus() == OP_ASYNC) {
		theGUI->AddAsyncOp(Status.GetValue());
		Status.GetValue()->ShowMessage(tr("Retrieving license certificate..."));
	}
}

void CSettingsWindow::OnStartEval()
{
	StartEval(this, this, SLOT(OnCertData(const QByteArray&, const QVariantMap&)));
}

void CSettingsWindow::StartEval(QWidget* parent, QObject* receiver, const char* member)
{
	QString Name = theConf->GetString("User/Name", QString::fromLocal8Bit(qgetenv("USERNAME")));
	//#ifdef _DEBUG
	//	Name = QInputDialog::getText(parent, tr("MajorPrivacy - Get EVALUATION License"), tr("Please enter your Name"), QLineEdit::Normal, Name);
	//#endif

	QString eMail = QInputDialog::getText(parent, tr("MajorPrivacy - Get EVALUATION License"), tr("Please enter your email address to receive a free %1-day evaluation license, which will be issued to %2 and locked to the current hardware.\n"
		"You can request up to %3 evaluation licenses for each unique hardware ID.").arg(EVAL_DAYS).arg(Name).arg(EVAL_MAX), QLineEdit::Normal, theConf->GetString("User/eMail"));
	if (eMail.isEmpty()) return;
	theConf->SetValue("User/eMail", eMail);

	QVariantMap Params;
	Params["eMail"] = eMail;
	Params["Name"] = Name;

	PROGRESS Status = theGUI->m_pUpdater->GetSupportCert("", receiver, member, Params);
	if (Status.GetStatus() == OP_ASYNC) {
		theGUI->AddAsyncOp(Status.GetValue());
		Status.GetValue()->ShowMessage(tr("Retrieving License Certificate..."));
	}
}

void CSettingsWindow::OnCertData(const QByteArray& Certificate, const QVariantMap& Params)
{
	if (Certificate.isEmpty())
	{
		QString Error = Params["error"].toString();
		qDebug() << Error;
		if (Error == "max eval reached") {
			if (theConf->GetInt("User/EvalCount", 0) < EVAL_MAX) 
				theConf->SetValue("User/EvalCount", EVAL_MAX);
		}
		QString Message = tr("Error retrieving certificate: %1").arg(Error.isEmpty() ? tr("Unknown Error (probably a network issue)") : Error);
		CMajorPrivacy::ShowMessageBox(this, QMessageBox::Critical, Message);
		return;
	}
	ui.txtCertificate->setProperty("hidden", false);
	ui.txtCertificate->setPlainText(Certificate);
	ApplyCert();
}

void CSettingsWindow::ApplyCert()
{
	if (!theCore->Driver()->IsConnected())
		return;

	if (ui.txtCertificate->property("hidden").toBool())
		return;

	QByteArray Certificate = ui.txtCertificate->toPlainText().toUtf8();	
	if (g_Certificate != Certificate) {

		QPalette palette = QApplication::palette();

		if (CCustomTheme::IsDarkTheme())
			palette.setColor(QPalette::Text, Qt::black);

		ui.lblCertExp->setVisible(false);

		bool bRet = ApplyCertificate(Certificate, this);

		if (bRet && CERT_IS_TYPE(g_CertInfo, eCertEvaluation)) {
			int EvalCount = theConf->GetInt("User/EvalCount", 0);
			EvalCount++;
			theConf->SetValue("User/EvalCount", EvalCount);
		}

		if (CertRefreshRequired())
			TryRefreshCert(this, this, SLOT(OnCertData(const QByteArray&, const QVariantMap&)));

		if (Certificate.isEmpty())
			palette.setColor(QPalette::Base, Qt::white);
		else if (!bRet) 
			palette.setColor(QPalette::Base, QColor(255, 192, 192));
		else 
			palette.setColor(QPalette::Base, QColor(192, 255, 192));

		ui.txtCertificate->setPalette(palette);
	}

	m_CertChanged = false;
}

QString CSettingsWindow::GetCertType()
{
	QString CertType;
	if (g_CertInfo.type == eCertContributor)
		CertType = tr("Contributor");
	else if (CERT_IS_TYPE(g_CertInfo, eCertEternal))
		CertType = tr("Eternal");
	else if (g_CertInfo.type == eCertDeveloper)
		CertType = tr("Developer");
	else if (CERT_IS_TYPE(g_CertInfo, eCertBusiness))
		CertType = tr("Business");
	else if (CERT_IS_TYPE(g_CertInfo, eCertPersonal))
		CertType = tr("Personal");
	else if (g_CertInfo.type == eCertGreatPatreon)
		CertType = tr("Great Patreon");
	else if (CERT_IS_TYPE(g_CertInfo, eCertPatreon))
		CertType = tr("Patreon");
	else if (g_CertInfo.type == eCertFamily)
		CertType = tr("Family");
	else if (CERT_IS_TYPE(g_CertInfo, eCertHome))
		CertType = tr("Home");
	else if (CERT_IS_TYPE(g_CertInfo, eCertEvaluation))
		CertType = tr("Evaluation");
	else
		CertType = tr("Type %1").arg(g_CertInfo.type);
	return CertType;
}

QColor CSettingsWindow::GetCertColor()
{
	if (CERT_IS_TYPE(g_CertInfo, eCertEternal))
		return QColor(135, 0, 255, 255);
	else if (g_CertInfo.type == eCertDeveloper)
		return QColor(255, 215, 0, 255);
	else if (CERT_IS_TYPE(g_CertInfo, eCertBusiness))
		return QColor(211, 0, 0, 255);
	else if (CERT_IS_TYPE(g_CertInfo, eCertPersonal))
		return QColor(38, 127, 0, 255);
	else if (CERT_IS_TYPE(g_CertInfo, eCertPatreon))
		return QColor(38, 127, 0, 255);
	else if (g_CertInfo.type == eCertFamily)
		return QColor(0, 38, 255, 255);
	else if (CERT_IS_TYPE(g_CertInfo, eCertHome))
		return QColor(255, 106, 0, 255);
	else if (CERT_IS_TYPE(g_CertInfo, eCertEvaluation))
		return Qt::gray;
	else
		return Qt::black;
}

QString CSettingsWindow::GetCertLevel()
{
	QString CertLevel;
	if (g_CertInfo.level == eCertAdvanced)
		CertLevel = tr("Advanced");
	else if (g_CertInfo.level == eCertAdvanced1)
		CertLevel = tr("Advanced (L)");
	else if (g_CertInfo.level == eCertMaxLevel)
		CertLevel = tr("Max Level");
	else if (g_CertInfo.level != eCertStandard && g_CertInfo.level != eCertStandard2)
		CertLevel = tr("Level %1").arg(g_CertInfo.level);
	return CertLevel;
}


bool CSettingsWindow::ApplyCertificate(const QByteArray &Certificate, QWidget* widget)
{
	if (!Certificate.isEmpty()) 
	{
		auto Args = GetArguments(Certificate, L'\n', L':');

		bool bLooksOk = true;
		if (Args.value("NAME").isEmpty()) // mandatory
			bLooksOk = false;
		//if (Args.value("UPDATEKEY").isEmpty())
		//	bLooksOk = false;
		if (Args.value("SIGNATURE").isEmpty()) // absolutely mandatory
			bLooksOk = false;

		if (bLooksOk)
			SetCertificate(Certificate);
		else {
			QMessageBox::critical(widget, "MajorPrivacy", tr("This does not look like a license certificate. Please enter the entire data block, not just a portion of it."));
			return false;
		}
		g_Certificate = Certificate;
	}
	else
		SetCertificate("");

	if (Certificate.isEmpty())
		return false;

	STATUS Status = theGUI->ReloadCert(widget);

	if (!Status.IsError())
	{
		if (g_CertInfo.expired || g_CertInfo.outdated) {
			if(g_CertInfo.outdated)
				QMessageBox::information(widget, "MajorPrivacy", tr("This license is unfortunately not valid for the current build, you need to get a new license or downgrade to an earlier build."));
			else if(g_CertInfo.active && !g_CertInfo.grace_period)
				QMessageBox::information(widget, "MajorPrivacy", tr("Although this license has expired, for the currently installed version plus features remain enabled. However, you will no longer have access to updates."));
			else
				QMessageBox::information(widget, "MajorPrivacy", tr("This license has unfortunately expired, you need to get a new license."));
		}
		else {
			if(CERT_IS_TYPE(g_CertInfo, eCertEvaluation))
				QMessageBox::information(widget, "MajorPrivacy", tr("The evaluation license has been successfully applied. Enjoy your free trial!"));
			else
			{
				QString Message = tr("Thank you for supporting the development of MajorPrivacy.");
				if (g_CertInfo.type == eCertEntryPatreon)
					Message += tr("\nThis is a temporary Patreon certificate, valid for 3 months. "
						"Once it nears expiration, you can obtain a new certificate online that will be valid for the full term.");
				QMessageBox::information(widget, "MajorPrivacy", Message);
			}
		}
	}
	else
	{
		g_CertInfo.State = 0;
		if (Status.GetStatus() != 0xC000006EL /*STATUS_ACCOUNT_RESTRICTION*/)
			g_Certificate.clear();
	}

	theGUI->UpdateTitle();
	theGUI->UpdateLabel();
	return !Status.IsError();
}

bool CSettingsWindow::CertRefreshRequired()
{
	if (g_CertInfo.active) {
		if (COnlineUpdater::IsLockRequired() && g_CertInfo.type != eCertEternal && g_CertInfo.type != eCertContributor)
		{
			if(!g_CertInfo.locked || g_CertInfo.grace_period)
				return true;
		}
	} else {
		if (g_CertInfo.lock_req && !(g_CertInfo.expired || g_CertInfo.outdated))
			return true;
	}

	return false;
}

bool CSettingsWindow::TryRefreshCert(QWidget* parent, QObject* receiver, const char* member)
{
	if (theConf->GetInt("Options/AskCertRefresh", -1) != 1)
	{
		bool State = false;
		if(CCheckableMessageBox::question(parent, "MajorPrivacy", tr("A mandatory security update for your MajorPrivacy License Certificate is required. Would you like to download the updated certificate now?")
			, tr("Auto update in future"), &State, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes, QMessageBox::Information) != QDialogButtonBox::Yes)
			return false;

		if (State)
			theConf->SetValue("Options/AskCertRefresh", 1);
	}

	QVariantMap Params;
	Params["key"] = GetArguments(g_Certificate, L'\n', L':').value("UPDATEKEY");

	PROGRESS Status = theGUI->m_pUpdater->GetSupportCert("", receiver, member, Params);
	if (Status.GetStatus() == OP_ASYNC) {
		theGUI->AddAsyncOp(Status.GetValue());
		Status.GetValue()->ShowMessage(tr("Retrieving license certificate..."));
	}

	return true;
}

// Updater
void CSettingsWindow::UpdateUpdater()
{
	bool bOk = (g_CertInfo.active && !g_CertInfo.expired);
	//ui.radLive->setEnabled(false);
	if (!ui.chkAutoUpdate->isChecked()) 
	{
		ui.cmbInterval->setEnabled(false);
		ui.cmbUpdate->setEnabled(false);
		ui.cmbRelease->setEnabled(false);
		ui.lblRevision->setText(QString());
		ui.lblRelease->setText(QString());
	}
	else 
	{
		ui.cmbInterval->setEnabled(true);

		bool bAllowAuto;
		if (ui.radStable->isChecked() && !bOk) {
			ui.cmbUpdate->setEnabled(false);
			ui.cmbUpdate->setCurrentIndex(ui.cmbUpdate->findData("ignore"));

			ui.lblRevision->setText(tr("Valid License required for access"));
			bAllowAuto = false;
		} else {
			ui.cmbUpdate->setEnabled(true);

			ui.lblRevision->setText(QString());
			bAllowAuto = true;
		}

		ui.cmbRelease->setEnabled(true);
		QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui.cmbRelease->model());
		for (int i = 1; i < ui.cmbRelease->count(); i++) {
			QStandardItem* item = model->item(i);
			item->setFlags(bAllowAuto ? (item->flags() | Qt::ItemIsEnabled) : (item->flags() & ~Qt::ItemIsEnabled));
		}

		if(!bAllowAuto)
			ui.lblRelease->setText(tr("Valid Lciense required for automation"));
		else
			ui.lblRelease->setText(QString());
	}

	OnOptChanged();
}


void CSettingsWindow::GetUpdates()
{
	QVariantMap Params;
	Params["channel"] = "all";
	theGUI->m_pUpdater->GetUpdates(this, SLOT(OnUpdateData(const QVariantMap&, const QVariantMap&)), Params);
}

QString CSettingsWindow__MkVersion(const QString& Name, const QVariantMap& Releases)
{
	QVariantMap Release = Releases[Name].toMap();
	QString Version = Release.value("version").toString();
	//if (Release["build"].type() != QVariant::Invalid) 
	int iUpdate = Release["update"].toInt();
	if(iUpdate) Version += QChar('a' + (iUpdate - 1));
	return QString("<a href=\"%1\">%2</a>").arg(Name, Version);
}

void CSettingsWindow::OnUpdateData(const QVariantMap& Data, const QVariantMap& Params)
{
	if (Data.isEmpty() || Data["error"].toBool())
		return;

	m_UpdateData = Data;
	QVariantMap Releases = m_UpdateData["releases"].toMap();
	ui.lblCurrent->setText(tr("%1 (Current)").arg(theGUI->GetVersion(true)));
	ui.lblStable->setText(CSettingsWindow__MkVersion("stable", Releases));
	ui.lblPreview->setText(CSettingsWindow__MkVersion("preview", Releases));
}

void CSettingsWindow::OnUpdate(const QString& Channel)
{
	if (Channel == "check") {
		GetUpdates();
		return;
	}

	QVariantMap Releases = m_UpdateData["releases"].toMap();
	QVariantMap Release = Releases[Channel].toMap();

	QString VersionStr = Release["version"].toString();
	if (VersionStr.isEmpty())
		return;

	QVariantMap Installer = Releases["installer"].toMap();
	QString DownloadUrl = Installer["downloadUrl"].toString();
	//QString DownloadSig = Installer["signature"].toString();
	// todo xxx
	//if (!DownloadUrl.isEmpty() /*&& !DownloadSig.isEmpty()*/)
	//{
	//	// todo: signature
	//	if (QMessageBox("Sandboxie-Plus", tr("Do you want to download the installer for v%1?").arg(VersionStr), QMessageBox::Question, QMessageBox::Yes | QMessageBox::Default, QMessageBox::No | QMessageBox::Escape, QMessageBox::NoButton, this).exec() == QMessageBox::Yes)
	//		COnlineUpdater::Instance()->DownloadInstaller(DownloadUrl, true);
	//}
	//else
	{
		QString InfoUrl = Release["infoUrl"].toString();
		if (InfoUrl.isEmpty())
			InfoUrl = "https://sandboxie-plus.com/go.php?to=priv-get";
		QDesktopServices::openUrl(InfoUrl);
	}
}
//