#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeViewEx.h"
#include "../../MiscHelpers/Common/Finder.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "../Core/Presets/Preset.h"

class CPresetItemsModel;

class CPresetProperties : public QWidget
{
	Q_OBJECT

public:
	CPresetProperties(QWidget *parent = 0);
	virtual ~CPresetProperties();

	void SetPreset(const CPresetPtr& pPreset);
	QString GetCurrentGuid() const;

	void Update();

private slots:
	void AddExecRule();
	void AddResRule();
	void AddFwRule();
	void AddDnsRule();
	void AddTweak();
	void RemoveSelectedItems();
	void EnableSelectedItems();
	void DisableSelectedItems();
	void ActivateSelectedItems();
	void DeactivateSelectedItems();

	void OnSelectionChanged();
	void OnDoubleClicked(const QModelIndex& index);
	void OnMenu(const QPoint& Point);
	void OnEditRule();
	void OnRuleEnable();
	void OnRuleDisable();

protected:
	void ApplyChanges();
	void UpdateButtons();

	CPresetPtr m_pPreset;
	QMap<QFlexGuid, SItemPreset> m_Items;

private:
	QGridLayout*		m_pMainLayout;

	QToolBar*			m_pToolBar;
	QToolButton*		m_pBtnAddExec;
	QToolButton*		m_pBtnAddRes;
	QToolButton*		m_pBtnAddFw;
	QToolButton*		m_pBtnAddDns;
	QToolButton*		m_pBtnAddCfg;
	//QToolButton*		m_pBtnRemove;
	//QToolButton*		m_pBtnEnable;
	//QToolButton*		m_pBtnDisable;

	QGroupBox*			m_pGroup;
	QTreeViewEx*		m_pTreeItems;
	CPresetItemsModel*	m_pItemsModel;
	CSortFilterProxyModel* m_pSortProxy;
	CFinder*			m_pFinder;

	// Context menu
	QMenu*				m_pMenu;
	QMenu*				m_pAddMenu;
	QAction*			m_pAddExecAction;
	QAction*			m_pAddResAction;
	QAction*			m_pAddFwAction;
	QAction*			m_pAddDnsAction;
	QAction*			m_pAddTweakAction;
	QAction*			m_pRemoveAction;
	QAction*			m_pSetEnableAction;
	QAction*			m_pSetDisableAction;
	QAction*			m_pActivateAction;
	QAction*			m_pDeactivateAction;
	QMenu*				m_pRuleMenu;
	QAction*			m_pEditRuleAction;
	QAction*			m_pRuleEnableAction;
	QAction*			m_pRuleDisableAction;
};
