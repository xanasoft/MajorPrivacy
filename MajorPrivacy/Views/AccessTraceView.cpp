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
	m_pCmbAction->addItem(tr("Any Action"), (qint32)EEventStatus::eUndefined);
	m_pCmbAction->addItem(tr("Allowed"), (qint32)EEventStatus::eAllowed);
	m_pCmbAction->addItem(tr("Blocked"), (qint32)EEventStatus::eBlocked);
	connect(m_pCmbAction, SIGNAL(currentIndexChanged(int)), this, SLOT(UpdateFilter()));
	m_pToolBar->addWidget(m_pCmbAction);

	m_pBtnScroll = new QToolButton();
	m_pBtnScroll->setIcon(QIcon(":/Icons/Scroll.png"));
	m_pBtnScroll->setCheckable(true);
	m_pBtnScroll->setToolTip(tr("Auto Scroll"));
	m_pBtnScroll->setMaximumHeight(22);
	m_pToolBar->addWidget(m_pBtnScroll);

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

void CAccessTraceView::Sync(const struct SMergedLog* pLog)
{
	CTraceView::Sync(pLog);

	if(m_pBtnScroll->isChecked())
		m_pTreeView->scrollToBottom();
}

void CAccessTraceView::UpdateFilter()
{
	((CAccessTraceModel*)m_pItemModel)->SetFilter((EEventStatus)m_pCmbAction->currentData().toInt());
	m_FullRefresh = true;
}
