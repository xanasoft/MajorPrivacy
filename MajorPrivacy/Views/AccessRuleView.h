#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/AccessRuleModel.h"

class CAccessRuleView : public CPanelViewEx<CAccessRuleModel>
{
	Q_OBJECT

public:
	CAccessRuleView(QWidget *parent = 0);
	virtual ~CAccessRuleView();

	void					Sync(QList<CAccessRulePtr> RuleList, const QString& VolumeRoot = QString(), const QString& VolumeImage = QString());
	void					Clear();

protected:
	void					OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

	void					OpenRullDialog(const CAccessRulePtr& pRule);

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnAddRule();
	void					OnRuleAction();

	void					Refresh();
	void					CleanTemp();

protected:
	QList<CAccessRulePtr>	m_RuleList;
	QString				    m_VolumeRoot;
	QString				    m_VolumeImage;

private:

	QToolBar*				m_pToolBar;
	QToolButton*			m_pBtnAdd;
	QComboBox*				m_pCmbAction;
	QToolButton*			m_pBtnEnabled;
	QToolButton*			m_pBtnTemp;
	QToolButton*			m_pBtnRefresh;
	QToolButton*			m_pBtnCleanUp;

	QAction*				m_pCreateRule;
	QAction*				m_pEnableRule;
	QAction*				m_pDisableRule;
	QAction*				m_pRemoveRule;
	QAction*				m_pEditRule;
	QAction*				m_pCloneRule;
	QAction*				m_pImportRules;
	QAction*				m_pExportRules;
};
