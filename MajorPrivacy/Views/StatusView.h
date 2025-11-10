#pragma once

#include "../../MiscHelpers/Common/PanelView.h"
#include "../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../Core/Programs/ProgramItem.h"
#include "../../MiscHelpers/Common/FlowLayout.h"
#include "PresetView.h"

class CStatusView : public QWidget
{
	Q_OBJECT

public:
	CStatusView(QWidget *parent = 0);
	virtual ~CStatusView();

	void Refresh();
	void Update();

protected:
	bool			m_bHasKey = false;

private slots:

private:
	QFlowLayout*	m_pMainLayout;

	QGroupBox*		m_pSecBox;
	QGridLayout*	m_pSecLayout;
	QLabel*			m_pSecIcon;
	QLabel*			m_pSecService;
	QLabel*			m_pSecDriver;
	QLabel*			m_pSecKey;
	QLabel*			m_pSecConfig;
	QLabel*			m_pSecCore;

	QGroupBox*		m_pExecBox;
	QGridLayout*	m_pExecLayout;
	QLabel*			m_pExecIcon;
	QLabel*			m_pExecStatus;
	QLabel*			m_pExecRules;
	QLabel*			m_pExecEnclaves;

	QGroupBox*		m_pResBox;
	QGridLayout*	m_pResLayout;
	QLabel*			m_pResIcon;
	QLabel*			m_pResStatus;
	QLabel*			m_pResRules;
	QLabel*			m_pResFolders;
	QLabel*			m_pResVolumes;

	QGroupBox*		m_pFirewallBox;
	QGridLayout*	m_pFirewallLayout;
	QLabel*			m_pFirewallIcon;
	QLabel*			m_pFirewallStatus;
	QLabel*			m_pDnsStatus;
	QLabel*			m_pFirewallRules;
	QLabel*			m_pFirewallPending;
	QLabel*			m_pFirewallAltered;
	QLabel*			m_pFirewallMissing;

	QGroupBox*		m_pTweakBox;
	QGridLayout*	m_pTweakLayout;
	QLabel*			m_pTweakIcon;
	QLabel*			m_pTweakStatus;
	QLabel*			m_pTweakApplied;
	QLabel*			m_pTweakFailed;

	CPresetView*	m_pPresetView;
};
