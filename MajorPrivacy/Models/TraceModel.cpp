#include "pch.h"
#include "TraceModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Core/TraceLogUtils.h"
#include "../Core/Network/NetLogEntry.h"


#define FIRST_COLUMN 0

//#define PROCESS_MARK	0x10000000
//#define THREAD_MARK		0x20000000

CTraceModel::CTraceModel(QObject* parent)
	:QAbstractItemModelEx(parent)
{
	//m_bTree = false;

	m_Root = NULL;

	m_LastCount = 0;
}

CTraceModel::~CTraceModel()
{
	Q_ASSERT(m_Root == NULL);
}

QList<QModelIndex> CTraceModel::Sync(const QVector<SMergedLog::TLogEntry>& List)
{
	QList<QModelIndex> NewBranches;

	// Note: since this is a log and we ever always only add entries we save cpu time by always skipping the already know portion of the list

	int i = 0;
	if (List.count() >= m_LastCount && m_LastCount > 0)
	{
		i = m_LastCount - 1;
		if (m_LastID == List.at(i).second->GetUID())
			i++;
		else
			i = 0;
	}

	if (i == 0)
		Clear();

	bool bHasFilter = !m_bHighLight && !m_FilterExp.isEmpty();

	emit layoutAboutToBeChanged();

	for (; i < List.count(); i++)
	{
		const auto& Data = List.at(i);

		STraceNode* pNode = MkNode(Data);

		if (bHasFilter && !TestHighLight(pNode)) {
			FreeNode(pNode);
			continue;
		}

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

	m_LastCount = List.count();
	if (m_LastCount)
		m_LastID = List.last().second->GetUID();

	return NewBranches;
}

//__inline uint qHash(const CTraceModel::STracePath& var)
//{
//    unsigned int hash = 5381;
//    for (quint32* ptr = var.path; ptr < var.path + var.count; ptr++)
//        hash = ((hash << 15) + hash) ^ *ptr;
//    return hash;
//}
//
//bool operator == (const CTraceModel::STracePath& l, const CTraceModel::STracePath& r)
//{
//	if (l.count != r.count)
//		return false;
//	return memcmp(l.path, r.path, l.count) == 0;
//}

CTraceModel::STraceNode* CTraceModel::FindParentNode(STraceNode* pParent, quint64 Path, int PathsIndex, QList<QModelIndex>* pNewBranches)
{
	if (2 <= PathsIndex)
		return pParent;
	
	quint64 CurPath = PathsIndex == 0 ? Path & 0x00000000FFFFFFFF : Path;
	STraceNode* &pNode = m_Branches[CurPath];
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

void CTraceModel::Clear()
{
	m_LastCount = 0;
	m_LastID.clear();

	beginResetModel();

	m_Branches.clear();
	FreeNode(m_Root);

	m_Root = MkNode(0);

	endResetModel();
}

QVariant CTraceModel::NodeData(STraceNode* pNode, int role, int section) const
{
	switch(role)
	{
	case Qt::DisplayRole:
	case Qt::EditRole: // sort role
	{
		//
	}
	case Qt::BackgroundRole:
	{
		if(!CTreeItemModel::GetDarkMode())
			return (m_bHighLight && TestHighLight(pNode)) ? QColor(Qt::yellow) : QVariant();
		break;
	}
	case Qt::ForegroundRole:
	{
		if(CTreeItemModel::GetDarkMode())
			return (m_bHighLight && TestHighLight(pNode)) ? QColor(Qt::yellow) : QVariant();
		break;
	}
	}

	return QVariant();
}

bool CTraceModel::TestHighLight(STraceNode* pNode) const
{
	if (m_FilterExp.isEmpty())
		return false;
	for (int i = 0; i < columnCount(); i++) {
		if (NodeData(pNode, Qt::DisplayRole, i).toString().contains(m_FilterExp, Qt::CaseInsensitive))
			return true;
	}
	return false;
}

CLogEntryPtr CTraceModel::GetItem(const QModelIndex& index) const
{
	if (!index.isValid())
		return CLogEntryPtr();

	STraceNode* pNode = static_cast<STraceNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pLogEntry;
}

QVariant CTraceModel::data(const QModelIndex &index, int role) const
{
    return Data(index, role, index.column());
}

QVariant CTraceModel::GetItemID(const QModelIndex& index) const
{
	if (!index.isValid())
		return QVariant();

	STraceNode* pNode = static_cast<STraceNode*>(index.internalPointer());

	return pNode->ID;
}

QVariant CTraceModel::Data(const QModelIndex &index, int role, int section) const
{
	if (!index.isValid())
		return QVariant();

	STraceNode* pNode = static_cast<STraceNode*>(index.internalPointer());
	ASSERT(pNode);

	return NodeData(pNode, role, section);
}

Qt::ItemFlags CTraceModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
	if(index.column() == 0)
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex CTraceModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    STraceNode* pParent;
    if (!parent.isValid())
        pParent = m_Root;
    else
        pParent = static_cast<STraceNode*>(parent.internalPointer());

	if(STraceNode* pNode = pParent->Children.count() > row ? pParent->Children[row] : NULL)
        return createIndex(row, column, pNode);
    return QModelIndex();
}

QModelIndex CTraceModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    STraceNode* pNode = static_cast<STraceNode*>(index.internalPointer());
	ASSERT(pNode->Parent);
	STraceNode* pParent = pNode->Parent;
    if (pParent == m_Root)
        return QModelIndex();

	int row = 0;
	if(pParent->Parent)
		row = pParent->Parent->Children.indexOf(pParent);
    return createIndex(row, 0, pParent);
}

int CTraceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

	STraceNode* pNode;
    if (!parent.isValid())
        pNode = m_Root;
    else
        pNode = static_cast<STraceNode*>(parent.internalPointer());
	return pNode->Children.count();
}
