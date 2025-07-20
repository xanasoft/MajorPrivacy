#include "stdafx.h"
#include "AbstractTreeModel.h"
#include "Common.h"


CAbstractTreeModel::CAbstractTreeModel(QObject* parent)
	: QAbstractItemModelEx(parent), m_rootNode(nullptr)
{
}

CAbstractTreeModel::~CAbstractTreeModel()
{
	delete m_rootNode;
}

void CAbstractTreeModel::Update(const void* data)
{
	if (!m_rootNode) {
		// No existing root node, create a new one
		beginResetModel();
		m_rootNode = MkNode(data);
		endResetModel();
	}
	else {
		// Update existing root node
		updateNode(m_rootNode, data, QModelIndex());
	}
}

void CAbstractTreeModel::Clean()
{
	beginResetModel();
	delete m_rootNode;
	m_rootNode = nullptr;
	endResetModel();
}

QModelIndex CAbstractTreeModel::index(int row, int column, const QModelIndex& parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	SAbstractTreeNode* parentNode = nodeFromIndex(parent);
	if (!parentNode)
		parentNode = m_rootNode;

	if (row >= 0 && row < parentNode->childNodes.size())
		return createIndex(row, column, parentNode->childNodes.at(row));
	return QModelIndex();
}

QModelIndex CAbstractTreeModel::parent(const QModelIndex& index) const
{
	if (!index.isValid())
		return QModelIndex();

	SAbstractTreeNode* childNode = nodeFromIndex(index);
	SAbstractTreeNode* parentNode = childNode ? childNode->parentNode : nullptr;

	if (!parentNode || parentNode == m_rootNode)
		return QModelIndex();

	return createIndex(rowFromNode(parentNode), 0, parentNode);
}

int CAbstractTreeModel::rowCount(const QModelIndex& parent) const
{
	SAbstractTreeNode* parentNode = nodeFromIndex(parent);
	if (!parentNode)
		return m_rootNode ? nodeChildCount(m_rootNode) : 0;
	return nodeChildCount(parentNode);
}

QVariant CAbstractTreeModel::data(const QModelIndex& index, int role) const
{
	SAbstractTreeNode* node = nodeFromIndex(index);
	if (!node || index.column() >= node->values.count())
		return QVariant();
	return data(node, index.column(), role);
}

QVariant CAbstractTreeModel::data(SAbstractTreeNode* node, int column, int role) const
{
	switch (role)
	{
	case Qt::DisplayRole:
	case Qt::EditRole:
	{
		const SAbstractTreeNode::SValue& value = node->values[column];
		if (value.Formatted.isValid())
			return value.Formatted;
		return value.Raw;
	}
	case Qt::DecorationRole:
		if (column == 0)
			return node->icon;
		break;
	}
	return QVariant();
}

bool CAbstractTreeModel::hasChildren(const QModelIndex& parent) const
{
	SAbstractTreeNode* parentNode = nodeFromIndex(parent);
	return parentNode ? parentNode->childCount > 0 : (m_rootNode && m_rootNode->childCount > 0);
}

bool CAbstractTreeModel::canFetchMore(const QModelIndex& parent) const
{
	SAbstractTreeNode* parentNode = nodeFromIndex(parent);
	return parentNode ? canFetchMore(parentNode) : (m_rootNode && canFetchMore(m_rootNode));
}

void CAbstractTreeModel::fetchMore(const QModelIndex& parent)
{
	SAbstractTreeNode* parentNode = nodeFromIndex(parent);
	if (!parentNode)
		parentNode = m_rootNode;

	if (!parentNode || !canFetchMore(parentNode))
		return;

	if (parentNode->hasFetchedChildren)
		return;

	ForEachChild(parentNode, [this, parentNode](const QVariant& key, const void* data) {
		auto childNode = MkNode(data, key, parentNode);
		parentNode->childNodes.append(childNode);
		}
	);
	parentNode->hasFetchedChildren = true;
}

CAbstractTreeModel::SAbstractTreeNode* CAbstractTreeModel::nodeFromIndex(const QModelIndex& index) const
{
	return index.isValid() ? static_cast<SAbstractTreeNode*>(index.internalPointer()) : nullptr;
}

int CAbstractTreeModel::rowFromNode(SAbstractTreeNode* node) const
{
	if (!node || !node->parentNode)
		return 0;
	return node->parentNode->childNodes.indexOf(node);
}

int CAbstractTreeModel::nodeChildCount(SAbstractTreeNode* node) const
{
	if (node->hasFetchedChildren)
		return node->childNodes.size();
	return node->childCount;
}

bool CAbstractTreeModel::canFetchMore(SAbstractTreeNode* node) const
{
	return node->hasFetchedChildren || node->childCount > 0;
}

