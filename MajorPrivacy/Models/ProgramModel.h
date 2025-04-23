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

	enum EFilters {
		eAll = 0x0000,

		ePrograms = 0x0001,
		eApps = 0x0002,
		eSystem = 0x0004,
		eGroups = 0x0008,
		eTypeFilter = ePrograms | eApps | eSystem | eGroups,

		eRunning = 0x0010,
		eRanRecently = 0x1000,

		eWithProgRules = 0x0020,
		eWithResRules = 0x0040,
		eWithFwRules = 0x0080,
		eRulesFilter = eWithProgRules | eWithResRules | eWithFwRules,

		eRecentTraffic = 0x0100,
		eAllowedTraffic = 0x0200,
		eBlockedTraffic = 0x0400,
		eTrafficFilter = eRecentTraffic | eAllowedTraffic | eBlockedTraffic,

		eWithSockets = 0x0800,
	};

	void					SetFilter(int Filter)			{ m_Filter = Filter; }
	int 					GetFilter() const				{ return m_Filter; }

	void					SetRecentLimit(int Limit)		{ m_RecentLimit = Limit; }
	quint64					GetRecentLimit() const			{ return m_RecentLimit; }

	bool					TestFilter(EProgramType Type, const SProgramStats* pStats);

	QList<QVariant>			Sync(const CProgramSetPtr& pRoot);

	CProgramItemPtr			GetItem(const QModelIndex &index) const;

    int						columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant				headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	QString					GetColumHeader(int column) const;

	enum EColumns
	{
		eName = 0,
		eType,

		eRunningCount, // current running process count
		eProgramRules,
		eLastExecution,
		//eTotalUpTime,

		eOpenedFiles,
		//eOpenFiles,
		//eFsTotalRead,
		//eFsTotalWritten,
		eAccessRules,

		eSockets,
		eFwRules,
		eLastNetActivity,
		eUpload,
		eDownload,
		eUploaded,
		eDownloaded,

		eLogSize,

		ePath,

		//eInfo,

		eCount
	};

protected:
	struct SProgramNode: STreeNode
	{
		SProgramNode(/*CTreeItemModel* pModel,*/ const QVariant& Id) : STreeNode(/*pModel,*/ Id), iColor(0) { }

		CProgramItemPtr		pItem;

		QString				IconFile;

		int					iColor;

		QSet<int>			Bold;
	};

	void					Sync(const CProgramSetPtr& pRoot, const QString& RootID, const QList<QVariant>& Path, QMap<QList<QVariant>, QList<STreeNode*> >& New, QSet<QVariant>& Current, QHash<QVariant, STreeNode*>& Old, QList<QVariant>& Added);

	virtual QVariant		NodeData(STreeNode* pNode, int role, int section) const;

	virtual STreeNode*		MkNode(const QVariant& Id) { return new SProgramNode(/*this,*/ Id); }

	bool					m_bTree;

	int						m_Filter;
	quint64					m_RecentLimit;
};