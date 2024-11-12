#pragma once
#include <qwidget.h>
#include "../MiscHelpers/Common/TreeItemModel.h"
#include "../Library/Common/PoolAllocator.h"
#include "../Core/Processes/ProcessList.h"
#include "../Core/Programs/ProgramFile.h"
#include "TraceModel.h"
#include "../Service/Network/Firewall/FirewallDefs.h"

class CNetTraceModel : public CTraceModel
{
	Q_OBJECT

public:
	typedef CLogEntryPtr ItemType;

	CNetTraceModel(QObject* parent = 0);
	~CNetTraceModel();

	void 					SetFilter(EFwDirections Dir, EEventStatus Action, ENetProtocols Protocol) { m_Dir = Dir; m_Action = Action; m_Protocol = Protocol; }

	virtual int				columnCount(const QModelIndex &parent = QModelIndex()) const;
	virtual QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

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

	struct STreeNode: STraceNode
	{
		STreeNode(quint64 Id) : STraceNode(Id) { }
		//virtual ~STreeNode(){}

		CProgramFilePtr		pProgram;
	};

	EFwDirections			m_Dir = EFwDirections::Bidirectional;
	EEventStatus			m_Action = EEventStatus::eUndefined;
	ENetProtocols           m_Protocol = ENetProtocols::eAny;

	virtual bool			FilterNode(const SMergedLog::TLogEntry& Data) const;

	virtual STraceNode*		MkNode(quint64 Id);
	virtual STraceNode*		MkNode(const SMergedLog::TLogEntry& Data);
	virtual void			FreeNode(STraceNode* pNode);

	virtual QVariant		NodeData(STraceNode* pNode, int role, int section) const;

	static PoolAllocator<sizeof(STreeNode)> m_NodeAllocator;
};
