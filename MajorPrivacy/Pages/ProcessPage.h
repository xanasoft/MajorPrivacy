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
class CHashDBView;

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
	void	Clear();

private slots:

private:

	bool					m_bEmbedded = false;

	QVBoxLayout*			m_pMainLayout = nullptr;

	//QToolBar*				m_pToolBar = nullptr;

	QSplitter*				m_pVSplitter = nullptr;

	QTabWidget*				m_pRuleTabs = nullptr;
	CProgramRuleView*		m_pRuleView = nullptr;
	CHashDBView*			m_pHashDBView = nullptr;

	QTabWidget*				m_pTabs = nullptr;
	CProcessView*			m_pProcessView = nullptr;
	QSplitter*				m_pLibrarySplitter = nullptr;
	CLibraryView*			m_pLibraryView = nullptr;
	CLibraryInfoView*		m_pLibraryInfoView = nullptr;
	CExecutionView*			m_pExecutionView = nullptr;
	CIngressView*			m_pIngressView = nullptr;
	CProcessTraceView*		m_pTraceView = nullptr;
};

