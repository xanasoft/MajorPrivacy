#pragma once

#include <QtWidgets/QDialog>
#include "ui_FwGuardEntryWnd.h"
#include "../../Library/Status.h"

//
// Editor for a single firewall guard entry, a file path pattern plus what the guard is to do with
// rule changes of the programs it covers. Those entries live in the services configuration rather
// than in the rule list, hence they get their own small editor instead of being a rule window.
//

class CFwGuardEntryWnd : public QDialog
{
	Q_OBJECT

public:
	CFwGuardEntryWnd(const QString& Pattern, bool bApprove, const QStringList& Similar = QStringList(), QWidget* parent = Q_NULLPTR);
	~CFwGuardEntryWnd();

	QString				GetPattern() const;
	bool				IsApprove() const;

	// Adds the pattern to the services auto guard list, removing it from the opposing one.
	static STATUS		StoreEntry(const QString& Pattern, bool bApprove);

	// Tells whether an enabled entry which already covers Path is configured.
	static bool			IsCovered(const QString& Path, bool* pbApprove = nullptr);

private slots:
	void				OnPathChanged();
	void				BrowseFolder();
	void				BrowseFile();
	void				OnSaveAndClose();

private:
	Ui::FwGuardEntryWnd ui;
};
