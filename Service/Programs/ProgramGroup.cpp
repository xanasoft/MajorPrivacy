#include "pch.h"
#include "ProgramGroup.h"
#include "ServiceAPI.h"
#include "../Processes/ProcessList.h"
#include "../ServiceCore.h"

///////////////////////////////////////////////////////////////////////////////////////
// CProgramSet

bool CProgramSet::ContainsNode(const CProgramItemPtr& Item) const 
{ 
	std::shared_lock lock(m_Mutex);
	for (auto I = m_Nodes.begin(); I != m_Nodes.end(); I++) {
		if (*I == Item)
			return true;
	}
	return false;
}

void CProgramSet::WriteVariant(CVariant& Data) const
{
	CProgramItem::WriteVariant(Data);

	CVariant Items;
	Items.BeginList();
	for (auto pItem : m_Nodes)
		//Items.Write(pItem->ToVariant());
		Items.Write(pItem->GetUID());
	Items.Finish();
	Data.WriteVariant(SVC_API_PROG_ITEMS, Items);
}


///////////////////////////////////////////////////////////////////////////////////////
// CAllProgram

void CAllProgram::WriteVariant(CVariant& Data) const
{
	Data.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_All);

	CProgramSet::WriteVariant(Data);

	// todo:
	/*Item[SVC_API_SOCK_LAST_ACT] = LastActivity;

	Item[SVC_API_SOCK_UPLOAD] = Upload;
	Item[SVC_API_SOCK_DOWNLOAD] = Download;
	Item[SVC_API_SOCK_UPLOADED] = Uploaded;
	Item[SVC_API_SOCK_DOWNLOADED] = Downloaded;*/
}

///////////////////////////////////////////////////////////////////////////////////////
// CProgramGroup

void CProgramGroup::WriteVariant(CVariant& Data) const
{
	Data.Write(SVC_API_ID_TYPE, SVC_API_ID_TYPE_GROUP);

	CProgramList::WriteVariant(Data);
}