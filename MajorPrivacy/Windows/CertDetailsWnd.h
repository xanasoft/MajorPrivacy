#pragma once
/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

//
// What one entry of a Secure Boot key list actually is.
//
// The lists themselves can only show a type and a subject line, which is
// enough to recognise a key and not enough to decide whether to trust it. This
// shows the rest - who issued it, how long it is good for, what it is signed
// with, and the thumbprints needed to match it against a certificate from
// somewhere else.
//
// Hash entries get the same window: there is less to say about a bare SHA-256,
// but the one thing worth saying - what it is and what its value is - is the
// same question the user came here with.
//

#include <QDialog>
#include "ui_CertDetails.h"

#include "../../Library/Boot/MokManager.h"

class CCertDetailsWnd : public QDialog
{
	Q_OBJECT

public:
	//
	// Show one list entry. Works for both kinds, so a caller handling a
	// double-click does not have to ask which it has.
	//
	static void			Show(const SMokEntry& Entry, QWidget* parent);

	CCertDetailsWnd(const SMokEntry& Entry, QWidget* parent = nullptr);

private slots:
	void				OnSave();

private:
	void				AddField(const QString& Name, const QString& Value);
	void				FillCertificate();
	void				FillHash();

	Ui::CertDetails		ui;
	SMokEntry			m_Entry;
};
