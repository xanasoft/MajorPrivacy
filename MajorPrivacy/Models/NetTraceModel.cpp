#include "pch.h"
#include "NetTraceModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Service/ServiceAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Core/TraceLogUtils.h"
#include "../Core/Network/NetLogEntry.h"


#define FIRST_COLUMN 0

#define PROCESS_MARK	0x10000000
#define THREAD_MARK		0x20000000

CNetTraceModel::CNetTraceModel(QObject* parent)
	:QAbstractItemModelEx(parent)
{
	m_bTree = false;

	m_Root = MkNode(0);

	m_LastCount = 0;
}

CNetTraceModel::~CNetTraceModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex> CNetTraceModel::Sync(const struct SMergedLog* pLog)
{
	QList<QModelIndex> NewBranches;

	// Note: since this is a log and we ever always only add entries we save cpu time by always skipping the already know portion of the list

	int i = 0;
	if (pLog->List.count() >= m_LastCount && m_LastCount > 0)
	{
		i = m_LastCount - 1;
		if (m_LastID == pLog->List.at(i).second->GetUID())
			i++;
		else
			i = 0;
	}

	if (i == 0)
		Clear();

	emit layoutAboutToBeChanged();

	for (; i < pLog->List.count(); i++)
	{
		const auto& Data = pLog->List.at(i);

		quint64 ID = Data.second->GetUID();

		STreeNode* pNode = MkNode(ID);
		pNode->pLogEntry = Data.second;
		pNode->pProgram = Data.first;
		/*if (m_bTree)
		{
			quint64 Path = PROCESS_MARK | pEntry->GetProcessId();
			Path |= quint64(THREAD_MARK | pEntry->GetThreadId()) << 32;

			pNode->Parent = FindParentNode(m_Root, Path, 0, &NewBranches);
		}
		else*/
			pNode->Parent = m_Root;

		//pNode->Row = pNode->Parent->Children.size();
		pNode->Parent->Children.append(pNode);
	}

	emit layoutChanged();

	m_LastCount = pLog->List.count();
	if (m_LastCount)
		m_LastID = pLog->List.last().second->GetUID();

	return NewBranches;
}

//__inline uint qHash(const CNetTraceModel::STracePath& var)
//{
//    unsigned int hash = 5381;
//    for (quint32* ptr = var.path; ptr < var.path + var.count; ptr++)
//        hash = ((hash << 15) + hash) ^ *ptr;
//    return hash;
//}
//
//bool operator == (const CNetTraceModel::STracePath& l, const CNetTraceModel::STracePath& r)
//{
//	if (l.count != r.count)
//		return false;
//	return memcmp(l.path, r.path, l.count) == 0;
//}

CNetTraceModel::STreeNode* CNetTraceModel::FindParentNode(STreeNode* pParent, quint64 Path, int PathsIndex, QList<QModelIndex>* pNewBranches)
{
	if (2 <= PathsIndex)
		return pParent;
	
	quint64 CurPath = PathsIndex == 0 ? Path & 0x00000000FFFFFFFF : Path;
	STreeNode* &pNode = m_Branches[CurPath];
	if(!pNode)
	{
		pNode = MkNode(PathsIndex == 0 ? Path & 0x00000000FFFFFFFF : (Path >> 32));
		pNode->Parent = pParent;
		pNewBranches->append(createIndex(pParent->Children.size(), FIRST_COLUMN, pNode));

		//pNode->Row = pParent->Children.size();
		pParent->Children.append(pNode);
	}
	return FindParentNode(pNode, Path, PathsIndex + 1, pNewBranches);
}

void CNetTraceModel::Clear(bool bMem)
{
	m_LastCount = 0;
	m_LastID.clear();

	beginResetModel();

	m_Branches.clear();
	FreeNode(m_Root);

	if (bMem)
		m_NodeAllocator.dispose();

	m_Root = MkNode(0);

	endResetModel();
}

PoolAllocator<sizeof(CNetTraceModel::STreeNode)> CNetTraceModel::m_NodeAllocator;

CNetTraceModel::STreeNode*	CNetTraceModel::MkNode(quint64 Id) 
{ 
	STreeNode* pNode = (STreeNode*)m_NodeAllocator.allocate(sizeof(STreeNode));
	new (pNode) STreeNode(Id);
	return pNode;
	//return new STreeNode(Id);
}

void CNetTraceModel::FreeNode(STreeNode* pNode) 
{ 
	foreach(STreeNode* pSubNode, pNode->Children)
		FreeNode(pSubNode);
	pNode->~STreeNode();
	m_NodeAllocator.free(pNode);
	//delete pNode; 
}

bool CNetTraceModel::TestHighLight(STreeNode* pNode) const
{
	if (m_HighLightExp.isEmpty())
		return false;
	for (int i = 0; i < eCount; i++) {
		if (NodeData(pNode, Qt::DisplayRole, i).toString().contains(m_HighLightExp, Qt::CaseInsensitive))
			return true;
	}
	return false;
}

