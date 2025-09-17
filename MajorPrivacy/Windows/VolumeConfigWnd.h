#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_VolumeConfigWnd.h"
#include "../Core/Volumes/Volume.h"

class CVolumeConfigWnd : public QDialog
{
	Q_OBJECT

public:
	CVolumeConfigWnd(const CVolumePtr& pVolume, QWidget *parent = Q_NULLPTR);
	~CVolumeConfigWnd();

signals:
	void Closed();

private slots:

	void OnSaveAndClose();
	bool OnSave();

	void EditScript();

protected:
	void closeEvent(QCloseEvent* e);

	bool Save();

	CVolumePtr m_pVolume;

	QString m_Script;

private:
	Ui::VolumeConfigWnd ui;
};
