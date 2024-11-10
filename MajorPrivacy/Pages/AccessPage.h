#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"
#include "../Core/TraceLogUtils.h"

class CAccessRuleView;
class CHandleView;
class CAccessView;
class CAccessInfoView;
class CAccessTraceView;

class CAccessPage : public QWidget
{
	Q_OBJECT
public:
	CAccessPage(QWidget* parent);

	void SetMergePanels(bool bMerge);

public slots:
	void	Update();

private:

	QVBoxLayout*			m_pMainLayout;

	//QToolBar*				m_pToolBar;

	QSplitter*				m_pVSplitter;

	//QTabWidget*				m_pRuleTabs;
	CAccessRuleView*		m_pRuleView;


	QTabWidget*				m_pTabs;
	CHandleView*			m_pHandleView;
	QSplitter*				m_pAccessSplitter;
	CAccessView*			m_pAccessView;
	CAccessInfoView*		m_pAccessInfoView;
	CAccessTraceView*		m_pTraceView;

	SMergedLog				m_Log;
};

