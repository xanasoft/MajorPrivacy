#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../Core/Programs/ProgramItem.h"

class CInfoView : public QWidget
{
	Q_OBJECT

public:
	CInfoView(QWidget *parent = 0);
	virtual ~CInfoView();

	void Sync(const QList<CProgramItemPtr>& Items);

protected:

private slots:

private:
	QGridLayout*			m_pMainLayout;
	CPanelWidgetEx*			m_pInfo;
};
