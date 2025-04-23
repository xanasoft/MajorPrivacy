#include "pch.h"
#include "ImageSignInfo.h"
#include "../../Library/API/DriverAPI.h"

CImageSignInfo::CImageSignInfo()
{
}

void CImageSignInfo::Update(const struct SVerifierInfo* pVerifyInfo)
{
	if (pVerifyInfo->SignAuthority != KphUntestedAuthority)
		m_SignInfo.Authority = (uint8)pVerifyInfo->SignAuthority;
	if (!pVerifyInfo->FileHash.empty())
		m_FileHash = pVerifyInfo->FileHash;
	if (pVerifyInfo->VerificationFlags & KPH_VERIFY_FLAG_CI_SIG_DUMMY)
	{
		if (m_SignerHash.empty()) {
			if (pVerifyInfo->SignerHash.empty())
				m_HashStatus = EHashStatus::eHashNone;
			else
			{
				m_HashStatus = EHashStatus::eHashDummy;
				m_SignerHash = pVerifyInfo->SignerHash;
				m_SignerName = pVerifyInfo->SignerName;
			}
		}
	}
	else if (pVerifyInfo->VerificationFlags & KPH_VERIFY_FLAG_CI_SIG_OK)
	{
		if (pVerifyInfo->SignLevel) m_SignInfo.Level = (uint8)pVerifyInfo->SignLevel;
		if (pVerifyInfo->SignPolicy) m_SignInfo.Policy = pVerifyInfo->SignPolicy;
		if (!pVerifyInfo->SignerHash.empty()) {
			m_HashStatus = EHashStatus::eHashOk;
			m_SignerHash = pVerifyInfo->SignerHash;
			m_SignerName = pVerifyInfo->SignerName;
		}
	}
	else if (pVerifyInfo->VerificationFlags & KPH_VERIFY_FLAG_CI_SIG_FAIL)
	{
		m_SignInfo.Level = 0;
		m_SignInfo.Policy = 0;
		m_HashStatus = EHashStatus::eHashFail;
		m_SignerHash.clear();
		m_SignerName.clear();
	}
}

StVariant CImageSignInfo::ToVariant(const SVarWriteOpt& Opts) const
{
	StVariantWriter Data;
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Data.BeginIndex();
		WriteIVariant(Data, Opts);
	} else {  
		Data.BeginMap();
		WriteMVariant(Data, Opts);
	}
	return Data.Finish();
}

NTSTATUS CImageSignInfo::FromVariant(const class StVariant& Data)
{
	if (Data.GetType() == VAR_TYPE_MAP)         StVariantReader(Data).ReadRawMap([&](const SVarName& Name, const StVariant& Data) { ReadMValue(Name, Data); });
	else if (Data.GetType() == VAR_TYPE_INDEX)  StVariantReader(Data).ReadRawIndex([&](uint32 Index, const StVariant& Data) { ReadIValue(Index, Data); });
	else
		return STATUS_UNKNOWN_REVISION;
	return STATUS_SUCCESS;
}