#pragma once
#include "../Library/Common/XVariant.h"
#include "../../MiscHelpers/Common/PanelView.h"


class CMountMgrWnd: public QMainWindow
{
	Q_OBJECT

public:
	CMountMgrWnd(QWidget *parent = 0);
	~CMountMgrWnd();

private slots:
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
	
	QDialogButtonBox*	m_pButtons;
};


