#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/DnsCacheModel.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/WindowsService.h"

class CDnsCacheView : public CPanelViewEx<CDnsCacheModel>
{
	Q_OBJECT

public:
	CDnsCacheView(QWidget *parent = 0);
	virtual ~CDnsCacheView();

	void					Sync();

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					FlushDnsCache();

private:

	QToolBar*				m_pToolBar;
	QToolButton*			m_pBtnClear;
};
