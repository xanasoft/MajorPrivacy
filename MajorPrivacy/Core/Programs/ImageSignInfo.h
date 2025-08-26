#pragma once

#include "../Library/API/PrivacyDefs.h"
#include "./Common/QtVariant.h"
#include "./Common/QtFlexGuid.h"


struct CImageSignInfo
{
public:
	CImageSignInfo();

	KPH_VERIFY_AUTHORITY GetPrivateAuthority() const { return m_PrivateAuthority; }

	uint64 GetUnion() const;

	bool IsComplete() const;
	USignatures GetSignatures() const { return m_Signatures; }

	bool HashFileHash() const { return m_FileHash.size() > 0; }
	QByteArray GetFileHash() const { return m_FileHash; }

	bool HashSignerHash() const { return m_SignerHash.size() > 0; }
	QByteArray GetSignerHash() const { return m_SignerHash; }
	QString GetSignerName() const { return m_SignerName; }

	bool HashIssuerHash() const { return m_IssuerHash.size() > 0; }
	QByteArray GetIssuerHash() const { return m_IssuerHash; }
	QString GetIssuerName() const { return m_IssuerName; }

	QtVariant ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	NTSTATUS FromVariant(const QtVariant& Data);

protected:
	friend class CProgramFile;

	void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	void ReadIValue(uint32 Index, const QtVariant& Data);
	void ReadMValue(const SVarName& Name, const QtVariant& Data);

	uint32						m_StatusFlags = 0;

	KPH_VERIFY_AUTHORITY		m_PrivateAuthority = KphUntestedAuthority; // 3 bits
	uint32						m_SignLevel = 0;	// 4 bits
	uint32						m_SignPolicyBits = 0;	// 16/32 bits

	ULONG 						m_FileHashAlgorithm = 0;
	QByteArray					m_FileHash;

	ULONG 						m_SignerHashAlgorithm = 0;
	QByteArray					m_SignerHash;
	QString						m_SignerName;

	ULONG 						m_IssuerHashAlgorithm = 0;
	QByteArray					m_IssuerHash;
	QString						m_IssuerName;

	USignatures					m_Signatures = {0};

	quint64						m_TimeStamp = 0;
};

struct SLibraryInfo
{
	QFlexGuid					EnclaveGuid;
	uint64						LastLoadTime = 0;
	uint32						TotalLoadCount = 0;
	EEventStatus				LastStatus = EEventStatus::eUndefined;
	CImageSignInfo				SignInfo;
};