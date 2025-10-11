#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/ProgramRuleModel.h"

class CProgramRuleView : public CPanelViewEx<CProgramRuleModel>
{
	Q_OBJECT

public:
	CProgramRuleView(QWidget *parent = 0);
	virtual ~CProgramRuleView();

	void					Sync(QList<CProgramRulePtr> RuleList, const QFlexGuid& EnclaveGuid = QString());
	void					Clear();

protected:
	void					OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

	void					OpenRullDialog(const CProgramRulePtr& pRule);

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnAddRule();
	void					OnRuleAction();

	void					Refresh();
	//void					CleanTemp();

protected:
	QList<CProgramRulePtr>	m_RuleList;
	QFlexGuid				m_EnclaveGuid;

private:

	QToolBar*				m_pToolBar;
	QToolButton*			m_pBtnAdd;
	QComboBox*				m_pCmbAction;
	QToolButton*			m_pBtnEnabled;
	QToolButton*			m_pBtnRefresh;
	QToolButton*			m_pBtnCleanUp;

	QAction*				m_pCreateRule;
	QAction*				m_pEnableRule;
	QAction*				m_pDisableRule;
	QAction*				m_pRemoveRule;
	QAction*				m_pEditRule;
	QAction*				m_pCloneRule;
};
