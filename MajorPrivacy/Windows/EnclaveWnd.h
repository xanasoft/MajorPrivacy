#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_EnclaveWnd.h"
#include "../Core/Enclaves/Enclave.h"

class CEnclaveWnd : public QDialog
{
	Q_OBJECT

public:
	CEnclaveWnd(const CEnclavePtr& pEnclave, QWidget *parent = Q_NULLPTR);
	~CEnclaveWnd();

signals:
	void Closed();

private slots:

	void OnNameChanged(const QString& Text);
	
	void OnSaveAndClose();
	bool OnSave();

	void OnAddCollection();

	void PickIcon();
	void BrowseImage();
	//void ResetIcon();

	void EditScript();

protected:
	void closeEvent(QCloseEvent* e);

	bool Save();

	CEnclavePtr m_pEnclave;
	QString m_IconFile;

	QString m_Script;

private:
	Ui::EnclaveWnd ui;
	class CWinboxMultiCombo* m_pCollections;

};
