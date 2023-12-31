#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/TweakModel.h"

class CTweakView : public CPanelViewEx<CTweakModel>
{
	Q_OBJECT

public:
	CTweakView(QWidget *parent = 0);
	virtual ~CTweakView();

	void					Sync(const CTweakPtr& pRoot);

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnCheckChanged(const QModelIndex& Index, bool State);

private:

};
