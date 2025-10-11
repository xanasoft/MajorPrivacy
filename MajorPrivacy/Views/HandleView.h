#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/HandleModel.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/WindowsService.h"

class CHandleView : public CPanelViewEx<CHandleModel>
{
	Q_OBJECT

public:
	CHandleView(QWidget *parent = 0);
	virtual ~CHandleView();

	void					Sync(const QHash<quint64, CProcessPtr>& Processes, const QString& RootPath = QString());
	void					Clear();
	
protected:
	virtual void			OnMenu(const QPoint& Point) override;
	//void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

private:

	QToolBar*				m_pToolBar;

};
