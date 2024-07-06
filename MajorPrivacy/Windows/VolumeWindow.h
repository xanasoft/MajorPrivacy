#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_VolumeWindow.h"

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

	CVolumeWindow(EAction Action, QWidget *parent = Q_NULLPTR);
	~CVolumeWindow();

	QString		GetPassword() const { return m_Password; }
	QString		GetNewPassword() const { return m_NewPassword; }
	void		SetImageSize(quint64 uSize) const { return ui.txtImageSize->setText(QString::number(uSize / 1024)); }
	quint64		GetImageSize() const { return ui.txtImageSize->text().toULongLong() * 1024; }
	QString 	GetMountPoint() const { return ui.cmbMount->currentText(); }
	bool		UseProtection() const { return ui.chkProtect->isChecked(); }

private slots:
	void		OnShowPassword();
	void		OnImageSize();
	void		CheckPassword();
	void		BrowseMountPoint();

private:
	Ui::VolumeWindow ui;

	EAction m_Action;
	QString m_Password;
	QString m_NewPassword;
};
