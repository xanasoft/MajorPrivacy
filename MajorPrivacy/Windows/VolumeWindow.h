#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_VolumeWindow.h"
#include "../../Library/Crypto/SecurePassword.h"

class CPasswordStrengthWidget;
class QSpinBox;

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

	static void FillKdfCombo(QComboBox* Combo, bool bAuto = true, int Selected = -2);
	static QString GetKdfName(int iKdf);

private slots:
	void		OnShowPassword();
	void		OnImageSize();
	void		OnNewPasswordChanged();
	void		CheckPassword();
	void		BrowseMountPoint();
	void		OnSecurePasswordEntry();
	void		OnSecureNewPasswordEntry();


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
};
