#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_HashEntryWnd.h"
#include "../Core/HashDB/HashEntry.h"

class CHashEntryWnd : public QDialog
{
	Q_OBJECT

public:
	CHashEntryWnd(const CHashPtr& pHash, QWidget *parent = Q_NULLPTR);
	~CHashEntryWnd();

signals:
	void Closed();

private slots:

	void OnSaveAndClose();
	bool OnSave();

	void OnAddCollection();

protected:
	void closeEvent(QCloseEvent* e);

	bool Save();

	CHashPtr m_pEntry;

private:
	Ui::HashEntryWnd ui;
	class CWinboxMultiCombo* m_pEnclaves;
	class CWinboxMultiCombo* m_pCollections;
};
