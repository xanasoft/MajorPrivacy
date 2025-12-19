#include "pch.h"
#include "ProgramModel.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Programs/AppPackage.h"
#include "../../MiscHelpers/Common/Common.h"
#include "../../Library/Helpers/NtUtil.h"
#include "../../Library/Helpers/NtPathMgr.h"


CProgramModel::CProgramModel(QObject *parent)
:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bTree = true;
	m_bUseIcons = true;

	m_Filter = EFilters::eAll;
}

CProgramModel::~CProgramModel()
{
}

QList<QVariant> CProgramModel::Sync(const CProgramSetPtr& pRoot)
{
	QList<QVariant> Added;
#pragma warning(push)
#pragma warning(disable : 4996)
	TNewNodesMap New;
#pragma warning(pop)
	QSet<QVariant> Current;
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach(const CProgramItemPtr& pItem, pRoot->GetNodes())
	{
		if (!TestFilter(pItem->GetType(), pItem->GetStats()))
			continue;

		Sync(pItem, QString::number(pRoot->GetUID()), QList<QVariant>(), New, Current, Old, Added);
	}

	CTreeItemModel::Sync(New, Old);
	return Added;
}

QList<QVariant> CProgramModel::Sync(const QHash<quint64, CProgramItemPtr>& Items)
{
	QList<QVariant> Added;
#pragma warning(push)
#pragma warning(disable : 4996)
	TNewNodesMap New;
#pragma warning(pop)
	QSet<QVariant> Current;
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach(const CProgramItemPtr& pItem, Items)
	{
		if(pItem->GetUID() == 1)
			continue; // skip root

		if (!TestFilter(pItem->GetType(), pItem->GetStats(), false))
			continue;

		Sync(pItem, "", QList<QVariant>(), New, Current, Old, Added);
	}

	CTreeItemModel::Sync(New, Old);
	return Added;
}

bool CProgramModel::TestFilter(EProgramType Type, const SProgramStats* pStats, bool bTree)
{
	quint64 uRecentLimit = m_RecentLimit ? QDateTime::currentMSecsSinceEpoch() - m_RecentLimit : 0;

	if (m_Filter & EFilters::eTypeFilter)
	{
		bool bPass = false;

		if (m_Filter & EFilters::ePrograms) {
			if ((bTree && pStats->ProgramsCount > 0) || (Type == EProgramType::eProgramFile))
				bPass = true;
		}
		if (m_Filter & EFilters::eSystem) {
			if ((bTree && pStats->ServicesCount > 0) || (Type == EProgramType::eWindowsService))
				bPass = true;
		}
		if (m_Filter & EFilters::eApps) {
			if ((bTree && pStats->AppsCount > 0) || (Type == EProgramType::eAppPackage))
				bPass = true;
		}
		if (m_Filter & EFilters::eGroups) {
			if ((bTree && pStats->GroupCount > 0) || (Type == EProgramType::eFilePattern || Type == EProgramType::eAppInstallation || Type == EProgramType::eProgramGroup))
				bPass = true;
		}
		if(Type == EProgramType::eAllPrograms)
			bPass = true;

		if(!bPass)
			return false;
	}

	if (m_Filter & EFilters::eRunning) {
		if (pStats->ProcessCount == 0) {
			if (!(m_Filter & EFilters::eRanRecently))
				return false;
			else if (pStats->LastExecution <= uRecentLimit)
				return false;
		}
	}

	if (m_Filter & EFilters::eRulesFilter) 
	{
		bool bPass = false;

		if (m_Filter & EFilters::eWithProgRules) {
			if (pStats->ProgRuleTotal > 0)
				bPass = true;
		}
		if (m_Filter & EFilters::eWithResRules) {
			if (pStats->ResRuleTotal > 0)
				bPass = true;
		}
		if (m_Filter & EFilters::eWithFwRules) {
			if (pStats->FwRuleTotal > 0)
				bPass = true;
		}

		if (!bPass)
			return false;
	}

	if (m_Filter & EFilters::eTrafficFilter)
	{
		bool bPass = false;

		if (m_Filter & EFilters::eRecentTraffic) {
			if (pStats->LastNetActivity > uRecentLimit)
				bPass = true;
		}
		if (m_Filter & EFilters::eBlockedTraffic) {
			if (FILETIME2ms(pStats->LastFwBlocked) > uRecentLimit)
				bPass = true;
		}
		if (m_Filter & EFilters::eAllowedTraffic) {
			if (FILETIME2ms(pStats->LastFwAllowed) > uRecentLimit)
				bPass = true;
		}

		if (!bPass)
			return false;
	}

	if (m_Filter & EFilters::eWithSockets)
	{
		if (pStats->SocketCount == 0)
			return false;
	}

	return true;
}

