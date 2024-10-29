#pragma once
#include <qwidget.h>
#include "../MiscHelpers/Common/TreeItemModel.h"
#include "../Library/Common/PoolAllocator.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramFile.h"
#include "../Core/Programs/ProgramLibrary.h"
#include "TraceModel.h"

class CProcessTraceModel : public CTraceModel
{
	Q_OBJECT

public:
	typedef CLogEntryPtr ItemType;

	CProcessTraceModel(QObject* parent = 0);
	~CProcessTraceModel();

	void					SetFilter(EExecLogRole Role, EEventStatus Action) { m_Role = Role; m_Action = Action; }

	virtual int				columnCount(const QModelIndex &parent = QModelIndex()) const;
	virtual QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eRole,
		eType,
		eStatus,
		eTarget,
		eTimeStamp,
		eProgram,
		eCount
	};

protected:

	struct STreeNode: STraceNode
	{
		STreeNode(quint64 Id) : STraceNode(Id) {}
		//virtual ~STreeNode(){}

		CProgramFilePtr		pActor;
		CProgramFilePtr		pTarget;
		CProgramLibraryPtr	pLibrary;
	};

	virtual QVariant		NodeData(STraceNode* pNode, int role, int section) const;

	EExecLogRole			m_Role = EExecLogRole::eUndefined;
	EEventStatus			m_Action = EEventStatus::eUndefined;

	virtual bool			FilterNode(const SMergedLog::TLogEntry& Data) const;

	virtual STraceNode*		MkNode(quint64 Id);
	virtual STraceNode*		MkNode(const SMergedLog::TLogEntry& Data);
	virtual void			FreeNode(STraceNode* pNode);

	static PoolAllocator<sizeof(STreeNode)> m_NodeAllocator;
};
