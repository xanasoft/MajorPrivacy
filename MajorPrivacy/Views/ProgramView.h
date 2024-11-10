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

signals:
	void	ProgramsChanged(const QList<CProgramItemPtr>& Programs);

public slots:
	void	FilterUpdate();
	void	Update();

private slots:
	void	OnProgramChanged(const QModelIndexList& Selection);
	void	OnDoubleClicked(const QModelIndex& Index);
	void	OnProgramAction();

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

	CFinder*					m_pFinder;

private:
	virtual void				OnMenu(const QPoint& Point);

	QVBoxLayout*				m_pMainLayout;

#ifdef SPLIT_TREE
	CSplitTreeView*				m_pTreeList;
#else
	QTreeViewEx*				m_pTreeList;
#endif
	CProgramModel*				m_pProgramModel;
	QSortFilterProxyModel*		m_pSortProxy;

	QToolBar*					m_pToolBar;
	//QToolButton*				m_pBtnClear;

	QToolButton*				m_pTypeFilter;
	QMenu*						m_pTypeMenu;
	QAction*						m_pPrograms;
	QAction*						m_pSystem;
	QAction*						m_pApps;
	QAction*						m_pGroups;

	QToolButton*				m_pRunFilter;
	QMenu*						m_pRunMenu;
	QAction*						m_pRanRecently;

	QToolButton*				m_pRulesFilter;
	QMenu*						m_pRulesMenu;
	QAction*						m_pExecRules;
	QAction*						m_pAccessRules;
	QAction*						m_pFwRules;

	QToolButton*				m_pTrafficFilter;
	QMenu*						m_pTrafficMenu;
	QActionGroup*					m_pActionGroup;
	QAction*						m_pTrafficRecent;
	QAction*						m_pTrafficBlocked;
	QAction*						m_pTrafficAllowed;

	QToolButton*				m_pSocketFilter;
	/*QMenu*						m_pSocketMenu;
	QAction*						m_pAnySockets;
	QAction*						m_pWebSockets;
	QAction*						m_pTcpSockets;
	QAction*						m_pTcpClients;
	QAction*						m_pTcpServers;
	QAction*						m_pUdpSockets;*/


	QAction*					m_pCreateProgram;
	QAction*					m_pCreateGroup;
	QAction*					m_pAddToGroup;
	QAction*					m_pRenameItem;
	QAction*					m_pRemoveItem;
};

