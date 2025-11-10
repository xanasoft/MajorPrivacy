#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/EventLog.h"

class CPresetView : public QWidget
{
	Q_OBJECT

public:
	CPresetView(QWidget *parent = 0);
	virtual ~CPresetView();

public slots:
	void 				Update();

private slots:
	void				OnMenu(const QPoint& Point);
	void				OnAction();

	void				OnPresetClicked(QTreeWidgetItem*);
	void				OnPresetDoubleClicked(QTreeWidgetItem*);

protected:

	enum EColumns
	{
		eName = 0,
		eDescription,
		eCount
	};

private:
	QGridLayout*		m_pMainLayout;
	QLabel*				m_pLabel;

	//QToolBar*			m_pPresetToolBar;

	CPanelWidgetEx*		m_pPresets;

	QAction*			m_pAddPreset;
	QAction*			m_pActivate;
	QAction*			m_pDeactivate;
	QAction*			m_pEditPreset;
	QAction*			m_pRemovePreset;
	
};
