#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/ProcessTraceModel.h"
#include "../Core/Programs/ProgramFile.h"
#include "TraceView.h"

class CProcessTraceView : public CTraceView
{
	Q_OBJECT

public:
	CProcessTraceView(QWidget *parent = 0);
	virtual ~CProcessTraceView();

	void					Sync(const struct SMergedLog* pLog);

private slots:
	void					UpdateFilter();

protected:

	QToolBar*				m_pToolBar;
	QComboBox*				m_pCmbRole;
	QComboBox*				m_pCmbAction;
	QToolButton*			m_pBtnScroll;

};
