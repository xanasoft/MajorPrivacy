#pragma once
#include <QtWidgets/QMainWindow>
#include "ui_PopUpWindow.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramItem.h"
#include "../MiscHelpers/Common/PanelView.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Core/Network/NetworkManager.h"



class CPopUpWindow : public QMainWindow
{
	Q_OBJECT
public:
	CPopUpWindow(QWidget* parent = 0);
	~CPopUpWindow();

	void				PushFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);
	void				PushFwRuleEvent(EConfigEvent EventType, const CFwRulePtr& pFwRule);

	void				PushExecEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);

	void				PushResEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry, uint32 TimeOut);

	//static void			SetDarkMode(bool bDark) { extern bool CPopUpWindow__DarkMode;  CPopUpWindow__DarkMode = bDark; }

public slots:
	void				Show();
	void				Poke();

private slots:

	// Firewall
	void				OnFwNext();
	void				OnFwPrev();
	void				OnFwAction();
	void				OnFwRule();
	void				OnFwRuleDblClick(QTreeWidgetItem* pItem);

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

	struct SLogEntry
	{
		SLogEntry(CLogEntryPtr pEntry) : pPtr(pEntry) {}

		void AddWaiting(CLogEntryPtr pEntry, uint32 TimeOut)
		{
			SWaitingEvent Event;
			Event.EventId = pEntry->GetUID();
			Event.TimeOut = GetCurTick() + TimeOut;
			Waiting.append(Event);
		}

		CLogEntryPtr pPtr;

		struct SWaitingEvent
		{
			uint64 EventId = 0;
			uint64 TimeOut = 0; 
		};
		QList<SWaitingEvent> Waiting;
	};


	enum ETabs
	{
		eTabFw,
		eTabRes,
		eTabExec,
		eTabCfg,
		eTabMax
	};

	void UpdateTimeOut(ETabs Tab);

	// Firewall
	struct SFwProgram
	{
		enum EType
		{
			eNetworkEvent = 0,
			eRuleEvent
		}				Type = eNetworkEvent;
		CProgramItemPtr pItem;

		bool operator==(const SFwProgram& other) const {return pItem == other.pItem && Type == other.Type;}
	};
	void				PushFwEvent(const SFwProgram& Prog);
	void				PopFwEvent();
	void				UpdateFwCount();
	void				LoadFwEntry(bool bUpdate = false);
	void				LoadFwEvent(const CProgramFilePtr& pProgram, bool bUpdate = false);
	void				LoadFwRule(const CProgramItemPtr& pProgram, bool bUpdate = false);
	struct SFwRuleEvent
	{
		EConfigEvent EventType = EConfigEvent::eUnknown;
		CFwRulePtr pFwRule;

		bool operator==(const SFwRuleEvent& other) const {return EventType == other.EventType && pFwRule == other.pFwRule;}
	};
	void				UpdateFwRule(const SFwRuleEvent& Event, QTreeWidgetItem* pItem);

	int					m_iFwIndex = -1;
	QList<SFwProgram> m_FwPrograms;
	QMap<CProgramFilePtr, QList<SLogEntry>> m_FwEvents;
	QMap<CProgramItemPtr, QList<SFwRuleEvent>> m_FwRuleEvents;

	// Resources
	void				PopResEvent(const QString& Path, EAccessRuleType Action);
	void				UpdateResCount();
	void				LoadResEntry(bool bUpdate = false);

	int					m_iResIndex = -1;
	QList<CProgramFilePtr> m_ResPrograms;
	QMap<CProgramFilePtr, QList<SLogEntry>> m_ResEvents;


	// Execution
	void				PopExecEvent(const QStringList& Paths);
	void				UpdateExecCount();
	void				LoadExecEntry(bool bUpdate = false);

	int					m_iExecIndex = -1;
	QList<CProgramFilePtr> m_ExecPrograms;
	QMap<CProgramFilePtr, QList<SLogEntry>> m_ExecEvents;



	void				TryHide();

private:

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

	enum EFwRuleColumns
	{
		eFwEvent,
		eFwAction,
		eFwRuleName,
	};

	QAction*			m_pFwAlwaysIgnoreApp;
	QAction*			m_pFwCustom;
	//QAction*			m_pFwLanOnly;
	QAction*			m_pFwApproveAll;
	QAction*			m_pFwRejectAll;

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
		eExecIssuer,
		eExecTimeStamp,
		eExecEnclave,
		eExecProcessId,
		eExecPath,
		eExecColMax
	};

	QAction*			m_pExecAlwaysIgnoreFile;
	//QAction*			m_pExecSignAll;
	QAction*			m_pExecSignCert;
	QAction*			m_pExecSignCA;
};
