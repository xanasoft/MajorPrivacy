#include "pch.h"
#include "EventView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/EventLog.h"
#include "../../Library/Helpers/NtUtil.h"
#include "TweakView.h"

CEventView::CEventView(QWidget *parent)
	:QWidget(parent)
{
	m_pMainLayout = new QGridLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pLabel = new QLabel(tr("Recent privacy relevant events:"));
	m_pMainLayout->addWidget(m_pLabel, 0, 0);



	m_pEventToolBar = new QToolBar();
	m_pMainLayout->addWidget(m_pEventToolBar, 1, 0);

	m_pBtnClear = new QToolButton();
	m_pBtnClear->setIcon(QIcon(":/Icons/Trash.png"));
	m_pBtnClear->setToolTip(tr("Clear Privacy Event Log"));
	//m_pBtnClear->setFixedHeight(22);
	connect(m_pBtnClear, SIGNAL(clicked()), this, SLOT(OnClearEventLog()));
	m_pEventToolBar->addWidget(m_pBtnClear);

	m_pInfo = new CPanelWidgetEx();
	//m_pInfo->GetView()->setItemDelegate(theGUI->GetItemDelegate());
	m_pInfo->GetTree()->setHeaderLabels(tr("Level|Time Stamp|Information").split("|"));
	m_pInfo->GetTree()->setIndentation(0);
	m_pInfo->GetTree()->setItemDelegate(new CTreeItemDelegate2());
	m_pInfo->GetTree()->setIconSize(QSize(32, 32));
	m_pMainLayout->addWidget(m_pInfo, 2, 0);


	QByteArray Columns = theConf->GetBlob("MainWindow/ProgramEventView_Columns");
	if (Columns.isEmpty()) {
		m_pInfo->GetTree()->setColumnWidth(0, 300);
	} else
		m_pInfo->GetTree()->header()->restoreState(Columns);
}

CEventView::~CEventView()
{
	theConf->SetBlob("MainWindow/ProgramEventView_Columns", m_pInfo->GetTree()->header()->saveState());
}

void CEventView::OnClearEventLog()
{
	if(!QMessageBox::question(this, tr("Clear Privacy Event Log"), tr("Do you really want to clear the Privacy Event Log?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
		return;

	theCore->ClearPrivacyLog();
}

void CEventView::Update()
{
	auto Entries = theCore->EventLog()->GetEntries();

	QMap<quint64, QTreeWidgetItem*> Old;
	for(int i = 0; i < m_pInfo->GetTree()->topLevelItemCount(); i++) {
		QTreeWidgetItem* pItem = m_pInfo->GetTree()->topLevelItem(i);
		quint64 Ref = pItem->data(eName, Qt::UserRole).toULongLong();
		Q_ASSERT(!Old.contains(Ref));
		Old.insert(Ref, pItem);
	}	

	foreach(const CEventLogEntryPtr& pEntry, Entries)
	{
		quint64 Ref = pEntry->GetUID();
		QTreeWidgetItem* pItem = Old.take(Ref);

		if (!pItem) {
			pItem = new QTreeWidgetItem();
			pItem->setData(eName, Qt::UserRole, Ref);
			pItem->setIcon(eLevel, CEventLog::GetEventLevelIcon(pEntry->GetEventLevel()));
			pItem->setText(eLevel, CEventLog::GetEventLevelStr(pEntry->GetEventLevel()));
			pItem->setText(eTimeStamp, QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss"));
			pItem->setText(eName, CEventLog::GetEventInfoStr(pEntry));
			m_pInfo->GetTree()->insertTopLevelItem(0, pItem);
		}
	}

	foreach(QTreeWidgetItem* pItem, Old)
		delete pItem;
}