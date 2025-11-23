#include "pch.h"
#include "ScriptWindow.h"
#include "../MajorPrivacy.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/OtherFunctions.h"
#include "../MiscHelpers/Common/Finder.h"
#include "../MiscHelpers/Common/CodeEdit.h"
#include "ProgramWnd.h"
#include "../Helpers/Highliter/qsourcehighliter.h"
#include "../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/checkablemessagebox.h"

CScriptWindow::CScriptWindow(const QFlexGuid& Guid, EItemType Type, QWidget* parent)
	: QDialog(parent)
{
	m_Guid = Guid; 
	m_Type = Type;

	setAttribute(Qt::WA_DeleteOnClose);

	Qt::WindowFlags flags = windowFlags();
	flags |= Qt::CustomizeWindowHint;
	//flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	//flags &= ~Qt::WindowMinMaxButtonsHint;
	//flags |= Qt::WindowMinimizeButtonHint;
	//flags &= ~Qt::WindowCloseButtonHint;
	flags &= ~Qt::WindowContextHelpButtonHint;
	//flags &= ~Qt::WindowSystemMenuHint;
	setWindowFlags(flags);

	//setWindowState(Qt::WindowActive);
	//SetForegroundWindow((HWND)QWidget::winId());

	ui.setupUi(this);
	this->setWindowTitle(tr("MajorPrivacy - Script Editor"));

	m_pToolBar = new QToolBar(this);
	m_pToolBar->addAction(QIcon(":/Icons/Entry.png"), tr("Load from File"), this, SLOT(OnImport()));
	m_pToolBar->addAction(QIcon(":/Icons/Exit.png"), tr("Save to File"), this, SLOT(OnExport()));
	m_pToolBar->addSeparator();
	m_pToolBar->addAction(QIcon(":/Icons/Run.png"), tr("Run onTest Function"), this, SLOT(OnTest()));
	m_pToolBar->addAction(QIcon(":/Icons/Clean.png"), tr("CleanUp Log"), this, SLOT(OnCleanUp()));

	ui.vScript->insertWidget(0, m_pToolBar);

	connect(ui.buttonBox, SIGNAL(accepted()), SLOT(OnSave()));
	connect(ui.buttonBox, SIGNAL(rejected()), SLOT(reject()));
	connect(ui.buttonBox->button(QDialogButtonBox::Apply), SIGNAL(clicked(bool)), this, SLOT(OnApply()));

	//set highlighter
	m_pCodeEdit = new CCodeEdit();
	QSourceHighlite::QSourceHighliter* highlighter = new QSourceHighlite::QSourceHighliter(m_pCodeEdit->GetDocument());
	highlighter->setCurrentLanguage(QSourceHighlite::QSourceHighliter::CodeJs);
	m_pCodeEdit->installEventFilter(this);
	ui.txtScript->parentWidget()->layout()->replaceWidget(ui.txtScript, m_pCodeEdit);
	delete ui.txtScript;
	ui.txtScript = nullptr;
	connect(m_pCodeEdit, SIGNAL(textChanged()), this, SLOT(OnScriptChanged()));

	m_pCodeEdit->setFocus();

	m_uTimerID = startTimer(250);

	ui.splitter->restoreState(theConf->GetBlob("ScriptWindow/Splitter"));
	restoreGeometry(theConf->GetBlob("ScriptWindow/Window_Geometry"));
}

CScriptWindow::~CScriptWindow()
{
	killTimer(m_uTimerID);

	theConf->SetBlob("ScriptWindow/Splitter", ui.splitter->saveState());
	theConf->SetBlob("ScriptWindow/Window_Geometry", saveGeometry());
}

void CScriptWindow::timerEvent(QTimerEvent* pEvent)
{
	if (pEvent->timerId() != m_uTimerID)
		return;

	UpdateLog();
}

void CScriptWindow::SetScript(const QString& Script) 
{ 
	m_pCodeEdit->SetCode(Script); 
	ui.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
}

QString CScriptWindow::GetScript() const 
{
	return m_pCodeEdit->GetCode(); 
}

