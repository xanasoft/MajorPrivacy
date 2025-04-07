#pragma once
#include "./Common/QtVariant.h"
#include "../../MiscHelpers/Common/PanelView.h"


class CSignatureDbWnd: public QMainWindow
{
	Q_OBJECT

public:
	CSignatureDbWnd(QWidget *parent = 0);
	~CSignatureDbWnd();

private slots:
	void				OnAction();
	void				OnClicked(QAbstractButton* pButton);

protected:
	void Load();

	enum EColumns
	{
		eName = 0,
		ePath,
		eCount,
	};

	QWidget*			m_pMainWidget;
	QVBoxLayout*		m_pMainLayout;

	QTreeWidget*		m_pTree;
	CPanelWidgetEx*		m_pPanel;
	
	QAction*			m_pDelete;

	QDialogButtonBox*	m_pButtons;
};


