#ifndef HCHECKLISTDIALOG_H
#define HCHECKLISTDIALOG_H

#include <QDialogButtonBox>
#include <QListWidget>

class CCheckListDialogPrivate;

#include "../mischelpers_global.h"

class MISCHELPERS_EXPORT CCheckListDialog: public QDialog
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(QPixmap iconPixmap READ iconPixmap WRITE setIconPixmap)
    Q_PROPERTY(QDialogButtonBox::StandardButtons buttons READ standardButtons WRITE setStandardButtons)
    Q_PROPERTY(QDialogButtonBox::StandardButton defaultButton READ defaultButton WRITE setDefaultButton)

public:
    explicit CCheckListDialog(QWidget *parent = NULL);
    virtual ~CCheckListDialog();

    QString text() const;
    void setText(const QString &);

	void addItem(const QString& text, const QVariant& data = QVariant(), bool checked = true);
	QList<int> getCheckedIndices() const;
	QList<QVariant> getCheckedData() const;
	int getCheckedCount() const;

    QDialogButtonBox::StandardButtons standardButtons() const;
    void setStandardButtons(QDialogButtonBox::StandardButtons s);
    QPushButton *button(QDialogButtonBox::StandardButton b) const;
    QPushButton *addButton(const QString &text, QDialogButtonBox::ButtonRole role);

    QDialogButtonBox::StandardButton defaultButton() const;
    void setDefaultButton(QDialogButtonBox::StandardButton s);

    // See static QMessageBox::standardPixmap()
    QPixmap iconPixmap() const;
    void setIconPixmap (const QPixmap &p);

    // Query the result
    QAbstractButton *clickedButton() const;
    QDialogButtonBox::StandardButton clickedStandardButton() const;

private slots:
    void slotClicked(QAbstractButton *b);
	void onSelectAll();
	void onDeselectAll();

private:
    CCheckListDialogPrivate *d;
};

#endif // HCHECKLISTDIALOG_H
