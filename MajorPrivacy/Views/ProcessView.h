#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/ProcessModel.h"

class CProcessView : public CPanelViewEx<CProcessModel>
{
	Q_OBJECT

public:
	CProcessView(QWidget *parent = 0);
	virtual ~CProcessView();

	void					Sync(QHash<quint64, CProcessPtr> ProcessMap);

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	//void					OnDoubleClicked(const QModelIndex& Index) override;
	
private:

	QToolBar*				m_pToolBar;
	QComboBox*				m_pCmbScope;
	QToolButton*			m_pBtnTree;

};
