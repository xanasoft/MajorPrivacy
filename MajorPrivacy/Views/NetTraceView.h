#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/NetTraceModel.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/windowsService.h"
#include "TraceView.h"

class CNetTraceView : public CTraceView
{
	Q_OBJECT

public:
	CNetTraceView(QWidget *parent = 0);
	virtual ~CNetTraceView();

	void					Sync(ETraceLogs Log, const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services);

private slots:
	void					UpdateFilter();

	void					OnAreaFilter();
	
	void					OnClearTraceLog();

protected:

	QToolBar*				m_pToolBar;
	QComboBox*				m_pCmbDir;
	QComboBox*				m_pCmbAction;
	QComboBox*				m_pCmbType;
	QToolButton*			m_pAreaFilter = nullptr;
	QMenu*					m_pAreaMenu = nullptr;
	QAction*					m_pInternet = nullptr;
	QAction*					m_pLocalArea = nullptr;
	QAction*					m_pLocalHost = nullptr;
	QToolButton*			m_pBtnClear;

	int						m_AreaFilter = 0;
};
