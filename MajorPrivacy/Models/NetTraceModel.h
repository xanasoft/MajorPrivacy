#pragma once
#include <qwidget.h>
#include "../MiscHelpers/Common/TreeItemModel.h"
#include "../Library/Common/PoolAllocator.h"
#include "../Core/ProcessList.h"
#include "../Core/Programs/ProgramFile.h"

class CNetTraceModel : public QAbstractItemModelEx
{
	Q_OBJECT

public:
	CNetTraceModel(QObject* parent = 0);
	~CNetTraceModel();

	void			SetTree(bool bTree)				{ m_bTree = bTree; }
	bool			IsTree() const					{ return m_bTree; }
	
	void			SetHighLight(const QString& Exp) { m_HighLightExp = Exp; }

	QList<QModelIndex>	Sync(const struct SMergedLog* pLog);

	CLogEntryPtr	GetLogEntry(const QModelIndex& index) const;
	CProgramFilePtr	GetProgram(const QModelIndex& index) const;
	QVariant		GetItemID(const QModelIndex& index) const;
	QVariant		Data(const QModelIndex &index, int role, int section) const;

	// derived functions
    virtual QVariant		data(const QModelIndex &index, int role) const;
    virtual Qt::ItemFlags	flags(const QModelIndex &index) const;
    virtual QModelIndex		index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual QModelIndex		parent(const QModelIndex &index) const;
    virtual int				rowCount(const QModelIndex &parent = QModelIndex()) const;
	virtual int				columnCount(const QModelIndex &parent = QModelIndex()) const;
	virtual QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	void			Clear(bool bMem = false);

	enum EColumns
	{
		eName = 0,
		eProtocol,
		eAction,
		eDirection,
		eRAddress,
		eRPort,
		eLAddress,
		eLPort,
		eUploaded,
		eDownloaded,
		eTimeStamp,
		eProgram,
		eCount
	};

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

	struct STreeNode
	{
		STreeNode(quint64 Id) { ID = Id; }
		//virtual ~STreeNode(){}

		quint64				ID;

		STreeNode*			Parent = NULL;
		//int					Row = 0;
		QVector<STreeNode*>	Children;

		CLogEntryPtr		pLogEntry;
		CProgramFilePtr		pProgram;
	};


	bool					m_bTree;
	QVariant				m_LastID;
	int						m_LastCount;

	virtual QVariant		NodeData(STreeNode* pNode, int role, int section) const;

	virtual STreeNode*		MkNode(quint64 Id);
	virtual void			FreeNode(STreeNode* pNode);

	STreeNode*				FindParentNode(STreeNode* pParent, quint64 Path, int PathsIndex, QList<QModelIndex>* pNewBranches);

	STreeNode*				m_Root;
	QHash<quint64, STreeNode*> m_Branches;

	QString					m_HighLightExp;

	bool					TestHighLight(STreeNode* pNode) const;

	static PoolAllocator<sizeof(STreeNode)> m_NodeAllocator;
};
