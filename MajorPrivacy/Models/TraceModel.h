#pragma once
#include <qwidget.h>
#include "../MiscHelpers/Common/TreeItemModel.h"
#include "../Library/Common/PoolAllocator.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/TraceLogUtils.h"

class CTraceModel : public QAbstractItemModelEx
{
	Q_OBJECT

public:
	typedef CLogEntryPtr ItemType;

	CTraceModel(QObject* parent = 0);
	~CTraceModel();

	//void					SetTree(bool bTree)				{ m_bTree = bTree; }
	//bool					IsTree() const					{ return m_bTree; }
	
	void					SetTextFilter(const QString& Exp, bool bHighLight) { m_FilterExp = Exp; m_bHighLight = bHighLight; }

	virtual QList<QModelIndex> Sync(const QVector<SMergedLog::TLogEntry>& List, quint64 uRecentLimit);

	CLogEntryPtr			GetItem(const QModelIndex& index) const;
	QVariant				GetItemID(const QModelIndex& index) const;
	QVariant				Data(const QModelIndex &index, int role, int section) const;

	// derived functions
    virtual QVariant		data(const QModelIndex &index, int role) const;
    virtual Qt::ItemFlags	flags(const QModelIndex &index) const;
    virtual QModelIndex		index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual QModelIndex		parent(const QModelIndex &index) const;
    virtual int				rowCount(const QModelIndex &parent = QModelIndex()) const;
	//virtual int				columnCount(const QModelIndex &parent = QModelIndex()) const;
	//virtual QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	virtual void			Clear();
protected:

	//struct STracePath {
	//	STracePath() { Q_ASSERT(0); }
	//	STracePath(quint32* p, int c) : path(p), count(c), owner(false) {}
	//	STracePath(const STracePath& other) 
	//	{
	//		count = other.count;
	//		path = new quint32[count];
	//		memcpy(path, other.path, count * sizeof(quint32));
	//		owner = true;
	//	}
	//	~STracePath() {
	//		if (owner) 
	//			delete[] path;
	//	}
	//	STracePath& operator = (const STracePath& other) { Q_ASSERT(0); return *this; }
	//	quint32*		path;
	//	int				count;
	//	bool			owner;
	//};
	//
	//friend uint qHash(const STracePath& var);
	//friend bool operator == (const STracePath& l, const STracePath& r);

	struct STraceNode
	{
		STraceNode(quint64 Id) { ID = Id; }
		virtual ~STraceNode(){}

		quint64				ID;

		STraceNode*			Parent = NULL;
		//int					Row = 0;
		QVector<STraceNode*>	Children;

		CLogEntryPtr		pLogEntry;
	};


	//bool					m_bTree;
	QVariant				m_LastID;
	int						m_LastCount;

	virtual QVariant		NodeData(STraceNode* pNode, int role, int section) const;

	virtual bool			FilterNode(const SMergedLog::TLogEntry& Data) const {return true;};

	virtual STraceNode*		MkNode(quint64 Id) = 0;
	virtual STraceNode*		MkNode(const SMergedLog::TLogEntry& Data) = 0;
	virtual void			FreeNode(STraceNode* pNode) = 0;

	STraceNode*				FindParentNode(STraceNode* pParent, quint64 Path, int PathsIndex, QList<QModelIndex>* pNewBranches);

	STraceNode*				m_Root;
	QHash<quint64, STraceNode*> m_Branches;

	QString					m_FilterExp;
	bool					m_bHighLight;

	virtual bool			TestHighLight(STraceNode* pNode) const;
};
