#pragma once

#include "../Library/API/PrivacyDefs.h"
#include "./Common/QtVariant.h"
#include "./Common/QtFlexGuid.h"

struct CImageSignInfo
{
public:
	CImageSignInfo();

	UCISignInfo	GetInfo() const { return m_SignInfo; }
	uint64 GetRawInfo() const { return m_SignInfo.Data; }
	EHashStatus GetHashStatus() const { return m_HashStatus; }

	QByteArray GetFileHash() const { return m_FileHash; }
	QByteArray GetSignerHash() const { return m_SignerHash; }
	QString GetSignerName() const { return m_SignerName; }

	QtVariant ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	NTSTATUS FromVariant(const QtVariant& Data);

protected:
	void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	void ReadIValue(uint32 Index, const QtVariant& Data);
	void ReadMValue(const SVarName& Name, const QtVariant& Data);

	UCISignInfo					m_SignInfo;
	EHashStatus					m_HashStatus = EHashStatus::eHashUnknown;
	QByteArray					m_FileHash;
	QByteArray					m_SignerHash;
	QString						m_SignerName;
};

struct SLibraryInfo
{
	QFlexGuid					EnclaveGuid;
	uint64						LastLoadTime = 0;
	uint32						TotalLoadCount = 0;
	EEventStatus				LastStatus = EEventStatus::eUndefined;
	CImageSignInfo				SignInfo;
};