#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"
#include "../Views/DnsCacheView.h"

class CDnsPage : public QWidget
{
	Q_OBJECT
public:
	CDnsPage(QWidget* parent);

	void	Update();

private:

	QVBoxLayout*			m_pMainLayout;

	//QToolBar*				m_pToolBar;

	QSplitter*				m_pVSplitter;

	CDnsCacheView*			m_pDnsCacheView;
};

