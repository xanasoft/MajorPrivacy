#include "pch.h"
#include "EnclaveModel.h"
#include "../Core/PrivacyCore.h"
#include "../../MiscHelpers/Common/Common.h"
#include "../core/Volumes/VolumeManager.h"


CEnclaveModel::CEnclaveModel(QObject *parent)
	: CTreeItemModel(parent)
{
	m_bTree = true;

	m_bUseIcons = true;

	m_Root = MkNode(QVariant());
}

CEnclaveModel::~CEnclaveModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QVariant> CEnclaveModel::MakeProcPath(const QVariant& EnclaveId, const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList)
{
	QList<QVariant> Path;
	MakeProcPath(pProcess, ProcessList, Path);
	Path.prepend(EnclaveId);
	return Path;
}

void CEnclaveModel::MakeProcPath(const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList, QList<QVariant>& Path)
{
	quint32 ParentID = pProcess->GetParentId();
	CProcessPtr pParent = ProcessList.value(ParentID);

	if (!pParent.isNull() && ParentID != pProcess->GetProcessId() && !Path.contains(ParentID))
	{
		Path.prepend(ParentID);
		MakeProcPath(pParent, ProcessList, Path);
	}
}

bool CEnclaveModel::TestProcPath(const QList<QVariant>& Path, const QVariant& EnclaveId, const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList, int Index)
{
	if (Index == 0)
	{
		if (Path.isEmpty() || EnclaveId != Path[0])
			return false;

		return TestProcPath(Path, EnclaveId, pProcess, ProcessList, 1);
	}

	quint32 ParentID = pProcess->GetParentId();
	CProcessPtr pParent = ProcessList.value(ParentID);

	if (!pParent.isNull() && ParentID != pProcess->GetProcessId())
	{
		if(Index >= Path.size() || Path[Path.size() - Index] != ParentID)
			return false;

		return TestProcPath(Path, EnclaveId, pParent, ProcessList, Index + 1);
	}

	return Path.size() == Index;
}

QList<QVariant> CEnclaveModel::Sync(const QMap<QFlexGuid, CEnclavePtr>& EnclaveManager)
{
	QList<QVariant> Added;
#pragma warning(push)
#pragma warning(disable : 4996)
	TNewNodesMap New;
#pragma warning(pop)
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach (const CEnclavePtr& pEnclave, EnclaveManager)
	{
		QVariant ID = pEnclave->GetGuid().ToQV();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SEnclaveNode* pNode = I != Old.end() ? static_cast<SEnclaveNode*>(I.value()) : NULL;
		if(!pNode)
		{
			pNode = static_cast<SEnclaveNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pEnclave = pEnclave;
			New[pNode->Path.count()][pNode->Path].append(pNode);
			Added.append(ID);
		}
		else
		{
			I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		int Col = 0;
		bool State = false;
		int Changed = 0;
		if (pNode->pProcess) {
			pNode->Icon = pNode->pProcess->GetIcon();
			Changed = 1;
		} else if (pNode->Icon.isNull() || pNode->IconFile != pNode->pEnclave->GetIconFile()) {
			pNode->IconFile = pNode->pEnclave->GetIconFile();
			pNode->Icon = pNode->pEnclave->GetIcon();
			Changed = 1;
		}

		QMap<quint64, CProcessPtr> ProcessList = pEnclave->GetProcesses();

		Sync(pEnclave, pNode->Path, ProcessList, New, Old, Added);

		for(int section = 0; section < columnCount(); section++)
		{
			if (!IsColumnEnabled(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch(section)
			{
			case eName:				Value = pEnclave->GetName(); break;
			case eID:				Value = pEnclave->GetVolumeGuid().ToQV(); break;
			case eSigners:			Value = CEnclave::GetAllowedSigners(pEnclave->GetAllowedSignatures(), pEnclave->GetAllowedCollections()); break;
			case eOnSpawn:			Value = ((uint32)pEnclave->GetOnTrustedSpawn()) << 16 | ((uint32)pEnclave->GetOnSpawn()); break;
			}

			SEnclaveNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eName:				ColValue.ToolTip = pEnclave->GetGuid().ToQS(); ColValue.Formatted = pEnclave->GetName(); break;
				case eID:				{ auto pVolume = theCore->VolumeManager()->GetVolume(pEnclave->GetVolumeGuid()); ColValue.Formatted = pVolume ? pVolume->GetName() : pEnclave->GetVolumeGuid().ToQS(); break; }
				case eSigners:			ColValue.Formatted = Value.toStringList().join(tr(", ")); break;
				case eOnSpawn:			ColValue.Formatted = CEnclave::GetOnSpawnStr((EProgramOnSpawn)(Value.toUInt() >> 16)) + "/" + CEnclave::GetOnSpawnStr((EProgramOnSpawn)(Value.toUInt() & 0xFFFF)); break;
				}
			}

			if(State != (Changed != 0))
			{
				if(State && Index.isValid())
					emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), section-1, pNode));
				State = (Changed != 0);
				Col = section;
			}
			if(Changed == 1)
				Changed = 0;
		}
		if(State && Index.isValid())
			emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), columnCount()-1, pNode));
	}

	CTreeItemModel::Sync(New, Old);
	return Added;
}

