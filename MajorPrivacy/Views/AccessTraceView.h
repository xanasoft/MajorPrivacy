#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/AccessTraceModel.h"
#include "../Core/Programs/ProgramFile.h"
#include "TraceView.h"

class CAccessTraceView : public CTraceView
{
	Q_OBJECT

public:
	CAccessTraceView(QWidget *parent = 0);
	virtual ~CAccessTraceView();

	void					Sync(const struct SMergedLog* pLog);

private slots:
	void					UpdateFilter();

protected:

	QToolBar*				m_pToolBar;
	QComboBox*				m_pCmbAction;
	QToolButton*			m_pBtnScroll;

};
