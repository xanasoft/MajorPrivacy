#include "pch.h"
#include "AccessModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Helpers/IconCache.h"
#include "../../Library/Helpers/NtUtil.h"
#include "..\Core\Access\ResLogEntry.h"

CAccessModel::CAccessModel(QObject* parent) 
	: CAbstractTreeModel(parent) 
{
	m_DeviceIcon = QIcon(":/Icons/CPU.png");
	m_MonitorIcon = QIcon(":/Icons/Monitor.png");
	m_DiskIcon = QIcon(":/Icons/Disk.png");
	m_FileIcon = QIcon(":/Icons/File.png");
	m_DirectoryIcon = QIcon(":/Icons/Directory.png");
	m_NetworkIcon = QIcon(":/Icons/Network.png");
	m_ObjectsIcon = QIcon(":/Icons/Objects.png");
	m_RegEditIcon = QIcon(":/Icons/RegEdit.png");
}

void CAccessModel::Update(const SAccessItemPtr& pRoot)
{
	CAbstractTreeModel::Update(&pRoot);
}

CAbstractTreeModel::SAbstractTreeNode* CAccessModel::MkNode(const void* data, const QVariant& key, SAbstractTreeNode* parent)
{
	SAccessModelNode* pNode = new SAccessModelNode;
	pNode->data = *(SAccessItemPtr*)data;
	pNode->key = key;
	pNode->parentNode = parent;
	pNode->values.resize(columnCount());
	updateNodeData(pNode, data, QModelIndex());
	return pNode;
}

void CAccessModel::ForEachChild(SAbstractTreeNode* parentNode, const std::function<void(const QVariant& key, const void* data)>& func)
{
	SAccessModelNode* node = (SAccessModelNode*)parentNode;
	for (auto I = node->data->Branches.begin(); I != node->data->Branches.end(); ++I)
		func(I.key(), &I.value());
}

void CAccessModel::updateNodeData(SAbstractTreeNode* node, const void* data, const QModelIndex& nodeIndex)
{
	SAccessModelNode* pNode = (SAccessModelNode*)node;
	pNode->data = *(SAccessItemPtr*)data;

	int Col = 0;
	bool State = false;
	int Changed = 0;

	if (pNode->icon.isNull())
	{
		switch (pNode->data->Type)
		{
		case SAccessItem::eDevice:		pNode->icon = m_DeviceIcon; break;
		case SAccessItem::eComputer:	pNode->icon = m_MonitorIcon; break;
		case SAccessItem::eDrive:		pNode->icon = m_DiskIcon; break;
		case SAccessItem::eFile: // or folder
			if (pNode->data->Branches.isEmpty()) {
				pNode->icon = m_FileIcon; // GetFileIcon(pNode->pItem->Name, 16);
				break;
			}
			else
				pNode->data->Type = SAccessItem::eFolder; // this value is reused by the list view
		case SAccessItem::eFolder:
			pNode->icon = m_DirectoryIcon;
			break;
		case SAccessItem::eNetwork:		pNode->icon = m_NetworkIcon; break;
		case SAccessItem::eHost:		pNode->icon = m_MonitorIcon; break;
		case SAccessItem::eObjects:		pNode->icon = m_ObjectsIcon; break;
		case SAccessItem::eRegistry:	pNode->icon = m_RegEditIcon; break;
		}
	}

	for (int section = 0; section < columnCount(); section++)
	{
		if (!IsColumnEnabled(section))
			continue; // ignore columns which are hidden

		QVariant Value;
		switch (section)
		{
		case eName:				Value = pNode->data->Name; break;
		case eLastAccess:		if (pNode->data->LastAccess) Value = pNode->data->LastAccess;
		}

		SAbstractTreeNode::SValue& ColValue = pNode->values[section];

		if (ColValue.Raw != Value)
		{
			if (Changed == 0)
				Changed = 1;
			ColValue.Raw = Value;

			switch (section)
			{
			case eLastAccess:		
				if (pNode->data->LastAccess)
					ColValue.Formatted = QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pNode->data->LastAccess)).toString("dd.MM.yyyy hh:mm:ss.zzz");
				break;
			}
		}

		if (State != (Changed != 0))
		{
			if (State && nodeIndex.isValid())
				emit dataChanged(createIndex(nodeIndex.row(), Col, pNode), createIndex(nodeIndex.row(), section - 1, pNode));
			State = (Changed != 0);
			Col = section;
		}
		if (Changed == 1)
			Changed = 0;
	}
	if (State && nodeIndex.isValid())
		emit dataChanged(createIndex(nodeIndex.row(), Col, pNode), createIndex(nodeIndex.row(), columnCount() - 1, pNode));

	// Check if hasChildren() status has changed
	bool hadChildren = node->childCount > 0;
	bool hasChildrenNow = !pNode->data->Branches.isEmpty();

	node->childCount = pNode->data->Branches.size();

	// Emit dataChanged() if necessary to update the expand/collapse indicator
	if (hadChildren != hasChildrenNow && nodeIndex.isValid())
		emit dataChanged(nodeIndex, nodeIndex);
}

