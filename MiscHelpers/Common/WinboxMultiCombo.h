// WinboxMultiCombo.h
// A Qt6 widget that mimics MikroTik Winbox-style multi-selection using
// repeatable QComboBoxes with up/down (add/remove) arrow buttons.
// Single-header, header-only implementation.
//
// Usage example:
//   QList<QPair<QString,QVariant>> items = {
//       {"Disabled", 0}, {"TCP", 6}, {"UDP", 17}, {"SCTP", 132}
//   };
//   QList<QVariant> initial = {6, 17};
//
//   auto *w = new WinboxMultiCombo(items, initial, this);
//   connect(w, &WinboxMultiCombo::valuesChanged, this, [](const QList<QVariant>& v){
//       qDebug() << "valuesChanged ->" << v;
//   });
//
//   // Later:
//   QList<QVariant> selected = w->values();
//   w->setValues({132});
//
// Notes:
//  * The Down (▼) button adds a new row below the current one.
//  * The Up (▲) button removes that specific row.
//  * By default at least one row is kept (minRows = 1). You can change this via setMinimumRows(0).
//  * setItems() defines the shared item list (label + data) used by every combo.
//  * setValues() rebuilds rows based on the provided list of QVariant datas.
//  * values() returns the list of current datas, skipping invalid/placeholder items.
//
// Requires: Qt 6.x
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QWidget>
#include <QComboBox>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QStyle>
#include <QVariant>
#include <QList>
#include <QPointer>
#include <QSignalBlocker>
#include <QLabel>
#include <QSpacerItem>

#include "../mischelpers_global.h"

class MISCHELPERS_EXPORT CWinboxMultiCombo : public QWidget 
{
    Q_OBJECT
public:
    struct Item {
        QString  label;
        QVariant data;
    };

    explicit CWinboxMultiCombo(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        initUi();
    }

    explicit CWinboxMultiCombo(const QList<QPair<QString, QVariant>>& items,
                              const QList<QVariant>& initialValues = {},
                              QWidget *parent = nullptr)
        : QWidget(parent)
    {
        initUi();
        QList<Item> conv;
        conv.reserve(items.size());
        for (const auto &p : items) conv.push_back({p.first, p.second});
        setItems(conv);
        setValues(initialValues);
    }

    // Define the shared item list used by each combo.
    void setItems(const QList<Item>& items)
    {
        m_items = items;
        refreshAllCombos();
    }

    void setItems(const QList<QPair<QString,QVariant>>& items)
    {
        QList<Item> conv; conv.reserve(items.size());
        for (const auto &p : items) conv.push_back({p.first, p.second});
        setItems(conv);
    }

    QList<Item> items() const { return m_items; }

    // Adjust the minimum number of rows kept in the widget (default 1).
    void setMinimumRows(int n)
    {
        m_minRows = qMax(0, n);
        ensureMinimumRows();
    }

    int minimumRows() const { return m_minRows; }

    // Placeholder shown as the first item (invalid data). Empty => no placeholder.
    void setPlaceholderText(const QString& text)
    {
        m_placeholder = text;
        refreshAllCombos();
    }

    QString placeholderText() const { return m_placeholder; }

    // Allow duplicate selections across rows (default: true).
    void setAllowDuplicates(bool allow)
    {
        m_allowDuplicates = allow;
        // No immediate refresh needed; enforcement happens on selection changes.
    }

    bool allowDuplicates() const { return m_allowDuplicates; }

    // Replace the entire set of values; rows are rebuilt to match count.
    void setValues(const QList<QVariant>& values)
    {
        // Rebuild rows to match values.size(), then select each by data.
        rebuildRows(values.size());
        // Set selection for each row
        const int n = values.size();
        for (int i = 0; i < n; ++i) {
            setRowData(i, values[i]);
        }
        emitValuesChanged();
    }

    // Return the current list of valid datas (skips placeholder/invalid rows).
    QList<QVariant> values() const
    {
        QList<QVariant> out;
        out.reserve(m_rows.size());
        for (const auto &r : m_rows) {
            if (!r.combo) continue;
            QVariant d = r.combo->currentData();
            if (d.isValid()) out.push_back(d);
        }
        return out;
    }

signals:
    void valuesChanged(const QList<QVariant>& values);

private slots:
    void OnIndexChanged()
    {
        QComboBox* combo = (QComboBox*)sender();

        // Enforce no-duplicates if requested
        if (!m_allowDuplicates) {
            const QVariant d = combo->currentData();
            if (d.isValid()) {
                // If any other combo already has this data, revert this selection to placeholder/invalid.
                int count = 0;
                for (const auto &row : m_rows) {
                    if (row.combo && row.combo->currentData() == d) ++count;
                }
                if (count > 1) {
                    QSignalBlocker b(combo);
                    // Set to placeholder (or -1) if duplicate detected
                    combo->setCurrentIndex(m_placeholder.isEmpty() ? -1 : 0);
                }
            }
        }
        emitValuesChanged();
    }

private:
    struct Row {
        QWidget    *w = nullptr;
        QComboBox  *combo = nullptr;
        QToolButton *btnAdd = nullptr;    // ▼ add
        QToolButton *btnRemove = nullptr; // ▲ remove
    };

    QList<Item> m_items;
    QList<Row>  m_rows;

    QVBoxLayout *m_vbox = nullptr; // holds all rows

    int     m_minRows = 1;
    bool    m_allowDuplicates = true;
    QString m_placeholder; // optional placeholder text

    // ----- UI helpers -----
    void initUi()
    {
        m_vbox = new QVBoxLayout();
        m_vbox->setContentsMargins(0,0,0,0);
        m_vbox->setSpacing(4);
        setLayout(m_vbox);

        ensureMinimumRows();
    }

