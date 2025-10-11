#pragma once
#include <QWidget>
#include "../../MiscHelpers/Common/TreeViewEx.h"
#include "ProcessPage.h"

class CEnclaveView;

class CEnclavePage : public QWidget
{
	Q_OBJECT
public:
	CEnclavePage(QWidget* parent);
	~CEnclavePage();

	void LoadState();
	void SetMergePanels(bool bMerge);

	void	Update();
	void	Clear();

private:

	QVBoxLayout*			m_pMainLayout;

	CEnclaveView*			m_pEnclaveView;

	//QToolBar*				m_pToolBar;

	QSplitter*				m_pHSplitter;

	CProcessPage*			m_pProcessPage;

};

