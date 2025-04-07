#include "pch.h"
#include "ImageSignInfo.h"
#include "../../Library/API/DriverAPI.h"

CImageSignInfo::CImageSignInfo()
{
}

QtVariant CImageSignInfo::ToVariant(const SVarWriteOpt& Opts) const
{
	QtVariantWriter Data;
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Data.BeginIndex();
		WriteIVariant(Data, Opts);
	} else {  
		Data.BeginMap();
		WriteMVariant(Data, Opts);
	}
	return Data.Finish();
}

NTSTATUS CImageSignInfo::FromVariant(const class QtVariant& Data)
{
	if (Data.GetType() == VAR_TYPE_MAP)         QtVariantReader(Data).ReadRawMap([&](const SVarName& Name, const QtVariant& Data) { ReadMValue(Name, Data); });
	else if (Data.GetType() == VAR_TYPE_INDEX)  QtVariantReader(Data).ReadRawIndex([&](uint32 Index, const QtVariant& Data) { ReadIValue(Index, Data); });
	else
		return STATUS_UNKNOWN_REVISION;
	return STATUS_SUCCESS;
}