void CAbstractTreeModel::updateNode(SAbstractTreeNode* node, const void* data, const QModelIndex& nodeIndex)
{
	updateNodeData(node, data, nodeIndex);

	// Handle children
	if (!node->hasFetchedChildren)
		return;

	bool bChanged = node->childCount != node->childNodes.size();
	// Build maps of old and new child keys to positions
	QHash<QVariant, int> newKeyPositions;
	ForEachChild(node, [this, node, &newKeyPositions, &bChanged](const QVariant& key, const void* data) {
		int i = newKeyPositions.size();
		newKeyPositions[key] = i;
		if (!bChanged && node->childNodes[i]->key != key)
			bChanged = true;
	});

	// if children did not change we dont need to update the structure
	if (!bChanged)
	{
		// only update the data
		for (int i = 0; i < node->childNodes.size(); i++)
		{
			SAbstractTreeNode* childNode = node->childNodes[i];
			QModelIndex childIndex = index(i, 0, nodeIndex);
			updateNode(childNode, childData(node, childNode->key), childIndex);
		}
		return;
	}

	QHash<QVariant, int> oldKeyPositions;
	for (int i = 0; i < node->childNodes.size(); ++i) {
		oldKeyPositions[node->childNodes[i]->key] = i;
	}


	// Nodes to be added, removed, updated
	QSet<QVariant> oldKeys = ListToSet(oldKeyPositions.keys());
	QSet<QVariant> newKeysSet = ListToSet(newKeyPositions.keys());

	QSet<QVariant> keysToAdd = newKeysSet - oldKeys;
	QSet<QVariant> keysToRemove = oldKeys - newKeysSet;
	QSet<QVariant> keysToUpdate = oldKeys & newKeysSet;

	// First, remove nodes that no longer exist
	QList<int> rowsToRemove;
	for (const QVariant& key : keysToRemove) {
		int row = oldKeyPositions[key];
		rowsToRemove.append(row);
	}

	// Remove in reverse order to keep indices valid
	std::sort(rowsToRemove.begin(), rowsToRemove.end(), std::greater<int>());
	for (int row : rowsToRemove) {
		beginRemoveRows(nodeIndex, row, row);
		delete node->childNodes.takeAt(row);
		endRemoveRows();
		// Update oldKeyPositions
		for (auto it = oldKeyPositions.begin(); it != oldKeyPositions.end(); ++it) {
			if (it.value() > row) {
				it.value() -= 1;
			}
		}
	}

	// Next, add new nodes
	QMap<int, QVariant> rowsToAdd;
	for (const QVariant& key : keysToAdd) {
		int row = newKeyPositions[key];
		rowsToAdd.insert(row, key);
	}

	// Add in order to keep indices valid
	for (auto I = rowsToAdd.begin(); I != rowsToAdd.end(); ++I)
	{
		int position = I.key();
		beginInsertRows(nodeIndex, position, position);
		auto childNode = MkNode(childData(node, I.value()), I.value(), node);
		node->childNodes.insert(position, childNode);
		endInsertRows();
		// Update oldKeyPositions
		for (auto it = oldKeyPositions.begin(); it != oldKeyPositions.end(); ++it) {
			if (it.value() >= position) {
				it.value() += 1;
			}
		}
		oldKeyPositions[I.value()] = position;
	}

	// Finally, move and update existing nodes
	QList<QVariant> keysToProcess = SetToList(keysToUpdate);
	// Sort keys based on new positions
	std::sort(keysToProcess.begin(), keysToProcess.end(), [&](const QVariant& a, const QVariant& b) {
		return newKeyPositions[a] < newKeyPositions[b];
		});

	for (const QVariant& key : keysToProcess) {
		int oldRow = oldKeyPositions[key];
		int newRow = newKeyPositions[key];

		if (oldRow != newRow) {
			// Move the node
			if (beginMoveRows(nodeIndex, oldRow, oldRow, nodeIndex, newRow > oldRow ? newRow + 1 : newRow)) {
				node->childNodes.move(oldRow, newRow);
				endMoveRows();
				// Update oldKeyPositions
				// Adjust indices between oldRow and newRow
				int start = qMin(oldRow, newRow);
				int end = qMax(oldRow, newRow);
				int direction = (newRow > oldRow) ? -1 : 1;
				for (auto it = oldKeyPositions.begin(); it != oldKeyPositions.end(); ++it) {
					if (it.key() != key && it.value() >= start && it.value() <= end) {
						it.value() += direction;
					}
				}
				oldKeyPositions[key] = newRow;
			}
			else {
				// Handle error if move fails
				Q_ASSERT(false);
			}
		}

		// Update the child node recursively
		SAbstractTreeNode* childNode = node->childNodes[oldKeyPositions[key]];
		QModelIndex childIndex = index(oldKeyPositions[key], 0, nodeIndex);
		updateNode(childNode, childData(node, key), childIndex);
	}
}