#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"

class CVolumeView;

class CVolumePage : public QWidget
{
	Q_OBJECT
public:
	CVolumePage(QWidget* parent);

public slots:
	void	Update();

private:

	QVBoxLayout*			m_pMainLayout;

	CVolumeView*			m_pVolumeView;

	//QToolBar*				m_pToolBar;

	//QSplitter*				m_pVSplitter;

};

