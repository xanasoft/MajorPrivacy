#include "pch.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "HomePage.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../../Library/Helpers/NtUtil.h"
#include "../../Library/Helpers/EvtUtil.h"
#include "../../Library/Helpers/Scoped.h"
#include "../MiscHelpers/Common/CustomStyles.h"
#include "../MajorPrivacy.h"
#include "../Views/EventView.h"
#include "../Views/IssueView.h"
#include "../Views/StatusView.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Tweaks/TweakManager.h"


CHomePage::CHomePage(QWidget* parent)
	: QWidget(parent)
{
	m_pMainLayout = new QVBoxLayout(this);
	m_pMainLayout->setContentsMargins(0, 0, 0, 0);

	m_pVSplitter = new QSplitter(Qt::Vertical, this);
	m_pMainLayout->addWidget(m_pVSplitter);

	m_pHSplitter = new QSplitter(Qt::Horizontal, this);

	m_pStatusWidget = new QWidget();
	m_pStatusLayout = new QVBoxLayout(m_pStatusWidget);
	m_pStatusLayout->setContentsMargins(0,0,0,0);

	m_pStatusView = new CStatusView();

	/*auto* pScrollArea = new QScrollArea(this);
	pScrollArea->setWidget(m_pStatusView);
	pScrollArea->setWidgetResizable(true);
	pScrollArea->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
	pScrollArea->setFrameShape(QFrame::NoFrame);
	pScrollArea->setFrameShadow(QFrame::Plain);
	pScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	pScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	pScrollArea->setSizePolicy(
		QSizePolicy::Preferred,   // in width, still expand to fill
		QSizePolicy::Maximum      // in height, only as tall as contents
	);

	m_pStatusLayout->addWidget(pScrollArea);*/

	m_pStatusLayout->addWidget(m_pStatusView);

	m_pIssueView = new CIssueView();
	m_pStatusLayout->addWidget(m_pIssueView);

	m_pHSplitter->addWidget(m_pStatusWidget);

	m_pEventView = new CEventView();

	m_pHSplitter->addWidget(m_pEventView);

	m_pVSplitter->addWidget(m_pHSplitter);

	m_pEventWidget = new QWidget();
	m_pEventLayout = new QVBoxLayout(m_pEventWidget);
	m_pEventLayout->setContentsMargins(0, 0, 0, 0);
	m_pVSplitter->addWidget(m_pEventWidget);

	m_pEventToolBar = new QToolBar();
	m_pEventLayout->addWidget(m_pEventToolBar);	

	m_pBtnClear = new QToolButton();
	m_pBtnClear->setIcon(QIcon(":/Icons/Trash.png"));
	m_pBtnClear->setToolTip(tr("Clear System Event Log"));
	m_pBtnClear->setFixedHeight(22);
	connect(m_pBtnClear, SIGNAL(clicked()), this, SLOT(OnClearEventLog()));
	m_pEventToolBar->addWidget(m_pBtnClear);

	// Event Log
	m_pEventLog = new QTreeWidgetEx();
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pEventLog->setStyle(pStyle);
	m_pEventLog->setItemDelegate(new CTreeItemDelegate());
	//m_pEventLog->setHeaderLabels(tr("#|Symbol|Stack address|Frame address|Control address|Return address|Stack parameters|File info").split("|"));
	m_pEventLog->setHeaderLabels(tr("Time Stamp|Type|Event|Message").split("|"));
	m_pEventLog->setMinimumHeight(50);

	m_pEventLog->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pEventLog->setSortingEnabled(false);

	m_pEventLog->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pEventLog, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));
	// 

	m_pEventPanel = new CPanelWidgetX(m_pEventLog, this);
	m_pEventLayout->addWidget(m_pEventPanel);

	connect(theCore, SIGNAL(EnclavesChanged()), this, SLOT(OnChange()));
	connect(theCore, SIGNAL(ExecRulesChanged()), this, SLOT(OnChange()));
	connect(theCore, SIGNAL(ResRulesChanged()), this, SLOT(OnChange()));
	connect(theCore, SIGNAL(FwRulesChanged()), this, SLOT(OnChange()));
	connect(theCore, SIGNAL(DnsRulesChanged()), this, SLOT(OnChange()));
	connect(theCore->TweakManager(), SIGNAL(TweaksChanged()), this, SLOT(OnChange()));


	ReadServiceLog();

	QueryDriverLog();

	m_pEventListener = new CEventLogCallback([](PVOID pParam, EVT_HANDLE hEvent) {
		AddLogEventFunc((CHomePage*)pParam, hEvent);
	}, this);

	std::wstring XmlQuery = 
		L"<QueryList>"
		L"  <Query Id=\"0\" Path=\"" APP_NAME L"\">"
		L"    <Select Path=\"" APP_NAME L"\">*</Select>"
		L"  </Query>"
		L"  <Query Id=\"1\" Path=\"System\">"
		L"    <Select Path=\"System\">*[System[Provider[@Name='" API_DRIVER_NAME L"']]]</Select>"
		L"  </Query>"
		L"</QueryList>";

	m_pEventListener->Start(XmlQuery);

	m_pVSplitter->restoreState(theConf->GetBlob("HomePage/VSplitter"));
	m_pHSplitter->restoreState(theConf->GetBlob("HomePage/HSplitter"));
}

