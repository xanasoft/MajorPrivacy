#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"

class CStatusView;
class CIssueView;
class CEventView;

class CHomePage : public QWidget
{
	Q_OBJECT
public:
	CHomePage(QWidget* parent);
	~CHomePage();

	void 				Refresh();
	void 				Update();

private slots:

	void				OnChange();

protected:

	void					ReadServiceLog();
	void					QueryDriverLog();
	static void				AddLogEventFunc(CHomePage* This, HANDLE hEvent);

	struct SLogData
	{
		QString Source;

		int Type;
		int Class;
		int Event;

		QStringList Message;

		QDateTime TimeStamp;
	};

	static SLogData ReadLogData(HANDLE hEvent);

	class CEventLogCallback*m_pEventListener;

private slots:
	void					OnClearEventLog();	

	void					AddLogEvent(const QString& Source, const QDateTime& TimeStamp, int Type, int Class, int Event, const QString& Message);

protected:
	bool					m_RefreshPending = false;

private:

	enum EColumns
	{
		eTimeStamp = 0,
		eType,
		//eClass,
		eEvent,
		eMessage,
		eCount
	};

	QTreeWidgetItem*		MakeLogItem(const QString& Source, const QDateTime& TimeStamp, int Type, int Class, int Event, const QString& Message);

	QVBoxLayout*			m_pMainLayout;

	QSplitter*				m_pVSplitter;

	//QSplitter*				m_pHSplitter;

	QWidget*				m_pStatusWidget;
	QVBoxLayout*			m_pStatusLayout;

	CStatusView*			m_pStatusView;
	CIssueView*				m_pIssueView;
	CEventView*				m_pEventView;

	QTabWidget*				m_pTabWidget;

	QWidget*				m_pEventWidget;
	QVBoxLayout*			m_pEventLayout;
	QToolBar*				m_pEventToolBar;
	QToolButton*			m_pBtnClear;

	QTreeWidgetEx*			m_pEventLog;
	CPanelWidgetX*			m_pEventPanel;
};

