#include "stdafx.h"
#include "CheckListDialog.h"

#include <QPushButton>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>


class CCheckListDialogPrivate
{
public:
    CCheckListDialogPrivate(QDialog *q)
        : clickedButton(0)
    {
        QSizePolicy sizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);

        pixmapLabel = new QLabel(q);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(pixmapLabel->sizePolicy().hasHeightForWidth());
        pixmapLabel->setSizePolicy(sizePolicy);
        pixmapLabel->setVisible(false);

        QSpacerItem *pixmapSpacer =
            new QSpacerItem(0, 5, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);

        messageLabel = new QLabel(q);
        messageLabel->setMinimumSize(QSize(300, 0));
        messageLabel->setWordWrap(true);
        messageLabel->setOpenExternalLinks(true);
        messageLabel->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard|Qt::LinksAccessibleByMouse);

        listWidget = new QListWidget(q);
		listWidget->setMinimumHeight(200);

		QHBoxLayout *selectionLayout = new QHBoxLayout();
		selectAllButton = new QPushButton(QObject::tr("Select All"), q);
		deselectAllButton = new QPushButton(QObject::tr("Deselect All"), q);
		selectionLayout->addWidget(selectAllButton);
		selectionLayout->addWidget(deselectAllButton);
		selectionLayout->addStretch();

        QSpacerItem *buttonSpacer =
            new QSpacerItem(0, 1, QSizePolicy::Minimum, QSizePolicy::Minimum);

        buttonBox = new QDialogButtonBox(q);
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        QVBoxLayout *verticalLayout = new QVBoxLayout();
        verticalLayout->addWidget(pixmapLabel);
        verticalLayout->addItem(pixmapSpacer);

        QHBoxLayout *horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->addLayout(verticalLayout);
        horizontalLayout_2->addWidget(messageLabel);

        QVBoxLayout *verticalLayout_2 = new QVBoxLayout(q);
        verticalLayout_2->addLayout(horizontalLayout_2);
        verticalLayout_2->addWidget(listWidget);
		verticalLayout_2->addLayout(selectionLayout);
        verticalLayout_2->addItem(buttonSpacer);
        verticalLayout_2->addWidget(buttonBox);
    }

    QLabel *pixmapLabel;
    QLabel *messageLabel;
    QListWidget *listWidget;
	QPushButton *selectAllButton;
	QPushButton *deselectAllButton;
    QDialogButtonBox *buttonBox;
    QAbstractButton *clickedButton;
};

CCheckListDialog::CCheckListDialog(QWidget *parent) :
    QDialog(parent),
    d(new CCheckListDialogPrivate(this))
{
    setModal(true);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    d->listWidget->setFocus();
	connect(d->selectAllButton, SIGNAL(clicked()), SLOT(onSelectAll()));
	connect(d->deselectAllButton, SIGNAL(clicked()), SLOT(onDeselectAll()));
    connect(d->buttonBox, SIGNAL(accepted()), SLOT(accept()));
    connect(d->buttonBox, SIGNAL(rejected()), SLOT(reject()));
    connect(d->buttonBox, SIGNAL(clicked(QAbstractButton*)),
        SLOT(slotClicked(QAbstractButton*)));
}

CCheckListDialog::~CCheckListDialog()
{
    delete d;
}

void CCheckListDialog::onSelectAll()
{
	for (int i = 0; i < d->listWidget->count(); ++i) {
		QListWidgetItem* item = d->listWidget->item(i);
		item->setCheckState(Qt::Checked);
	}
}

void CCheckListDialog::onDeselectAll()
{
	for (int i = 0; i < d->listWidget->count(); ++i) {
		QListWidgetItem* item = d->listWidget->item(i);
		item->setCheckState(Qt::Unchecked);
	}
}

void CCheckListDialog::slotClicked(QAbstractButton *b)
{
    d->clickedButton = b;
}

QAbstractButton *CCheckListDialog::clickedButton() const
{
    return d->clickedButton;
}

QDialogButtonBox::StandardButton CCheckListDialog::clickedStandardButton() const
{
    if (d->clickedButton)
        return d->buttonBox->standardButton(d->clickedButton);
    return QDialogButtonBox::NoButton;
}

QString CCheckListDialog::text() const
{
    return d->messageLabel->text();
}

void CCheckListDialog::setText(const QString &t)
{
    d->messageLabel->setText(t);
}

void CCheckListDialog::addItem(const QString& text, const QVariant& data, bool checked)
{
	QListWidgetItem* item = new QListWidgetItem(text, d->listWidget);
	item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
	item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
	item->setData(Qt::UserRole, data);
}

QList<int> CCheckListDialog::getCheckedIndices() const
{
	QList<int> indices;
	for (int i = 0; i < d->listWidget->count(); ++i) {
		if (d->listWidget->item(i)->checkState() == Qt::Checked)
			indices.append(i);
	}
	return indices;
}

QList<QVariant> CCheckListDialog::getCheckedData() const
{
	QList<QVariant> dataList;
	for (int i = 0; i < d->listWidget->count(); ++i) {
		QListWidgetItem* item = d->listWidget->item(i);
		if (item->checkState() == Qt::Checked)
			dataList.append(item->data(Qt::UserRole));
	}
	return dataList;
}

int CCheckListDialog::getCheckedCount() const
{
	int count = 0;
	for (int i = 0; i < d->listWidget->count(); ++i) {
		if (d->listWidget->item(i)->checkState() == Qt::Checked)
			count++;
	}
	return count;
}

QPixmap CCheckListDialog::iconPixmap() const
{
    return d->pixmapLabel->pixmap(Qt::ReturnByValue);
}

void CCheckListDialog::setIconPixmap(const QPixmap &p)
{
    d->pixmapLabel->setPixmap(p);
    d->pixmapLabel->setVisible(!p.isNull());
}

QDialogButtonBox::StandardButtons CCheckListDialog::standardButtons() const
{
    return d->buttonBox->standardButtons();
}

void CCheckListDialog::setStandardButtons(QDialogButtonBox::StandardButtons s)
{
    d->buttonBox->setStandardButtons(s);
}

QPushButton *CCheckListDialog::button(QDialogButtonBox::StandardButton b) const
{
    return d->buttonBox->button(b);
}

QPushButton *CCheckListDialog::addButton(const QString &text, QDialogButtonBox::ButtonRole role)
{
    return d->buttonBox->addButton(text, role);
}

QDialogButtonBox::StandardButton CCheckListDialog::defaultButton() const
{
    foreach (QAbstractButton *b, d->buttonBox->buttons())
        if (QPushButton *pb = qobject_cast<QPushButton *>(b))
            if (pb->isDefault())
               return d->buttonBox->standardButton(pb);
    return QDialogButtonBox::NoButton;
}

void CCheckListDialog::setDefaultButton(QDialogButtonBox::StandardButton s)
{
	if (QPushButton *b = d->buttonBox->button(s)) {
		b->setDefault(true);
		b->setFocus();
	}
}
