#include "pch.h"
#include "FwGuardEntryWnd.h"
#include "../MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Core/GenericRule.h"
#include "../Helpers/PathPattern.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/Settings.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include <QFileDialog>

#define FW_AUTO_APPROVE_KEY "Service/FwAutoApprove"
#define FW_AUTO_REJECT_KEY  "Service/FwAutoReject"

static QStringList CFwGuardEntryWnd__LoadList(bool bApprove)
{
	QStringList List = theCore->GetConfigStr(bApprove ? FW_AUTO_APPROVE_KEY : FW_AUTO_REJECT_KEY).split("|");
	List.removeAll("");
	return List;
}

static STATUS CFwGuardEntryWnd__StoreList(bool bApprove, const QStringList& List)
{
	QString Key = bApprove ? FW_AUTO_APPROVE_KEY : FW_AUTO_REJECT_KEY;
	STATUS Status = theCore->SetConfig(Key, List.join("|"));
	// the change broadcast which drops the cached value is still on its way, a second entry added
	// right after this one would otherwise be built on top of the list as it was before
	theCore->ForgetConfig(Key);
	return Status;
}

// entries are stored with a leading '-' when they are present but switched off
static QString CFwGuardEntryWnd__EntryPath(const QString& Entry)
{
	return Entry.startsWith("-") ? Entry.mid(1) : Entry;
}

static bool CFwGuardEntryWnd__TakeEntry(QStringList& List, const QString& Pattern)
{
	bool bFound = false;
	for (int i = 0; i < List.count(); ) {
		if (CFwGuardEntryWnd__EntryPath(List[i]).compare(Pattern, Qt::CaseInsensitive) == 0) {
			List.removeAt(i);
			bFound = true;
		} else
			i++;
	}
	return bFound;
}

CFwGuardEntryWnd::CFwGuardEntryWnd(const QString& Pattern, bool bApprove, const QStringList& Similar, QWidget* parent)
	: QDialog(parent)
{
	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui.setupUi(this);

	setWindowTitle(tr("Firewall Guard Entry"));

	AddColoredComboBoxEntry(ui.cmbAction, tr("Auto Approve"), QColor(144, 238, 144), true);
	AddColoredComboBoxEntry(ui.cmbAction, tr("Auto Reject"), QColor(255, 182, 193), false);
	ColorComboBox(ui.cmbAction);
	SetComboBoxValue(ui.cmbAction, bApprove);

	ui.cmbPath->setEditText(Pattern);
	ui.btnBrowse->setToolTip(tr("Browse for Folder"));
	QMenu* pBrowseMenu = new QMenu(ui.btnBrowse);
	pBrowseMenu->addAction(tr("Browse for File"), this, SLOT(BrowseFile()));
	ui.btnBrowse->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnBrowse->setMenu(pBrowseMenu);

	ui.lblHint->setText(tr("Rule changes of programs whose path matches this pattern are handled without asking. "
		"'*' stands for any part of one path element, '**' crosses path elements as well."));

	foreach(const QString& Path, Similar)
		ui.lstSimilar->addItem(Path);
	ui.grpSimilar->setVisible(!Similar.isEmpty());
	if (Similar.isEmpty()) // nothing it was compared against, so it is a guess and has to be read as one
		ui.lblHint->setText(ui.lblHint->text() + tr(" Nothing similar is known yet, so this pattern was read off the one path, "
			"with its version number wildcarded where one was found. Check it before saving."));

	connect(ui.cmbPath, SIGNAL(editTextChanged(const QString&)), this, SLOT(OnPathChanged()));
	connect(ui.btnBrowse, SIGNAL(clicked()), this, SLOT(BrowseFolder()));
	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	OnPathChanged();

	restoreGeometry(theConf->GetBlob("FwGuardEntryWindow/Window_Geometry"));
}

CFwGuardEntryWnd::~CFwGuardEntryWnd()
{
	theConf->SetBlob("FwGuardEntryWindow/Window_Geometry", saveGeometry());
}

QString CFwGuardEntryWnd::GetPattern() const
{
	return ui.cmbPath->currentText().trimmed();
}

bool CFwGuardEntryWnd::IsApprove() const
{
	return GetComboBoxValue(ui.cmbAction).toBool();
}

void CFwGuardEntryWnd::OnPathChanged()
{
	QString Pattern = GetPattern();
	bool bValid = !Pattern.isEmpty() && CGenericRule::IsPathValid(Pattern);

	ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(bValid);

	QLineEdit* pEdit = ui.cmbPath->lineEdit();
	QPalette Palette = pEdit->palette();
	Palette.setColor(QPalette::Text, bValid ? QApplication::palette().color(QPalette::Text) : QColor(Qt::red));
	pEdit->setPalette(Palette);
}

void CFwGuardEntryWnd::BrowseFolder()
{
	QString Value = QFileDialog::getExistingDirectory(this, tr("Select Directory")).replace("/", "\\");
	if (Value.isEmpty())
		return;
	ui.cmbPath->setEditText(Value + "\\*");
}

void CFwGuardEntryWnd::BrowseFile()
{
	QString Value = QFileDialog::getOpenFileName(this, tr("Select File")).replace("/", "\\");
	if (Value.isEmpty())
		return;
	ui.cmbPath->setEditText(Value);
}

void CFwGuardEntryWnd::OnSaveAndClose()
{
	QString Pattern = GetPattern();
	if (Pattern.isEmpty() || !CGenericRule::IsPathValid(Pattern)) {
		QApplication::beep();
		return;
	}

	accept();
}

STATUS CFwGuardEntryWnd::StoreEntry(const QString& Pattern, bool bApprove)
{
	QString Clean = Pattern.trimmed();

	// the entries of both lists end up in one single match tree, so one pattern means one thing
	QStringList Other = CFwGuardEntryWnd__LoadList(!bApprove);
	if (CFwGuardEntryWnd__TakeEntry(Other, Clean)) {
		STATUS Status = CFwGuardEntryWnd__StoreList(!bApprove, Other);
		if (Status.IsError())
			return Status;
	}

	QStringList List = CFwGuardEntryWnd__LoadList(bApprove);
	CFwGuardEntryWnd__TakeEntry(List, Clean); // re-added, a switched off entry is to come back on
	List.append(Clean);

	return CFwGuardEntryWnd__StoreList(bApprove, List);
}

bool CFwGuardEntryWnd::IsCovered(const QString& Path, bool* pbApprove)
{
	if (Path.isEmpty())
		return false;

	for (int i = 0; i < 2; i++)
	{
		bool bApprove = (i == 0);
		foreach(const QString& Entry, CFwGuardEntryWnd__LoadList(bApprove))
		{
			if (Entry.startsWith("-"))
				continue; // the entry is configured but switched off
			if (!MatchPathPattern(Entry, Path))
				continue;
			if (pbApprove) *pbApprove = bApprove;
			return true;
		}
	}
	return false;
}
