#include "pch.h"
#include "VariantEditorWnd.h"
#include "../Core/PrivacyCore.h"
#include "../MajorPrivacy.h"
#include <QDateTimeEdit>
#include "../MiscHelpers/Common/SettingsWidgets.h"
#include "../Library/Common/FileIO.h"

/*QMap<QString, QVariant::Type> InitVarTypes() 
{
    QMap<QString, QVariant::Type> VarTypes;
	VarTypes["Map"] = QVariant::Map;
	VarTypes["List"] = QVariant::List;
	VarTypes["StringList"] = QVariant::StringList;
	VarTypes["Bool"] = QVariant::Bool;
	VarTypes["Char"] = QVariant::Char;
	VarTypes["Int"] = QVariant::Int;
	VarTypes["UInt"] = QVariant::UInt;
	VarTypes["LongLong"] = QVariant::LongLong;
	VarTypes["ULongLong"] = QVariant::ULongLong;
	VarTypes["Double"] = QVariant::Double;
	VarTypes["String"] = QVariant::String;
	VarTypes["ByteArray"] = QVariant::ByteArray;
	VarTypes["Date"] = QVariant::Date;
	VarTypes["Time"] = QVariant::Time;
	VarTypes["DateTime"] = QVariant::DateTime;
    return VarTypes;
}
QMap<QString, QVariant::Type> g_VarTypes = InitVarTypes();*/

CVariantEditorWnd::CVariantEditorWnd(QWidget *parent)
:QMainWindow(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);

	m_ReadOnly = false;

	m_pFileMenu = menuBar()->addMenu(tr("File"));
		m_pFileMenu->addAction(tr("Open File"), this, SLOT(OpenFile()));
		//m_pFileMenu->addAction(tr("Save File"), this, SLOT(SaveFile()));
		m_pFileMenu->addSeparator();
		m_pFileMenu->addAction(tr("Close Editor"), this, SLOT(close()));


	m_pMainWidget = new QWidget();
	m_pMainLayout = new QVBoxLayout();
	m_pMainWidget->setLayout(m_pMainLayout);
	setCentralWidget(m_pMainWidget);

	m_pPropertiesTree = new QTreeWidget();
	m_pPropertiesTree->setHeaderLabels(tr("Key|Value|Type").split("|"));
	//m_pPropertiesTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	QStyle* pStyle = QStyleFactory::create("windows");
	m_pPropertiesTree->setStyle(pStyle);
	m_pPropertiesTree->setSortingEnabled(true);
	
	m_pPropertiesTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pPropertiesTree, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));

	m_pMenu = new QMenu();

	m_pAddRoot = new QAction(tr("Add to Root"), m_pMenu);
	connect(m_pAddRoot, SIGNAL(triggered()), this, SLOT(OnAddRoot()));
	m_pMenu->addAction(m_pAddRoot);

	m_pAdd = new QAction(tr("Add"), m_pMenu);
	connect(m_pAdd, SIGNAL(triggered()), this, SLOT(OnAdd()));
	m_pMenu->addAction(m_pAdd);

	m_pRemove = new QAction(tr("Remove"), m_pMenu);
	connect(m_pRemove, SIGNAL(triggered()), this, SLOT(OnRemove()));
	m_pMenu->addAction(m_pRemove);

	m_pMainLayout->addWidget(m_pPropertiesTree);

	/*if(bWithButtons)
	{
		m_pButtons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Reset, Qt::Horizontal, this);
		QObject::connect(m_pButtons, SIGNAL(clicked(QAbstractButton *)), this, SLOT(OnClicked(QAbstractButton*)));
		m_pMainLayout->addWidget(m_pButtons);
	}
	else
		m_pButtons = NULL;*/
	
	setWindowState(Qt::WindowNoState);
	restoreGeometry(theConf->GetBlob("VariantEditor/Window_Geometry"));
	restoreState(theConf->GetBlob("VariantEditor/Window_State"));

	m_pPropertiesTree->header()->restoreState(theConf->GetBlob("VariantEditor/AccessView_Columns"));
}