bool CEnclaveModel::Sync(const CEnclavePtr& pEnclave, const QList<QVariant>& Path, const QMap<quint64, CProcessPtr>& ProcessList, TNewNodesMap& New, QHash<QVariant, STreeNode*>& Old, QList<QVariant>& Added)
{
	QVariant EnclaveID = pEnclave->GetGuid().ToQV();

	int ActiveCount = 0;

	foreach(const CProcessPtr& pProc, ProcessList)
	{
		QSharedPointer<CProcess> pProcess = pProc.objectCast<CProcess>();
		QVariant ID = pProcess->GetProcessId();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SEnclaveNode* pNode = I != Old.end() ? static_cast<SEnclaveNode*>(I.value()) : NULL;
		if (!pNode || (m_bTree ? !TestProcPath(pNode->Path.mid(Path.length()), EnclaveID, pProcess, ProcessList) : !pNode->Path.isEmpty())) // todo: improve that
		{
			pNode = static_cast<SEnclaveNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			if (m_bTree)
				pNode->Path = Path + MakeProcPath(EnclaveID, pProcess, ProcessList);
			pNode->pEnclave = pEnclave;
			pNode->pProcess = pProcess;
			New[pNode->Path.count()][pNode->Path].append(pNode);
			Added.append(ID);
		}
		else
		{
			I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

		int Col = 0;
		bool State = false;
		int Changed = 0;

		bool bIsTerminated = false; //pProcess->IsTerminated(); // todo
		if (pNode->IsGray != bIsTerminated)
		{
			pNode->IsGray = bIsTerminated;
			Changed = 2; // update all columns for this item
		}

		if (!bIsTerminated)
			ActiveCount++;

		if (pNode->Icon.isNull())
		{
			pNode->Icon = pProcess->GetIcon();
			Changed = 1;
		}

		for (int section = 0; section < columnCount(); section++)
		{
			if (!IsColumnEnabled(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch (section)
			{
			case eName:					Value = pProcess->GetName(); break;
			case eID:					Value = pProcess->GetProcessId(); break;
			case eSigners:				Value = pProcess->GetSignInfo().GetUnion(); break;
			case eProgram:				Value = pProcess->GetNtPath(); break;
			}

			SEnclaveNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eSigners:			ColValue.Formatted = CProgramFile::GetSignatureInfoStr(pProcess->GetSignInfo()); break;
				case eProgram:			ColValue.Formatted = theCore->NormalizePath(pProcess->GetNtPath()); break;
				}
			}

			if (State != (Changed != 0))
			{
				if (State && Index.isValid())
					emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), section - 1, pNode));
				State = (Changed != 0);
				Col = section;
			}
			if (Changed == 1)
				Changed = 0;
		}
		if (State && Index.isValid())
			emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), columnCount() - 1, pNode));
	}

	return ActiveCount != 0;
}

QVariant CEnclaveModel::NodeData(STreeNode* pNode, int role, int section) const
{
	/*if (section == 0 && role == Qt::InitialSortOrderRole) {
		return ((SEnclaveNode*)pNode)->OrderNumber;
	}*/
	return CTreeItemModel::NodeData(pNode, role, section);
}

CEnclaveModel::SModelItem CEnclaveModel::GetItem(const QModelIndex &index) const
{
	if (!index.isValid())
		return SModelItem();

	SEnclaveNode* pNode = static_cast<SEnclaveNode*>(index.internalPointer());
	ASSERT(pNode);

	return SModelItem { pNode->pEnclave, pNode->pProcess };
}

QVariant CEnclaveModel::GetID(const QModelIndex &index) const
{
	if (!index.isValid())
		return QVariant();

	SEnclaveNode* pNode = static_cast<SEnclaveNode*>(index.internalPointer());
	ASSERT(pNode);

	if (!pNode->pProcess && !pNode->pEnclave)
		return pNode->ID.toString();

	return pNode->ID;
}

CEnclaveModel::ETypes CEnclaveModel::GetType(const QModelIndex &index) const
{
	if (!index.isValid())
		return eNone;

	SEnclaveNode* pNode = static_cast<SEnclaveNode*>(index.internalPointer());
	ASSERT(pNode);

	if (pNode->pProcess)
		return eProcess;
	if (pNode->pEnclave)
		return eEnclave;
	return eNone;
}

int CEnclaveModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CEnclaveModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
		case eName:				return tr("Name");
		case eID:				return tr("ID");
		case eSigners:			return tr("Signers");
		case eOnSpawn:			return tr("Trusted Spawn/UnTrusted");
		case eProgram:			return tr("Program");
		}
	}
	return QVariant();
}
