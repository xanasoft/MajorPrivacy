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

	void					LoadLog(const std::wstring& XmlQuery);
	static void				AddLogEventFunc(CHomePage* This, HANDLE hEvent);

	class CEventLogCallback*m_pEventListener;

private slots:
	void					AddLogEvent(const QDateTime& TimeStamp, int Type, int Class, int Event, const QString& Message);

private:

	QVBoxLayout*			m_pMainLayout;

	CFinder*				m_pFinder;
	QTreeWidgetEx*			m_pEventLog;
};

