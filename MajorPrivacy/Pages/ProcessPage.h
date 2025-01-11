#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeviewEx.h"

class CProcessView;
class CProgramRuleView;
class CLibraryView;
class CLibraryInfoView;
class CExecutionView;
class CIngressView;
class CProcessTraceView;

class CProcessPage : public QWidget
{
	Q_OBJECT
public:
	CProcessPage(bool bEmbedded, QWidget* parent);
	~CProcessPage();

	void LoadState();
	void SetMergePanels(bool bMerge);

	void	Update();
	void	Update(const class QFlexGuid& EnclaveGuid);

private slots:

private:

	bool					m_bEmbedded;

	QVBoxLayout*			m_pMainLayout;

	//QToolBar*				m_pToolBar;

	QSplitter*				m_pVSplitter;

	QTabWidget*				m_pRuleTabs;
	CProgramRuleView*		m_pRuleView;

	QTabWidget*				m_pTabs;
	CProcessView*			m_pProcessView;
	QSplitter*				m_pLibrarySplitter;
	CLibraryView*			m_pLibraryView;
	CLibraryInfoView*		m_pLibraryInfoView;
	CExecutionView*			m_pExecutionView;
	CIngressView*			m_pIngressView;
	CProcessTraceView*		m_pTraceView;
};

