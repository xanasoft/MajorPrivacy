#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"

class CTweakView;

class CTweakPage : public QWidget
{
	Q_OBJECT
public:
	CTweakPage(QWidget* parent);

	void	Update();

private:

	QVBoxLayout*			m_pMainLayout;

	//QToolBar*				m_pToolBar;

	QSplitter*				m_pVSplitter;
	CTweakView*				m_pTweakView;
};

