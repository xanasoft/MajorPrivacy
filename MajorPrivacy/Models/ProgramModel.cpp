#include "pch.h"
#include "ProgramModel.h"
#include "../Core/PrivacyCore.h"
#include "../../MiscHelpers/Common/Common.h"


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
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QSet<QVariant> Current;
	QHash<QVariant, STreeNode*> Old = m_Map;

	Sync(pRoot, QString::number(pRoot->GetUID()), QList<QVariant>(), New, Current, Old, Added);

	CTreeItemModel::Sync(New, Old);
	return Added;
}

bool CProgramModel::FilterByType(EProgramType Type)
{
	switch (Type)
	{
	case EProgramType::eProgramFile:	return (m_Filter & EFilters::ePrograms);

	case EProgramType::eAllPrograms:

	case EProgramType::eWindowsService: return (m_Filter & EFilters::eSystem);

	case EProgramType::eAppPackage:		return (m_Filter & EFilters::eApps);

	//case EProgramType::eProgramSet:
	//case EProgramType::eProgramList:

	case EProgramType::eFilePattern:
	case EProgramType::eAppInstallation:return m_Filter != EFilters::eApps;

	case EProgramType::eProgramGroup:	return true;
	}
	return false;
}

void CProgramModel::Sync(const CProgramSetPtr& pRoot, const QString& RootID, const QList<QVariant>& Path, QMap<QList<QVariant>, QList<STreeNode*> >& New, QSet<QVariant>& Current, QHash<QVariant, STreeNode*>& Old, QList<QVariant>& Added)
{
	foreach(const CProgramItemPtr& pItem, pRoot->GetNodes())
	{
		quint64 uID = pItem->GetUID();
		QString sID = RootID + "/" + QString::number(uID);
		QVariant ID = sID;

		CProgramSetPtr pGroup = pItem.objectCast<CProgramSet>();

		if (m_Filter) 
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
			New[pNode->Path].append(pNode);
			Added.append(ID);
		}
		else
		{
			I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		if (pGroup)
			Sync(pGroup, sID, QList<QVariant>(pNode->Path) << ID, New, Current, Old, Added);

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

		int Col = 0;
		bool State = false;
		int Changed = 0;
		if (pNode->Icon.isNull() && !pItem->GetIcon().isNull())
		{
			pNode->Icon = pItem->GetIcon();
			Changed = 1;
		}

		for (int section = 0; section < columnCount(); section++)
		{
			//if (!IsColumnEnabled(section)) // todo xxxxxxxxx
			//	continue; // ignore columns which are hidden

			QVariant Value;
			switch (section)
			{
			case eName: 			Value = pItem->GetNameEx(); break;
			case eType:				Value = pItem->GetTypeStr(); break;

			case eRunning:			Value = pItem->GetStats()->ProcessCount; break;
			case eProgramRules:		Value = pItem->GetStats()->ProgRuleCount; break;
			//case eTotalUpTime:

			//case eOpenFiles:		break;
			//case eFsTotalRead:
			//case eFsTotalWritten:
			case eAccessRules:		Value = pItem->GetStats()->ResRuleCount; break;

			case eSockets:			Value = pItem->GetStats()->SocketCount; break;
			case eFwRules:			Value = pItem->GetStats()->FwRuleCount; break;
			case eLastActivity:		Value = pItem->GetStats()->LastActivity; break;
			case eUpload:			Value = pItem->GetStats()->Upload; break;
			case eDownload:			Value = pItem->GetStats()->Download; break;
			case eUploaded:			Value = pItem->GetStats()->Uploaded; break;
			case eDownloaded:		Value = pItem->GetStats()->Downloaded; break;

			case ePath:				Value = pItem->GetPath(EPathType::eDisplay); break;

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
				//case eName: 		ColValue.Formatted = pItem->GetNameEx(); break;

				case eLastActivity:	ColValue.Formatted = Value.toULongLong() ? QDateTime::fromMSecsSinceEpoch(Value.toULongLong()).toString("dd.MM.yyyy hh:mm:ss") : ""; break;
				case eUpload:		ColValue.Formatted = FormatSize(Value.toULongLong()); break;
				case eDownload:		ColValue.Formatted = FormatSize(Value.toULongLong()); break;
				case eUploaded:		ColValue.Formatted = FormatSize(Value.toULongLong()); break;
				case eDownloaded:	ColValue.Formatted = FormatSize(Value.toULongLong()); break;
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
}

QVariant CProgramModel::NodeData(STreeNode* pNode, int role, int section) const
{
    switch(role)
	{
		case Qt::FontRole:
		{
			SProgramNode* pProcessNode = static_cast<SProgramNode*>(pNode);
			if (pProcessNode->Bold.contains(section))
			{
				QFont fnt;
				fnt.setBold(true);
				return fnt;
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

		case eRunning:				return tr("Processes");
		case eProgramRules:			return tr("Program Rules");
		//case eTotalUpTime:			return tr("Up Time");

		//case eOpenFiles:			return tr("Open Files");
		//case eFsTotalRead:
		//case eFsTotalWritten:
		case eAccessRules:			return tr("Access Rules");
		
		case eSockets:				return tr("Open Sockets");
		case eFwRules:				return tr("FW Rules");
		case eLastActivity:			return tr("Last Net Activity");
		case eUpload:				return tr("Upload");
		case eDownload:				return tr("Download");
		case eUploaded:				return tr("Uploaded");
		case eDownloaded:			return tr("Downloaded");

		case ePath:					return tr("Path");

		//case eInfo:					return tr("Info");
	}
	return "";
}
