#pragma once
#include <qwidget.h>
#include "../Core/Processes/Process.h"
#include "../../MiscHelpers/Common/TreeItemModel.h"

class CProcessModel : public CTreeItemModel
{
    Q_OBJECT

public:
    CProcessModel(QObject *parent = 0);
	~CProcessModel();

	typedef CProcessPtr ItemType;

	void			SetTree(bool bTree)				{ m_bTree = bTree; }
	bool			IsTree() const					{ return m_bTree; }

	QSet<quint64>	Sync(QHash<quint64, CProcessPtr> ProcessList);

	CProcessPtr		GetItem(const QModelIndex &index) const;

    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	QString			GetColumHeader(int column) const;

	enum EColumns
	{
		eProcess = 0,
		ePID,
		eParentPID,
		eEnclave,
		eTrustLevel,
		eSID,
		eStatus,
		eImageStats,
		eHandles,
		eSockets,
		eFileName,
		//eFileHash,

		eCount
	};

protected:
	struct SProcessNode: STreeNode
	{
		SProcessNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id), iColor(0) { }

		CProcessPtr			pProcess;

		int					iColor;

		QSet<int>			Bold;
	};

	virtual QVariant		NodeData(STreeNode* pNode, int role, int section) const;

	virtual STreeNode*		MkNode(const QVariant& Id) { return new SProcessNode(/*this,*/ Id); }
		
	void					MakeProcPath(const CProcessPtr& pProcess, const QHash<quint64, CProcessPtr>& ProcessList, QList<QVariant>& Path);
	bool					TestProcPath(const QList<QVariant>& Path, const CProcessPtr& pProcess, const QHash<quint64, CProcessPtr>& ProcessList, int Index = 0);
	
	bool					m_bTree;
};