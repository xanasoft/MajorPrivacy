#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeviewEx.h"

class CProcessView;

class CProcessPage : public QWidget
{
	Q_OBJECT
public:
	CProcessPage(QWidget* parent);
	~CProcessPage();

public slots:
	void	Update();

private slots:

private:

	QVBoxLayout*			m_pMainLayout;

	//QToolBar*				m_pToolBar;

	QSplitter*				m_pVSplitter;
	QTreeViewEx*			m_pRuleView;
	QTabWidget*				m_pTabs;
	CProcessView*			m_pProcessView;
	QTreeViewEx*			m_pTraceView;
};

