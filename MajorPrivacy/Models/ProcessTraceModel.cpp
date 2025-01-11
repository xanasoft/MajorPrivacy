#include "pch.h"
#include "ProcessTraceModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Processes/ExecLogEntry.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Enclaves/EnclaveManager.h"
#include "../Windows/AccessRuleWnd.h"
#include "../Windows/ProgramRuleWnd.h"

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

bool CProcessTraceModel::FilterNode(const SMergedLog::TLogEntry& Data) const
{
	const CExecLogEntry* pEntry = dynamic_cast<const CExecLogEntry*>(Data.second.constData());
	if (m_Role != EExecLogRole::eUndefined && pEntry->GetRole() != m_Role)
		return false;
	if (m_Action != EEventStatus::eUndefined)
	{
		switch (pEntry->GetStatus())
		{
		case EEventStatus::eAllowed:	
		case EEventStatus::eUntrusted:	
		case EEventStatus::eEjected:	return m_Action == EEventStatus::eAllowed;
		case EEventStatus::eProtected:
		case EEventStatus::eBlocked:	return m_Action == EEventStatus::eBlocked;
		}
	}
	return true;
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

	EExecLogRole Role = pEntry->GetRole();
	CProgramFilePtr	pProgram = Role == EExecLogRole::eTarget ? pNode->pTarget : pNode->pActor;
	CProgramFilePtr pSubject = Role == EExecLogRole::eTarget ? pNode->pActor : pNode->pTarget;

	if (role == (CTreeItemModel::GetDarkMode() ? Qt::ForegroundRole : Qt::BackgroundRole))
	{
		QColor Color;
		switch (section)
		{
		case eRole:				Color = CProgramRuleWnd::GetRoleColor(Role); break;
		case eOperation:		Color = pEntry->GetTypeColor(); break;
		case eStatus:			Color = CAccessRuleWnd::GetStatusColor(pEntry->GetStatus()); break;
		}
		if (Color.isValid())
			return Color;
	}
	else switch (role)
	{
	case Qt::DisplayRole:
	case Qt::EditRole: // sort role
		switch (section)
		{
		case eName:
			if (Role == EExecLogRole::eTarget)
				return (pProgram ? pProgram->GetNameEx() : tr("PROGRAM MISSING"));
			else
				return (pProgram ? pProgram->GetNameEx() : tr("PROGRAM MISSING")) + (!pEntry->GetOwnerService().isEmpty() ? tr(" [%1]").arg(pEntry->GetOwnerService()) : "");
		case eEnclave: {
			QFlexGuid Enclave = pEntry->GetEnclaveGuid();
			if (!Enclave.IsNull()) {
				CEnclavePtr pEnclave = theCore->EnclaveManager()->GetEnclave(Enclave);
				if (pEnclave)
					return pEnclave->GetName();
				return Enclave.ToQS();
			}
			break;
		}
		case eRole:				return pEntry->GetRoleStr();
		case eOperation:		return pEntry->GetTypeStr();
		case eStatus:			return pEntry->GetStatusStr();
		case eSubject:			
			if (pEntry->GetType() == EExecLogType::eImageLoad)
				return pNode->pLibrary ? pNode->pLibrary->GetPath() : tr("MODULE MISSING");
			if (Role == EExecLogRole::eActor)
				return (pSubject ? pSubject->GetPath() : tr("PROGRAM MISSING"));
			else
				return (pSubject ? pSubject->GetPath() : tr("PROGRAM MISSING")) + (!pEntry->GetOwnerService().isEmpty() ? tr(" [%1]").arg(pEntry->GetOwnerService()) : "");
		case eTimeStamp:		return QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss.zzz");
		case eProgram:			return pProgram ? pProgram->GetPath() : tr("PROGRAM MISSING");
		}
		break;
	case Qt::ToolTipRole:
		switch (section)
		{
		case eOperation:		return pEntry->GetTypeStrEx();
		}
		break;
	case Qt::DecorationRole:
		if (section == 0)
			return pProgram ? pProgram->GetIcon() : CProcess::DefaultIcon();
		break;
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
		case eEnclave:				return tr("Enclave");
		case eRole:					return tr("Role");
		case eOperation:			return tr("Operation");
		case eStatus:				return tr("Status");
		case eSubject:				return tr("Subject");
		case eTimeStamp:			return tr("Time Stamp");
		case eProgram:				return tr("Program");
		}
	}
	return QVariant();
}