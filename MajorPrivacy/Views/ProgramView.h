#pragma once
#include <QWidget>
#include "../Models/ProgramModel.h"
#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/SplitTreeView.h"

//#define SPLIT_TREE

class CProgramView : public CPanelView
{
	Q_OBJECT
public:
	CProgramView(QWidget* parent = nullptr);
	~CProgramView();

	QList<CProgramItemPtr> GetCurrentProgs() const { return m_CurPrograms; }
	int GetFilter() const { return m_pProgramModel->GetFilter(); }
	bool TestFilter(EProgramType Type, const SProgramStats* pStats) { return m_pProgramModel->TestFilter(Type, pStats); }

	void	SetColumnSet(const QString& ColumnSet);

signals:
	void	ProgramsChanged(const QList<CProgramItemPtr>& Programs);

public slots:
	void	FilterUpdate();
	void	Update();

private slots:
	void	OnProgramChanged(const QModelIndexList& Selection);
	void	OnDoubleClicked(const QModelIndex& Index);

	void	OnAddProgram();
	void	OnProgramAction();
	void	OnAddToGroup();

	void	OnTypeFilter();
	void	OnRunFilter();
	void	OnRulesFilter();
	void	OnTrafficFilter();
	void	OnSocketFilter();

protected:
	friend class CMajorPrivacy;

	//virtual void				OnMenu(const QPoint& Point);
#ifdef SPLIT_TREE
	virtual QTreeView*			GetView() 				{ return m_pTreeList->GetView(); }
#else
	virtual QTreeView*			GetView() 				{ return m_pTreeList; }
#endif
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }

	QList<CProgramItemPtr>		m_CurPrograms;

	CFinder*					m_pFinder = nullptr;

private:
	virtual void				OnMenu(const QPoint& Point);

	QVBoxLayout*				m_pMainLayout = nullptr;

	QSplitter*					m_pInfoSplitter = nullptr;
	class CInfoView*			m_pInfoView = nullptr;

	QWidget*					m_pProgramWidget = nullptr;
	QVBoxLayout*				m_pProgramLayout = nullptr;
#ifdef SPLIT_TREE
	CSplitTreeView*				m_pTreeList = nullptr;
#else
	QTreeViewEx*				m_pTreeList = nullptr;
#endif
	CProgramModel*				m_pProgramModel = nullptr;
	QSortFilterProxyModel*		m_pSortProxy = nullptr;

	QToolBar*					m_pToolBar = nullptr;
	QToolButton*				m_pBtnAdd;
	QToolButton*				m_pBtnExpand;

	QToolButton*				m_pTypeFilter = nullptr;
	QMenu*						m_pTypeMenu = nullptr;
	QAction*						m_pPrograms = nullptr;
	QAction*						m_pSystem = nullptr;
	QAction*						m_pApps = nullptr;
	QAction*						m_pGroups = nullptr;

	QToolButton*				m_pRunFilter = nullptr;
	QMenu*						m_pRunMenu = nullptr;
	QAction*						m_pRanRecently = nullptr;

	QToolButton*				m_pRulesFilter = nullptr;
	QMenu*						m_pRulesMenu = nullptr;
	QAction*						m_pExecRules = nullptr;
	QAction*						m_pAccessRules = nullptr;
	QAction*						m_pFwRules = nullptr;

	QToolButton*				m_pTrafficFilter = nullptr;
	QMenu*						m_pTrafficMenu = nullptr;
	QActionGroup*					m_pActionGroup = nullptr;
	QAction*						m_pTrafficRecent = nullptr;
	QAction*						m_pTrafficBlocked = nullptr;
	QAction*						m_pTrafficAllowed = nullptr;

	QToolButton*				m_pSocketFilter = nullptr;
	/*QMenu*						m_pSocketMenu = nullptr;
	QAction*						m_pAnySockets = nullptr;
	QAction*						m_pWebSockets = nullptr;
	QAction*						m_pTcpSockets = nullptr;
	QAction*						m_pTcpClients = nullptr;
	QAction*						m_pTcpServers = nullptr;
	QAction*						m_pUdpSockets = nullptr;*/

	QToolButton*				m_pBtnCleanUp = nullptr;
	QMenu*						m_pCleanUpMenu = nullptr;
	QAction*						m_pReGroup = nullptr;

	QAction*					m_pCreateProgram = nullptr;
	QAction*					m_pEditProgram = nullptr;
	QMenu*						m_pClearTrace = nullptr;
		QAction*					m_pClearExec = nullptr;
		QAction*					m_pClearRes = nullptr;
		QAction*					m_pClearNet = nullptr;
	QMenu*						m_pTraceConfig = nullptr;
		QMenu*						m_pExecTraceConfig = nullptr;
		QMenu*						m_pResTraceConfig = nullptr;
		QMenu*						m_pNetTraceConfig = nullptr;
		QMenu*						m_pStoreTraceConfig = nullptr;
	QMenu*						m_pAddToGroup = nullptr;
	QAction*					m_pRemoveItem = nullptr;

	QVector<QAction*>			m_Groups;

	QString						m_ColumnSet;
};

