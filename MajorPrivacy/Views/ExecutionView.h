#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/ExecutionModel.h"
#include "../Core/Programs/WindowsService.h"

class CExecutionView : public CPanelViewEx<CExecutionModel>
{
	Q_OBJECT

public:
	CExecutionView(QWidget *parent = 0);
	virtual ~CExecutionView();

	void					Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, bool bAllPrograms);

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	//void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnCleanUpDone();

protected:

	QToolBar*				m_pToolBar;
	QComboBox*				m_pCmbRole;

	QSet<CProgramFilePtr>						m_CurPrograms;
	QSet<CWindowsServicePtr>					m_CurServices;
	QMap<SExecutionKey, SExecutionItemPtr>		m_ParentMap;
	QMap<SExecutionKey, SExecutionItemPtr>		m_ExecutionMap;
	qint32										m_FilterRole = 0;
	quint64										m_RecentLimit = 0;
};
