#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../Core/Programs/ProgramItem.h"

class CEventView : public QWidget
{
	Q_OBJECT

public:
	CEventView(QWidget *parent = 0);
	virtual ~CEventView();

	void 				Update();

protected:

	enum EColumns
	{
		eLevel = 0,
		eTimeStamp,
		eName,
		eCount
	};

private slots:
	void					OnClearEventLog();	

private:
	QGridLayout*			m_pMainLayout;
	//QLabel*					m_pLabel;

	QToolBar*				m_pEventToolBar;
	QToolButton*			m_pBtnClear;

	CPanelWidgetEx*			m_pInfo;
};
