#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/AbstractTreeModel.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramItem.h"
#include "../Core/Access/AccessEntry.h"

struct SAccessItem
{
	QString Name;
	quint64 LastAccess = 0;
	uint32 AccessMask = 0;
	QString Path;
	enum EType {
		eDevice = 0,
		eObjects,
		eComputer,
		eDrive,
		eFile,
		eFolder,
		eNetwork,
		eHost,
		eRegistry,
	} Type = eDevice;
	QHash<CProgramItemPtr, SAccessStatsPtr> Stats;
	QHash<QString, QSharedPointer<SAccessItem>> Branches;
};

typedef QSharedPointer<SAccessItem> SAccessItemPtr;

class CAccessModel : public CAbstractTreeModel
{
	Q_OBJECT
public:
	CAccessModel(QObject* parent = nullptr);

	typedef SAccessItemPtr ItemType;

	void			Update(const SAccessItemPtr& pRoot);

	SAccessItemPtr	GetItem(const QModelIndex& index);
	
	int				columnCount(const QModelIndex& parent = QModelIndex()) const { return eCount; }
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eLastAccess,
		eCount
	};

protected:

	struct SAccessModelNode : SAbstractTreeNode
	{
		SAccessItemPtr data;
	};

	SAbstractTreeNode* MkNode(const void* data, const QVariant& key = QVariant(), SAbstractTreeNode* parent = nullptr) override;

	void ForEachChild(SAbstractTreeNode* parentNode, const std::function<void(const QVariant& key, const void* data)>& func) override;

	void updateNodeData(SAbstractTreeNode* node, const void* data, const QModelIndex& nodeIndex) override;

	const void* childData(SAbstractTreeNode* node, const QVariant& key) override;

	QVariant data(SAbstractTreeNode* node, int column, int role = Qt::DisplayRole) const override;

private:
	QIcon m_DeviceIcon;
	QIcon m_MonitorIcon;
	QIcon m_DiskIcon;
	QIcon m_FileIcon;
	QIcon m_DirectoryIcon;
	QIcon m_NetworkIcon;
	QIcon m_ObjectsIcon;
	QIcon m_RegEditIcon;
};

