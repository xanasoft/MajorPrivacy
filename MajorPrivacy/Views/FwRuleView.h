#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeviewEx.h"
#include "../Models/FwRuleModel.h"

class CFwRuleView : public CPanelViewEx<CFwRuleModel>
{
	Q_OBJECT

public:
	CFwRuleView(QWidget *parent = 0);
	virtual ~CFwRuleView();

	void					Sync(const QList<CFwRulePtr>& RuleList);

protected:
	void					OnMenu(const QPoint& Point) override;
	void					OnDoubleClicked(const QModelIndex& Index) override;

	void					OpenFwRullDialog(const CFwRulePtr& pRule);

private slots:
	//void					OnResetColumns();
	//void					OnColumnsChanged();

	void					OnRuleAction();

private:

	QAction*				m_pCreateRule;
	QAction*				m_pEnableRule;
	QAction*				m_pDisableRule;
	QAction*				m_pRuleBlock;
	QAction*				m_pRuleAllow;
	QAction*				m_pRemoveRule;
	QAction*				m_pEditRule;
	QAction*				m_pCloneRule;
};
