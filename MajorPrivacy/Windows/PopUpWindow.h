#pragma once
#include <QtWidgets/QMainWindow>
#include "ui_PopUpWindow.h"
#include "../MajorPrivacy.h"
#include "../Core/Programs/ProgramItem.h"


class CPopUpWindow : public QMainWindow
{
	Q_OBJECT
public:
	CPopUpWindow(QWidget* parent = 0);
	~CPopUpWindow();

	void				PushFwEvent(const CProgramFilePtr& pProgram, const CLogEntryPtr& pEntry);

	//static void			SetDarkMode(bool bDark) { extern bool CPopUpWindow__DarkMode;  CPopUpWindow__DarkMode = bDark; }

public slots:
	void				Show();
	void				Poke();

private slots:
	void				OnFwNext();
	void				OnFwPrev();
	void				OnFwAction();

protected:
	void				closeEvent(QCloseEvent *e);
	void				timerEvent(QTimerEvent* pEvent);
	int					m_uTimerID;

	void				PopFwEvent();
	void				UpdateFwCount();
	void				LoadFwEntry(bool bUpdate = false);
	int					m_iFwIndex = -1;
	QList<CProgramFilePtr> m_FwPrograms;
	QMap<CProgramFilePtr, QList<CLogEntryPtr>> m_FwEvents;

	void				TryHide();

private:

	enum ETabs
	{
		eTabFw,
		eTabRes,
		eTabCfg,
		eTabMax
	};

	enum EFwColumns
	{
		eFwProtocol,
		eFwEndpoint,
		eFwHostName,
		eFwTimeStamp,
		eFwProcessId,
		eFwColMax
	};

	bool				m_ResetPosition;
	int					m_iTopMost;

	Ui::PopUpWindow ui;

	QAction*			m_pFwAlwaysIgnore;
	QAction*			m_pFwCustom;
	QAction*			m_pFwLanOnly;
};