void CScriptWindow::UpdateLog()
{
	auto Ret = theCore->GetScriptLog(m_Guid, m_Type, m_LastId);
	if (Ret.IsError())
		return;

	QtVariant& Entries = Ret.GetValue();

	QScrollBar *sb = ui.txtLog->verticalScrollBar();
	bool isAtBottom = (sb->value() == sb->maximum());

	/*QtVariantReader(Entries).ReadRawList([&](const FW::CVariant& vData) {
	const QtVariant& Entry = *(QtVariant*)&vData;

	});*/

	if(Entries.Count() == 0)
		return;

	if(m_LastId + 1 != ((const QtVariant&)Entries[(uint32)0]).Get(API_V_ID).To<uint32>(0))
		ui.txtLog->clear();

	for(int i = 0; i < Entries.Count(); i++)
	{
		const QtVariant& Entry = Entries[i];
		uint32 uId = Entry.Get(API_V_ID).To<uint32>(0);

		QString Time = QDateTime::fromMSecsSinceEpoch(FILETIME2ms(Entry.Get(API_V_TIME_STAMP).To<uint64>(0))).toString("dd.MM.yyyy hh:mm:ss.zzz");
		ELogLevels Type = (ELogLevels)Entry.Get(API_V_TYPE).To<int>(0);
		QString Message = Entry.Get(API_V_DATA).AsQStr();
		switch (Type) {
		case ELogLevels::eInfo: Message = QString("<span style=\"color:blue;\">%1</span>").arg(Message.toHtmlEscaped()); break;
		case ELogLevels::eSuccess: Message = QString("<span style=\"color:green;\">%1</span>").arg(Message.toHtmlEscaped()); break;
		case ELogLevels::eWarning: Message = QString("<span style=\"color:orange;\">%1</span>").arg(Message.toHtmlEscaped()); break;
		case ELogLevels::eError: Message = QString("<span style=\"color:red;\">%1</span>").arg(Message.toHtmlEscaped()); break;
		default: Message = Message.toHtmlEscaped(); break;
		}
		ui.txtLog->appendHtml(QString("<b>[%1]</b> %3").arg(Time, Message));
	}

	m_LastId = ((const QtVariant&)Entries[(size_t)(Entries.Count() - 1)]).Get(API_V_ID).To<uint32>(0);

	if (isAtBottom) {
		ui.txtLog->moveCursor(QTextCursor::End);
		ui.txtLog->ensureCursorVisible();
	}
}

void CScriptWindow::OnImport()
{
	QString FileName = QFileDialog::getOpenFileName(this, tr("Import Script"), "", tr("JavaScript Files (*.js);;All Files (*.*)"));
	if (FileName.isEmpty())
		return;
	QString Script = ReadFileAsString(FileName);
	if (Script.isNull()) {
		QMessageBox::critical(this, tr("Error"), tr("Failed to read file '%1'").arg(FileName));
		return;
	}
	m_pCodeEdit->SetCode(Script); 
}

void CScriptWindow::OnExport()
{
	QString FileName = QFileDialog::getSaveFileName(this, tr("Export Script"), "", tr("JavaScript Files (*.js);;All Files (*.*)"));
	if (FileName.isEmpty())
		return;
	if (!WriteStringToFile(FileName, m_pCodeEdit->GetCode())) {
		QMessageBox::critical(this, tr("Error"), tr("Failed to write file '%1'").arg(FileName));
		return;
	}
}

void CScriptWindow::OnTest()
{
	if(ui.buttonBox->button(QDialogButtonBox::Apply)->isEnabled()) 
	{
		bool bSave = false;

		int iSave = theConf->GetInt("Options/AskApplyScript", -1);
		if (iSave == -1) {
			bool State = false;
			int ret = CCheckableMessageBox::question(this, "MajorPrivacy", tr("You have unapplied changes. Do you want to apply them before the test?")
				, tr("Remember this choice."), &State, QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel, QDialogButtonBox::Yes, QMessageBox::Question);

			if (ret == QMessageBox::Cancel)
				return;

			bSave = ret == QMessageBox::Yes;

			if (State)
				theConf->SetValue("Options/AskApplyScript", bSave ? 1 : 0);
		}
		else
			bSave = iSave == 1;

		if(bSave)
			OnApply();
	}

	QTimer::singleShot(500, this, [&]() {
		theCore->CallScriptFunc(m_Guid, m_Type, "onTest");
		UpdateLog();
	});
}

void CScriptWindow::OnCleanUp()
{
	theCore->ClearScriptLog(m_Guid, m_Type);
	ui.txtLog->clear();
	m_LastId = 0;
}

void CScriptWindow::OnScriptChanged()
{
	ui.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
}

void CScriptWindow::OnSave()
{
	m_Save(GetScript(), false);
	accept();
}

void CScriptWindow::OnApply()
{
	STATUS Status = m_Save(GetScript(), true);
	if(theGUI->CheckResults(QList<STATUS>() << Status, this) == 0)
		ui.buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
} 