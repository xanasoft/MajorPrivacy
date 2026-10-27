#include "pch.h"
#include "FwTemplateWnd.h"
#include "../MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Core/GenericRule.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/Settings.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "RuleLifetime.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Helpers/PathPattern.h"
#include <QFileDialog>

CFwTemplateWnd::CFwTemplateWnd(const QString& Pattern, bool bAllow, EFwDirections Direction, const QStringList& Similar, QWidget* parent, int Lifetime)
	: QDialog(parent)
{
	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui.setupUi(this);

	setWindowTitle(tr("Firewall Rule Template"));

	AddColoredComboBoxEntry(ui.cmbAction, tr("Allow"), QColor(144, 238, 144), true);
	AddColoredComboBoxEntry(ui.cmbAction, tr("Block"), QColor(255, 182, 193), false);
	ColorComboBox(ui.cmbAction);
	SetComboBoxValue(ui.cmbAction, bAllow);

	// what was seen is what is proposed, a template decides no more than it was asked about unless told to
	ui.cmbDirection->addItem(QIcon(":/Icons/ArrowUp.png"), tr("Outbound"), (int)EFwDirections::Outbound);
	ui.cmbDirection->addItem(QIcon(":/Icons/ArrowDown.png"), tr("Inbound"), (int)EFwDirections::Inbound);
	ui.cmbDirection->addItem(QIcon(":/Icons/ArrowUpDown.png"), tr("Bidirectional"), (int)EFwDirections::Bidirectional);
	int DirIndex = ui.cmbDirection->findData((int)Direction);
	ui.cmbDirection->setCurrentIndex(DirIndex != -1 ? DirIndex : 0);
	connect(ui.cmbDirection, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateTakeOver()));

	// a template lasts as long as this says, and the rules made from it with it
	PopulateRuleLifetime(ui.cmbLifetime);
	int Index = ui.cmbLifetime->findData(Lifetime);
	ui.cmbLifetime->setCurrentIndex(Index != -1 ? Index : 0);
	ui.cmbLifetime->setToolTip(tr("The rules made from this template go when it does."));

	ui.cmbPath->setEditText(Pattern);
	ui.btnBrowse->setToolTip(tr("Browse for Folder"));
	QMenu* pBrowseMenu = new QMenu(ui.btnBrowse);
	pBrowseMenu->addAction(tr("Browse for File"), this, SLOT(BrowseFile()));
	ui.btnBrowse->setPopupMode(QToolButton::MenuButtonPopup);
	ui.btnBrowse->setMenu(pBrowseMenu);

	ui.lblHint->setText(tr("A rule is made for every program whose path matches this pattern, which is what keeps "
		"the next version of one from asking again. '*' stands for any part of one path element, "
		"'**' crosses path elements as well."));

	foreach(const QString& Path, Similar)
		ui.lstSimilar->addItem(Path);
	ui.grpSimilar->setVisible(!Similar.isEmpty());
	if (Similar.isEmpty()) // nothing it was compared against, so it is a guess and has to be read as one
		ui.lblHint->setText(ui.lblHint->text() + tr(" Nothing similar is known yet, so this pattern was read off the one path, "
			"with its version number wildcarded where one was found. Check it before saving."));

	//
	// Everything else a rule can say is left at what covers the common case, so the rest of the
	// editor is offered rather than imposed.
	//
	QPushButton* pFullEditor = ui.buttonBox->addButton(tr("More Settings..."), QDialogButtonBox::ActionRole);
	pFullEditor->setToolTip(tr("Carry on in the full rule editor, where ports, addresses, profiles and users can be set."));
	connect(pFullEditor, SIGNAL(clicked()), this, SLOT(OnFullEditor()));

	connect(ui.cmbPath, SIGNAL(editTextChanged(const QString&)), this, SLOT(OnPathChanged()));
	connect(ui.btnBrowse, SIGNAL(clicked()), this, SLOT(BrowseFolder()));
	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(OnSaveAndClose()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	OnPathChanged();

	restoreGeometry(theConf->GetBlob("FwTemplateWindow/Window_Geometry"));
}

CFwTemplateWnd::~CFwTemplateWnd()
{
	theConf->SetBlob("FwTemplateWindow/Window_Geometry", saveGeometry());
}

QString CFwTemplateWnd::GetPattern() const
{
	return ui.cmbPath->currentText().trimmed();
}

bool CFwTemplateWnd::IsAllow() const
{
	return GetComboBoxValue(ui.cmbAction).toBool();
}

EFwDirections CFwTemplateWnd::GetDirection() const
{
	return (EFwDirections)ui.cmbDirection->currentData().toInt();
}

int CFwTemplateWnd::GetLifetime() const
{
	return ui.cmbLifetime->currentData().toInt();
}

