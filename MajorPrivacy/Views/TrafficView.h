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
	void					Clear();

protected:
	virtual void			OnMenu(const QPoint& Point) override;
	//void					OnDoubleClicked(const QModelIndex& Index) override;

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void 					OnItemAction();	

	void					OnAreaFilter();

	void					OnRefresh();
	void					OnCleanUpDone();
	void					OnClearRecords();

protected:

	QSet<CProgramFilePtr>			m_CurPrograms;
	QSet<CWindowsServicePtr>		m_CurServices;

	QMap<QVariant, STrafficItemPtr>	m_CachedTrafficMap;
	QHash<QString, quint64>			m_TrafficLogTimestamps; // hostname -> last timestamp

private:

	QToolBar*				m_pToolBar = nullptr;
	QToolButton*			m_pBtnExpand = nullptr;
	QToolButton*			m_pBtnTree = nullptr;
	QComboBox*				m_pCmbGrouping = nullptr;
	QToolButton*			m_pAreaFilter = nullptr;
	QMenu*					m_pAreaMenu = nullptr;
	QAction*				m_pInternet = nullptr;
	QAction*				m_pLocalArea = nullptr;
	QAction*				m_pLocalHost = nullptr;
	QToolButton*			m_pBtnHold = nullptr;
	QToolButton*			m_pBtnRefresh = nullptr;
	QToolButton*			m_pBtnClear = nullptr;

	QAction*				m_pFilterDNS = nullptr;
	QAction*				m_pBlockFW = nullptr;

	bool					m_bGroupByProgram = false;
	bool					m_bTreeDomains = true;
	quint64					m_RecentLimit = 0;
	int						m_AreaFilter = 0;

	bool					m_FullRefresh = false;
	int 					m_RefreshCount = 0;

	int 					m_SlowCount = 0;
};
