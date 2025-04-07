#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"
#include "../Views/DnsCacheView.h"
#include "../Views/DnsRuleView.h"

class CDnsPage : public QWidget
{
	Q_OBJECT
public:
	CDnsPage(QWidget* parent);
	~CDnsPage();

	void LoadState();
	void SetMergePanels(bool bMerge);

	void	Update();

private:

	QVBoxLayout*			m_pMainLayout;

	//QToolBar*				m_pToolBar;	
	QSplitter*				m_pVSplitter;

	QTabWidget*				m_pRuleTabs;
	CDnsRuleView*			m_pRuleView;

	QTabWidget*				m_pTabs;
	CDnsCacheView*			m_pDnsCacheView;
};

