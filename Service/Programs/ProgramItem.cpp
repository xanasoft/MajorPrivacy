#include "pch.h"
#include "ProgramItem.h"
#include "../Processes/ProcessList.h"
#include "../ServiceCore.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Network/NetworkManager.h"
#include "../Network/SocketList.h"
#include "../Network/Firewall/FirewallRule.h"
#include "../Programs/ProgramRule.h"
#include "../Access/AccessRule.h"


CProgramItem::CProgramItem()
{
	static volatile LONG64 VolatileIdCounter = 0;
	
	m_UID = InterlockedIncrement64(&VolatileIdCounter);
}

CVariant CProgramItem::ToVariant(const SVarWriteOpt& Opts) const
{
	std::unique_lock Lock(m_Mutex);

	CVariant Data;
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Data.BeginIMap();
		WriteIVariant(Data, Opts);
	} else {  
		Data.BeginMap();
		WriteMVariant(Data, Opts);
	}
	Data.Finish();
	return Data;
}

NTSTATUS CProgramItem::FromVariant(const class CVariant& Data)
{
	std::unique_lock Lock(m_Mutex);

	if (Data.GetType() == VAR_TYPE_MAP)         Data.ReadRawMap([&](const SVarName& Name, const CVariant& Data) { ReadMValue(Name, Data); });
	else if (Data.GetType() == VAR_TYPE_INDEX)  Data.ReadRawIMap([&](uint32 Index, const CVariant& Data)        { ReadIValue(Index, Data); });
	else
		return STATUS_UNKNOWN_REVISION;
	return STATUS_SUCCESS;
}

CVariant CProgramItem::CollectFwRules() const
{
	CVariant FwRules;
	FwRules.BeginList();
	for (auto FwRule : m_FwRules)
		FwRules.Write(FwRule->GetGuid());
	FwRules.Finish();
	return FwRules;
}

CVariant CProgramItem::CollectProgRules() const
{
	CVariant ProgRules;
	ProgRules.BeginList();
	for (auto ProgRule : m_ProgRules)
		ProgRules.Write(ProgRule->GetGuid());
	ProgRules.Finish();
	return ProgRules;
}

CVariant CProgramItem::CollectResRules() const
{
	CVariant ResRules;
	ResRules.BeginList();
	for (auto ResRule : m_ResRules)
		ResRules.Write(ResRule->GetGuid());
	ResRules.Finish();
	return ResRules;
}