QVariant CNetTraceModel::NodeData(STreeNode* pNode, int role, int section) const
{
	const CNetLogEntry* pEntry = dynamic_cast<const CNetLogEntry*>(pNode->pLogEntry.constData());
	/*if(!pEntry.constData())
	{
		if (section != FIRST_COLUMN || (role != Qt::DisplayRole && role != Qt::EditRole))
			return QVariant();

		quint32 id = pNode->ID;
		if (id & PROCESS_MARK) {
			CLogEntryPtr pProcEntry; // pick first log entry of first thread to query the process name
			if (!pNode->Children.isEmpty()) {
				STreeNode* pSubNode = pNode->Children.first();
				if (!pSubNode->Children.isEmpty())
					pProcEntry = pSubNode->Children.first()->pEntry;
			}
			if (pProcEntry && !pProcEntry->GetProcessName().isEmpty())
				return tr("%1 (%2)").arg(pProcEntry->GetProcessName()).arg(pProcEntry->GetProcessId());
			return tr("Process %1").arg(id & 0x0FFFFFFF);
		}
		else if (id & THREAD_MARK)
			return tr("Thread %1").arg(id & 0x0FFFFFFF);
		else
			return QString::number(id, 16).rightJustified(8, '0');
	}*/
	
	switch(role)
	{
		case Qt::DisplayRole:
		case Qt::EditRole: // sort role
		{
			switch (section)
			{
			case eName:				return (pNode->pProgram ? pNode->pProgram->GetName() : tr("Unknown Program"))
				+ (!pEntry->GetOwnerService().isEmpty() ? tr(" (%1)").arg(pEntry->GetOwnerService()) : "");
			//case eAction:			return pEntry->GetState(); 
			case eAction:			return pEntry->GetAction(); 
			case eDirection:		return pEntry->GetDirection(); 
			case eProtocol:			return pEntry->GetProtocolType(); 
			case eRAddress:		{
									QString Data = pEntry->GetRemoteAddress().toString();
									QString Host = pEntry->GetRemoteHostName();
									if(!Host.isEmpty()) Data += " (" + Host + ")";
									return Data;
								}
			case eRPort:			return pEntry->GetRemotePort(); 
			case eLAddress:			return pEntry->GetLocalAddress().toString(); 
			case eLPort:			return pEntry->GetLocalPort(); 
			//case eUploaded:		return Connection[].AsQStr(); 
			//case eDownloaded:		return Connection[].AsQStr(); 
			case eTimeStamp:		return QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss.zzz"); 
			case eProgram:			return pNode->pProgram ? pNode->pProgram->GetPath() : tr("PROCESS MISSING"); 
			}
		}
		case Qt::BackgroundRole:
		{
			if(!CTreeItemModel::GetDarkMode())
				return TestHighLight(pNode) ? QColor(Qt::yellow) : QVariant();
			break;
		}
		case Qt::ForegroundRole:
		{
			if(CTreeItemModel::GetDarkMode())
				return TestHighLight(pNode) ? QColor(Qt::yellow) : QVariant();
			break;
		}
	}

	return QVariant();
}

CLogEntryPtr CNetTraceModel::GetLogEntry(const QModelIndex& index) const
{
	if (!index.isValid())
		return CLogEntryPtr();

	STreeNode* pNode = static_cast<STreeNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pLogEntry;
}

CProgramFilePtr	CNetTraceModel::GetProgram(const QModelIndex& index) const
{
	if (!index.isValid())
		return CProgramFilePtr();

	STreeNode* pNode = static_cast<STreeNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pProgram;
}

QVariant CNetTraceModel::data(const QModelIndex &index, int role) const
{
    return Data(index, role, index.column());
}

QVariant CNetTraceModel::GetItemID(const QModelIndex& index) const
{
	if (!index.isValid())
		return QVariant();

	STreeNode* pNode = static_cast<STreeNode*>(index.internalPointer());

	return pNode->ID;
}

QVariant CNetTraceModel::Data(const QModelIndex &index, int role, int section) const
{
	if (!index.isValid())
		return QVariant();

	STreeNode* pNode = static_cast<STreeNode*>(index.internalPointer());
	ASSERT(pNode);

	return NodeData(pNode, role, section);
}

Qt::ItemFlags CNetTraceModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
	if(index.column() == 0)
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex CNetTraceModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    STreeNode* pParent;
    if (!parent.isValid())
        pParent = m_Root;
    else
        pParent = static_cast<STreeNode*>(parent.internalPointer());

	if(STreeNode* pNode = pParent->Children.count() > row ? pParent->Children[row] : NULL)
        return createIndex(row, column, pNode);
    return QModelIndex();
}

QModelIndex CNetTraceModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    STreeNode* pNode = static_cast<STreeNode*>(index.internalPointer());
	ASSERT(pNode->Parent);
	STreeNode* pParent = pNode->Parent;
    if (pParent == m_Root)
        return QModelIndex();

	int row = 0;
	if(pParent->Parent)
		row = pParent->Parent->Children.indexOf(pParent);
    return createIndex(row, 0, pParent);
}

int CNetTraceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

	STreeNode* pNode;
    if (!parent.isValid())
        pNode = m_Root;
    else
        pNode = static_cast<STreeNode*>(parent.internalPointer());
	return pNode->Children.count();
}

int CNetTraceModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CNetTraceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eProtocol:				return tr("Protocol");
		case eAction:				return tr("Action");
		case eDirection:			return tr("Direction");
		case eRAddress:				return tr("Remote Address");
		case eRPort:				return tr("Remote Port");
		case eLAddress:				return tr("Local Address");
		case eLPort:				return tr("Local Port");
		case eUploaded:				return tr("Uploaded");
		case eDownloaded:			return tr("Downloaded");
		case eTimeStamp:			return tr("Time Stamp");
		case eProgram:				return tr("Program");
		}
	}
	return QVariant();
}