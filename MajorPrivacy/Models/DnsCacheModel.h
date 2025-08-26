#pragma once
#include <qwidget.h>
#include "../MiscHelpers/Common/TreeItemModel.h"
#include "../Core/Network/DnsCacheEntry.h"

class CDnsCacheModel : public CTreeItemModel
{
    Q_OBJECT

public:
    CDnsCacheModel(QObject *parent = 0);
	~CDnsCacheModel();

	typedef CDnsCacheEntryPtr ItemType;

	//void			SetProcessFilter(const QList<QSharedPointer<QObject> >& Processes) { m_ProcessFilter = true; m_Processes = Processes; }

	void			Sync(QMap<quint64, CDnsCacheEntryPtr> List);
	
	CDnsCacheEntryPtr	GetItem(const QModelIndex &index) const;

    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		//eProcess = 0,
		eHostName,
		eType,
		eTTL,
		eStatus,
		eTimeStamp,
		eCueryCount,
		eResolvedData,
		eCount
	};

protected:
	/*bool					m_ProcessFilter;
	QList<QSharedPointer<QObject> > m_Processes;*/

	struct SDnsNode: STreeNode
	{
		SDnsNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id), iColor(0) {}

		CDnsCacheEntryPtr		pEntry;

		int					iColor;
	};

	virtual void SyncEntry(TNewNodesMap& New, QHash<QVariant, STreeNode*>& Old, const CDnsCacheEntryPtr& pEntry/*, const CDnsProcRecordPtr& pRecord = CDnsProcRecordPtr()*/);

	virtual STreeNode* MkNode(const QVariant& Id) { return new SDnsNode(/*this,*/ Id); }
};
