#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/ExecutionModel.h"
#include "../Core/Programs/ProgramFile.h"

class CExecutionView : public CPanelViewEx<CExecutionModel>
{
	Q_OBJECT

public:
	CExecutionView(QWidget *parent = 0);
	virtual ~CExecutionView();

	void					Sync(const QSet<CProgramFilePtr>& Programs, bool bAllPrograms);

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

protected:

	QSet<CProgramFilePtr>					m_CurPrograms;
	QMap<SExecutionKey, SExecutionItemPtr>		m_ParentMap;
	QMap<SExecutionKey, SExecutionItemPtr>		m_ExecutionMap;
};
