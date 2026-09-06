#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_PresetWindow.h"
#include "../Core/Presets/Preset.h"

class CPresetWindow : public QDialog
{
	Q_OBJECT

public:
	CPresetWindow(const CPresetPtr& pPreset, QWidget *parent = Q_NULLPTR);
	~CPresetWindow();

signals:
	void Closed();

private slots:

	void OnNameChanged(const QString& Text);

	void OnSaveAndClose();
	bool OnSave();

	void PickIcon();
	void BrowseImage();

	void EditScript();

protected:
	void closeEvent(QCloseEvent* e);

	bool Save();

	CPresetPtr m_pPreset;
	QString m_IconFile;

	QString m_Script;

private:
	Ui::PresetWindow ui;

};
