#pragma once
#include "../Library/Common/XVariant.h"

class CVariantEditorWnd: public QMainWindow
{
	Q_OBJECT

public:
	CVariantEditorWnd(QWidget *parent = 0);
	~CVariantEditorWnd();

private slots:
	//void				OnClicked(QAbstractButton* pButton);
	//void				UpdateSettings();
	//void				ApplySettings();

	void				OpenFile();
	void				SaveFile();

	void				OnMenuRequested(const QPoint &point);

	void				OnAdd();
	void				OnAddRoot();
	void				OnRemove();

protected:
	friend class CPropertiesGetJob;

	/*void				WriteProperties(const QVariantMap& Root);
	void				WriteProperties(QTreeWidgetItem* pItem, const QVariant& Value, QMap<QTreeWidgetItem*,QWidget*>& Widgets);
	QVariantMap			ReadProperties();
	QVariant			ReadProperties(QTreeWidgetItem* pItem);*/
	void				WriteProperties(const CVariant& Root);
	void				WriteProperties(QTreeWidgetItem* pItem, const CVariant& Value, QMap<QTreeWidgetItem*,QWidget*>& Widgets);
	CVariant			ReadProperties();
	CVariant			ReadProperties(QTreeWidgetItem* pItem);

	enum EColumns
	{
		eKey = 0,
		eValue,
		eType,
	};

	QVariantMap			m_Request;
	bool				m_ReadOnly;

	QWidget*			m_pMainWidget;
	QVBoxLayout*		m_pMainLayout;

	QTreeWidget*		m_pPropertiesTree;

	//QDialogButtonBox*	m_pButtons;

	QMenu*				m_pFileMenu;

	QMenu*				m_pMenu;
	QAction*			m_pAdd;
	QAction*			m_pAddRoot;
	QAction*			m_pRemove;
};