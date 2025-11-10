#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_ItemPicker.h"
#include "../Core/Programs/ProgramItem.h"


class CItemPicker : public QDialog
{
	Q_OBJECT

public:

	struct SItem
	{
		QString Name;
		QIcon Icon;
		QString Description;

		QMap<QVariant, SItem> SubItems;
	};

	CItemPicker(const QString& Prompt, const QMap<QVariant, SItem>& Items, QWidget *parent = Q_NULLPTR);
	~CItemPicker();

	QVariant GetItem();

private slots:
	void SetFilter(const QRegularExpression& Exp, int iOptions, int Column);
	void OnPick();

protected:
	void LoadList();
	int LoadSubList(QTreeWidgetItem* pParent, const QMap<QVariant, SItem>& SubItems);

	QMap<QVariant, SItem> m_Items;

private:
	Ui::ItemPicker ui;
	QRegularExpression m_FilterExp;
};
