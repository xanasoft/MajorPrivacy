#include "pch.h"
#include "LibraryModel.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Core/PrivacyCore.h"
#include "../Core/HashDB/HashDB.h"
#include "../Library/Helpers/NtUtil.h"
#include "../MiscHelpers/Common/OtherFunctions.h"
#include "../Windows/AccessRuleWnd.h"

CLibraryModel::CLibraryModel(QObject* parent)
	:CTreeItemModel(parent)
{
	m_Root = MkNode(QVariant());

	m_bUseIcons = true;
}

CLibraryModel::~CLibraryModel()
{
	FreeNode(m_Root);
	m_Root = NULL;
}

QList<QModelIndex> CLibraryModel::Sync(const QMap<SLibraryKey, SLibraryItemPtr>& List)
{
#pragma warning(push)
#pragma warning(disable : 4996)
	TNewNodesMap New;
#pragma warning(pop)
	QHash<QVariant, STreeNode*> Old = m_Map;

	for(auto X = List.begin(); X != List.end(); ++X)
	{
		QVariant ID = X.value()->ID;

		QModelIndex Index;

		QHash<QVariant, STreeNode*>::iterator I = Old.find(ID);
		SLibraryNode* pNode = I != Old.end() ? static_cast<SLibraryNode*>(I.value()) : NULL;
		if (!pNode)
		{
			pNode = static_cast<SLibraryNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pItem = X.value();
			QList<QVariant> Path;
			if(pNode->pItem->Parent.isValid())
				Path.append(pNode->pItem->Parent);
			pNode->Path = Path;
			New[pNode->Path.count()][pNode->Path].append(pNode);
		}
		else
		{
			I.value() = NULL;
			Index = Find(m_Root, pNode);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

		CImageSignInfo SignInfo = pNode->pItem->pLibrary ? pNode->pItem->Info.SignInfo : pNode->pItem->pProg->GetSignInfo();

		bool IsSignedFile = !!theCore->HashDB()->GetHash(SignInfo.GetFileHash(), false);
		bool IsSignedCert = !!theCore->HashDB()->GetHash(SignInfo.GetSignerHash(), false) || !!theCore->HashDB()->GetHash(SignInfo.GetIssuerHash(), false);
		//if (pNode->pItem->pLibrary) {
			//IsSignedFile = theCore->HasFileSignature(pNode->pItem->pLibrary->GetPath(), SignInfo.GetFileHash());
			//IsSignedCert = theCore->HasCertSignature(SignInfo.GetSignerName(), SignInfo.GetSignerHash());	
		//} else {
			//IsSignedFile = theCore->HasFileSignature(pNode->pItem->pProg->GetPath(), pNode->pItem->pProg->GetSignInfo().GetFileHash());
			//IsSignedCert = theCore->HasCertSignature(pNode->pItem->pProg->GetSignInfo().GetSignerName(), pNode->pItem->pProg->GetSignInfo().GetSignerHash());
		//}

		int Col = 0;
		bool State = false;
		int Changed = 0;
		if (pNode->Icon.isNull() || IsSignedFile != pNode->IsSignedFile || IsSignedCert != pNode->IsSignedCert)
		{
			QIcon Icon;
			if (!pNode->pItem->pProg)
				Icon = CProgramLibrary::DefaultIcon();
			else if (!pNode->pItem->pProg->GetIcon().isNull())
				Icon = pNode->pItem->pProg->GetIcon();
			if (!Icon.isNull()) {
				if(IsSignedFile)
					Icon = IconAddOverlay(Icon, ":/Icons/Cert.png");
				else if(IsSignedCert)
					Icon = IconAddOverlay(Icon, ":/Icons/SignFile.png");
				pNode->Icon = Icon;
				Changed = 1;
			}
			pNode->IsSignedFile = IsSignedFile;
			pNode->IsSignedCert = IsSignedCert;
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
			case eName:				Value = pNode->pItem->pLibrary ? pNode->pItem->pLibrary->GetPath() : pNode->pItem->pProg->GetPath(); break;
			case eTrustLevel:		Value = SignInfo.GetUnion(); break;
			case eStatus:			Value = pNode->pItem->pLibrary ? (uint32)pNode->pItem->Info.LastStatus : -1; break;
			case eLastLoadTime:		Value = pNode->pItem->pLibrary ? pNode->pItem->Info.LastLoadTime : 0; break;
			case eNumber:			Value = pNode->pItem->pLibrary ? pNode->pItem->Info.TotalLoadCount : pNode->pItem->Count; break;
			case eHash:				Value = pNode->pItem->pLibrary ? pNode->pItem->Info.SignInfo.GetFileHash() : pNode->pItem->pProg->GetSignInfo().GetFileHash(); break;
			//case eSigner:			Value = pNode->pItem->pLibrary ? QByteArray((char*)pNode->pItem->Info.SignerHash.data(), pNode->pItem->Info.SignerHash.size()) : pNode->pItem->pProg->GetSignerHash(); break;
			case eSigner:			Value = pNode->pItem->pLibrary ? pNode->pItem->Info.SignInfo.GetSignerName() : pNode->pItem->pProg->GetSignInfo().GetSignerName(); break;
			case eModule:			Value = pNode->pItem->pLibrary ? pNode->pItem->pLibrary->GetPath() : pNode->pItem->pProg->GetPath(); break;
			}

			SLibraryNode::SValue& ColValue = pNode->Values[section];

			if (ColValue.Raw != Value)
			{
				if (Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
				case eName:				ColValue.Formatted = pNode->pItem->pLibrary ? pNode->pItem->pLibrary->GetName() : pNode->pItem->pProg->GetNameEx(); break;
				case eTrustLevel:		ColValue.Formatted = CProgramFile::GetSignatureInfoStr(SignInfo); 
					if (SignInfo.IsComplete())
					{
						auto Signs = SignInfo.GetSignatures();
						if (Signs.Developer || Signs.UserSign ||Signs.UserDB)
							ColValue.Color = QColor(144, 238, 144); // light green
						else if (Signs.Windows || Signs.Antimalware || Signs.Microsoft)
							ColValue.Color = QColor(173, 216, 230); // light blue
						else if (Signs.Authenticode || Signs.Store)
							ColValue.Color = QColor(255, 255, 224); // light yellow
						else
							ColValue.Color = QColor(255, 182, 193); // light red
					}
					break;
				case eStatus:			ColValue.Formatted = Value.toUInt() != -1 ? CAccessRuleWnd::GetStatusStr((EEventStatus)Value.toUInt()) : ""; 
										ColValue.Color = Value.toUInt() != -1 ? CAccessRuleWnd::GetStatusColor((EEventStatus)Value.toUInt()) : QVariant();
										break;
				case eLastLoadTime:		ColValue.Formatted = Value.toULongLong() ? QDateTime::fromMSecsSinceEpoch(FILETIME2ms(Value.toULongLong())).toString("dd.MM.yyyy hh:mm:ss") : ""; break;
				case eHash:				ColValue.Formatted = Value.toByteArray().toHex().toUpper(); break;
				//case eSigner:			ColValue.Formatted = Value.toByteArray().toHex().toUpper(); break;
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

CLibraryModel::STreeNode* CLibraryModel::MkVirtualNode(const QVariant& Id, STreeNode* pParent)
{ 
	STreeNode* pNode = CTreeItemModel::MkVirtualNode(Id, pParent);

	if (!pNode->Values[0].Raw.isValid()) {
		QStringList Paths = Id.toString().split("\\");
		pNode->Values[0].Raw = Paths.last();
	}

	return pNode;
}

QVariant CLibraryModel::NodeData(STreeNode* pNode, int role, int section) const
{
	SLibraryNode* pLibNode = static_cast<SLibraryNode*>(pNode);
	switch(role)
	{
	case Qt::FontRole:
		if (section == eTrustLevel)
		{
			if (pLibNode->IsSignedFile || pLibNode->IsSignedCert)
			{
				QFont Font;
				Font.setBold(true);
				return Font;
			}
		}
		break;
	case Qt::ForegroundRole:
		if (section == eSigner)
		{
			bool NotOK = !(pLibNode->pItem->pLibrary ? pLibNode->pItem->Info.SignInfo.IsComplete() : pLibNode->pItem->pProg->GetSignInfo().IsComplete());
			return NotOK ? QColor(192, 192, 192) : QVariant();
		}
		break;
	case Qt::ToolTipRole:
		if (section == eSigner)
		{
			QByteArray Hash = pLibNode->pItem->pLibrary ? pLibNode->pItem->Info.SignInfo.GetSignerHash() : pLibNode->pItem->pProg->GetSignInfo().GetSignerHash();
			return Hash.toHex().toUpper();
		}
		break;
	}

	return CTreeItemModel::NodeData(pNode, role, section);
}

SLibraryItemPtr CLibraryModel::GetItem(const QModelIndex& index)
{
	if (!index.isValid())
        return SLibraryItemPtr();

	SLibraryNode* pNode = static_cast<SLibraryNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pItem;
}

int CLibraryModel::columnCount(const QModelIndex& parent) const
{
	return eCount;
}

QVariant CLibraryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch (section)
		{
		case eName:					return tr("Name");
		case eTrustLevel:			return tr("Trust Level");
		case eStatus:				return tr("Last Status");
		case eLastLoadTime:			return tr("Last Load");
		case eNumber:				return tr("Count");
		case eHash:					return tr("File Hash");
		case eSigner:				return tr("Signing Certificate");
		case eModule:				return tr("Module");
		}
	}
	return QVariant();
}