#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/TrafficModel.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/WindowsService.h"

class CTrafficView : public CPanelViewEx<CTrafficModel>
{
	Q_OBJECT

public:
	CTrafficView(QWidget *parent = 0);
	virtual ~CTrafficView();

	void					Sync(const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services);

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	//void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnAreaFilter();

	void					OnCleanUpDone();

protected:

	QSet<CProgramFilePtr>			m_CurPrograms;
	QSet<CWindowsServicePtr>		m_CurServices;
	QMap<QString, STrafficItemPtr>	m_ParentMap;
	QMap<quint64, STrafficItemPtr>	m_TrafficMap;

private:

	QToolBar*				m_pToolBar;
	QToolButton*			m_pBtnExpand;
	QComboBox*				m_pCmbGrouping;
	QToolButton*			m_pAreaFilter = nullptr;
	QMenu*					m_pAreaMenu = nullptr;
	QAction*					m_pInternet = nullptr;
	QAction*					m_pLocalArea = nullptr;
	QAction*					m_pLocalHost = nullptr;

	bool					m_bGroupByProgram = false;
	quint64					m_RecentLimit = 0;
	int						m_AreaFilter = 0;
};
