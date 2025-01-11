#include "pch.h"
#include "AccessTraceModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Windows/AccessRuleWnd.h"
#include "../Core/Enclaves/EnclaveManager.h"


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

bool CAccessTraceModel::FilterNode(const SMergedLog::TLogEntry& Data) const
{
	const CResLogEntry* pEntry = dynamic_cast<const CResLogEntry*>(Data.second.constData());
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

	if (!m_RootPath.isNull())
	{
		if(m_RootPath.isEmpty())
			return false;

		QString Path = pEntry->GetNtPath();
		if (!PathStartsWith(Path, m_RootPath))
			return false;
	}

	return true;
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

	if (role == (CTreeItemModel::GetDarkMode() ? Qt::ForegroundRole : Qt::BackgroundRole))
	{
		QColor Color;
		switch (section)
		{
		case eOperation:	Color = CResLogEntry::GetAccessColor(pEntry->GetAccess()); break;
		case eStatus:		Color = CAccessRuleWnd::GetStatusColor(pEntry->GetStatus()); break;
		}
		if (Color.isValid())
			return Color;
	}
	else switch(role)
	{
	case Qt::DisplayRole:
	case Qt::EditRole: // sort role
		switch (section)
		{
		case eName:				return (pNode->pProgram ? pNode->pProgram->GetNameEx() : tr("Unknown Program")) + (!pEntry->GetOwnerService().isEmpty() ? tr(" (%1)").arg(pEntry->GetOwnerService()) : "");
		case ePath:				return theCore->NormalizePath(pEntry->GetNtPath());
		case eOperation:		return pEntry->GetAccessStr();
		case eStatus:			return pEntry->GetStatusStr();
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
		case eTimeStamp:		return QDateTime::fromMSecsSinceEpoch(FILETIME2ms(pEntry->GetTimeStamp())).toString("dd.MM.yyyy hh:mm:ss.zzz"); 
		case eProgram:			return pNode->pProgram ? pNode->pProgram->GetPath() : tr("PROGRAM MISSING");
		}
		break;
	case Qt::ToolTipRole:
		switch (section)
		{
		case ePath:				return pEntry->GetNtPath();
		case eOperation:		return pEntry->GetAccessStrEx();
		}
		break;
	case Qt::DecorationRole:
		if (section == 0)
			return pNode->pProgram ? pNode->pProgram->GetIcon() : CProcess::DefaultIcon();
		break;
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
		case eOperation:			return tr("Operation");
		case eStatus:				return tr("Status");
		case eEnclave:				return tr("Enclave");
		case eTimeStamp:			return tr("Time Stamp");
		case eProgram:				return tr("Program (Actor)");
		}
	}
	return QVariant();
}