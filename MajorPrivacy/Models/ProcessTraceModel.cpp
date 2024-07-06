#include "pch.h"
#include "ProcessTraceModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Core/TraceLogUtils.h"
#include "../Core/Processes/ExecLogEntry.h"
#include "../Core/Programs/ProgramManager.h"

CProcessTraceModel::CProcessTraceModel(QObject* parent)
	:CTraceModel(parent)
{
	m_Root = MkNode(0);
}

CProcessTraceModel::~CProcessTraceModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

CTraceModel::STraceNode* CProcessTraceModel::MkNode(const SMergedLog::TLogEntry& Data)
{
	quint64 ID = Data.second->GetUID();

	STreeNode* pNode = (STreeNode*)MkNode(ID);
	pNode->pLogEntry = Data.second;

	const CExecLogEntry* pEntry = dynamic_cast<const CExecLogEntry*>(pNode->pLogEntry.constData());

	if (pEntry->GetRole() == EExecLogRole::eBooth)	pNode->pActor = pNode->pTarget = Data.first;
	else if (pEntry->GetRole() == EExecLogRole::eActor) pNode->pActor = Data.first;
	else if (pEntry->GetRole() == EExecLogRole::eTarget) pNode->pTarget = Data.first;

	if (pEntry->GetType() == EExecLogType::eImageLoad)
		pNode->pLibrary = theCore->ProgramManager()->GetLibraryByUID(pEntry->GetMiscID());
	else {
		CProgramFilePtr pProgram = theCore->ProgramManager()->GetProgramByUID(pEntry->GetMiscID()).objectCast<CProgramFile>();
		if (pEntry->GetRole() == EExecLogRole::eActor) pNode->pTarget = pProgram;
		else if (pEntry->GetRole() == EExecLogRole::eTarget) pNode->pActor = pProgram;
	}

	return pNode;
}

PoolAllocator<sizeof(CProcessTraceModel::STreeNode)> CProcessTraceModel::m_NodeAllocator;

CProcessTraceModel::STraceNode*	CProcessTraceModel::MkNode(quint64 Id) 
{ 
	STreeNode* pNode = (STreeNode*)m_NodeAllocator.allocate(sizeof(STreeNode));
	new (pNode) STreeNode(Id);
	return pNode;
	//return new STreeNode(Id);
}

void CProcessTraceModel::FreeNode(STraceNode* pNode) 
{ 
	foreach(STraceNode* pSubNode, pNode->Children)
		FreeNode(pSubNode);
	pNode->~STraceNode();
	m_NodeAllocator.free(pNode);
	//delete pNode; 
}

QVariant CProcessTraceModel::NodeData(STraceNode* pTraceNode, int role, int section) const
{
	STreeNode* pNode = (STreeNode*)pTraceNode;
	const CExecLogEntry* pEntry = dynamic_cast<const CExecLogEntry*>(pNode->pLogEntry.constData());

	switch (role)
	{
	case Qt::DisplayRole:
	case Qt::EditRole: // sort role
	{
		switch (section)
		{
		case eName:				if (pEntry->GetType() == EExecLogType::eProcessStarted && pEntry->GetRole() == EExecLogRole::eTarget)
			return (pNode->pTarget ? pNode->pTarget->GetNameEx() : tr("Unknown Program"));
				  else
			return (pNode->pActor ? pNode->pActor->GetNameEx() : tr("Unknown Program"))
			+ (!pEntry->GetOwnerService().isEmpty() ? tr(" (%1)").arg(pEntry->GetOwnerService()) : "");
		case eRole:				return pEntry->GetRoleStr();
		case eType:				return pEntry->GetTypeStr();
		case eStatus:			return pEntry->GetStatusStr();
		case eTarget:			if (pEntry->GetType() == EExecLogType::eImageLoad)
			return pNode->pLibrary ? pNode->pLibrary->GetPath(EPathType::eDisplay) : tr("MODULE MISSING");
			return pNode->pTarget ? pNode->pTarget->GetPath(EPathType::eDisplay) : tr("PROCESS MISSING");
		case eTimeStamp:		return QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss.zzz");
		case eProgram:			return pNode->pActor ? pNode->pActor->GetPath(EPathType::eDisplay) : tr("PROCESS MISSING");
		}
	}
	case Qt::DecorationRole:
	{
		if (section == 0)
			return pNode->pActor ? pNode->pActor->GetIcon() : CProcess::DefaultIcon();
		break;
	}
	}

	return CTraceModel::NodeData(pTraceNode, role, section);
}

int CProcessTraceModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CProcessTraceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eRole:					return tr("Role");
		case eType:					return tr("Type");
		case eStatus:				return tr("Status");
		case eTarget:				return tr("Target");
		case eTimeStamp:			return tr("Time Stamp");
		case eProgram:				return tr("Program (Actor)");
		}
	}
	return QVariant();
}