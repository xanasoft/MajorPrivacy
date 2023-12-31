#include "pch.h"
#include "ProgramItem.h"
#include "../Processes/ProcessList.h"
#include "../ServiceCore.h"
#include "ServiceAPI.h"
#include "../Network/NetIsolator.h"
#include "../Network/SocketList.h"
#include "../Network/Firewall/FirewallRule.h"


CProgramItem::CProgramItem()
{
	static volatile LONG64 VolatileIdCounter = 0;
	
	m_UID = InterlockedIncrement64(&VolatileIdCounter);
}

CVariant CProgramItem::ToVariant() const
{
	std::unique_lock lock(m_Mutex);

	CVariant Data;
	Data.BeginMap();
	
	WriteVariant(Data);

	Data.Finish();
	return Data;
}

void CProgramItem::WriteVariant(CVariant& Data) const
{
	Data.Write(SVC_API_PROG_UID, m_UID);
	Data.WriteVariant(SVC_API_PROG_ID, m_ID.ToVariant());
	Data.Write(SVC_API_PROG_NAME, m_Name);

	Data.Write(SVC_API_PROG_ICON, m_Icon);
	Data.Write(SVC_API_PROG_INFO, m_Info);

	CVariant FwRules;
	FwRules.BeginList();
	for (auto FwRule : m_FwRules)
		FwRules.Write(FwRule->GetGuid());
	FwRules.Finish();
	Data.WriteVariant(SVC_API_PROG_FW_RULES, FwRules);
}