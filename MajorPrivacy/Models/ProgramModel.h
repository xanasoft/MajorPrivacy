#pragma once
#include <qwidget.h>
#include "../Core/Programs/ProgramGroup.h"
#include "../../MiscHelpers/Common/TreeItemModel.h"

class CProgramModel : public CTreeItemModel
{
    Q_OBJECT

public:
    CProgramModel(QObject *parent = 0);
	~CProgramModel();
	
	void					SetTree(bool bTree)				{ m_bTree = bTree; }
	bool					IsTree() const					{ return m_bTree; }

	QList<QVariant>			Sync(const CProgramSetPtr& pRoot);

	CProgramItemPtr			GetItem(const QModelIndex &index) const;

    int						columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant				headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	QString					GetColumHeader(int column) const;

	enum EColumns
	{
		eName = 0,
		eType,

		eRunning, // current running process count
		eProgramRules,
		//eTotalUpTime,

		//eOpenFiles,
		//eFsTotalRead,
		//eFsTotalWritten,
		eAccessRules,

		eSockets,
		eFwRules,
		eLastActivity,
		eUpload,
		eDownload,
		eUploaded,
		eDownloaded,

		ePath,

		//eInfo,

		eCount
	};

protected:
	struct SProgramNode: STreeNode
	{
		SProgramNode(CTreeItemModel* pModel, const QVariant& Id) : STreeNode(pModel, Id), iColor(0) { }

		CProgramItemPtr		pItem;

		int					iColor;

		QSet<int>			Bold;
	};

	void					Sync(const CProgramSetPtr& pRoot, const QString& RootID, const QList<QVariant>& Path, QMap<QList<QVariant>, QList<STreeNode*> >& New, QHash<QVariant, STreeNode*>& Old, QList<QVariant>& Added);

	virtual QVariant		NodeData(STreeNode* pNode, int role, int section) const;

	virtual STreeNode*		MkNode(const QVariant& Id) { return new SProgramNode(this, Id); }

	bool					m_bTree;
};