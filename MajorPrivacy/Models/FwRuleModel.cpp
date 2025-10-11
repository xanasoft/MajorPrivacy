#include "pch.h"
#include "FwRuleModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../MajorPrivacy.h"
#include "../Windows/FirewallRuleWnd.h"

CFwRuleModel::CFwRuleModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CFwRuleModel::~CFwRuleModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex>	CFwRuleModel::Sync(const QList<CFwRulePtr>& RuleList)
{
#pragma warning(push)
#pragma warning(disable : 4996)
	TNewNodesMap New;
#pragma warning(pop)
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach(const CFwRulePtr& pRule, RuleList)
	{
		QVariant Guid = pRule->GetGuid().ToQV();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(Guid);
		SRuleNode* pNode = I != Old.end() ? static_cast<SRuleNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SRuleNode*>(MkNode(Guid));
			pNode->Values.resize(columnCount());
			//pNode->Path = Path;
			pNode->pRule = pRule;
			New[pNode->Path.count()][pNode->Path].append(pNode);
		}
		else
		{
			I.value() = NULL;
			pNode->pRule = pRule;
			Index = Find(m_Root, pNode);
		}

		if (pNode->pProg.isNull() || pNode->pRule->GetProgramID() != pNode->pProg->GetID()) {
			pNode->pProg = theCore->ProgramManager()->GetProgramByID(pNode->pRule->GetProgramID());
			pNode->Icon.clear();
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

		int Col = 0;
		bool State = false;
		int Changed = 0;
		if (pNode->Icon.isNull() && pNode->pProg && !pNode->pProg->GetIcon().isNull())
		{
			pNode->Icon = pNode->pProg->GetIcon();
			Changed = 1;
		}
		if (pNode->pProg && pNode->IsMissing != pNode->pProg->IsMissing()) {
			pNode->IsMissing = pNode->pProg->IsMissing();
			pNode->TextColor = pNode->IsMissing ? QBrush(Qt::red) : QVariant();
			Changed = 2; // set change for all columns
		}
		if (pNode->IsGray != !pRule->IsEnabled()) {
			pNode->IsGray = !pRule->IsEnabled();
			Changed = 2; // set change for all columns
		}
		if (pNode->IsItalic != pRule->IsTemporary()) {
			pNode->IsItalic = pRule->IsTemporary();
			Changed = 2; // set change for all columns
		}

		for (int section = 0; section < columnCount(); section++)
		{
			if (!IsColumnEnabled(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch (section)
			{
			case eName:				Value = tr("%1 - %2").arg(pRule->GetName()).arg(pRule->GetGuid().ToQS()); break;
			case eGrouping:			Value = pRule->GetGrouping(); break;
			case eIndex:			Value = pRule->GetIndex(); break;
			case eState:			Value = (int)pRule->GetState(); break;
			case eSource:			Value = (int)pRule->GetSource(); break;
			case eEnabled:			Value = pRule->IsEnabled(); break;
			case eHitCount:			Value = pRule->GetHitCount(); break;
			case eProfiles:			Value = pRule->GetProfile(); break;
			case eAction:			Value = (uint32)pRule->GetAction(); break;
			case eDirection:		Value = (uint32)pRule->GetDirection(); break;
			case eProtocol:			Value = (uint32)pRule->GetProtocol(); break;
			case eRemoteAddress:	Value = pRule->GetRemoteAddresses(); break;
			case eLocalAddress:		Value = pRule->GetLocalAddresses(); break;
			case eRemotePorts:		Value = pRule->GetRemotePorts(); break;
			case eLocalPorts:		Value = pRule->GetLocalPorts(); break;
			case eICMPOptions:		Value = pRule->GetIcmpTypesAndCodes(); break;
			case eInterfaces:		Value = pRule->GetInterface(); break;
			case eEdgeTraversal:	Value = pRule->GetEdgeTraversal(); break;
			case eProgram:			Value = pRule->GetProgram(); break;
			}

			SRuleNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
					case eName:				ColValue.Formatted = CMajorPrivacy::GetResourceStr(pRule->GetName()); break;
					case eGrouping:			ColValue.Formatted = pRule->GetGrouping().isEmpty() ? "" : CMajorPrivacy::GetResourceStr(pRule->GetGrouping()); break;
					case eIndex:			ColValue.Formatted = pRule->GetIndex() == -1 ? "" : QString::number(pRule->GetIndex()); break;
					case eState: 			ColValue.Formatted = pRule->GetStateStr(); ColValue.Color = pRule->GetStateColor(); break;
					case eSource:			ColValue.Formatted = pRule->GetSourceStr(); break;
					case eEnabled:			ColValue.Formatted = (pRule->IsEnabled() ? tr("Yes") : tr("No")) + (pRule->IsTemporary() ? tr(" (Temporary)") : ""); break;
					case eProfiles:			ColValue.Formatted = pRule->GetProfileStr(); break;
					case eAction:			ColValue.Formatted = pRule->GetActionStr(); { QColor Color = CFirewallRuleWnd::GetActionColor(pRule->GetAction()); if(Color.isValid()) ColValue.Color = Color; } break;
					case eDirection:		ColValue.Formatted = pRule->GetDirectionStr(); { QColor Color = CFirewallRuleWnd::GetDirectionColor(pRule->GetDirection()); if(Color.isValid()) ColValue.Color = Color; } break;
					case eProtocol:			ColValue.Formatted = CFwRule::ProtocolToStr(pRule->GetProtocol()); break;
					case eRemoteAddress:	ColValue.Formatted = pRule->GetRemoteAddresses().join(", "); break;
					case eLocalAddress:		ColValue.Formatted = pRule->GetLocalAddresses().join(", "); break;
					case eRemotePorts:		ColValue.Formatted = pRule->GetRemotePorts().join(", "); break;
					case eLocalPorts:		ColValue.Formatted = pRule->GetLocalPorts().join(", "); break;
					case eICMPOptions:		ColValue.Formatted = pRule->GetIcmpTypesAndCodes().join(", "); break;
					case eInterfaces:		ColValue.Formatted = pRule->GetInterfaceStr(); break;
				}
			}

			if (State != (Changed != 0))
			{
				if (State && Index.isValid())
					emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), section - 1, pNode));
				State = (Changed != 0);
				Col = section;
			}
			if (Changed == 1)
				Changed = 0;
		}
		if (State && Index.isValid())
			emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), columnCount() - 1, pNode));

	}

	QList<QModelIndex>	NewBranches;
	CTreeItemModel::Sync(New, Old, &NewBranches);
	return NewBranches;
}

CFwRuleModel::STreeNode* CFwRuleModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

CFwRulePtr CFwRuleModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
        return CFwRulePtr();

	SRuleNode* pNode = static_cast<SRuleNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pRule;
}

int CFwRuleModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CFwRuleModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eGrouping:				return tr("Grouping");
		case eIndex:				return tr("Index");
		case eState:				return tr("State");
		case eSource:				return tr("Source");
		case eEnabled:				return tr("Enabled");
		case eHitCount:				return tr("Hit Count");
		case eProfiles:				return tr("Profiles");
		case eAction:				return tr("Action");
		case eDirection:			return tr("Direction");
		case eProtocol:				return tr("Protocol");
		case eRemoteAddress:		return tr("Remote Address");
		case eLocalAddress:			return tr("Local Address");
		case eRemotePorts:			return tr("Remote Ports");
		case eLocalPorts:			return tr("Local Ports");
		case eICMPOptions:			return tr("ICMP Options");
		case eInterfaces:			return tr("Interfaces");
		case eEdgeTraversal:		return tr("Edge Traversal");
		case eProgram:				return tr("Program");
		}
	}
	return QVariant();
}