void CProgramModel::Sync(const CProgramItemPtr& pItem, const QString& RootID, const QList<QVariant>& Path, TNewNodesMap& New, QSet<QVariant>& Current, QHash<QVariant, STreeNode*>& Old, QList<QVariant>& Added)
{
	quint64 uID = pItem->GetUID();
	QString sID = RootID + "/" + QString::number(uID);
	QVariant ID = sID;

	CProgramSetPtr pGroup;
	if (!RootID.isEmpty())
	{
		pGroup = pItem.objectCast<CProgramSet>();

		/*if (m_Filter & EFilters::eByType)
		{
			if(Current.contains(ID))
				continue;
			Current.insert(ID);

			EProgramType Type = pItem->GetType();
			if (!FilterByType(Type))
			{
				if (pGroup)
					Sync(pGroup, RootID, Path, New, Current, Old, Added);
				continue;
			}
		}*/
	}

	QModelIndex Index;

	QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
	SProgramNode* pNode = I != Old.end() ? static_cast<SProgramNode*>(I.value()) : NULL;
	if (!pNode)
	{
		pNode = static_cast<SProgramNode*>(MkNode(ID));
		pNode->Values.resize(columnCount());
		if (m_bTree)
			pNode->Path = Path;
		pNode->pItem = pItem;
		New[pNode->Path.count()][pNode->Path].append(pNode);
		Added.append(ID);
	}
	else
	{
		I.value() = NULL;
		Index = Find(m_Root, pNode);
	}

	if (pGroup)
	{
		foreach(const CProgramItemPtr& pItem, pGroup->GetNodes())
		{
			if (!TestFilter(pItem->GetType(), pItem->GetStats()))
				continue;

			Sync(pItem, sID, QList<QVariant>(pNode->Path) << ID, New, Current, Old, Added);
		}
	}

	//if(Index.isValid()) // this is to slow, be more precise
	//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

	const SProgramStats* pStats = pItem->GetStats();

	int Col = 0;
	bool State = false;
	int Changed = 0;
	if (pNode->Icon.isNull() && !pItem->GetIcon().isNull() || pNode->IconFile != pItem->GetIconFile())
	{
		pNode->IconFile = pItem->GetIconFile();
		pNode->Icon = pItem->GetIcon();
		Changed = 1;
	}
	if (pNode->IsMissing != pItem->IsMissing()) {
		pNode->IsMissing = pItem->IsMissing();
		pNode->TextColor = pNode->IsMissing ? QBrush(Qt::red) : QVariant();
		Changed = 2; // set change for all columns
	}
	//if (pNode->IsGray != pItem->IsMissing()) 
	//{
	//	pNode->IsGray = pItem->IsMissing();
	//	Changed = 2; // set change for all columns
	//}

	for (int section = 0; section < columnCount(); section++)
	{
		//if (!IsColumnEnabled(section)) // todo xxxxxxxxx
		//	continue; // ignore columns which are hidden

		QVariant Value;
		switch (section)
		{
		case eName: 			Value = pItem->GetNameEx(); break;
		case eType:				Value = pItem->GetTypeStr(); break;

		case eRunningCount:		Value = pStats->ProcessCount; break;
		case eProgramRules:		Value = pStats->ProgRuleTotal; break;
		case eLastExecution:	Value = pStats->LastExecution; break;
		//case eTotalUpTime:

		case eOpenedFiles:		Value = pStats->AccessCount; break;
		//case eOpenFiles:		break;
		//case eFsTotalRead:
		//case eFsTotalWritten:
		case eAccessRules:		Value = pStats->ResRuleTotal; break;

		case eSockets:			Value = pStats->SocketCount; break;
		case eFwRules:			Value = pStats->FwRuleTotal; break;
		case eLastNetActivity:	Value = pStats->LastNetActivity; break;
		case eUpload:			Value = pStats->Upload; break;
		case eDownload:			Value = pStats->Download; break;
		case eUploaded:			Value = pStats->Uploaded; break;
		case eDownloaded:		Value = pStats->Downloaded; break;

		case eLogSize:			Value = pItem->GetLogMemUsage(); break;

		case ePath:				Value = pItem->GetPath(); break;

		//case eInfo:				Value = pItem->GetInfo(); break;
		}

		STreeNode::SValue& ColValue = pNode->Values[section];

		if (ColValue.Raw != Value)
		{
			if (Changed == 0)
				Changed = 1;
			ColValue.Raw = Value;

			switch (section)
			{
			case eProgramRules:		if(pStats->ProgRuleCount != pStats->ProgRuleTotal) ColValue.Formatted = tr("%1/%2").arg(pStats->ProgRuleCount).arg(pStats->ProgRuleTotal); break;
			case eLastExecution:	ColValue.Formatted = Value.toULongLong() ? QDateTime::fromMSecsSinceEpoch(Value.toULongLong()).toString("dd.MM.yyyy hh:mm:ss") : ""; break;

			case eAccessRules:		if(pStats->ResRuleCount != pStats->ResRuleTotal) ColValue.Formatted = tr("%1/%2").arg(pStats->ResRuleCount).arg(pStats->ResRuleTotal); break;

			case eFwRules:			if(pStats->FwRuleCount != pStats->FwRuleTotal) ColValue.Formatted = tr("%1/%2").arg(pStats->FwRuleCount).arg(pStats->FwRuleTotal); break;
			case eLastNetActivity:	ColValue.Formatted = Value.toULongLong() ? QDateTime::fromMSecsSinceEpoch(Value.toULongLong()).toString("dd.MM.yyyy hh:mm:ss") : ""; break;
			case eUpload:			ColValue.Formatted = FormatSize(Value.toULongLong()); break;
			case eDownload:			ColValue.Formatted = FormatSize(Value.toULongLong()); break;
			case eUploaded:			ColValue.Formatted = FormatSize(Value.toULongLong()); break;
			case eDownloaded:		ColValue.Formatted = FormatSize(Value.toULongLong()); break;

			case eLogSize:			ColValue.Formatted = FormatSize(Value.toULongLong()); break;
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

QVariant CProgramModel::NodeData(STreeNode* pNode, int role, int section) const
{
	SProgramNode* pProcessNode = static_cast<SProgramNode*>(pNode);
    switch(role)
	{
		case Qt::FontRole:
		{
			if (pProcessNode->Bold.contains(section))
			{
				QFont fnt;
				fnt.setBold(true);
				return fnt;
			}
			break;
		}
		case Qt::ToolTipRole:
		{
			if(section == ePath)
				return  QString::fromStdWString(CNtPathMgr::Instance()->TranslateDosToNtPath(pProcessNode->pItem->GetPath().toStdWString()));
			else if (section == eName)
			{
				CAppPackagePtr pApp = pProcessNode->pItem.objectCast<CAppPackage>();
				if (pApp)
					return tr("App Name: %1\nPacket Name: %2\nPacket SID: %3\nPath: %4").arg(pApp->GetContainerName()).arg(pApp->GetPackageName()).arg(pApp->GetAppSid()).arg(pProcessNode->pItem->GetPath());
				else
					return pProcessNode->pItem->GetPath();
			}

			break;
		}
	}

	return CTreeItemModel::NodeData(pNode, role, section);
}

CProgramItemPtr CProgramModel::GetItem(const QModelIndex &index) const
{
	if (!index.isValid())
        return CProgramItemPtr();

	SProgramNode* pNode = static_cast<SProgramNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pItem;
}

int CProgramModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CProgramModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
		return GetColumHeader(section);
    return QVariant();
}

QString CProgramModel::GetColumHeader(int section) const
{
	switch(section)
	{
		case eName:					return tr("Name");
		case eType:					return tr("Type");

		case eRunningCount:			return tr("Processes");
		case eProgramRules:			return tr("Program Rules");
		case eLastExecution:		return tr("Last Execution");
		//case eTotalUpTime:			return tr("Up Time");

		case eOpenedFiles:			return tr("Accessed Files");
		//case eOpenFiles:			return tr("Open Files");
		//case eFsTotalRead:
		//case eFsTotalWritten:
		case eAccessRules:			return tr("Access Rules");
		
		case eSockets:				return tr("Open Sockets");
		case eFwRules:				return tr("FW Rules");
		case eLastNetActivity:		return tr("Last Net Activity");
		case eUpload:				return tr("Upload");
		case eDownload:				return tr("Download");
		case eUploaded:				return tr("Uploaded");
		case eDownloaded:			return tr("Downloaded");

		case eLogSize:				return tr("Log Size");

		case ePath:					return tr("Path");

		//case eInfo:					return tr("Info");
	}
	return "";
}
