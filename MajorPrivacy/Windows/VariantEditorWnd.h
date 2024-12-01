#pragma once
#include "../Library/Common/XVariant.h"
#include "../../MiscHelpers/Common/AbstractTreeModel.h"


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
	/*void				WriteProperties(const CVariant& Root);
	void				WriteProperties(QTreeWidgetItem* pItem, const CVariant& Value, QMap<QTreeWidgetItem*,QWidget*>& Widgets);
	CVariant			ReadProperties();
	CVariant			ReadProperties(QTreeWidgetItem* pItem);*/

	/*enum EColumns
	{
		eKey = 0,
		eValue,
		eType,
	};*/

	CVariant			m_Root;	

	//QVariantMap			m_Request;
	bool				m_ReadOnly;

	QWidget*			m_pMainWidget;
	QVBoxLayout*		m_pMainLayout;

	//QTreeWidget*		m_pPropertiesTree;
	QTreeView*			m_pPropertiesTree;
	class CVariantModel*m_pVariantModel;

	//QDialogButtonBox*	m_pButtons;

	QMenu*				m_pFileMenu;

	QMenu*				m_pMenu;
	QAction*			m_pAdd;
	QAction*			m_pAddRoot;
	QAction*			m_pRemove;
};

//////////////////////////////////////////////////////////////////////////
// CVariantModel




class CVariantModel : public CAbstractTreeModel
{
	Q_OBJECT
public:
	CVariantModel(QObject* parent = nullptr);

	void			Update(const CVariant& pRoot);

	int				columnCount(const QModelIndex& parent = QModelIndex()) const { return eCount; }
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eValue,
		eCount
	};

protected:

	struct SVariantNode : SAbstractTreeNode
	{
		CVariant data;
	};

	SAbstractTreeNode* MkNode(const void* data, const QVariant& key = QVariant(), SAbstractTreeNode* parent = nullptr) override;

	void ForEachChild(SAbstractTreeNode* parentNode, const std::function<void(const QVariant& key, const void* data)>& func) override;

	void updateNodeData(SAbstractTreeNode* node, const void* data, const QModelIndex& nodeIndex) override;

	const void* childData(SAbstractTreeNode* node, const QVariant& key) override;
};

