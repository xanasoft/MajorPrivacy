#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"

class CTweakView;
class CTweakInfo;

class CTweakPage : public QWidget
{
	Q_OBJECT
public:
	CTweakPage(QWidget* parent);

	void	Update();

private slots:
	void	OnCurrentChanged();

private:

	QVBoxLayout*			m_pMainLayout;

	//QToolBar*				m_pToolBar;

	QSplitter*				m_pHSplitter;
	CTweakView*				m_pTweakView;
	CTweakInfo*				m_pTweakInfo;
};

