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

signals:
	void	ProgramsChanged(const QList<CProgramItemPtr>& Programs);

public slots:
	void	Update();

private slots:
	void	OnProgramChanged(const QModelIndexList& Selection);
	void	OnDoubleClicked(const QModelIndex& Index);
	void	OnProgramAction();

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
	QToolButton*				m_pBtnPrograms;
	QToolButton*				m_pBtnApps;
	QToolButton*				m_pBtnSystem;

	QAction*					m_pCreateProgram;
	QAction*					m_pCreateGroup;
	QAction*					m_pAddToGroup;
	QAction*					m_pRenameItem;
	QAction*					m_pRemoveItem;
};

