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
	//void ResetIcon();

	void EditScript();

	void AddExecRule();
	void AddResRule();
	void AddFwRule();
	void AddDnsRule();
	void AddTweak();
	void RemoveSelectedItems();

	void OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column);
	void OnSelectionChanged() { CloseItemEdit(); }


protected:
	void closeEvent(QCloseEvent* e);
	bool eventFilter(QObject *watched, QEvent *e);

	bool Save();

	void CloseItemEdit(bool bSave = true);
	void CloseItemEdit(QTreeWidgetItem* pItem, bool bSave = true);

	enum EColumn
	{
		eName = 0,
		eAction,
		eCount,
	};

	CPresetPtr m_pPreset;
	QString m_IconFile;

	QString m_Script;

private:
	Ui::PresetWindow ui;

};
