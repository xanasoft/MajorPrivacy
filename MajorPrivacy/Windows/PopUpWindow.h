#pragma once
#include <QtWidgets/QMainWindow>
#include "ui_PopUpWindow.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramItem.h"
#include "../MiscHelpers/Common/PanelView.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Core/Network/NetworkManager.h"
#include "../Helpers/PathPattern.h"



class CPopUpWindow : public QMainWindow
{
	Q_OBJECT
public:
	CPopUpWindow(QWidget* parent = 0);
	~CPopUpWindow();

	void				PushFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);
	void				PushFwRuleEvent(EConfigEvent EventType, const CFwRulePtr& pFwRule, CProgramItemPtr pProgram = CProgramItemPtr());

	void				PushExecEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);

	void				PushResEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry, uint32 TimeOut);

	//static void			SetDarkMode(bool bDark) { extern bool CPopUpWindow__DarkMode;  CPopUpWindow__DarkMode = bDark; }

public slots:
	void				Show();
	void				TryShow();
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
	// drops all pending connection attempts which are already covered by one of the given rules
	int					ClearCoveredFwEvents(const QList<CFwRulePtr>& Rules);

	//
	// Windows changes binary paths on every update, store apps worst of all, which turns the
	// prompts into a formality nobody reads anymore. So whenever the program at hand looks like a
	// new version of something already decided about we offer to make the decision by path pattern,
	// either as a rule template or as a guard entry, instead of for this one single path.
	//
	void				UpdateFwPattern(const CProgramItemPtr& pProgram);
	void				UpdateFwOffers(const CProgramItemPtr& pProgram);
	EFwDirections		GetFwEventDirections(const CProgramItemPtr& pProgram);
	CProgramItemPtr		GetFwPatternItem(const QString& Pattern);
	// shows the deduced pattern as a template, saves it, and hands the rule back
	bool				MakeFwTemplate(bool bAllow, EFwDirections Direction, const CProgramItemPtr& pProgram, CFwRulePtr* ppRule = NULL, int Lifetime = -1);
	bool				MakeFwGuardEntry(bool bApprove);
	bool				MakeFwRuleTemplate(bool bApprove, CFwRulePtr* ppRule = NULL);
	// a template which already stands for this program, only not in this direction, is extended to both
	bool				WidenFwTemplate(const CFwRulePtr& pTemplate, CFwRulePtr* ppRule = NULL);
	// whether pressing allow or block should make a template of it, asking the user if need be
	bool				WantFwTemplate(const CProgramItemPtr& pProgram, bool bAllow);
	// which way a new template made from the connection prompt has to point
	EFwDirections		GetFwTemplateDirection(const CProgramItemPtr& pProgram, bool bAllow);
	// nothing is selected, so this asks about all of them, and about a standing answer if one fits
	bool				AskAboutAllFwRules(bool bApprove, bool& bAlsoTemplate);
	// what a template has to say for a change like the one on screen to be decided the same way again
	bool				GetFwRuleTemplateAction(const CProgramItemPtr& pProgram, bool bApprove, EFwActions& Action, EFwDirections& Direction);

	SPathPattern		m_FwPattern;
	bool				m_bFwTemplateOffer = false;	// a template could be made of this one
	bool				m_bFwGuardOffer = false;	// a guard entry could be written for it
	bool				m_bFwTemplateApprove = false;	// a template could approve what is pending
	bool				m_bFwTemplateReject = false;	// and one could turn it down
	// a template for this program which lacks only the direction at hand, to be widened rather than added to
	CFwRulePtr			m_pFwAllowWiden;
	CFwRulePtr			m_pFwBlockWiden;
	CFwRulePtr			m_pFwApproveWiden;
	CFwRulePtr			m_pFwRejectWiden;

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
	QAction*			m_pFwAllowTemplate;
	QAction*			m_pFwBlockTemplate;
	QAction*			m_pFwAllowOnly;
	QAction*			m_pFwBlockOnly;
	QAction*			m_pFwApproveAll;
	QAction*			m_pFwRejectAll;
	QAction*			m_pFwApproveAuto;
	QAction*			m_pFwRejectAuto;
	QAction*			m_pFwApproveTemplate;
	QAction*			m_pFwRejectTemplate;

	// Resources
	enum EResColumns
	{
		eResPath,
		eResStatus,
		eResTimeStamp,
		eResEnclave,
		eResUser,
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
