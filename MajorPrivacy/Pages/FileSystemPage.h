#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"

class CFileSystemPage : public QWidget
{
	Q_OBJECT
public:
	CFileSystemPage(QWidget* parent);

public slots:
	void	Update();

private:

	QVBoxLayout*			m_pMainLayout;

	//QToolBar*				m_pToolBar;

	QSplitter*				m_pVSplitter;
	QTreeViewEx*			m_pRuleView;
	QTabWidget*				m_pTabs;
	QTreeViewEx*			m_pActivityView;
	QTreeViewEx*			m_pTraceView;
	QTreeViewEx*			m_pAccessView;
};

