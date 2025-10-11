#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/DnsRuleModel.h"

class CDnsRuleView : public CPanelViewEx<CDnsRuleModel>
{
	Q_OBJECT

public:
	CDnsRuleView(QWidget *parent = 0);
	virtual ~CDnsRuleView();

	void					Sync(QList<CDnsRulePtr> RuleList);
	void					Clear();

protected:
	void					OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

	void					OpenRullDialog(const CDnsRulePtr& pRule);

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnAddRule();
	void					OnRuleAction();

	void					Refresh();
	//void					CleanTemp();

protected:
	QList<CDnsRulePtr>		m_RuleList;

private:

	QToolBar*				m_pToolBar;
	QToolButton*			m_pBtnAdd;
	QComboBox*				m_pCmbAction;
	QToolButton*			m_pBtnEnabled;
	QToolButton*			m_pBtnRefresh;
	//QToolButton*			m_pBtnCleanUp;

	QAction*				m_pCreateRule;
	QAction*				m_pEnableRule;
	QAction*				m_pDisableRule;
	QAction*				m_pRuleBlock;
	QAction*				m_pRuleAllow;
	QAction*				m_pRemoveRule;
	QAction*				m_pEditRule;
	QAction*				m_pCloneRule;
};
