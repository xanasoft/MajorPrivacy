#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"

class CDrivePage : public QWidget
{
	Q_OBJECT
public:
	CDrivePage(QWidget* parent);

public slots:
	void	Update();

private:

	QVBoxLayout*			m_pMainLayout;

	//QToolBar*				m_pToolBar;

	QSplitter*				m_pVSplitter;

};

