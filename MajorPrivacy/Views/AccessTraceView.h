#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/AccessTraceModel.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/windowsService.h"
#include "TraceView.h"

class CAccessTraceView : public CTraceView
{
	Q_OBJECT

public:
	CAccessTraceView(QWidget *parent = 0);
	virtual ~CAccessTraceView();

	void					Sync(ETraceLogs Log, const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, const QString& RootPath = QString());

private slots:
	void					UpdateFilter();

	void					OnClearTraceLog();

protected:

	QToolBar*				m_pToolBar;
	QComboBox*				m_pCmbAction;
	QToolButton*			m_pBtnClear;

};
