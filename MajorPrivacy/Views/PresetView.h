#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeViewEx.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/EventLog.h"

class CPresetsModel;

class CPresetView : public QWidget
{
	Q_OBJECT

public:
	CPresetView(QWidget *parent = 0);
	virtual ~CPresetView();

signals:
	void				CurrentChanged(const QString& presetGuid);

public slots:
	void 				Update();

private slots:
	void				OnMenu(const QPoint& Point);
	void				OnAction();

	void				OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
	void				OnDoubleClicked(const QModelIndex& index);

private:
	QGridLayout*		m_pMainLayout;

	QToolBar*			m_pToolBar;
	QToolButton*		m_pBtnAdd;
	QToolButton*		m_pBtnActivate;
	QToolButton*		m_pBtnDeactivate;

	QTreeViewEx*		m_pTreeView;
	CPresetsModel*		m_pModel;
	CFinder*			m_pFinder;

	QMenu*				m_pMenu;
	QAction*			m_pAddPreset;
	QAction*			m_pActivate;
	QAction*			m_pDeactivate;
	QAction*			m_pEditPreset;
	QAction*			m_pDuplicatePreset;
	QAction*			m_pRemovePreset;

};
