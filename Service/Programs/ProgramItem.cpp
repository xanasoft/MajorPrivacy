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

StVariant CProgramItem::ToVariant(const SVarWriteOpt& Opts) const
{
	std::unique_lock Lock(m_Mutex);

	StVariantWriter Data;
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Data.BeginIndex();
		WriteIVariant(Data, Opts);
	} else {  
		Data.BeginMap();
		WriteMVariant(Data, Opts);
	}
	return Data.Finish();;
}

NTSTATUS CProgramItem::FromVariant(const class StVariant& Data)
{
	std::unique_lock Lock(m_Mutex);

	if (Data.GetType() == VAR_TYPE_MAP)         StVariantReader(Data).ReadRawMap([&](const SVarName& Name, const StVariant& Data) { ReadMValue(Name, Data); });
	else if (Data.GetType() == VAR_TYPE_INDEX)  StVariantReader(Data).ReadRawIndex([&](uint32 Index, const StVariant& Data) { ReadIValue(Index, Data); });
	else
		return STATUS_UNKNOWN_REVISION;
	return STATUS_SUCCESS;
}

StVariant CProgramItem::CollectFwRules() const
{
	StVariantWriter FwRules;
	FwRules.BeginList();
	for (auto FwRule : m_FwRules)
		FwRules.WriteEx(FwRule->GetGuidStr());
	return FwRules.Finish();
}

StVariant CProgramItem::CollectProgRules() const
{
	StVariantWriter ProgRules;
	ProgRules.BeginList();
	for (auto ProgRule : m_ProgRules)
		ProgRules.WriteVariant(ProgRule->GetGuid().ToVariant(false));
	return ProgRules.Finish();
}

StVariant CProgramItem::CollectResRules() const
{
	StVariantWriter ResRules;
	ResRules.BeginList();
	for (auto ResRule : m_ResRules)
		ResRules.WriteVariant(ResRule->GetGuid().ToVariant(false));
	return ResRules.Finish();
}