    void refreshAllCombos()
    {
        for (int i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].combo)
                refillCombo(m_rows[i].combo);
        }
    }

    void refillCombo(QComboBox *cb)
    {
        QSignalBlocker b(cb);
        cb->clear();
        if (!m_placeholder.isEmpty()) {
            cb->addItem(m_placeholder, QVariant()); // invalid data for placeholder
        }
        for (const auto &it : m_items) {
            cb->addItem(it.label, it.data);
        }
    }

    int indexForData(const QVariant& data) const
    {
        if (!m_placeholder.isEmpty()) {
            // actual items start at index 1
            for (int i = 0; i < m_items.size(); ++i) {
                if (m_items[i].data == data) return i + 1;
            }
            return -1;
        } else {
            for (int i = 0; i < m_items.size(); ++i) {
                if (m_items[i].data == data) return i;
            }
            return -1;
        }
    }

    QVariant dataForIndex(int comboIndex) const
    {
        if (comboIndex < 0) return QVariant();
        if (!m_placeholder.isEmpty()) {
            if (comboIndex == 0) return QVariant();
            int idx = comboIndex - 1;
            if (idx >= 0 && idx < m_items.size()) return m_items[idx].data;
            return QVariant();
        } else {
            if (comboIndex >= 0 && comboIndex < m_items.size()) return m_items[comboIndex].data;
            return QVariant();
        }
    }

    void setRowData(int row, const QVariant& data)
    {
        if (row < 0 || row >= m_rows.size()) return;
        Row &r = m_rows[row];
        if (!r.combo) return;
        const int idx = indexForData(data);
        QSignalBlocker b(r.combo);
        r.combo->setCurrentIndex(idx);
    }

    void ensureMinimumRows()
    {
        while (m_rows.size() < qMax(1, m_minRows)) addRow(m_rows.size());
        // If m_minRows == 0 and no rows exist yet, leave it empty until first add.
    }

    void rebuildRows(int count)
    {
        // Remove extra rows
        while (m_rows.size() > count && m_rows.size() > m_minRows) {
            removeRow(m_rows.size() - 1, /*force*/true);
        }
        // Add missing rows
        while (m_rows.size() < count) {
            addRow(m_rows.size());
        }
    }

    Row makeRowWidgets()
    {
        Row r;
        r.w = new QWidget(this);
        auto *h = new QHBoxLayout(r.w);
        h->setContentsMargins(0,0,0,0);
        //h->setSpacing(3);

        r.combo = new QComboBox(r.w);
        r.combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        refillCombo(r.combo);

        // Vertical box for the two arrow buttons
        auto *vb = new QVBoxLayout();
        vb->setContentsMargins(0,0,0,0);
        vb->setSpacing(0);

        r.btnRemove = new QToolButton(r.w);
        r.btnRemove->setToolTip(tr("Remove"));
        //r.btnRemove->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
		r.btnRemove->setText(tr("▲"));
        r.btnRemove->setAutoRaise(true);
        r.btnRemove->setFixedSize(22, 12);

        r.btnAdd = new QToolButton(r.w);
        r.btnAdd->setToolTip(tr("Add"));
        //r.btnAdd->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
		r.btnAdd->setText(tr("▼"));
        r.btnAdd->setAutoRaise(true);
        r.btnAdd->setFixedSize(22, 11);

        vb->addWidget(r.btnRemove);
        vb->addWidget(r.btnAdd);
        vb->addStretch(1);

        h->addWidget(r.combo);
        h->addLayout(vb);

        return r;
    }

    void addRow(int insertIndex, const QVariant& initialData = QVariant())
    {
        Row r = makeRowWidgets();

        // Insert into vector and layout
        if (insertIndex < 0 || insertIndex > m_rows.size()) insertIndex = m_rows.size();
        m_rows.insert(insertIndex, r);

        m_vbox->insertWidget(insertIndex, r.w);

        // Connect signals (capture row pointer by value)
        QObject::connect(r.combo, SIGNAL(currentIndexChanged(int index)), SLOT(OnIndexChanged()));

        QObject::connect(r.btnAdd, &QToolButton::clicked, this, [this, r]() {
            // Add a new empty row below this one
            int idx = indexOfRowWidget(r.w);
            if(m_rows.count() >= m_items.count())
				return; // dont add more rows then we have items
            addRow(idx + 1);
            emitValuesChanged();
        });

        QObject::connect(r.btnRemove, &QToolButton::clicked, this, [this, r]() {
            int idx = indexOfRowWidget(r.w);
            if (idx >= 0) removeRow(idx);
        });

        // Set initial selection if provided
        if (initialData.isValid()) {
            setRowData(insertIndex, initialData);
        } else if (!m_placeholder.isEmpty()) {
            QSignalBlocker b(r.combo);
            r.combo->setCurrentIndex(0); // placeholder
        }
    }

    void removeRow(int index, bool force = false)
    {
        if (index < 0 || index >= m_rows.size()) return;
        if (!force && m_rows.size() <= qMax(1, m_minRows)) return; // keep at least minRows (or 1 if minRows==0)

        Row r = m_rows.takeAt(index);
        if (r.w) {
            r.w->deleteLater();
        }
        emitValuesChanged();
    }

    int indexOfRowWidget(QWidget *w) const
    {
        for (int i = 0; i < m_rows.size(); ++i) if (m_rows[i].w == w) return i;
        return -1;
    }

    void emitValuesChanged()
    {
        emit valuesChanged(values());
    }
};
