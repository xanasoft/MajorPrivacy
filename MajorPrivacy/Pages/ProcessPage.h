#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeviewEx.h"

class CProcessView;
class CProgramRuleView;
class CLibraryView;
class CExecutionView;
class CIngressView;
class CProcessTraceView;

class CProcessPage : public QWidget
{
	Q_OBJECT
public:
	CProcessPage(QWidget* parent);
	~CProcessPage();

	void LoadState();
	void SetMergePanels(bool bMerge);

	void	Update();

private slots:

private:

	QVBoxLayout*			m_pMainLayout;

	//QToolBar*				m_pToolBar;

	QSplitter*				m_pVSplitter;
	CProgramRuleView*		m_pRuleView;
	QTabWidget*				m_pTabs;
	CProcessView*			m_pProcessView;
	CLibraryView*			m_pLibraryView;
	CExecutionView*			m_pExecutionView;
	CIngressView*			m_pIngressView;
	CProcessTraceView*		m_pTraceView;
};

