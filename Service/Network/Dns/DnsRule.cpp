#include "pch.h"
#include "DnsFilter.h"
#include "../../ServiceCore.h"
#include "../../../Library/API/PrivacyAPI.h"
#include "../../../Library/Common/Strings.h"
#include "../../../Library/Common/FileIO.h"
#include "../../../Library/Helpers/WebUtils.h"
#include "../../../Library/Helpers/MiscHelpers.h"
#include "../../../Library/Helpers/WinUtil.h"

FW::StringW ReversePath(FW::AbstractMemPool* pMem, const char* pPath);

/////////////////////////////////////////////////////////////////////////////
// CDnsRule
//

StVariant CDnsRule::ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
	StVariantWriter Rule(pMemPool);
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Rule.BeginIndex();
		WriteIVariant(Rule, Opts);
	} else {  
		Rule.BeginMap();
		WriteMVariant(Rule, Opts);
	}
	return Rule.Finish();
}

STATUS CDnsRule::FromVariant(const class StVariant& Rule)
{
	if (Rule.GetType() == VAR_TYPE_MAP)         StVariantReader(Rule).ReadRawMap([&](const SVarName& Name, const StVariant& Data) { ReadMValue(Name, Data); });
	else if (Rule.GetType() == VAR_TYPE_INDEX)  StVariantReader(Rule).ReadRawIndex([&](uint32 Index, const StVariant& Data) { ReadIValue(Index, Data); });
	else
		return STATUS_UNKNOWN_REVISION;

	FW::StringW Path = ReversePath(Allocator(), m_HostName.c_str());
	SetPath(Path);
	return STATUS_SUCCESS;
}
