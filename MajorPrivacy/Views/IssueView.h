#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../Core/Programs/ProgramItem.h"

class CIssueView : public QWidget
{
	Q_OBJECT

public:
	CIssueView(QWidget *parent = 0);
	virtual ~CIssueView();

	void 				Refresh();
	void 				Update();

protected:

	enum EColumns
	{
		eSeverity = 0,
		eName,
		eCount
	};

private slots:
	void OnMenu(const QPoint& Point);
	void OnAction();

	void OnIssueClicked(QTreeWidgetItem*);
	void OnIssueDoubleClicked(QTreeWidgetItem*);

private:
	QGridLayout*			m_pMainLayout;
	QLabel*					m_pLabel;
	CPanelWidgetEx*			m_pInfo;

	QAction*				m_pFixIssue;
	QAction*				m_pFixAccept;
	QAction*				m_pFixReject;
	QAction*				m_pIgnoreIssue;
};