CVariantEditorWnd::~CVariantEditorWnd()
{
	theConf->SetBlob("VariantEditor/AccessView_Columns",m_pPropertiesTree->header()->saveState());

	theConf->SetBlob("VariantEditor/Window_Geometry", saveGeometry());
	theConf->SetBlob("VariantEditor/Window_State", saveState());
}

void CVariantEditorWnd::OpenFile()
{
	QString FileName = QFileDialog::getOpenFileName(this, tr("Open File"), theCore->GetConfigDir(), tr("All Files (*.dat)"));
	if (FileName.isEmpty())
		return;

	CBuffer Buffer;
	if (!ReadFile(FileName.toStdWString(), 0, Buffer)) {
		QMessageBox::critical(this, tr("Error"), tr("Can't open file"));
		return;
	}

	CVariant Data;
	//try {
	auto ret = Data.FromPacket(&Buffer, true);
	//} catch (const CException&) {
	//	return ERR(STATUS_UNSUCCESSFUL);
	//}
	if (ret != CVariant::eErrNone) {
		QMessageBox::critical(this, tr("Error"), tr("Can't parse file"));
		return;
	}

	WriteProperties(Data);

}

void CVariantEditorWnd::SaveFile()
{
	QString FileName = QFileDialog::getSaveFileName(this, tr("Save File"), "", tr("All Files (*.dat)"));
	if (FileName.isEmpty())
		return;

	CVariant Data = ReadProperties();
	CBuffer Buffer;
	Data.ToPacket(&Buffer);
	WriteFile(FileName.toStdWString(), 0, Buffer);
}

void CVariantEditorWnd::WriteProperties(const CVariant& Root)
{
	m_pPropertiesTree->clear();

	QMap<QTreeWidgetItem*,QWidget*> Widgets; // cache for performance

	QList<QTreeWidgetItem*> Items;
	for(int i=0; i < Root.Count(); i++)
	{
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		const char* Key = Root.Key(i);
		pItem->setText(eKey, Key);
		WriteProperties(pItem, Root[Key], Widgets);
		Items.append(pItem);
	}
	m_pPropertiesTree->addTopLevelItems(Items);

	foreach(QTreeWidgetItem* pNewItem, Widgets.keys())
	{
		if(m_ReadOnly)
		{
			if(QLineEdit* pEdit = qobject_cast<QLineEdit*>(Widgets[pNewItem]))
				pEdit->setReadOnly(true);
			else if(QPlainTextEdit* pEdit = qobject_cast<QPlainTextEdit*>(Widgets[pNewItem]))
				pEdit->setReadOnly(true);
			else if(QDateTimeEdit* pEdit = qobject_cast<QDateTimeEdit*>(Widgets[pNewItem]))
				pEdit->setReadOnly(true);
			else if(QCheckBox* pCheck = qobject_cast<QCheckBox*>(Widgets[pNewItem]))
				pEdit->setEnabled(false);
		}

		m_pPropertiesTree->setItemWidget(pNewItem, eValue, Widgets[pNewItem]);
	}

	//m_pPropertiesTree->expandAll();
	//m_pPropertiesTree->resizeColumnToContents(eKey);
	//m_pPropertiesTree->resizeColumnToContents(eValue);
	//m_pPropertiesTree->resizeColumnToContents(eType);
}