//
// What a template could have made itself: a rule for a binary by path, of its action, in a
// direction it covers. A rule for a service or an app package says more than a path pattern can
// carry, and following the template would lose that. The service holds to the same test.
//
bool CFwTemplateWnd::TakesOver(const CFwRulePtr& pRule, const QString& Pattern, EFwActions Action, EFwDirections Direction)
{
	if (pRule->IsTemplate() || pRule->IsBackup())
		return false;
	if (!pRule->GetServiceTag().isEmpty() || !pRule->GetAppPkgId().isEmpty() || !pRule->GetAppPFN().isEmpty())
		return false;
	if (pRule->GetAction() != Action)
		return false;
	if (Direction != EFwDirections::Bidirectional && pRule->GetDirection() != Direction)
		return false;
	QString Path = pRule->GetProgramID().GetFilePath();
	if (Path.isEmpty())
		Path = pRule->GetBinaryPath();
	return !Path.isEmpty() && MatchPathPattern(Pattern, Path);
}

void CFwTemplateWnd::SetTakeOver(const QList<CFwRulePtr>& Candidates)
{
	m_bTakeOver = true;
	m_TakeOverCandidates = Candidates;

	connect(ui.cmbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateTakeOver()));
	UpdateTakeOver();
}

void CFwTemplateWnd::UpdateTakeOver()
{
	if (!m_bTakeOver)
		return;

	EFwActions Action = IsAllow() ? EFwActions::Allow : EFwActions::Block;
	QString Pattern = GetPattern();

	ui.lstSimilar->clear();
	foreach(const CFwRulePtr& pRule, m_TakeOverCandidates) {
		if (!TakesOver(pRule, Pattern, Action, GetDirection()))
			continue;
		QString Path = pRule->GetProgramID().GetFilePath();
		if (Path.isEmpty())
			Path = pRule->GetBinaryPath();
		ui.lstSimilar->addItem(tr("%1 - %2, %3").arg(pRule->GetName()).arg(pRule->GetDirectionStr()).arg(Path));
	}

	ui.grpSimilar->setTitle(tr("Existing rules this template takes over (%1), they follow its settings from then on").arg(ui.lstSimilar->count()));
	ui.grpSimilar->setVisible(true);
}

CProgramItemPtr CFwTemplateWnd::GetPatternItem(const QString& Pattern, QWidget* pParent)
{
	if (Pattern.isEmpty())
		return CProgramItemPtr();

	CProgramID ID;
	ID.SetPath(Pattern);
	if (ID.GetType() != EProgramType::eFilePattern) {
		QMessageBox::warning(pParent, "MajorPrivacy", tr("'%1' can not be used as a rule template path.").arg(Pattern));
		return CProgramItemPtr();
	}

	CProgramPatternPtr pPattern = theCore->ProgramManager()->GetPattern(Pattern);
	if (!pPattern.isNull())
		return pPattern;

	//
	// Nobody has used this pattern yet. The item is deliberately not registered with the service
	// here, only the rule editor is being filled, and a cancelled prompt must not leave a program
	// entry behind. Saving the rule makes the service create the entry for its program id.
	//
	pPattern = CProgramPatternPtr(new CProgramPattern());
	pPattern->SetPattern(Pattern);
	pPattern->SetID(ID);
	pPattern->SetName(Pattern);
	return pPattern;
}

void CFwTemplateWnd::OnPathChanged()
{
	QString Pattern = GetPattern();

	//
	// A template without a wildcard is just a rule for one binary, which is not what this window
	// is for, but it is not wrong either, so it only has to be a path we can work with.
	//
	bool bValid = !Pattern.isEmpty() && CGenericRule::IsPathValid(Pattern);

	ui.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(bValid);

	UpdateTakeOver();

	QLineEdit* pEdit = ui.cmbPath->lineEdit();
	QPalette Palette = pEdit->palette();
	Palette.setColor(QPalette::Text, bValid ? QApplication::palette().color(QPalette::Text) : QColor(Qt::red));
	pEdit->setPalette(Palette);
}

void CFwTemplateWnd::BrowseFolder()
{
	QString Value = QFileDialog::getExistingDirectory(this, tr("Select Directory")).replace("/", "\\");
	if (Value.isEmpty())
		return;
	ui.cmbPath->setEditText(Value + "\\*");
}

void CFwTemplateWnd::BrowseFile()
{
	QString Value = QFileDialog::getOpenFileName(this, tr("Select File")).replace("/", "\\");
	if (Value.isEmpty())
		return;
	ui.cmbPath->setEditText(Value);
}

void CFwTemplateWnd::OnFullEditor()
{
	if (GetPattern().isEmpty() || !CGenericRule::IsPathValid(GetPattern())) {
		QApplication::beep();
		return;
	}

	// what was set here is carried over, the caller opens the editor on top of it
	m_bFullEditor = true;
	accept();
}

void CFwTemplateWnd::OnSaveAndClose()
{
	if (GetPattern().isEmpty() || !CGenericRule::IsPathValid(GetPattern())) {
		QApplication::beep();
		return;
	}

	accept();
}
