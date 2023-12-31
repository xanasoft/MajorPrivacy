#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/ProcessModel.h"

class CProcessView : public CPanelView
{
	Q_OBJECT

public:
	CProcessView(QWidget *parent = 0);
	virtual ~CProcessView();

	void					Sync(const QMap<quint64, CProcessPtr>& ProcessMap);
	CProcessPtr				GetCurrentProcess()	{ return m_CurrentProcess; }
	QList<CProcessPtr>		GetSelectedProcess(){ return m_SelectedProcesses; }

signals:
	void					CurrentChanged(const CProcessPtr& pProcess);
	void					SelectionChanged(const QList<CProcessPtr>& Process);

protected:
	virtual void			OnMenu(const QPoint& Point);
	virtual QTreeView*		GetView()			{ return m_pTreeView; }
	virtual QAbstractItemModel* GetModel()		{ return m_pSortProxy; }

	CProcessPtr				m_CurrentProcess;
	QList<CProcessPtr>		m_SelectedProcesses;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnDoubleClicked(const QModelIndex& Index);
	void					OnCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
	void					OnSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected);

private:

	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pTreeView;
	CProcessModel*			m_pItemModel;
	QSortFilterProxyModel*	m_pSortProxy;
};
