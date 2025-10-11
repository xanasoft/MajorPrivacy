#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"
#include "AccessPage.h"

class CVolumeView;

class CVolumePage : public QWidget
{
	Q_OBJECT
public:
	CVolumePage(QWidget* parent);
	~CVolumePage();

	void LoadState();
	void SetMergePanels(bool bMerge);

	void	Update();
	void	Clear();

private:

	QVBoxLayout*			m_pMainLayout;

	CVolumeView*			m_pVolumeView;

	//QToolBar*				m_pToolBar;

	QSplitter*				m_pHSplitter;

	CAccessPage*			m_pAccessPage;

};

