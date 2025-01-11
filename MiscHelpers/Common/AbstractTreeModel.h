#pragma once
#include "TreeViewEx.h"

#include "../mischelpers_global.h"

class MISCHELPERS_EXPORT CAbstractTreeModel : public QAbstractItemModelEx 
{
	Q_OBJECT
public:
	CAbstractTreeModel(QObject* parent = nullptr);
	~CAbstractTreeModel();

	void Clean();

	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& index) const override;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override = 0;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override = 0;

	QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

	bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;

	bool canFetchMore(const QModelIndex& parent) const override;
	void fetchMore(const QModelIndex& parent) override;

protected:

	struct SAbstractTreeNode
	{
		virtual ~SAbstractTreeNode() {}

		QVariant key;

		struct SValue
		{
			QVariant Raw;
			QVariant Formatted;
		};
		QVector<SValue>		values;
		QIcon				icon;
		int childCount = 0;

		SAbstractTreeNode* parentNode = nullptr;

		bool hasFetchedChildren = false;
		QVector<SAbstractTreeNode*> childNodes;
	};

	void Update(const void* data);

	virtual SAbstractTreeNode* MkNode(const void* data, const QVariant& key = QVariant(), SAbstractTreeNode* parent = nullptr) = 0;

	virtual void ForEachChild(SAbstractTreeNode* parentNode, const std::function<void(const QVariant& key, const void* data)>& func) = 0;

	SAbstractTreeNode* nodeFromIndex(const QModelIndex& index) const;

	int rowFromNode(SAbstractTreeNode* node) const;

	int nodeChildCount(SAbstractTreeNode* node) const;
	bool canFetchMore(SAbstractTreeNode* node) const;

	void updateNode(SAbstractTreeNode* node, const void* data, const QModelIndex& nodeIndex);
	virtual void updateNodeData(SAbstractTreeNode* node, const void* data, const QModelIndex& nodeIndex) = 0;
	virtual const void* childData(SAbstractTreeNode* node, const QVariant& key) = 0;

	virtual QVariant data(SAbstractTreeNode* node, int column, int role = Qt::DisplayRole) const;

	SAbstractTreeNode* m_rootNode;
};
