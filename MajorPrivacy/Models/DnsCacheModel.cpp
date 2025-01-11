#include "pch.h"
#include "DnsCacheModel.h"
#include "../../MiscHelpers/Common/Common.h"

CDnsCacheModel::CDnsCacheModel(QObject *parent)
 : CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());
	//m_ProcessFilter = false;
}

CDnsCacheModel::~CDnsCacheModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

void CDnsCacheModel::Sync(QMap<quint64, CDnsCacheEntryPtr> List)
{
#pragma warning(push)
#pragma warning(disable : 4996)
	QMap<QList<QVariant>, QList<STreeNode*>> New;
#pragma warning(pop)
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach (const CDnsCacheEntryPtr& pEntry, List)
	{
		SyncEntry(New, Old, pEntry);

		/*QMap<QPair<QString, quint64>, CDnsProcRecordPtr> Records = pEntry->GetProcessRecords();
		if (Records.isEmpty() || !m_Columns.contains(eProcess))
		{
			SyncEntry(New, Old, pEntry);
		}
		else
		{
			for (QMap<QPair<QString, quint64>, CDnsProcRecordPtr>::iterator I = Records.begin(); I != Records.end(); ++I)
			{
				SyncEntry(New, Old, pEntry, I.value());
			}
		}*/
	}

	CTreeItemModel::Sync(New, Old);
}

void CDnsCacheModel::SyncEntry(QMap<QList<QVariant>, QList<STreeNode*>>& New, QHash<QVariant, STreeNode*>& Old, const CDnsCacheEntryPtr& pEntry/*, const CDnsProcRecordPtr& pRecord*/)
{
	// todo-later: make this prettier
	/*if (m_ProcessFilter)
	{
		if (!pRecord)
		{
			bool bFound = false;
			foreach(const CDnsProcRecordPtr& pCurRecord, pEntry->GetProcessRecords())
			{
				if (m_Processes.contains(pCurRecord->GetProcess()))
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
				return;
		}
		else if(!m_Processes.contains(pRecord->GetProcess()))
			return;
	}

	QVariant ID = pRecord ? (quint64)pRecord.data() : (quint64)pEntry.data();*/

	QVariant ID = (quint64)pEntry.data();

	QModelIndex Index;

	QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
	SDnsNode* pNode = I != Old.end() ? static_cast<SDnsNode*>(I.value()) : NULL;
	if(!pNode)
	{
		pNode = static_cast<SDnsNode*>(MkNode(ID));
		pNode->Values.resize(columnCount());
		pNode->pEntry = pEntry;
		New[pNode->Path].append(pNode);
	}
	else
	{
		I.value() = NULL;
		Index = Find(m_Root, pNode);
	}

	int Col = 0;
	bool State = false;
	int Changed = 0;

	// Note: icons are loaded asynchroniusly
	/*if (m_bUseIcons && !pNode->Icon.isValid() && m_Columns.contains(eProcess) && pRecord)
	{
		CProcessPtr pProcess = pRecord->GetProcess().toStrongRef().staticCast<CProcessInfo>();
		CModulePtr pModule = pProcess ? pProcess->GetModuleInfo() : CModulePtr();
		if (pModule)
		{
			QPixmap Icon = pModule->GetFileIcon();
			if (!Icon.isNull()) {
				Changed = 1; // set change for first column
				pNode->Icon = Icon;
			}
		}
	}*/

	if (pNode->IsGray != (pEntry->GetDeadTime() > 0))
	{
		pNode->IsGray = (pEntry->GetDeadTime() > 0);
		Changed = 2;
	}

	for(int section = 0; section < columnCount(); section++)
	{
		if (!IsColumnEnabled(section))
			continue; // ignore columns which are hidden

		QVariant Value;
		switch(section)
		{
			//case eProcess:		Value = pRecord ? pRecord->GetProcessName() : tr("Unknown process"); break;
			case eHostName:		Value = pEntry->GetHostName(); break;
			case eType:			Value = pEntry->GetType(); break;
			case eResolvedData:	Value = pEntry->GetResolvedString(); break;
			case eTTL:			Value = pEntry->GetTTL(); break;
			//case eTimeStamp:	Value = pRecord ? pRecord->GetLastSeen() : pEntry->GetCreateTimeStamp(); break;
			//case eCueryCount:	Value = pRecord ? pRecord->GetCounter() : pEntry->GetQueryCounter(); break;
			case eTimeStamp:	Value = pEntry->GetCreateTimeStamp(); break;
			case eCueryCount:	Value = pEntry->GetQueryCounter(); break;
		}

		SDnsNode::SValue& ColValue = pNode->Values[section];

		if (ColValue.Raw != Value)
		{
			if(Changed == 0)
				Changed = 1;
			ColValue.Raw = Value;

			switch (section)
			{
				/*case eProcess:		if(pRecord)
									{
										quint64 ProcessId = pRecord->GetProcessId();
										if (ProcessId)	
											ColValue.Formatted = tr("%1 (%2)").arg(pRecord->GetProcessName()).arg(ProcessId); 
									}
									break;*/
				case eType:			ColValue.Formatted = pEntry->GetTypeString(); break;
                case eTTL:			ColValue.Formatted = FormatNumber(Value.toULongLong() / 1000); break; // in seconds
				case eTimeStamp:	if(Value.toULongLong() != 0) ColValue.Formatted = QDateTime::fromSecsSinceEpoch(Value.toULongLong() / 1000).toString("dd.MM.yyyy hh:mm:ss"); break;
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

CDnsCacheEntryPtr CDnsCacheModel::GetItem(const QModelIndex &index) const
{
	if (!index.isValid())
        return CDnsCacheEntryPtr();

	SDnsNode* pNode = static_cast<SDnsNode*>(index.internalPointer());
	return pNode->pEntry;
}

int CDnsCacheModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CDnsCacheModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			//case eProcess:		return tr("Process");
			case eHostName:		return tr("Host name");
			case eType:			return tr("Type");
			case eTTL:			return tr("TTL (sec)");
			case eTimeStamp:	return tr("Last seen");
			case eCueryCount:	return tr("Counter");
			case eResolvedData:	return tr("Resolved data");
		}
	}
    return QVariant();
}