CHomePage::~CHomePage()
{
	theConf->SetBlob("HomePage/VSplitter", m_pVSplitter->saveState());
	theConf->SetBlob("HomePage/HSplitter", m_pHSplitter->saveState());

	m_pEventListener->Stop();
	delete m_pEventListener;
}

void CHomePage::OnChange()
{
	if (!m_RefreshPending) {
		m_RefreshPending = true;
		QTimer::singleShot(500, this, [&]() {
			m_RefreshPending = false;
			Refresh();
		});
	}
}

void CHomePage::Refresh()
{
	theCore->TweakManager()->GetRoot();
	m_pIssueView->Refresh();
	Update();
}

void CHomePage::Update()
{
	if (!isVisible())
		return;

	m_pStatusView->Update();
	m_pIssueView->Update();
	m_pEventView->Update();
}

void CHomePage::OnClearEventLog()
{
	if (QMessageBox::question(this, "MajorPrivacy", tr("Are you sure you want to clear the event log?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
		return;

	CEventLogger::ClearLog(APP_NAME);
	m_pEventLog->clear();

	theCore->Log()->LogEventLine(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_SVC_STATUS_MSG, L"Log Cleared");
}

void CHomePage::AddLogEventFunc(CHomePage* This, HANDLE hEvent)
{
	//auto test = CEventLogCallback::GetEventXml(hEvent);
/*
<Event
	xmlns='http://schemas.microsoft.com/win/2004/08/events/event'>
	<System>
		<Provider Name='MajorPrivacy'/>
		<EventID Qualifiers='0'>222</EventID>
		<Version>0</Version>
		<Level>4</Level>
		<Task>111</Task>
		<Opcode>0</Opcode>
		<Keywords>0x80000000000000</Keywords>
		<TimeCreated SystemTime='2023-09-25T19:29:21.5293474Z'/>
		<EventRecordID>28</EventRecordID>
		<Correlation/>
		<Execution ProcessID='0' ThreadID='0'/>
		<Channel>MajorPrivacy</Channel>
		<Computer>dev2019</Computer>
		<Security/>
	</System>
	<EventData>
		<Data>I'm in an event log BAM!!!!</Data>
		<Data>Test 123</Data>
		<Binary>74657374203132333400</Binary>
	</EventData>
</Event>
*/
	SLogData Data = ReadLogData(hEvent);

	QMetaObject::invokeMethod(This, "AddLogEvent", Qt::QueuedConnection, 
		Q_ARG(QString, Data.Source),
		Q_ARG(QDateTime, Data.TimeStamp),
		Q_ARG(int, Data.Type), 
		Q_ARG(int, Data.Class), 
		Q_ARG(int, Data.Event), 
		Q_ARG(QString, Data.Message.join(" ")));
}

void CHomePage::AddLogEvent(const QString& Source, const QDateTime& TimeStamp, int Type, int Class, int Event, const QString& Message)
{
	m_pEventLog->insertTopLevelItem(0, MakeLogItem(Source, TimeStamp, Type, Class, Event, Message));

	QSystemTrayIcon::MessageIcon Icon = QSystemTrayIcon::NoIcon;
	switch (Type) {
	case EVENTLOG_SUCCESS:			Icon = QSystemTrayIcon::Information; break;
	case EVENTLOG_ERROR_TYPE:		Icon = QSystemTrayIcon::Critical; break;
	case EVENTLOG_WARNING_TYPE:		Icon = QSystemTrayIcon::Warning; break;
	case EVENTLOG_INFORMATION_TYPE: Icon = QSystemTrayIcon::Information; break;
	}

	if (Icon == QSystemTrayIcon::Critical || Icon == QSystemTrayIcon::Warning)
		theGUI->ShowNotification(Source, Message, Icon);
}

QTreeWidgetItem* CHomePage::MakeLogItem(const QString& Source, const QDateTime& TimeStamp, int Type, int Class, int Event, const QString& Message)
{
	QTreeWidgetItem* pItem = new QTreeWidgetItem();

	QString sType;
	switch (Type) {
	case EVENTLOG_SUCCESS: sType = tr("Success"); break;
	case EVENTLOG_ERROR_TYPE: sType = tr("Error"); break;
	case EVENTLOG_WARNING_TYPE: sType = tr("Warning"); break;
	case EVENTLOG_INFORMATION_TYPE: sType = tr("Information"); break;
	case EVENTLOG_AUDIT_SUCCESS: sType = tr("Audit Success"); break;
	case EVENTLOG_AUDIT_FAILURE: sType = tr("Audit Failure"); break;
	}

	pItem->setText(eTimeStamp, TimeStamp.toString("dd.MM.yyyy hh:mm:ss"));
	pItem->setData(eTimeStamp, Qt::UserRole, TimeStamp.toMSecsSinceEpoch());
	pItem->setText(eType, sType);
	//pItem->setText(eClsss, QString::number(Class));
	pItem->setText(eEvent, QString::number(Event));
	pItem->setText(eMessage, Message);


	if (Source.toStdWString() == API_DRIVER_NAME) {
		for(int i = 0; i < pItem->columnCount(); i++)
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
			pItem->setBackgroundColor(i, QColor(255, 0, 0, 128));
#else
			pItem->setBackground(i, QBrush(QColor(255, 0, 0, 128)));
#endif
	}

	return pItem;
}

void CHomePage::ReadServiceLog()
{
	QList<QTreeWidgetItem*> Items;

	QByteArray buff;
	buff.resize(8 * 1024);
	CScopedHandle hEventLog(OpenEventLogW(NULL, APP_NAME), CloseEventLog);
	if (!hEventLog) return;
	for (;;)
	{
		ULONG bytesRead, bytesNeeded;
		if (!ReadEventLogW(hEventLog, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_FORWARDS_READ, 0, buff.data(), buff.size(), &bytesRead, &bytesNeeded))
			break;

		EVENTLOGRECORD* rec = (EVENTLOGRECORD*)buff.data();
		while (bytesRead > 0)
		{
			//rec->DataLength;
			//rec->DataOffset;

			//WCHAR *source = (WCHAR *) ((UCHAR *)rec + sizeof(EVENTLOGRECORD));			

			QStringList Message;
			WCHAR* str = (WCHAR*)((UCHAR*)rec + rec->StringOffset);
			for (int i = 0; i < rec->NumStrings; i++) {
				size_t len = wcslen(str);
				Message.append(QString::fromWCharArray(str, len));
				str += len + 1;
			}

			QTreeWidgetItem* pItem = MakeLogItem(QString::fromWCharArray(APP_NAME),
				QDateTime::fromSecsSinceEpoch(rec->TimeGenerated), 
				rec->EventType, 
				rec->EventCategory, 
				rec->EventID, 
				Message.join(" "));

			Items.prepend(pItem);

			bytesRead -= rec->Length;
			rec = (EVENTLOGRECORD*)((UCHAR*)rec + rec->Length);
		}
	}

	m_pEventLog->insertTopLevelItems(0, Items);
}

void CHomePage::QueryDriverLog()
{
	std::wstring XmlQuery = 
		L"<QueryList>"
		L"  <Query Id=\"0\" Path=\"System\">"
		L"    <Select Path=\"System\">*[System[Provider[@Name='" API_DRIVER_NAME L"']]]</Select>"
		L"  </Query>"
		L"</QueryList>";

	int InsertIndex = m_pEventLog->topLevelItemCount();

    EVT_HANDLE hQuery = CEventLogCallback::EvtQuery(NULL, NULL, XmlQuery.c_str(), EvtQueryChannelPath);
	if (hQuery) 
	{
		for(;;)
		{
			EVT_HANDLE hEvent[100];

			DWORD count = 0;
			if (!CEventLogCallback::EvtNext(hQuery, ARRAYSIZE(hEvent), hEvent, 0, 0, &count)) 
			{
				DWORD status = GetLastError();
				if (status == ERROR_NO_MORE_ITEMS)
					break;
				//else {
				//	// error
				//	break;
				//}
			}

			for (DWORD i = 0; i < count; i++)
			{
				//auto test = CEventLogCallback::GetEventXml(hEvent[i]);
				

				SLogData Data = ReadLogData(hEvent[i]);

				QTreeWidgetItem* pItem = MakeLogItem(Data.Source, Data.TimeStamp, Data.Type, Data.Class, Data.Event, Data.Message.join(" "));

				// find right place to insert item
				quint64 uTime = Data.TimeStamp.toMSecsSinceEpoch();
				while (InsertIndex > 0) {
					quint64 uCurTime = m_pEventLog->topLevelItem(InsertIndex - 1)->data(0, Qt::UserRole).toLongLong();
					if (uCurTime > uTime)
						break;
					InsertIndex--;
				}

				m_pEventLog->insertTopLevelItem(InsertIndex, pItem);

				CEventLogCallback::EvtClose(hEvent[i]);
			}
		}

		CEventLogCallback::EvtClose(hQuery);
	}
}

CHomePage::SLogData CHomePage::ReadLogData(HANDLE hEvent)
{
	static PCWSTR eventProperties[] = {
		L"Event/System/Provider/@Name",			// 0

		L"Event/System/Level",					// 1	// wType
		L"Event/System/Task",					// 2	// wCategory
		L"Event/System/EventID",				// 3	// dwEventID

		L"Event/EventData/Data",				// 4	// lpStrings
		L"Event/EventData/Binary",				// 5	// lpRawData

		L"Event/System/TimeCreated/@SystemTime",// 6
	};

	std::vector<BYTE> data = CEventLogCallback::GetEventData(hEvent, ARRAYSIZE(eventProperties), eventProperties);

	PEVT_VARIANT values = (PEVT_VARIANT)data.data();

	PEVT_VARIANT vSource = &values[0];	// EvtVarTypeString

	PEVT_VARIANT vType  = &values[1];	// EvtVarTypeByte
	PEVT_VARIANT vClass = &values[2];	// EvtVarTypeUInt16
	PEVT_VARIANT vEvent = &values[3];	// EvtVarTypeUInt16

	PEVT_VARIANT vText  = &values[4];	// EvtVarTypeString | EVT_VARIANT_TYPE_ARRAY	

	PEVT_VARIANT uTime  = &values[6]; // EvtVarTypeFileTime

	SLogData Data;

	Data.Source = QString::fromStdWString(vSource->StringVal);

	Data.Type = CEventLogListener::GetWinVariantNumber<int>(*vType, 0); 
	Data.Class = CEventLogListener::GetWinVariantNumber<int>(*vClass, 0);
	Data.Event = CEventLogListener::GetWinVariantNumber<int>(*vEvent, 0);

	if (vText->Type & EVT_VARIANT_TYPE_ARRAY) {
		for (DWORD i = 0; i < vText->Count; i++)
			Data.Message.append(QString::fromStdWString(vText->StringArr[i]));
	} else
		Data.Message.append(QString::fromStdWString(values[4].StringVal));

	if(Data.Source.toStdWString() == API_DRIVER_NAME)
		Data.Message.removeAll(QString());

	Data.TimeStamp = QDateTime::fromMSecsSinceEpoch(FILETIME2ms(CEventLogListener::GetWinVariantNumber<quint64>(*uTime, 0)));

	return Data;
}