const void* CAccessModel::childData(SAbstractTreeNode* node, const QVariant& key)
{
	SAccessModelNode* pNode = (SAccessModelNode*)node;
	return &pNode->data->Branches[key.toString()];
}

QVariant CAccessModel::data(SAbstractTreeNode* node, int section, int role) const
{
	SAccessModelNode* pAccessNode = static_cast<SAccessModelNode*>(node);

	switch (role)
	{
	case Qt::ToolTipRole:
		switch (section)
		{
		case eName:			return pAccessNode->data ? pAccessNode->data->NtPath : QVariant();
		case eLastAccess:	return pAccessNode->data ? CResLogEntry::GetAccessStr(pAccessNode->data->AccessMask) : QVariant();
		}

	case Qt::ForegroundRole:
		if (section == eLastAccess)
		{
			uint32 uAccessMask = pAccessNode->data->AccessMask;

			if ((uAccessMask & GENERIC_WRITE) == GENERIC_WRITE
				|| (uAccessMask & GENERIC_ALL) == GENERIC_ALL
				|| (uAccessMask & DELETE) == DELETE
				|| (uAccessMask & WRITE_DAC) == WRITE_DAC
				|| (uAccessMask & WRITE_OWNER) == WRITE_OWNER
				|| (uAccessMask & FILE_WRITE_DATA) == FILE_WRITE_DATA
				|| (uAccessMask & FILE_APPEND_DATA) == FILE_APPEND_DATA)
				return QColor(Qt::red);

			if ((uAccessMask & FILE_WRITE_ATTRIBUTES) == FILE_WRITE_ATTRIBUTES
				|| (uAccessMask & FILE_WRITE_EA) == FILE_WRITE_EA)
				return QColor(255, 165, 0);

			if ((uAccessMask & GENERIC_READ) == GENERIC_READ
				|| (uAccessMask & GENERIC_EXECUTE) == GENERIC_EXECUTE
				|| (uAccessMask & READ_CONTROL) == READ_CONTROL
				|| (uAccessMask & FILE_READ_DATA) == FILE_READ_DATA
				|| (uAccessMask & FILE_EXECUTE) == FILE_EXECUTE)
				return QColor(Qt::green);

			if ((uAccessMask & SYNCHRONIZE) == SYNCHRONIZE
				|| (uAccessMask & FILE_READ_ATTRIBUTES) == FILE_READ_ATTRIBUTES
				|| (uAccessMask & FILE_READ_EA) == FILE_READ_EA)
				return QColor(Qt::blue);
		}

	default:
		return CAbstractTreeModel::data(node, section, role);
	}
}

SAccessItemPtr CAccessModel::GetItem(const QModelIndex& index)
{
	SAccessModelNode* node = (SAccessModelNode*)nodeFromIndex(index);
	return node ? node->data : nullptr;
}

QVariant CAccessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:			return tr("Name");
		case eLastAccess:	return tr("Last Access");
		}
	}
	return QVariant();
}
