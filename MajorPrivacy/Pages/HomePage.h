#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"

class CHomePage : public QWidget
{
	Q_OBJECT
public:
	CHomePage(QWidget* parent);
	~CHomePage();

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
	void					AddLogEvent(const QString& Source, const QDateTime& TimeStamp, int Type, int Class, int Event, const QString& Message);

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

	CFinder*				m_pFinder;
	QTreeWidgetEx*			m_pEventLog;
};

