#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/TraceModel.h"

class CTraceView : public CPanelView
{
	Q_OBJECT

public:
	CTraceView(CTraceModel* pModel, QWidget *parent = 0);
	virtual ~CTraceView();

	void						Sync(const struct SMergedLog* pLog);

public slots:
	void						SetFilter(const QString& Exp, int iOptions = 0, int Column = -1);

protected:
	virtual void				OnMenu(const QPoint& Point);

	QTreeView*					GetView()			{ return m_pTreeView; }
	QAbstractItemModel*			GetModel()			{ return m_pItemModel; }

	QVBoxLayout*				m_pMainLayout;

	QTreeViewEx*				m_pTreeView;
	CTraceModel*				m_pItemModel;

	CFinder*					m_pFinder;

	bool						m_FullRefresh;

	quint64						m_RecentLimit = 0;

	//QAction*					m_pAutoScroll; // todo
	//QRegularExpression		m_FilterExp;
	QString						m_FilterExp;
	bool						m_bHighLight;
	//int						m_FilterCol;
};
