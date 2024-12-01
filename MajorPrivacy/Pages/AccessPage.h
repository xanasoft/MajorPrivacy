#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"

class CAccessRuleView;
class CHandleView;
class CAccessView;
class CAccessListView;
class CAccessTraceView;

class CAccessPage : public QWidget
{
	Q_OBJECT
public:
	CAccessPage(QWidget* parent);
	~CAccessPage();

	void LoadState();
	void SetMergePanels(bool bMerge);

	void	Update();
	void	Update(const QString& VolumeRoot, const QString& VolumeImage);

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
	CAccessListView*		m_pAccessListView;
	CAccessTraceView*		m_pTraceView;
};