void CVariantEditorWnd::WriteProperties(QTreeWidgetItem* pItem, const CVariant& Value, QMap<QTreeWidgetItem*, QWidget*>& Widgets)
{
#ifdef _DEBUG
	if(QApplication::keyboardModifiers() & Qt::ControlModifier)
		return;
#endif

	CVariant::EType Type = Value.GetType();	
	pItem->setData(eValue, Qt::UserRole, (int)Type);
	//pItem->setText(eType, );
	QList<QTreeWidgetItem*> Items;
	switch(Value.GetType())
	{
		case VAR_TYPE_MAP: {
			for(int i=0; i < Value.Count(); i++)
			{
				QTreeWidgetItem* pSubItem = new QTreeWidgetItem();
				const char* Key = Value.Key(i);
				pSubItem->setText(eKey, Key);
				WriteProperties(pSubItem, Value[Key], Widgets);
				Items.append(pSubItem);
			}
			break;
		}
		case VAR_TYPE_LIST: {
			for(int i=0; i < Value.Count(); i++)
			{
				QTreeWidgetItem* pSubItem = new QTreeWidgetItem();
				pSubItem->setText(eKey, QString::number(i));
				WriteProperties(pSubItem, Value[i], Widgets);
				Items.append(pSubItem);
			}
			break;
		}
		case VAR_TYPE_INDEX: {
			for(int i=0; i < Value.Count(); i++)
			{
				QTreeWidgetItem* pSubItem = new QTreeWidgetItem();
				uint32 Id = Value.Id(i);
				QString Name = QString::fromLatin1((char*)&Id, 4);
				std::reverse(Name.begin(), Name.end());
				pSubItem->setText(eKey, Name);
				WriteProperties(pSubItem, Value[Id], Widgets);
				Items.append(pSubItem);
			}
			break;
		}

		case VAR_TYPE_BYTES: {
			QLineEdit* pEdit = new QLineEdit();
			pEdit->setText(QByteArray((char*)Value.GetData(), Value.GetSize()).toBase64());
			Widgets[pItem] = pEdit;
			break;
		}

		case VAR_TYPE_UNICODE:
		case VAR_TYPE_UTF8:
		case VAR_TYPE_ASCII: {
			QLineEdit* pEdit = new QLineEdit();
			pEdit->setText(QString::fromStdWString(Value.AsStr()));
			Widgets[pItem] = pEdit;
			break;
		}

		case VAR_TYPE_SINT: {
			QLineEdit* pEdit = new QLineEdit();
			pEdit->setText(QString::number(Value.AsNum<sint64>()));
			Widgets[pItem] = pEdit;
			break;
		}
		case VAR_TYPE_UINT: {
			QLineEdit* pEdit = new QLineEdit();
			pEdit->setText(QString::number(Value.AsNum<uint64>()));
			Widgets[pItem] = pEdit;
			break;
		}

		//case VAR_TYPE_DOUBLE: // todo
		//	return double(*this);

		case VAR_TYPE_EMPTY:
		default:
			break;
	}
	pItem->addChildren(Items);
}

CVariant CVariantEditorWnd::ReadProperties() 
{
	return CVariant();
}

CVariant CVariantEditorWnd::ReadProperties(QTreeWidgetItem* pItem)
{
	return CVariant();
}

/*void CVariantEditorWnd::OnClicked(QAbstractButton* pButton)
{
	ASSERT(m_pButtons);
	switch(m_pButtons->buttonRole(pButton))
	{
	case QDialogButtonBox::ApplyRole:
		ApplySettings();
	case QDialogButtonBox::ResetRole: // reset after apply to check if all values ware accepted properly
		UpdateSettings();
		break;
	}
}*/

/*void CVariantEditorWnd::UpdateSettings()
{
	if(m_ReadOnly)
		return;

	// todo
}*/

