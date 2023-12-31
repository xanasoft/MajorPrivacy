#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/NetTraceModel.h"
#include "../Core/Programs/ProgramFile.h"

class CNetTraceView : public CPanelView
{
	Q_OBJECT

public:
	CNetTraceView(QWidget *parent = 0);
	virtual ~CNetTraceView();

	void					Sync(const struct SMergedLog* pLog);
	CLogEntryPtr			GetCurrentEntry() { return m_CurrentEntry; }
	QList<CLogEntryPtr>		GetSelectedEntries() { return m_SelectedEntries; }

signals:
	void					CurrentChanged(const CLogEntryPtr& pEntry);
	void					SelectionChanged(const QList<CLogEntryPtr>& Entries);

protected:
	virtual void			OnMenu(const QPoint& Point);
	virtual QTreeView*		GetView()		{ return m_pTreeView; }
	//virtual QAbstractItemModel* GetModel()		{ return m_pSortProxy; }
	virtual QAbstractItemModel* GetModel()		{ return m_pItemModel; }

	CLogEntryPtr			m_CurrentEntry;
	QList<CLogEntryPtr>		m_SelectedEntries;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnDoubleClicked(const QModelIndex& Index);
	void					OnCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
	void					OnSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected);

private:

	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pTreeView;
	CNetTraceModel*			m_pItemModel;
	//QSortFilterProxyModel*	m_pSortProxy;
};
