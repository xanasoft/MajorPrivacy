#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Tweaks/TweakManager.h"

class CTweakInfo : public QWidget
{
	Q_OBJECT

public:
	CTweakInfo(QWidget *parent = 0);
	virtual ~CTweakInfo();

	void ShowTweak(const CTweakPtr& pTweak);

private slots:
	void OnActivate();
	void OnDeactivate();

protected:
	CTweakPtr				m_pTweak;

private:
	QGridLayout*			m_pMainLayout;

	QToolBar*				m_pToolBar;
	QAction*				m_pActivate;
	QAction*				m_pDeactivate;

	QGroupBox*				m_pGroup;
	QLineEdit*				m_pName;
	QPlainTextEdit*			m_pDescription;
};
