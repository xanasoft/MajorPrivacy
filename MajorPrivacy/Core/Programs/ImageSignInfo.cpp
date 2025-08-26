#include "pch.h"
#include "ImageSignInfo.h"
#include "../../Library/API/DriverAPI.h"

CImageSignInfo::CImageSignInfo()
{
}

uint64 CImageSignInfo::GetUnion() const
{
	union UImageSignInfo
	{
		uint64					Data = 0;
		struct {
			uint8				Authority : 3;	// 3 bits - KPH_VERIFY_AUTHORITY
			uint8				Unused : 1;
			uint8				Level : 4;		// 4 bits
			uint8				Policy : 8;
			uint32				Signatures;
		};
	} u = {0};
	u.Authority = (uint8)m_PrivateAuthority;
	u.Level = (uint8)m_SignLevel;
	u.Policy = (uint8)m_SignPolicyBits;
	u.Signatures = m_Signatures.Value;
	return u.Data;
}

bool CImageSignInfo::IsComplete() const
{
	if ((m_StatusFlags & MP_VERIFY_FLAG_SA) && m_PrivateAuthority >= KphUserAuthority)
		return true;
	if ((m_StatusFlags & MP_VERIFY_FLAG_CI) && m_SignPolicyBits != 0) 
		return true;
	if ((m_StatusFlags & MP_VERIFY_FLAG_SL) && m_SignLevel != 0)
		return true;
	return false;
}

QtVariant CImageSignInfo::ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
	QtVariantWriter Data(pMemPool);
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