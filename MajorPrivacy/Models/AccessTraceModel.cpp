#include "pch.h"
#include "AccessTraceModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Core/TraceLogUtils.h"
#include "../Core/Programs/ProgramManager.h"


CAccessTraceModel::CAccessTraceModel(QObject* parent)
	:CTraceModel(parent)
{
	m_Root = MkNode(0);
}

CAccessTraceModel::~CAccessTraceModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

CTraceModel::STraceNode* CAccessTraceModel::MkNode(const SMergedLog::TLogEntry& Data)
{
	quint64 ID = Data.second->GetUID();

	STreeNode* pNode = (STreeNode*)MkNode(ID);
	pNode->pLogEntry = Data.second;
	pNode->pProgram = Data.first;

	return pNode;
}

PoolAllocator<sizeof(CAccessTraceModel::STreeNode)> CAccessTraceModel::m_NodeAllocator;

CAccessTraceModel::STraceNode*	CAccessTraceModel::MkNode(quint64 Id) 
{ 
	STreeNode* pNode = (STreeNode*)m_NodeAllocator.allocate(sizeof(STreeNode));
	new (pNode) STreeNode(Id);
	return pNode;
	//return new STreeNode(Id);
}

void CAccessTraceModel::FreeNode(STraceNode* pNode) 
{ 
	foreach(STraceNode* pSubNode, pNode->Children)
		FreeNode(pSubNode);
	pNode->~STraceNode();
	m_NodeAllocator.free(pNode);
	//delete pNode; 
}

QVariant CAccessTraceModel::NodeData(STraceNode* pTraceNode, int role, int section) const
{
	STreeNode* pNode = (STreeNode*)pTraceNode;
	const CResLogEntry* pEntry = dynamic_cast<const CResLogEntry*>(pNode->pLogEntry.constData());

	switch(role)
	{
	case Qt::DisplayRole:
	case Qt::EditRole: // sort role
	{
		switch (section)
		{
		case eName:				return (pNode->pProgram ? pNode->pProgram->GetNameEx() : tr("Unknown Program"))
			+ (!pEntry->GetOwnerService().isEmpty() ? tr(" (%1)").arg(pEntry->GetOwnerService()) : "");
		case ePath:				return pEntry->GetPath(EPathType::eDisplay);
		case eAccess:			return pEntry->GetAccessStr();
		case eStatus:			return pEntry->GetStatusStr();
		case eTimeStamp:		return QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss.zzz"); 
		case eProgram:			return pNode->pProgram ? pNode->pProgram->GetPath(EPathType::eDisplay) : tr("PROCESS MISSING");
		}
	}
	case Qt::DecorationRole:
	{
		if (section == 0)
			return pNode->pProgram ? pNode->pProgram->GetIcon() : CProcess::DefaultIcon();
		break;
	}
	}

	return CTraceModel::NodeData(pTraceNode, role, section);
}

int CAccessTraceModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CAccessTraceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case ePath:					return tr("Path");
		case eAccess:				return tr("Access");
		case eStatus:				return tr("Status");
		case eTimeStamp:			return tr("Time Stamp");
		case eProgram:				return tr("Program (Actor)");
		}
	}
	return QVariant();
}