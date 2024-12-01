#include "pch.h"
#include "TraceView.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include "..\..\MiscHelpers\Common\Common.h"
#include "../MiscHelpers/Common/CustomStyles.h"

CTraceView::CTraceView(CTraceModel* pModel, QWidget *parent)
	:CPanelView(parent)
{
	m_FullRefresh = true;

	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);
	this->setLayout(m_pMainLayout);

	m_pTreeView = new QTreeViewEx();
	m_pMainLayout->addWidget(m_pTreeView);

	m_pItemModel = pModel;
	//m_pItemModel->SetTree(false);

	QStyle* pStyle = QStyleFactory::create("windows");
	m_pTreeView->setStyle(pStyle);
	m_pTreeView->setItemDelegate(new CTreeItemDelegate());
	m_pTreeView->setMinimumHeight(50);

	m_pTreeView->setModel(m_pItemModel);

	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pTreeView, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	//m_pMainLayout->addWidget(CFinder::AddFinder(m_pTreeList, this, CFinder::eHighLightDefault, &m_pFinder));
	m_pMainLayout->addWidget(CFinder::AddFinder(m_pTreeView, this, CFinder::eHighLightDefault, &m_pFinder));
	m_pFinder->SetModel(m_pItemModel);

	m_pTreeView->setColumnReset(2);
	//connect(m_pTreeView, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	//connect(m_pTreeView, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	AddPanelItemsToMenu();

	connect(theCore, SIGNAL(CleanUpDone()), this, SLOT(OnCleanUpDone()));
}

CTraceView::~CTraceView()
{
}

void CTraceView::OnMenu(const QPoint& Point)
{
	CPanelView::OnMenu(Point);
}

void CTraceView::SetFilter(const QRegularExpression& RegExp, int iOptions, int Column)
{
	QString Pattern = RegExp.pattern();
	QString Exp;
	if (Pattern.length() > 4) {
		Exp = Pattern.mid(2, Pattern.length() - 4); // remove leading and tailing ".*" from the pattern
		// unescape, remove backslashes but not double backslashes
		for (int i = 0; i < Exp.length(); i++) {
			if (Exp.at(i) == '\\')
				Exp.remove(i, 1);
		}
	}
	bool bReset = m_bHighLight != ((iOptions & CFinder::eHighLight) != 0) || (!m_bHighLight && m_FilterExp != Exp);

	//QString ExpStr = ((iOptions & CFinder::eRegExp) == 0) ? Exp : (".*" + QRegularExpression::escape(Exp) + ".*");
	//QRegularExpression RegExp(ExpStr, (iOptions & CFinder::eCaseSens) != 0 ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
	//m_FilterExp = RegExp;
	m_FilterExp = Exp;
	m_bHighLight = (iOptions & CFinder::eHighLight) != 0;
	//m_FilterCol = Col;

	if (bReset)
		m_FullRefresh = true;
}

void CTraceView::Sync(ETraceLogs Log, const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services)
{
	MergeTraceLogs(&m_Log, Log, Programs, Services);

	if (m_FullRefresh || m_RecentLimit != theGUI->GetRecentLimit()) 
	{
		//quint64 start = GetCurCycle();
		m_pItemModel->Clear();
		//qDebug() << "Clear took" << (GetCurCycle() - start) / 1000000.0 << "s";

		m_RecentLimit = theGUI->GetRecentLimit();

		m_FullRefresh = false;
	}

	m_pItemModel->SetTextFilter(m_FilterExp, m_bHighLight);	

	quint64 uRecentLimit = m_RecentLimit ? QDateTime::currentMSecsSinceEpoch() - m_RecentLimit : 0;

	//quint64 start = GetCurCycle();
	QList<QModelIndex> NewBranches = m_pItemModel->Sync(m_Log.List, uRecentLimit);
	//qDebug() << "Sync took" << (GetCurCycle() - start) / 1000000.0 << "s";

	/*if (m_pItemModel->IsTree())
	{
		QTimer::singleShot(10, this, [this, NewBranches]() {
			quint64 start = GetCurCycle();
			foreach(const QModelIndex& Index, NewBranches)
				GetTree()->expand(Index);
			qDebug() << "Expand took" << (GetCurCycle() - start) / 1000000.0 << "s";
			});
	}

	if(m_pAutoScroll->isChecked())
		m_pTreeList->scrollToBottom();*/
}

void CTraceView::ClearTraceLog(ETraceLogs Log)
{
	if (QMessageBox::question(this, "MajorPrivacy", tr("Are you sure you want to clear the the trace logs for the current program items?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	auto Current = theGUI->GetCurrentItems();
	
	if (Current.bAllPrograms)
		theCore->ClearTraceLog(Log);
	else
	{
		foreach(CProgramFilePtr pProgram, Current.Programs)
			theCore->ClearTraceLog(Log, pProgram);

		foreach(CWindowsServicePtr pService, Current.ServicesEx | Current.ServicesIm)
			theCore->ClearTraceLog(Log, pService);
	}

	emit theCore->CleanUpDone();

	m_FullRefresh = true;
}

void CTraceView::OnCleanUpDone()
{
	m_FullRefresh = true;
}