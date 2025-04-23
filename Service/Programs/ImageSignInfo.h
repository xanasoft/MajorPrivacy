#pragma once

#include "../Library/API/PrivacyDefs.h"
#include "../Library/Common/StVariant.h"

struct CImageSignInfo
{
public:
	CImageSignInfo();

	UCISignInfo	GetInfo() const { return m_SignInfo; }
	uint64 GetRawInfo() const { return m_SignInfo.Data; }
	//void SetRawInfo(uint64 Info) { m_SignInfo.Data = Info; }
	void SetAuthority(uint8 Authority) { m_SignInfo.Authority = Authority; }
	EHashStatus GetHashStatus() const { return m_HashStatus; }

	void Update(const struct SVerifierInfo* pVerifyInfo);

	StVariant ToVariant(const SVarWriteOpt& Opts) const;
	NTSTATUS FromVariant(const StVariant& Data);

protected:

	void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const;
	void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const;
	void ReadIValue(uint32 Index, const StVariant& Data);
	void ReadMValue(const SVarName& Name, const StVariant& Data);

	UCISignInfo					m_SignInfo;
	EHashStatus					m_HashStatus = EHashStatus::eHashUnknown;
	std::vector<uint8>			m_FileHash;
	std::vector<uint8>			m_SignerHash;
	std::string					m_SignerName;
};

struct SLibraryInfo
{
	uint64						LastLoadTime = 0;
	uint32						TotalLoadCount = 0;
	CImageSignInfo				SignInfo;
	EEventStatus				LastStatus = EEventStatus::eUndefined;
};