/*void CVariantEditorWnd::WriteProperties(const QVariantMap& Root)
{
	m_pPropertiesTree->clear();

	QMap<QTreeWidgetItem*,QWidget*> Widgets; // cache for performance

	foreach(const QString& Key, Root.keys())
	{
		QTreeWidgetItem* pItem = new QTreeWidgetItem();
		pItem->setText(eKey, Key);
		WriteProperties(pItem, Root[Key], Widgets);
		m_pPropertiesTree->addTopLevelItem(pItem);
	}

	foreach(QTreeWidgetItem* pNewItem, Widgets.keys())
	{
		if(m_ReadOnly)
		{
			if(QLineEdit* pEdit = qobject_cast<QLineEdit*>(Widgets[pNewItem]))
				pEdit->setReadOnly(true);
			else if(QPlainTextEdit* pEdit = qobject_cast<QPlainTextEdit*>(Widgets[pNewItem]))
				pEdit->setReadOnly(true);
			else if(QDateTimeEdit* pEdit = qobject_cast<QDateTimeEdit*>(Widgets[pNewItem]))
				pEdit->setReadOnly(true);
			else if(QCheckBox* pCheck = qobject_cast<QCheckBox*>(Widgets[pNewItem]))
				pEdit->setEnabled(false);
		}

		m_pPropertiesTree->setItemWidget(pNewItem, eValue, Widgets[pNewItem]);
	}

	m_pPropertiesTree->expandAll();
	m_pPropertiesTree->resizeColumnToContents(eKey);
	m_pPropertiesTree->resizeColumnToContents(eValue);
	m_pPropertiesTree->resizeColumnToContents(eType);
}

void CVariantEditorWnd::WriteProperties(QTreeWidgetItem* pItem, const QVariant& Value, QMap<QTreeWidgetItem*,QWidget*>& Widgets)
{
	QVariant::Type Type = Value.type();
	pItem->setData(eValue, Qt::UserRole, (int)Type);
	pItem->setText(eType, g_VarTypes.key(Type));
	switch(Type)
	{
		case QVariant::Invalid:
			pItem->setText(eType, "Missing");
			break;
		case QVariant::Map:
		//case QVariant::Hash:
		{
			QVariantMap Map = Value.toMap();
			foreach(const QString& Key, Map.keys())
			{
				QTreeWidgetItem* pSubItem = new QTreeWidgetItem();
				pSubItem->setText(eKey, Key);
				pItem->addChild(pSubItem);
				WriteProperties(pSubItem, Map[Key], Widgets);
			}
			break;
		}
		case QVariant::List:
		{
			QVariantList List = Value.toList();
			foreach(const QVariant& SubValue, List)
			{
				QTreeWidgetItem* pSubItem = new QTreeWidgetItem();
				pSubItem->setText(eKey, "");
				pItem->addChild(pSubItem);
				WriteProperties(pSubItem, SubValue, Widgets);
			}
			break;
		}
		case QVariant::StringList:
		{
			CMultiLineEdit* pMultiLineEdit = new CMultiLineEdit();
			pMultiLineEdit->SetLines(Value.toStringList());	
			Widgets[pItem] = pMultiLineEdit;
			break;
		}
		case QVariant::Bool:
		{
			QCheckBox* pCheck = new QCheckBox();
			pCheck->setChecked(Value.toBool());
			Widgets[pItem] = pCheck;
			break;
		}
		case QVariant::Char:
		case QVariant::Int:
		case QVariant::UInt:
		case QVariant::LongLong:
		case QVariant::ULongLong:
		case QVariant::Double:
		{
			QLineEdit* pEdit = new QLineEdit();
			pEdit->setText(Value.toString());
			Widgets[pItem] = pEdit;
			break;
		}
		case QVariant::String:
		{
			QLineEdit* pEdit = new QLineEdit();
			pEdit->setText(Value.toString());
			Widgets[pItem] = pEdit;
			break;
		}
		case QVariant::ByteArray:
		{
			QLineEdit* pEdit = new QLineEdit();
			pEdit->setText(Value.toByteArray().toBase64());
			Widgets[pItem] = pEdit;
			break;
		}
		//case QVariant::BitArray:
		//case QVariant::Url:
		case QVariant::Date:
		{
			QDateTimeEdit* pDateTimeEdit = new QDateTimeEdit();
			pDateTimeEdit->setDate(Value.toDate());
			Widgets[pItem] = pDateTimeEdit;
			break;
		}
		case QVariant::Time:
		{
			QDateTimeEdit* pDateTimeEdit = new QDateTimeEdit();
			pDateTimeEdit->setTime(Value.toTime());
			Widgets[pItem] = pDateTimeEdit;
			break;
		}
		case QVariant::DateTime:
		{
			QDateTimeEdit* pDateTimeEdit = new QDateTimeEdit();
			pDateTimeEdit->setDateTime(Value.toDateTime());
			Widgets[pItem] = pDateTimeEdit;
			break;
		}
		default:
			ASSERT(0);
	}
}

QVariantMap CVariantEditorWnd::ReadProperties()
{
	QVariantMap Root;
	for(int i=0; i < m_pPropertiesTree->topLevelItemCount(); i++)
	{
		QTreeWidgetItem* pItem = m_pPropertiesTree->topLevelItem(i);
		Root[pItem->text(0)] = ReadProperties(pItem);
	}
	return Root;
}

QVariant CVariantEditorWnd::ReadProperties(QTreeWidgetItem* pItem)
{
	QVariant::Type Type = (QVariant::Type)pItem->data(1, Qt::UserRole).toInt();
	switch(Type)
	{
		case QVariant::Invalid:
			return QVariant(); // this means delete the value
		case QVariant::Map:
		//case QVariant::Hash:
		{
			QVariantMap Map;
			for(int i=0; i < pItem->childCount(); i++)
			{
				QTreeWidgetItem* pSubItem = pItem->child(i);
				Map.insert(pSubItem->text(0), ReadProperties(pSubItem));
			}
			return Map;
		}
		case QVariant::List:
		{
			QVariantList List;
			for(int i=0; i < pItem->childCount(); i++)
			{
				QTreeWidgetItem* pSubItem = pItem->child(i);
				List.append(ReadProperties(pSubItem));
			}
			return List;
		}
		case QVariant::StringList:
		{
			CMultiLineEdit* pMultiLineEdit = (CMultiLineEdit*)m_pPropertiesTree->itemWidget(pItem, 1);
			return pMultiLineEdit->GetLines();
		}
		case QVariant::Bool:
		{
			QCheckBox* pCheck = (QCheckBox*)m_pPropertiesTree->itemWidget(pItem, 1);
			return pCheck->isChecked();
		}
		case QVariant::Char:
		case QVariant::Int:
		case QVariant::UInt:
		case QVariant::LongLong:
		case QVariant::ULongLong:
		case QVariant::Double:
		{
			QLineEdit* pEdit = (QLineEdit*)m_pPropertiesTree->itemWidget(pItem, 1);
			QVariant Value = pEdit->text();
			Value.convert(Type);
			return Value;
		}
		case QVariant::String:
		{
			QLineEdit* pEdit = (QLineEdit*)m_pPropertiesTree->itemWidget(pItem, 1);
			return pEdit->text();
		}
		case QVariant::ByteArray:
		{
			QLineEdit* pEdit = (QLineEdit*)m_pPropertiesTree->itemWidget(pItem, 1);
			return QByteArray::fromBase64(pEdit->text().toLatin1());
		}
		//case QVariant::BitArray:
		//case QVariant::Url:
		case QVariant::Date:
		{
			QDateTimeEdit* pDateTimeEdit = (QDateTimeEdit*)m_pPropertiesTree->itemWidget(pItem, 1);
			return pDateTimeEdit->date();
		}
		case QVariant::Time:
		{
			QDateTimeEdit* pDateTimeEdit = (QDateTimeEdit*)m_pPropertiesTree->itemWidget(pItem, 1);
			return pDateTimeEdit->time();
		}
		case QVariant::DateTime:
		{
			QDateTimeEdit* pDateTimeEdit = (QDateTimeEdit*)m_pPropertiesTree->itemWidget(pItem, 1);
			return pDateTimeEdit->dateTime();
		}
		default:
			ASSERT(0);
	}
	return QVariant();
}*/

