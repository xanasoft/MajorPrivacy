#include "pch.h"
#include "ImageSignInfo.h"
#include "../../Library/API/DriverAPI.h"
#include "../../Library/Helpers/NtUtil.h"

CImageSignInfo::CImageSignInfo()
{
}

void CImageSignInfo::Update(const struct SVerifierInfo* pVerifyInfo)
{
	m_StatusFlags = pVerifyInfo->StatusFlags;

	if (!pVerifyInfo->FileHash.empty()) {
		m_FileHashAlgorithm = pVerifyInfo->FileHashAlgorithm;
		m_FileHash = pVerifyInfo->FileHash;
	}

	if (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_SA)
		m_PrivateAuthority = pVerifyInfo->PrivateAuthority;

	if (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_CI)
	{
		m_SignPolicyBits = pVerifyInfo->SignPolicyBits;

		m_SignerHashAlgorithm = pVerifyInfo->SignerHashAlgorithm;
		m_SignerHash = pVerifyInfo->SignerHash;
		m_SignerName = pVerifyInfo->SignerName;
		
		m_IssuerHashAlgorithm = pVerifyInfo->IssuerHashAlgorithm;
		m_IssuerHash = pVerifyInfo->IssuerHash;
		m_IssuerName = pVerifyInfo->IssuerName;
	}

	if (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_SL)
		m_SignLevel = pVerifyInfo->SignLevel;

	m_Signatures = pVerifyInfo->FoundSignatures;

	m_TimeStamp = GetCurrentTimeAsFileTime();
}

StVariant CImageSignInfo::ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
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

NTSTATUS CImageSignInfo::FromVariant(const class StVariant& Data)
{
	if (Data.GetType() == VAR_TYPE_MAP)         StVariantReader(Data).ReadRawMap([&](const SVarName& Name, const StVariant& Data) { ReadMValue(Name, Data); });
	else if (Data.GetType() == VAR_TYPE_INDEX)  StVariantReader(Data).ReadRawIndex([&](uint32 Index, const StVariant& Data) { ReadIValue(Index, Data); });
	else
		return STATUS_UNKNOWN_REVISION;
	return STATUS_SUCCESS;
}