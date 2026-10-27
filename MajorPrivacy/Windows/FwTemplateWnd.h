#pragma once

#include <QtWidgets/QDialog>
#include "ui_FwTemplateWnd.h"
#include "../Core/Network/FwRule.h"
#include "../Core/Programs/ProgramItem.h"

//
// Editor for a firewall rule template. A template is little more than a file path pattern and
// what to do with the programs it covers, so it gets a window of that size rather than the full
// rule editor, which asks about ports, addresses, profiles and users nobody is changing here.
// Anyone who does want those is one button away from it.
//

class CFwTemplateWnd : public QDialog
{
	Q_OBJECT

public:
	// Direction: what was observed, the user may widen or narrow it; Lifetime as in RuleLifetime.h: -1 permanent, -2 until restart, or seconds from now
	CFwTemplateWnd(const QString& Pattern, bool bAllow, EFwDirections Direction, const QStringList& Similar = QStringList(), QWidget* parent = Q_NULLPTR, int Lifetime = -1);
	~CFwTemplateWnd();

	QString				GetPattern() const;
	bool				IsAllow() const;
	EFwDirections		GetDirection() const;
	int					GetLifetime() const;

	// whether the user asked to carry on in the full rule editor instead of saving from here
	bool				WantsFullEditor() const { return m_bFullEditor; }

	//
	// Made from an existing rule, the template can stand for the rules already there for the
	// programs it covers. Those are listed, and kept up to date as the pattern and action change.
	//
	void				SetTakeOver(const QList<CFwRulePtr>& Candidates);
	// the rules a template of this pattern, action and direction would take over
	static bool			TakesOver(const CFwRulePtr& pRule, const QString& Pattern, EFwActions Action, EFwDirections Direction);

	// the program item a template of this pattern belongs to, made up for the editor if nobody has used it yet
	static CProgramItemPtr GetPatternItem(const QString& Pattern, QWidget* pParent);

private slots:
	void				OnPathChanged();
	void				UpdateTakeOver();
	void				BrowseFolder();
	void				BrowseFile();
	void				OnFullEditor();
	void				OnSaveAndClose();

private:
	bool				m_bFullEditor = false;

	bool				m_bTakeOver = false;
	QList<CFwRulePtr>	m_TakeOverCandidates;

	Ui::FwTemplateWnd ui;
};