/*void CVariantEditorWnd::ApplySettings()
{
	if(m_ReadOnly)
		return;

	// todo
}*/

void CVariantEditorWnd::OnMenuRequested(const QPoint &point)
{
	if(m_ReadOnly)
		return;

	QTreeWidgetItem* pItem = m_pPropertiesTree->currentItem();

	m_pRemove->setEnabled(pItem != 0);

	QVariant::Type Type = pItem ? (QVariant::Type)pItem->data(1, Qt::UserRole).toInt() : QVariant::Map;
	m_pAdd->setEnabled(Type == QVariant::Invalid || Type == QVariant::Map || Type == QVariant::Hash || Type == QVariant::List);

	m_pMenu->popup(QCursor::pos());	
}

/*class CAddValueDialog : public QDialog
{
	//Q_OBJECT

public:
	CAddValueDialog(bool bNamed, QWidget *pMainWindow = NULL)
		: QDialog(pMainWindow)
	{
		setWindowTitle(CVariantEditorWnd::tr("Add Value"));

		m_pMainLayout = new QFormLayout(this);

		if(bNamed)
		{
			m_pName = new QLineEdit();
			m_pName->setMaximumWidth(200);
			m_pMainLayout->setWidget(0, QFormLayout::LabelRole, new QLabel(CVariantEditorWnd::tr("Name:")));
			m_pMainLayout->setWidget(0, QFormLayout::FieldRole, m_pName);
		}
		else
			m_pName = NULL;

		m_pType = new QComboBox();
		m_pType->addItems(g_VarTypes.keys());
		m_pType->setMaximumWidth(200);
		m_pMainLayout->setWidget(1, QFormLayout::LabelRole, new QLabel(CVariantEditorWnd::tr("Type:")));
		m_pMainLayout->setWidget(1, QFormLayout::FieldRole, m_pType);

		m_pButtonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
		QObject::connect(m_pButtonBox, SIGNAL(accepted()), this, SLOT(accept()));
		QObject::connect(m_pButtonBox, SIGNAL(rejected()), this, SLOT(reject()));
		m_pMainLayout->setWidget(2, QFormLayout::FieldRole, m_pButtonBox);
	}

	QString				GetName()	{return m_pName ? m_pName->text() : "";}
	QVariant::Type		GetType()	{return g_VarTypes.value(m_pType->currentText(), QVariant::Invalid);}

protected:
	QComboBox*			m_pType;
	QLineEdit*			m_pName;
	QDialogButtonBox*	m_pButtonBox;
	QFormLayout*		m_pMainLayout;
};*/

