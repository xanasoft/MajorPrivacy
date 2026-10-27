/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

#include "pch.h"
#include "CertDetailsWnd.h"

#include <QMessageBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QFile>

void CCertDetailsWnd::Show(const SMokEntry& Entry, QWidget* parent)
{
	CCertDetailsWnd Wnd(Entry, parent);
	Wnd.exec();
}

CCertDetailsWnd::CCertDetailsWnd(const SMokEntry& Entry, QWidget* parent)
	: QDialog(parent), m_Entry(Entry)
{
	Qt::WindowFlags flags = windowFlags();
	flags &= ~Qt::WindowContextHelpButtonHint;
	setWindowFlags(flags);

	ui.setupUi(this);
	setWindowTitle(tr("MajorPrivacy - Certificate"));

	ui.treeFields->setColumnWidth(0, 180);
	ui.treeFields->header()->setStretchLastSection(true);

	connect(ui.btnSave, &QPushButton::clicked, this, &CCertDetailsWnd::OnSave);
	connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	if (m_Entry.bIsCert)
		FillCertificate();
	else
		FillHash();
}

void CCertDetailsWnd::AddField(const QString& Name, const QString& Value)
{
	if (Value.isEmpty())
		return;		// a field nothing could be read for is noise, not information

	QTreeWidgetItem* pItem = new QTreeWidgetItem();
	pItem->setText(0, Name);

	//
	// A multi-line value - an X.500 name, say - would otherwise be one row
	// deep with the rest of it cut off. The first line goes in the row and the
	// whole thing in the tooltip.
	//
	QString First = Value;
	int iBreak = First.indexOf('\n');
	if (iBreak >= 0)
		First = First.left(iBreak) + tr("  ...");

	pItem->setText(1, First.trimmed());
	pItem->setToolTip(1, Value);

	ui.treeFields->addTopLevelItem(pItem);
}

void CCertDetailsWnd::FillCertificate()
{
	RESULT(SMokCertInfo) Res = CMokManager::GetCertInfo(m_Entry.Data);
	if (Res.IsError())
	{
		ui.lblHeading->setText(tr("<b>This entry could not be read as a certificate.</b>"));
		AddField(tr("Error"), QString::fromWCharArray(Res.GetMessageText()));
		AddField(tr("Size"), tr("%1 bytes").arg(m_Entry.Data.size()));
		ui.btnSave->setEnabled(!m_Entry.Data.empty());
		return;
	}

	SMokCertInfo Info = Res.GetValue();

	QString Name = Info.SubjectName.empty()
		? tr("(no subject name)") : QString::fromStdWString(Info.SubjectName);

	ui.lblHeading->setText(QString("<b>%1</b>").arg(Name.toHtmlEscaped()));

	AddField(tr("Subject"), QString::fromStdWString(Info.Subject));
	AddField(tr("Issued by"), QString::fromStdWString(Info.Issuer));

	//
	// Worth calling out: a self-signed certificate vouches for nothing but
	// itself, which is exactly what a machine owner key usually is.
	//
	if (Info.bSelfSigned)
		AddField(tr("Chain"), tr("self-signed"));

	AddField(tr("Valid from"), QString::fromStdWString(Info.NotBefore));
	AddField(tr("Valid until"), QString::fromStdWString(Info.NotAfter));

	AddField(tr("Serial number"), QString::fromStdWString(Info.Serial));
	AddField(tr("Version"), tr("v%1").arg(Info.iVersion));

	AddField(tr("Signature algorithm"), QString::fromStdWString(Info.SignatureAlgorithm));

	QString Key = QString::fromStdWString(Info.PublicKeyAlgorithm);
	if (Info.iPublicKeyBits > 0)
		Key += tr(", %1 bits").arg(Info.iPublicKeyBits);
	AddField(tr("Public key"), Key);

	AddField(tr("SHA-1 thumbprint"), QString::fromStdWString(Info.Sha1));
	AddField(tr("SHA-256 thumbprint"), QString::fromStdWString(Info.Sha256));

	AddField(tr("Size"), tr("%1 bytes").arg(m_Entry.Data.size()));
}

void CCertDetailsWnd::FillHash()
{
	QString Type = QString::fromStdWString(m_Entry.TypeName);

	ui.lblHeading->setText(tr("<b>%1 hash</b><br>This entry names one binary by its hash "
		"rather than a key that signed it, so it applies to that one file and nothing else.")
		.arg(Type.toHtmlEscaped()));

	AddField(tr("Type"), Type);
	AddField(tr("Value"), QString::fromStdWString(m_Entry.ToHex()));
	AddField(tr("Size"), tr("%1 bytes").arg(m_Entry.Data.size()));

	ui.btnSave->setText(tr("Save to File..."));
}

void CCertDetailsWnd::OnSave()
{
	if (m_Entry.Data.empty())
		return;

	QString Filter = m_Entry.bIsCert
		? tr("Certificates (*.der);;All Files (*.*)")
		: tr("Hashes (*.hash);;All Files (*.*)");

	QString Path = QFileDialog::getSaveFileName(this, tr("Save Certificate"), QString(), Filter);
	if (Path.isEmpty())
		return;

	QFile File(Path);
	if (!File.open(QIODevice::WriteOnly)) {
		QMessageBox::critical(this, "MajorPrivacy", tr("Could not write to \"%1\".").arg(Path));
		return;
	}

	qint64 uWritten = File.write((const char*)m_Entry.Data.data(), (qint64)m_Entry.Data.size());
	File.close();

	if (uWritten != (qint64)m_Entry.Data.size())
		QMessageBox::critical(this, "MajorPrivacy", tr("Could not write to \"%1\".").arg(Path));
}
