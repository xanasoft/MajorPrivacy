#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Core/Programs/windowsService.h"
#include "../Models/TraceModel.h"

class CTraceView : public CPanelView
{
	Q_OBJECT

public:
	CTraceView(CTraceModel* pModel, QWidget *parent = 0);
	virtual ~CTraceView();

	void						Sync(ETraceLogs Log, const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& Services, const QFlexGuid& EnclaveGuid = QString());

public slots:
	void						SetFilter(const QRegularExpression& RegExp, int iOptions = 0, int Column = -1);

private slots:
	void						OnCleanUpDone();

protected:
	virtual void				OnMenu(const QPoint& Point);

	QTreeView*					GetView()			{ return m_pTreeView; }
	QAbstractItemModel*			GetModel()			{ return m_pItemModel; }

	void						ClearTraceLog(ETraceLogs Log);

	SMergedLog					m_Log;

	QVBoxLayout*				m_pMainLayout;

	QTreeViewEx*				m_pTreeView;
	CTraceModel*				m_pItemModel;

	CFinder*					m_pFinder;

	bool						m_FullRefresh;
	QFlexGuid					m_EnclaveGuid;

	quint64						m_RecentLimit = 0;

	QToolButton*				m_pBtnScroll = nullptr;
	QToolButton*				m_pBtnHold = nullptr;
	//QRegularExpression		m_FilterExp;
	QString						m_FilterExp;
	bool						m_bHighLight;
	//int						m_FilterCol;
};
