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
	static volatile LONG64 VolatileIdCounter = 0x10;
	
	m_UID = InterlockedIncrement64(&VolatileIdCounter);
}

StVariant CProgramItem::ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
	std::unique_lock Lock(m_Mutex);

	StVariantWriter Data(pMemPool);
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Data.BeginIndex();
		WriteIVariant(Data, Opts);
	} else {  
		Data.BeginMap();
		WriteMVariant(Data, Opts);
	}
	return Data.Finish();
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

ETracePreset StrToTracePreset(const FW::StringA& Str);
ESavePreset StrToSavePreset(const FW::StringA& Str);

NTSTATUS CProgramItem::UpdateFromVariant(const class StVariant& vReq)
{
	if (vReq.GetType() == VAR_TYPE_MAP) 
	{
		if (vReq.Has(API_S_NAME)) SetName(vReq[API_S_NAME]);
		if (vReq.Has(API_S_ICON)) SetIcon(vReq[API_S_ICON]);
		if (vReq.Has(API_S_INFO)) SetInfo(vReq[API_S_INFO]);

		if (vReq.Has(API_S_EXEC_TRACE)) SetExecTrace(StrToTracePreset(vReq[API_S_EXEC_TRACE]));
		if (vReq.Has(API_S_RES_TRACE))  SetResTrace(StrToTracePreset(vReq[API_S_RES_TRACE]));
		if (vReq.Has(API_S_NET_TRACE))  SetNetTrace(StrToTracePreset(vReq[API_S_NET_TRACE]));
		if (vReq.Has(API_S_SAVE_TRACE)) SetSaveTrace(StrToSavePreset(vReq[API_S_SAVE_TRACE]));
	}
	else
	{
		if (vReq.Has(API_V_NAME)) SetName(vReq[API_V_NAME]);
		if (vReq.Has(API_V_ICON)) SetIcon(vReq[API_V_ICON]);
		if (vReq.Has(API_V_INFO)) SetInfo(vReq[API_V_INFO]);

		if (vReq.Has(API_V_EXEC_TRACE)) SetExecTrace((ETracePreset)vReq[API_V_EXEC_TRACE].To<int>());
		if (vReq.Has(API_V_RES_TRACE))  SetResTrace((ETracePreset)vReq[API_V_RES_TRACE].To<int>());
		if (vReq.Has(API_V_NET_TRACE))  SetNetTrace((ETracePreset)vReq[API_V_NET_TRACE].To<int>());
		if (vReq.Has(API_V_SAVE_TRACE)) SetSaveTrace((ESavePreset)vReq[API_V_SAVE_TRACE].To<int>());
	}

	return STATUS_SUCCESS;
}

StVariant CProgramItem::CollectFwRules(FW::AbstractMemPool* pMemPool) const
{
	StVariantWriter FwRules(pMemPool);
	FwRules.BeginList();
	for (auto FwRule : m_FwRules)
		FwRules.WriteEx(FwRule->GetGuidStr());
	return FwRules.Finish();
}

StVariant CProgramItem::CollectProgRules(FW::AbstractMemPool* pMemPool) const
{
	StVariantWriter ProgRules(pMemPool);
	ProgRules.BeginList();
	for (auto ProgRule : m_ProgRules)
		ProgRules.WriteVariant(ProgRule->GetGuid().ToVariant(false, pMemPool));
	return ProgRules.Finish();
}

StVariant CProgramItem::CollectResRules(FW::AbstractMemPool* pMemPool) const
{
	StVariantWriter ResRules(pMemPool);
	ResRules.BeginList();
	for (auto ResRule : m_ResRules)
		ResRules.WriteVariant(ResRule->GetGuid().ToVariant(false, pMemPool));
	return ResRules.Finish();
}
