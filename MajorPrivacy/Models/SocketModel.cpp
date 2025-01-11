#include "pch.h"
#include "SocketModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Core/Network/NetworkManager.h"

CSocketModel::CSocketModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CSocketModel::~CSocketModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex>	CSocketModel::Sync(const QMap<quint64, CSocketPtr>& SocketList)
{
#pragma warning(push)
#pragma warning(disable : 4996)
	QMap<QList<QVariant>, QList<STreeNode*> > New;
#pragma warning(pop)
	QHash<QVariant, STreeNode*> Old = m_Map;

	foreach(const CSocketPtr& pSocket, SocketList)
	{
		QVariant ID = pSocket->GetSocketRef();

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SSocketNode* pNode = I != Old.end() ? static_cast<SSocketNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SSocketNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			//pNode->Path = Path;
			pNode->pSocket = pSocket;
			pNode->pProcess = theCore->ProcessList()->GetProcess(pSocket->GetProcessId(), true);
			New[pNode->Path].append(pNode);
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
		if (pNode->Icon.isNull() && pNode->pProcess && !pNode->pProcess->GetIcon().isNull())
		{
			pNode->Icon = pNode->pProcess->GetIcon();
			Changed = 1;
		}
		/*if (pNode->IsGray != ...) {
			pNode->IsGray = ...;
			Changed = 2; // set change for all columns
		}*/

		for (int section = 0; section < columnCount(); section++)
		{
			if (!IsColumnEnabled(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch (section)
			{
			case eName:				Value = (pNode->pProcess ? pNode->pProcess->GetName() : tr("Unknown Process"))
				+ (!pSocket->GetOwnerService().isEmpty() ? tr(" (%1)").arg(pSocket->GetOwnerService()) : "");
									break;
			case eProtocol:			Value = pSocket->GetProtocolType(); break;
			case eRAddress:		{
									QString Data = pSocket->GetRemoteAddress().toString();
									QString Host = pSocket->GetRemoteHostName();
									if(!Host.isEmpty()) Data += " (" + Host + ")";
									Value = Data;
									break;
								}
			case eRPort:			Value = pSocket->GetRemotePort(); break;
			case eLAddress:			Value = pSocket->GetLocalAddress().toString(); break;
			case eLPort:			Value = pSocket->GetLocalPort(); break;
			//case eAccess:			Value = Socket[SVC_API_SOCK_ACCESS].AsQStr(); break;
			case eState:			Value = pSocket->GetState(); break;
			case eLastActive:		Value = pSocket->GetLastActivity(); break;
			case eUpload:			Value = pSocket->GetUploadRate(); break;
			case eDownload:			Value = pSocket->GetDownloadRate(); break;
			case eUploaded:			Value = pSocket->GetUploadTotal(); break;
			case eDownloaded:		Value = pSocket->GetDownloadTotal(); break;
			case eTimeStamp:		Value = pSocket->GetCreateTimeStamp(); break;
			case eProgram:			Value = pNode->pProcess ? pNode->pProcess->GetNtPath() : tr("PROCESS MISSING"); break;
			}

			SSocketNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eProtocol:		ColValue.Formatted = CFwRule::ProtocolToStr((EFwKnownProtocols)pSocket->GetProtocolType()); break;
				case eState:		ColValue.Formatted = pSocket->GetStateString(); break;
				case eLastActive:	ColValue.Formatted = Value.toULongLong() ? QDateTime::fromMSecsSinceEpoch(Value.toULongLong()).toString("dd.MM.yyyy hh:mm:ss") : ""; break;
				case eUpload:		ColValue.Formatted = FormatSize(Value.toULongLong()); break;
				case eDownload:		ColValue.Formatted = FormatSize(Value.toULongLong()); break;
				case eUploaded:		ColValue.Formatted = FormatSize(Value.toULongLong()); break;
				case eDownloaded:	ColValue.Formatted = FormatSize(Value.toULongLong()); break;
				case eTimeStamp:	ColValue.Formatted = QDateTime::fromMSecsSinceEpoch(Value.toULongLong()).toString("dd.MM.yyyy hh:mm:ss"); break;
				case eProgram:		ColValue.Formatted = pNode->pProcess ? theCore->NormalizePath(pNode->pProcess->GetNtPath()) : tr("PROCESS MISSING"); break;
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

	QList<QModelIndex>	NewBranches;
	CTreeItemModel::Sync(New, Old, &NewBranches);
	return NewBranches;
}

CSocketModel::STreeNode* CSocketModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

CSocketPtr CSocketModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
        return CSocketPtr();

	SSocketNode* pNode = static_cast<SSocketNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pSocket;
}

int CSocketModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CSocketModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eProtocol:				return tr("Protocol");
		case eRAddress:				return tr("Remote Address");
		case eRPort:				return tr("Remote Port");
		case eLAddress:				return tr("Local Address");
		case eLPort:				return tr("Local Port");
		case eAccess:				return tr("Access");
		case eState:				return tr("State");
		case eLastActive:			return tr("Last Activity");
		case eUpload:				return tr("Upload");
		case eDownload:				return tr("Download");
		case eUploaded:				return tr("Uploaded");
		case eDownloaded:			return tr("Downloaded");
		case eTimeStamp:			return tr("Time Stamp");
		case eProgram:				return tr("Program");
		}
	}
	return QVariant();
}