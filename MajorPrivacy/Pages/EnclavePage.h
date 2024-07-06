#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"

class CEnclaveView;

class CEnclavePage : public QWidget
{
	Q_OBJECT
public:
	CEnclavePage(QWidget* parent);

public slots:
	void	Update();

private:

	QVBoxLayout*			m_pMainLayout;

	CEnclaveView*			m_pEnclaveView;

	//QToolBar*				m_pToolBar;

	//QSplitter*				m_pVSplitter;

};

