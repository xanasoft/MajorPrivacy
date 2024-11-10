#include "pch.h"
#include "NetTraceModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Core/TraceLogUtils.h"
#include "../Core/Network/NetLogEntry.h"
#include "../Core/Network/NetworkManager.h"


CNetTraceModel::CNetTraceModel(QObject* parent)
	:CTraceModel(parent)
{
	m_Root = MkNode(0);
}

CNetTraceModel::~CNetTraceModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

bool CNetTraceModel::FilterNode(const SMergedLog::TLogEntry& Data) const
{
	const CNetLogEntry* pEntry = dynamic_cast<const CNetLogEntry*>(Data.second.constData());
	if (m_Protocol != ENetProtocols::eAny)
	{
		switch (m_Protocol)
		{
		case ENetProtocols::eWeb:			if (pEntry->GetProtocolType() != (quint32)EFwKnownProtocols::TCP || (pEntry->GetRemotePort() != 80 && pEntry->GetRemotePort() != 443)) return false; break;
		case ENetProtocols::eTCP:			if (pEntry->GetProtocolType() != (quint32)EFwKnownProtocols::TCP) return false; break;
		case ENetProtocols::eTCP_Server:	if (pEntry->GetProtocolType() != (quint32)EFwKnownProtocols::TCP || pEntry->GetDirection() != EFwDirections::Inbound) return false; break;
		case ENetProtocols::eTCP_Client:	if (pEntry->GetProtocolType() != (quint32)EFwKnownProtocols::TCP || pEntry->GetDirection() != EFwDirections::Outbound) return false; break;
		case ENetProtocols::eUDP:			if (pEntry->GetProtocolType() != (quint32)EFwKnownProtocols::UDP) return false; break;
		}
	}
	if (m_Action != EEventStatus::eUndefined)
	{
		switch (pEntry->GetAction())
		{
		case EFwActions::Allow:	return m_Action == EEventStatus::eAllowed;
		case EFwActions::Block:	return m_Action == EEventStatus::eBlocked;
		}
	}
	return true;
}

CTraceModel::STraceNode* CNetTraceModel::MkNode(const SMergedLog::TLogEntry& Data)
{
	quint64 ID = Data.second->GetUID();

	STreeNode* pNode = (STreeNode*)MkNode(ID);
	pNode->pLogEntry = Data.second;
	pNode->pProgram = Data.first;

	return pNode;
}

QVariant CNetTraceModel::NodeData(STraceNode* pTraceNode, int role, int section) const
{
	STreeNode* pNode = (STreeNode*)pTraceNode;
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

	switch (role)
	{
	case Qt::DisplayRole:
	case Qt::EditRole: // sort role
	{
		switch (section)
		{
		case eName:				return (pNode->pProgram ? pNode->pProgram->GetNameEx() : tr("Unknown Program"))
			+ (!pEntry->GetOwnerService().isEmpty() ? tr(" (%1)").arg(pEntry->GetOwnerService()) : "");
		case eAction:			return pEntry->GetActionStr();
		case eDirection:		return pEntry->GetDirectionStr();
		case eProtocol:			return CFwRule::ProtocolToStr((EFwKnownProtocols)pEntry->GetProtocolType()); break;
		case eRAddress: {
			QString Data = pEntry->GetRemoteAddress().toString();
			QString Host = pEntry->GetRemoteHostName();
			if (!Host.isEmpty()) Data += " (" + Host + ")";
			return Data;
		}
		case eRPort:			return pEntry->GetRemotePort();
		case eLAddress:			return pEntry->GetLocalAddress().toString();
		case eLPort:			return pEntry->GetLocalPort();
			//case eUploaded:		return Connection[].AsQStr(); 
			//case eDownloaded:		return Connection[].AsQStr(); 
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

PoolAllocator<sizeof(CNetTraceModel::STreeNode)> CNetTraceModel::m_NodeAllocator;

CNetTraceModel::STraceNode*	CNetTraceModel::MkNode(quint64 Id) 
{ 
	STreeNode* pNode = (STreeNode*)m_NodeAllocator.allocate(sizeof(STreeNode));
	new (pNode) STreeNode(Id);
	return pNode;
	//return new STraceNode(Id);
}

void CNetTraceModel::FreeNode(STraceNode* pNode) 
{ 
	foreach(STraceNode* pSubNode, pNode->Children)
		FreeNode(pSubNode);
	pNode->~STraceNode();
	m_NodeAllocator.free(pNode);
	//delete pNode; 
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