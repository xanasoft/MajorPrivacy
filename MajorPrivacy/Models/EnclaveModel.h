#pragma once
#include <qwidget.h>
#include "../Core/Processes/Enclave.h"
#include "../../MiscHelpers/Common/TreeItemModel.h"

class CEnclaveModel : public CTreeItemModel
{
	Q_OBJECT

public:
	CEnclaveModel(QObject *parent = 0);
	~CEnclaveModel();

	typedef CEnclavePtr ItemType;

	QList<QVariant>	Sync(const QMap<quint64, CEnclavePtr>& EnclaveList);

	void			SetTree(bool bTree)				{ m_bTree = bTree; }
	bool			IsTree() const					{ return m_bTree; }

	CEnclavePtr		GetItem(const QModelIndex& index) const {return GetEnclave(index);}

	CEnclavePtr		GetEnclave(const QModelIndex &index) const;
	CProcessPtr		GetProcess(const QModelIndex &index) const;
	QVariant		GetID(const QModelIndex &index) const;

	enum ETypes
	{
		eNone = 0,
		eEnclave,
		eProcess
	}				GetType(const QModelIndex &index) const;

	int				columnCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eID,
		eSignAuthority,
		eOnSpawn,
		eProgram,
		eCount
	};

protected:
	bool			Sync(const CEnclavePtr& pEnclave, const QList<QVariant>& Path, const QMap<quint64, CProcessPtr>& ProcessList, QMap<QList<QVariant>, QList<STreeNode*> >& New, QHash<QVariant, STreeNode*>& Old, QList<QVariant>& Added);

	struct SEnclaveNode: STreeNode
	{
		SEnclaveNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id) { }

		CEnclavePtr	pEnclave;

		CProcessPtr pProcess;
	};

	virtual QVariant		NodeData(STreeNode* pNode, int role, int section) const;

	virtual STreeNode*		MkNode(const QVariant& Id) { return new SEnclaveNode(/*this,*/ Id); }

	QList<QVariant>			MakeProcPath(const quint64 EnclaveId, const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList);
	void					MakeProcPath(const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList, QList<QVariant>& Path);
	bool					TestProcPath(const QList<QVariant>& Path, const quint64 EnclaveId, const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList, int Index = 0);

	//virtual QVariant		GetDefaultIcon() const;

private:

	bool								m_bTree;
};