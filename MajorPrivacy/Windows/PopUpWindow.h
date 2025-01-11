#pragma once
#include <QtWidgets/QMainWindow>
#include "ui_PopUpWindow.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramItem.h"
#include "../MiscHelpers/Common/PanelView.h"



class CPopUpWindow : public QMainWindow
{
	Q_OBJECT
public:
	CPopUpWindow(QWidget* parent = 0);
	~CPopUpWindow();

	void				PushFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);

	void				PushExecEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);

	void				PushResEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);

	//static void			SetDarkMode(bool bDark) { extern bool CPopUpWindow__DarkMode;  CPopUpWindow__DarkMode = bDark; }

public slots:
	void				Show();
	void				Poke();

private slots:

	// Firewall
	void				OnFwNext();
	void				OnFwPrev();
	void				OnFwAction();

	// Resources
	void				OnResNext();
	void				OnResPrev();
	void				OnResAction();

	// Execution
	void				OnExecNext();
	void				OnExecPrev();
	void				OnExecAction();

protected:
	void				closeEvent(QCloseEvent *e);
	void				timerEvent(QTimerEvent* pEvent);
	int					m_uTimerID;

	// Firewall
	void				PopFwEvent();
	void				UpdateFwCount();
	void				LoadFwEntry(bool bUpdate = false);

	int					m_iFwIndex = -1;
	QList<CProgramFilePtr> m_FwPrograms;
	QMap<CProgramFilePtr, QList<CLogEntryPtr>> m_FwEvents;

	// Resources
	void				PopResEvent(const QString& Path);
	void				UpdateResCount();
	void				LoadResEntry(bool bUpdate = false);

	int					m_iResIndex = -1;
	QList<CProgramFilePtr> m_ResPrograms;
	QMap<CProgramFilePtr, QList<CLogEntryPtr>> m_ResEvents;


	// Execution
	void				PopExecEvent(const QStringList& Paths);
	void				UpdateExecCount();
	void				LoadExecEntry(bool bUpdate = false);

	int					m_iExecIndex = -1;
	QList<CProgramFilePtr> m_ExecPrograms;
	QMap<CProgramFilePtr, QList<CLogEntryPtr>> m_ExecEvents;



	void				TryHide();

private:

	enum ETabs
	{
		eTabFw,
		eTabRes,
		eTabExec,
		eTabCfg,
		eTabMax
	};

	bool				m_ResetPosition;
	int					m_iTopMost;

	Ui::PopUpWindow ui;

	// Firewall
	enum EFwColumns
	{
		eFwProtocol,
		eFwEndpoint,
		eFwTimeStamp,
		eFwProcessId,
		eFwCmdLine,
		eFwColMax
	};

	QAction*			m_pFwAlwaysIgnoreApp;
	QAction*			m_pFwCustom;
	QAction*			m_pFwLanOnly;

	// Resources
	enum EResColumns
	{
		eResPath,
		eResStatus,
		eResTimeStamp,
		eResEnclave,
		eResProcessId,
		eResCmdLine,
		eResColMax
	};

	QAction*			m_pResAlwaysIgnoreApp;
	QAction*			m_pResAlwaysIgnorePath;
	QAction*			m_pResRO;
	QAction*			m_pResEnum;
	QAction*			m_pResCustom;

	// Execution
	enum EExecColumns
	{
		eExecName,
		eExecStatus,
		eExecTrust,
		eExecSigner,
		eExecTimeStamp,
		eExecEnclave,
		eExecProcessId,
		eExecPath,
		eExecColMax
	};

	QAction*			m_pExecAlwaysIgnoreFile;
	//QAction*			m_pExecSignAll;
	QAction*			m_pExecSignCert;
};
