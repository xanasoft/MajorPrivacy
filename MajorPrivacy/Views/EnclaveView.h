#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/EnclaveModel.h"

class CEnclaveView : public CPanelViewEx<CEnclaveModel>
{
	Q_OBJECT

public:
	CEnclaveView(QWidget *parent = 0);
	virtual ~CEnclaveView();

	void					Sync();

protected:
	virtual void			OnMenu(const QPoint& Point) override;

	void					OnDoubleClicked(const QModelIndex& Index) override;
	
private:

};
