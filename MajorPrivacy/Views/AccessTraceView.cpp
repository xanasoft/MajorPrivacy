#include "pch.h"
#include "AccessTraceView.h"
#include "../Core/PrivacyCore.h"

CAccessTraceView::CAccessTraceView(QWidget *parent)
	:CTraceView(new CAccessTraceModel(), parent)
{
	QByteArray Columns = theConf->GetBlob("MainWindow/AccessTraceView_Columns");
	if (Columns.isEmpty()) {
		m_pTreeView->setColumnWidth(0, 300);
	} else
		m_pTreeView->restoreState(Columns);

	m_pMainLayout->setSpacing(1);

	m_pToolBar = new QToolBar();
	m_pMainLayout->insertWidget(0, m_pToolBar);

	m_pCmbAction = new QComboBox();
	m_pCmbAction->addItem(QIcon(":/Icons/NoAccess.png"), tr("Any Action"), (qint32)EEventStatus::eUndefined);
	m_pCmbAction->addItem(QIcon(":/Icons/Go.png"), tr("Allowed"), (qint32)EEventStatus::eAllowed);
	m_pCmbAction->addItem(QIcon(":/Icons/Disable.png"), tr("Blocked"), (qint32)EEventStatus::eBlocked);
	connect(m_pCmbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbAction);

	m_pToolBar->addSeparator();

	m_pBtnScroll = new QToolButton();
	m_pBtnScroll->setIcon(QIcon(":/Icons/Scroll.png"));
	m_pBtnScroll->setCheckable(true);
	m_pBtnScroll->setToolTip(tr("Auto Scroll"));
	m_pBtnScroll->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnScroll);

	m_pToolBar->addSeparator();

	m_pBtnClear = new QToolButton();
	m_pBtnClear->setIcon(QIcon(":/Icons/Trash.png"));
	m_pBtnClear->setToolTip(tr("Clear Trace Log"));
	m_pBtnClear->setFixedHeight(22);
	connect(m_pBtnClear, SIGNAL(clicked()), this, SLOT(OnClearTraceLog()));
	m_pToolBar->addWidget(m_pBtnClear);

	QWidget* pSpacer = new QWidget();
	pSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_pToolBar->addWidget(pSpacer);

	QAbstractButton* pBtnSearch = m_pFinder->GetToggleButton();
	pBtnSearch->setIcon(QIcon(":/Icons/Search.png"));
	pBtnSearch->setMaximumHeight(22);
	m_pToolBar->addWidget(pBtnSearch);
}

CAccessTraceView::~CAccessTraceView()
{
	theConf->SetBlob("MainWindow/AccessTraceView_Columns", m_pTreeView->saveState());
}

void CAccessTraceView::Sync(ETraceLogs Log, const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, const QString& RootPath)
{
	CAccessTraceModel* pModel = (CAccessTraceModel*)m_pItemModel;

	pModel->SetPathFilter(RootPath);

	CTraceView::Sync(Log, Programs, Services);

	if(m_pBtnScroll->isChecked())
		m_pTreeView->scrollToBottom();
}

void CAccessTraceView::UpdateFilter()
{
	((CAccessTraceModel*)m_pItemModel)->SetFilter((EEventStatus)m_pCmbAction->currentData().toInt());
	m_FullRefresh = true;
}

void CAccessTraceView::OnClearTraceLog()
{
	ClearTraceLog(ETraceLogs::eResLog);
}