void CVariantEditorWnd::OnAdd()
{
	/*QTreeWidgetItem* pItem = m_pPropertiesTree->currentItem();

	QVariant::Type Type = pItem ? (QVariant::Type)pItem->data(1, Qt::UserRole).toInt() : QVariant::Map;
	CAddValueDialog AddValueDialog(Type != QVariant::List && Type != QVariant::Invalid);
	if(!AddValueDialog.exec())
		return;

	QTreeWidgetItem* pNewItem;
	if(Type == QVariant::Invalid)
		pNewItem = pItem;
	else
	{
		pNewItem = new QTreeWidgetItem();
		pNewItem->setText(eKey, AddValueDialog.GetName());
		if(pItem)
			pItem->addChild(pNewItem);
		else
			m_pPropertiesTree->addTopLevelItem(pNewItem);
	}

	QVariant NewValue(AddValueDialog.GetType());
	QMap<QTreeWidgetItem*,QWidget*> Widgets;
	WriteProperties(pNewItem, NewValue, Widgets); // add proper controll element
	foreach(QTreeWidgetItem* pNewItem, Widgets.keys())
		m_pPropertiesTree->setItemWidget(pNewItem, eValue, Widgets[pNewItem]);*/
}

void CVariantEditorWnd::OnAddRoot()
{
	/*CAddValueDialog AddValueDialog(true);
	if(!AddValueDialog.exec())
		return;

	QTreeWidgetItem* pNewItem;
	pNewItem = new QTreeWidgetItem();
	pNewItem->setText(eKey, AddValueDialog.GetName());
	m_pPropertiesTree->addTopLevelItem(pNewItem);

	QVariant NewValue(AddValueDialog.GetType());
	QMap<QTreeWidgetItem*,QWidget*> Widgets;
	WriteProperties(pNewItem, NewValue, Widgets); // add proper controll element
	foreach(QTreeWidgetItem* pNewItem, Widgets.keys())
		m_pPropertiesTree->setItemWidget(pNewItem, eValue, Widgets[pNewItem]);*/
}

void CVariantEditorWnd::OnRemove()
{
	/*QTreeWidgetItem* pItem = m_pPropertiesTree->currentItem();
	for(int i=0; i < pItem->childCount(); i++)
		delete pItem->child(i);

	pItem->setData(eValue, Qt::UserRole, (int)QVariant::Invalid);
	pItem->setText(eType, "Removed");
	m_pPropertiesTree->setItemWidget(pItem, eValue, NULL);*/
}