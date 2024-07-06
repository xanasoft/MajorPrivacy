#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../Core/Programs/ProgramItem.h"

class CAccessInfoView : public QWidget
{
	Q_OBJECT

public:
	CAccessInfoView(QWidget *parent = 0);
	virtual ~CAccessInfoView();

public slots:
	void					OnSelectionChanged();

protected:


private:

	enum EColumns
	{
		eName = 0,
		eLastAccess,
		eStatus,
		eCount
	};

	QGridLayout*			m_pMainLayout;
	CPanelWidgetEx*			m_pInfo;
};
