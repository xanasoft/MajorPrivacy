#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/NetTraceModel.h"
#include "../Core/Programs/ProgramFile.h"
#include "TraceView.h"

class CNetTraceView : public CTraceView
{
	Q_OBJECT

public:
	CNetTraceView(QWidget *parent = 0);
	virtual ~CNetTraceView();

	void					Sync(const struct SMergedLog* pLog);

private slots:
	void					UpdateFilter();

protected:

	QToolBar*				m_pToolBar;
	QComboBox*				m_pCmbDir;
	QComboBox*				m_pCmbAction;
	QComboBox*				m_pCmbType;
	QToolButton*			m_pBtnScroll;

};
