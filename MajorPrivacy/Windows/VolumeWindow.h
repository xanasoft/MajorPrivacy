#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_VolumeWindow.h"
#include "../../Library/Crypto/SecurePassword.h"
#include "KeyFilesWnd.h"

class CPasswordStrengthWidget;
class QSpinBox;
class QToolButton;

class CVolumeWindow : public QDialog
{
	Q_OBJECT

public:
	enum EAction {
		eSetPW,
		eGetPW,
		eNew,
		eMount,
		eChange,
	};

	CVolumeWindow(const QString& Prompt, EAction Action, QWidget *parent = Q_NULLPTR);
	~CVolumeWindow();

	CSecurePassword	GetPassword() const;
	CSecurePassword	GetNewPassword() const;
	void		SetImageSize(quint64 uSize) const { return ui.txtImageSize->setText(QString::number(uSize / 1024)); }
	quint64		GetImageSize() const { return ui.txtImageSize->text().toULongLong() * 1024; }
	QString 	GetMountPoint() const { return ui.cmbMount->currentText().replace("/", "\\"); }
	bool		UseProtection() const { return ui.chkProtect->isChecked(); }
	bool		UseLockdown() const { return ui.chkLockdown->isChecked(); }
	void		SetAutoLock(int iSeconds, const QString& Text = "") const;
	int			GetAutoLock() const { return ui.cmbAutoLock->currentData().toInt(); }
	int			GetKdf() const;
	int			GetNewKdf() const { return ui.cmbNewKdf->currentData().toInt(); }
	void		SetKdf(int iKdf) const;
	void		SetNoAutoKdf();

	//
	// This dialog is asking for a password that is not feeding a key derivation
	// at all - the MOK password the boot screen checks, for one. The KDF row and
	// the keyfiles that go with it would only be a choice with no effect, so
	// they come off.
	//
	void		SetNoKdf();

	static void FillKdfCombo(QComboBox* Combo, bool bAuto = true, int Selected = -2);
	static QString GetKdfName(int iKdf);

	//
	// Keyfiles, when the volume library is there to mix them. They are folded
	// into the password on accept, so GetPassword and GetNewPassword hand back
	// the finished thing and no caller has to know keyfiles exist.
	//
	bool		HasKeyfiles() const { return !m_KeyFiles.IsEmpty() || !m_NewKeyFiles.IsEmpty(); }
	SKeyFiles	GetKeyFileSet() const { return m_KeyFiles; }
	SKeyFiles	GetNewKeyFileSet() const { return m_NewKeyFiles; }
	void		SetKeyFileSet(const SKeyFiles& KeyFiles);

	//
	// This dialog is being used to enter a virtual keyfile's own secret, so its
	// keyfile list may not offer virtual keyfiles of its own. One level deep is
	// useful, a tree of them is a way to lose a volume - DiskCryptor draws the
	// line in the same place.
	//
	void		SetNestedKeyFiles() { m_bAllowVirtualKeyFiles = false; }

private slots:
	void		OnShowPassword();
	void		OnShowNewPassword();
	void		OnImageSize();
	void		OnNewPasswordChanged();
	void		CheckPassword();
	void		BrowseMountPoint();
	void		OnSecurePasswordEntry();
	void		OnSecureNewPasswordEntry();
	void		OnKeyFiles();
	void		OnNewKeyFiles();


private:
	CSecurePassword	RequestSecurePassword(bool bConfirm);

	Ui::VolumeWindow ui;

	EAction m_Action;
	CPasswordStrengthWidget* m_pStrengthWidget = nullptr;
	QLabel* m_pPasswordStatus = nullptr;
	QLabel* m_pOldKdfLabel = nullptr;
	QSpinBox* m_pOldKdfSpin = nullptr;

	// Secure password entry (cached passwords from secure desktop entry)
	CSecurePassword m_SecurePassword;       // Cached password from secure entry
	CSecurePassword m_SecureNewPassword;    // Cached new password from secure entry

	// Keyfiles, and the mixed results built from them on accept
	SKeyFiles		m_KeyFiles;
	SKeyFiles		m_NewKeyFiles;
	CSecurePassword m_MixedPassword;
	CSecurePassword m_MixedNewPassword;
	QToolButton*	m_pKeyFilesBtn = nullptr;
	QToolButton*	m_pNewKeyFilesBtn = nullptr;
	bool			m_bAllowVirtualKeyFiles = true;

	bool			MixKeyFiles();
	void			UpdateKeyFileButtons();
};
