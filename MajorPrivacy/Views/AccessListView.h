#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/AccessListModel.h"
#include "../Core/Programs/ProgramItem.h"

class CAccessListView : public CPanelViewEx<CAccessListModel>
{
	Q_OBJECT

public:
	CAccessListView(QWidget *parent = 0);
	virtual ~CAccessListView();

public slots:
	void					OnSelectionChanged();

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	//void					OnDoubleClicked(const QModelIndex& Index) override;

private:

	QToolBar*				m_pToolBar;
	QToolButton*			m_pBtnExpand;

};
