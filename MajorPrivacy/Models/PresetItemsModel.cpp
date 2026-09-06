#include "pch.h"
#include "PresetItemsModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/ProgramManager.h"
#include "../Core/Network/NetworkManager.h"
#include "../Core/Access/AccessManager.h"
#include "../Core/Tweaks/TweakManager.h"
#include "../MajorPrivacy.h"

CPresetItemsModel::CPresetItemsModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CPresetItemsModel::~CPresetItemsModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

void CPresetItemsModel::SetItems(const QMap<QFlexGuid, SItemPreset>& Items)
{
	m_Items = Items;
}

QList<QModelIndex> CPresetItemsModel::Sync()
{
#pragma warning(push)
#pragma warning(disable : 4996)
	TNewNodesMap New;
#pragma warning(pop)
	QHash<QVariant, STreeNode*> Old = m_Map;

	for (auto I = m_Items.constBegin(); I != m_Items.constEnd(); ++I)
	{
		const QFlexGuid& Guid = I.key();
		const SItemPreset& Item = I.value();

		QVariant ID = Guid.ToQV();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator J = Old.find(ID);
		SItemNode* pNode = J != Old.end() ? static_cast<SItemNode*>(J.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SItemNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->Guid = Guid;
			pNode->Item = Item;
			New[pNode->Path.count()][pNode->Path].append(pNode);
		}
		else
		{
			J.value() = NULL;
			pNode->Item = Item;
			Index = Find(m_Root, pNode);
		}

		// Get rule/tweak information
		QString Name;
		QString TypeStr;
		QString StatusStr;
		QString OwnerStr;
		QString Description;
		QIcon Icon;
		bool bEnabled = false;
		bool bUnknown = false;

		// Check if item has an owner (from item ownership)
		SItemOwnerInfo OwnerInfo = m_Ownership.value(Guid);
		if (!OwnerInfo.PresetGuid.IsNull()) {
			OwnerStr = OwnerInfo.PresetName;
		}

		switch (Item.Type)
		{
		case EItemType::eExecRule:
			TypeStr = tr("Program");
			if (CProgramRulePtr pRule = theCore->ProgramManager()->GetProgramRuleByGuid(Guid)) {
				Name = CMajorPrivacy::GetResourceStr(pRule->GetName());
				Description = CMajorPrivacy::GetResourceStr(pRule->GetDescription());
				bEnabled = pRule->IsEnabled();
				StatusStr = bEnabled ? tr("Enabled") : tr("Disabled");
				if (!OwnerInfo.PresetGuid.IsNull())
					StatusStr += QString(" (%1)").arg(OwnerInfo.bWasEnabled ? tr("Enable") : tr("Disable"));
				pNode->pProg = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID());
				if (pNode->pProg)
					Icon = pNode->pProg->GetIcon();
				else
					Icon = QIcon(":/Icons/Process.png");
			} else {
				Name = tr("Unknown Program Rule (%1)").arg(Guid.ToQS());
				StatusStr = tr("Unknown");
				bUnknown = true;
				Icon = QIcon(":/Icons/Process.png");
			}
			break;

		case EItemType::eResRule:
			TypeStr = tr("Resource");
			if (CAccessRulePtr pRule = theCore->AccessManager()->GetAccessRuleByGuid(Guid)) {
				Name = CMajorPrivacy::GetResourceStr(pRule->GetName());
				Description = CMajorPrivacy::GetResourceStr(pRule->GetDescription());
				bEnabled = pRule->IsEnabled();
				StatusStr = bEnabled ? tr("Enabled") : tr("Disabled");
				if (!OwnerInfo.PresetGuid.IsNull())
					StatusStr += QString(" (%1)").arg(OwnerInfo.bWasEnabled ? tr("Enable") : tr("Disable"));
				pNode->pProg = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID());
				if (pNode->pProg)
					Icon = pNode->pProg->GetIcon();
				else
					Icon = QIcon(":/Icons/Ampel.png");
			} else {
				Name = tr("Unknown Resource Rule (%1)").arg(Guid.ToQS());
				StatusStr = tr("Unknown");
				bUnknown = true;
				Icon = QIcon(":/Icons/Ampel.png");
			}
			break;

		case EItemType::eFwRule:
			TypeStr = tr("Firewall");
			if (CFwRulePtr pRule = theCore->NetworkManager()->GetFwRuleByGuid(Guid)) {
				Name = CMajorPrivacy::GetResourceStr(pRule->GetName());
				Description = CMajorPrivacy::GetResourceStr(pRule->GetDescription());
				bEnabled = pRule->IsEnabled();
				StatusStr = bEnabled ? tr("Enabled") : tr("Disabled");
				if (!OwnerInfo.PresetGuid.IsNull())
					StatusStr += QString(" (%1)").arg(OwnerInfo.bWasEnabled ? tr("Enable") : tr("Disable"));
				pNode->pProg = theCore->ProgramManager()->GetProgramByID(pRule->GetProgramID());
				if (pNode->pProg)
					Icon = pNode->pProg->GetIcon();
				else
					Icon = QIcon(":/Icons/Wall3.png");
			} else {
				Name = tr("Unknown Firewall Rule (%1)").arg(Guid.ToQS());
				StatusStr = tr("Unknown");
				bUnknown = true;
				Icon = QIcon(":/Icons/Wall3.png");
			}
			break;

		case EItemType::eDnsRule:
			TypeStr = tr("DNS");
			if (CDnsRulePtr pRule = theCore->NetworkManager()->GetDnsRuleByGuid(Guid)) {
				Name = CMajorPrivacy::GetResourceStr(pRule->GetHostName());
				// DNS rules typically don't have separate description
				bEnabled = pRule->IsEnabled();
				StatusStr = bEnabled ? tr("Enabled") : tr("Disabled");
				if (!OwnerInfo.PresetGuid.IsNull())
					StatusStr += QString(" (%1)").arg(OwnerInfo.bWasEnabled ? tr("Enable") : tr("Disable"));
				Icon = QIcon(":/Icons/Network2.png");
			} else {
				Name = tr("Unknown DNS Rule (%1)").arg(Guid.ToQS());
				StatusStr = tr("Unknown");
				bUnknown = true;
				Icon = QIcon(":/Icons/Network2.png");
			}
			break;

		case EItemType::eTweak:
			TypeStr = tr("Tweak");
			if (CTweakPtr pTweak = theCore->TweakManager()->GetTweakById(Guid.ToQS())) {
				Name = CMajorPrivacy::GetResourceStr(pTweak->GetName());
				Description = CMajorPrivacy::GetResourceStr(pTweak->GetDescription());
				ETweakStatus status = pTweak->GetStatus();
				if (status == ETweakStatus::eApplied || status == ETweakStatus::eSet) {
					StatusStr = tr("Applied");
					bEnabled = true;
				}
				else {
					StatusStr = tr("Not Applied");
					bEnabled = false;
				}
				if (!OwnerInfo.PresetGuid.IsNull())
					StatusStr += QString(" (%1)").arg(OwnerInfo.bWasEnabled ? tr("Apply") : tr("Undo"));
				auto pGroup = pTweak.objectCast<CTweakGroup>();
				if (pGroup)
					Icon = pGroup->GetIcon();
				else
					Icon = QIcon(":/Icons/Tweaks.png");
			} else {
				Name = tr("Unknown Tweak (%1)").arg(Guid.ToQS());
				StatusStr = tr("Unknown");
				bUnknown = true;
				Icon = QIcon(":/Icons/Tweaks.png");
			}
			break;
		}

		// Update icon
		if (pNode->Icon.isNull() && !Icon.isNull())
			pNode->Icon = Icon;

		// Update gray state for disabled items
		if (pNode->IsGray != !Item.Enabled) {
			pNode->IsGray = !Item.Enabled;
		}

		// Update missing status
		if (pNode->pProg && pNode->IsMissing != pNode->pProg->IsMissing()) {
			pNode->IsMissing = pNode->pProg->IsMissing();
			pNode->TextColor = pNode->IsMissing ? QBrush(Qt::red) : QVariant();
		}

		int Col = 0;
		bool State = false;
		int Changed = 0;

		for (int section = 0; section < columnCount(); section++)
		{
			if (!IsColumnEnabled(section))
				continue;

			QVariant Value;
			switch (section)
			{
			case eName:			Value = Name; break;
			case eType:			Value = TypeStr; break;
			case eAction:		Value = (int)Item.Activate; break;
			case eStatus:		Value = StatusStr; break;
			case eOwner:		Value = OwnerStr; break;
			case eDescription:	Value = Description; break;
			}

			SItemNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eName:			ColValue.Formatted = Name; break;
				case eType:			ColValue.Formatted = TypeStr; break;
				case eAction:
					switch (Item.Activate) {
					case SItemPreset::EActivate::eUndefined:	ColValue.Formatted = tr("Do Nothing"); break;
					case SItemPreset::EActivate::eEnable:		ColValue.Formatted = tr("Enable"); break;
					case SItemPreset::EActivate::eDisable:		ColValue.Formatted = tr("Disable"); break;
					}
					ColValue.Color = GetActionColor(Item.Activate);
					break;
				case eStatus:
					ColValue.Formatted = StatusStr;
					ColValue.Color = GetStatusColor(bEnabled, bUnknown);
					break;
				case eOwner:		ColValue.Formatted = OwnerStr; break;
				case eDescription:	ColValue.Formatted = Description; break;
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

	QList<QModelIndex> NewBranches;
	CTreeItemModel::Sync(New, Old, &NewBranches);
	return NewBranches;
}

