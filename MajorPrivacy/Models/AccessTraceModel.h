#pragma once
#include <qwidget.h>
#include "../MiscHelpers/Common/TreeItemModel.h"
#include "../Library/Common/PoolAllocator.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/ProgramLibrary.h"
#include "../Core/Access/ResLogEntry.h"
#include "TraceModel.h"

class CAccessTraceModel : public CTraceModel
{
	Q_OBJECT

public:
	typedef CLogEntryPtr ItemType;

	CAccessTraceModel(QObject* parent = 0);
	~CAccessTraceModel();

	void					SetPathFilter(const QString& RootPath) { m_RootPath = RootPath; }

	void					SetFilter(EEventStatus Action) { m_Action = Action; }

	virtual int				columnCount(const QModelIndex &parent = QModelIndex()) const;
	virtual QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		ePath,
		eAccess,
		eStatus,
		eTimeStamp,
		eProgram,
		eCount
	};

protected:

	struct STreeNode: STraceNode
	{
		STreeNode(quint64 Id) : STraceNode(Id) { }
		//virtual ~STreeNode(){}

		CProgramFilePtr		pProgram;
	};

	QString					m_RootPath;

	EEventStatus			m_Action = EEventStatus::eUndefined;

	virtual bool			FilterNode(const SMergedLog::TLogEntry& Data) const;

	virtual STraceNode*		MkNode(quint64 Id);
	virtual STraceNode*		MkNode(const SMergedLog::TLogEntry& Data);
	virtual void			FreeNode(STraceNode* pNode);

	virtual QVariant		NodeData(STraceNode* pNode, int role, int section) const;

	static PoolAllocator<sizeof(STreeNode)> m_NodeAllocator;
};
