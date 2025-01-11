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

	void PickIcon();
	void BrowseImage();
	//void ResetIcon();

protected:
	void closeEvent(QCloseEvent* e);

	bool Save();

	CEnclavePtr m_pEnclave;
	QString m_IconFile;

private:
	Ui::EnclaveWnd ui;
};
