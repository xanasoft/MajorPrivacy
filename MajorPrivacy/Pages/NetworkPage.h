#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"

class CFwRuleView;
class CSocketView;
class CNetTraceView;
class CTrafficView;

class CNetworkPage : public QWidget
{
	Q_OBJECT
public:
	CNetworkPage(QWidget* parent);
	~CNetworkPage();

	void LoadState();
	void SetMergePanels(bool bMerge);

	void	Update();
	void	Clear();

private slots:

private:

	QVBoxLayout*			m_pMainLayout;

	//QToolBar*				m_pToolBar;	
	QSplitter*				m_pVSplitter;
	
	QTabWidget*				m_pRuleTabs;
	CFwRuleView*			m_pRuleView;
	//QWidget*				m_pProxyView;

	QTabWidget*				m_pTabs;
	CSocketView*			m_pSocketView;
	CNetTraceView*			m_pTraceView;
	CTrafficView*			m_pTrafficView;
};

