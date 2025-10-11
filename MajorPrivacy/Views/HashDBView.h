#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/HashDBModel.h"
#include "../Core/Programs/ProgramFile.h"

class CHashDBView : public CPanelViewEx<CHashDBModel>
{
	Q_OBJECT

public:
	CHashDBView(QWidget *parent = 0);
	virtual ~CHashDBView();

	void					Sync(const QSet<CProgramFilePtr>& Programs, bool bAllPrograms, const QFlexGuid& EnclaveGuid = QString());
	void					Clear();

protected:
	void					OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

	void					OpenHashDialog(const CHashPtr& pEntry);

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnAddHash();
	void					OnHashAction();

	void					Refresh();
	//void					CleanTemp();

protected:

	QSet<CProgramFilePtr>					m_CurPrograms;
	QFlexGuid								m_CurEnclaveGuid;
	QMap<SHashItemKey, SHashItemPtr>		m_EntryMap;
	QHash<QString, int>						m_PathUIDs;

	bool					m_FullRefresh = false;
	int 					m_RefreshCount = 0;

private:

	QToolBar*				m_pToolBar;
	QToolButton*			m_pBtnAdd;
	QToolButton*			m_pBtnRefresh;
	//QToolButton*			m_pBtnCleanUp;
	QToolButton*			m_pBtnExpand;

	QAction*				m_pAddHash;
	QAction*				m_pEnableHash;
	QAction*				m_pDisableHash;
	QAction*				m_pRemoveHash;
	QAction*				m_pEditHash;
};