QFlexGuid CPresetItemsModel::GetItemGuid(const QModelIndex& index) const
{
	if (!index.isValid())
		return QFlexGuid();

	SItemNode* pNode = static_cast<SItemNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->Guid;
}

SItemPreset CPresetItemsModel::GetItem(const QModelIndex& index) const
{
	if (!index.isValid())
		return SItemPreset();

	SItemNode* pNode = static_cast<SItemNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->Item;
}

int CPresetItemsModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CPresetItemsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:			return tr("Name");
		case eType:			return tr("Type");
		case eAction:		return tr("Action");
		case eStatus:		return tr("Current Status");
		case eOwner:		return tr("Item Owner");
		case eDescription:	return tr("Description");
		}
	}
	return QVariant();
}

QColor CPresetItemsModel::GetActionColor(SItemPreset::EActivate Action)
{
	switch (Action)
	{
	case SItemPreset::EActivate::eEnable:		return QColor(144, 238, 144); // Light green
	case SItemPreset::EActivate::eDisable:		return QColor(255, 182, 193); // Light red/pink
	case SItemPreset::EActivate::eUndefined:	return QColor(211, 211, 211); // Light gray
	default: return QColor();
	}
}

QColor CPresetItemsModel::GetStatusColor(bool bEnabled, bool bUnknown)
{
	if (bUnknown)
		return QColor(255, 230, 180); // Very light orange/amber for unknown
	else if (bEnabled)
		return QColor(200, 255, 200); // Very light green
	else
		return QColor(255, 220, 220); // Very light red/pink
}
