#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"
#include "../Core/TraceLogUtils.h"

class CFwRuleView;
class CSocketView;
class CNetTraceView;
class CTrafficView;

class CFirewallPage : public QWidget
{
	Q_OBJECT
public:
	CFirewallPage(QWidget* parent);
	~CFirewallPage();

public slots:
	void	Update();

private slots:

private:

	QVBoxLayout*			m_pMainLayout;

	//QToolBar*				m_pToolBar;	
	QSplitter*				m_pVSplitter;
	QTabWidget*				m_pTabs;
		
	CFwRuleView*			m_pRuleView;
	CSocketView*			m_pSocketView;
	CNetTraceView*			m_pTraceView;
	CTrafficView*			m_pTrafficView;

	SMergedLog				m_Log;
};

