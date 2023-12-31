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

public slots:
	void	Update();

private slots:
	void	OnProgramChanged(const QModelIndexList& Selection);

protected:
	//virtual void				OnMenu(const QPoint& Point);
#ifdef SPLIT_TREE
	virtual QTreeView*			GetView() 				{ return m_pTreeList->GetView(); }
#else
	virtual QTreeView*			GetView() 				{ return m_pTreeList; }
#endif
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }

	QList<CProgramItemPtr>		m_CurPrograms;

private:
	QVBoxLayout*				m_pMainLayout;

#ifdef SPLIT_TREE
	CSplitTreeView*				m_pTreeList;
#else
	QTreeViewEx*				m_pTreeList;
#endif
	CProgramModel*				m_pProgramModel;
	QSortFilterProxyModel*		m_pSortProxy;
};

