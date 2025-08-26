#pragma once

#include "../Library/API/PrivacyDefs.h"
#include "../Library/Common/StVariant.h"

struct CImageSignInfo
{
public:
	CImageSignInfo();

	bool HasFileHash() const { return !m_FileHash.empty(); }

	KPH_VERIFY_AUTHORITY GetAuthority() const { return m_PrivateAuthority; }
	void SetAuthority(KPH_VERIFY_AUTHORITY Authority) { m_PrivateAuthority = Authority; }
	uint64 GetTimeStamp() const { return m_TimeStamp; }

	void Update(const struct SVerifierInfo* pVerifyInfo);

	StVariant ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	NTSTATUS FromVariant(const StVariant& Data);

protected:

	void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const;
	void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const;
	void ReadIValue(uint32 Index, const StVariant& Data);
	void ReadMValue(const SVarName& Name, const StVariant& Data);

	uint32						m_StatusFlags = 0;

	KPH_VERIFY_AUTHORITY		m_PrivateAuthority = KphUntestedAuthority;
	uint32						m_SignLevel = 0;
	uint32						m_SignPolicyBits = 0;

	ULONG 						m_FileHashAlgorithm = 0;
	std::vector<uint8>			m_FileHash;

	ULONG						m_SignerHashAlgorithm = 0;
	std::vector<uint8>			m_SignerHash;
	std::string					m_SignerName;

	ULONG						m_IssuerHashAlgorithm = 0;
	std::vector<uint8>			m_IssuerHash;
	std::string					m_IssuerName;

	USignatures					m_Signatures = {0};

	uint64						m_TimeStamp = 0;
};

struct SLibraryInfo
{
	uint64						LastLoadTime = 0;
	uint32						TotalLoadCount = 0;
	CImageSignInfo				SignInfo;
	EEventStatus				LastStatus = EEventStatus::eUndefined;
};