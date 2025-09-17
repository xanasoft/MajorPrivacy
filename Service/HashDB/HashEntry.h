#pragma once
#include "../Library/Common/StVariant.h"
#include "../Library/API/PrivacyDefs.h"
#include "../Library/Common/FlexGuid.h"

class CHash
{
	TRACK_OBJECT(CHash)
public:
	CHash();
	~CHash();

	void Update(const std::shared_ptr<CHash>& pHash);

	bool IsEnabled() const { std::shared_lock Lock(m_Mutex); return m_bEnabled; }
	void SetEnabled(bool bEnabled) { std::unique_lock Lock(m_Mutex); m_bEnabled = bEnabled; }

	bool IsTemporary() const { std::shared_lock Lock(m_Mutex); return m_bTemporary; }
	void SetTemporary(bool bTemporary) { std::unique_lock Lock(m_Mutex); m_bTemporary = bTemporary; }

	EHashType GetType() const { std::shared_lock Lock(m_Mutex); return m_Type; }
	void SetType(EHashType Type) { std::unique_lock Lock(m_Mutex); m_Type = Type; }

	CBuffer GetHash() const { std::shared_lock Lock(m_Mutex); return m_Hash; }
	void SetHash(const CBuffer& Hash) { std::unique_lock Lock(m_Mutex); m_Hash = Hash; }

	std::wstring GetName() const { std::shared_lock Lock(m_Mutex); return m_Name; }
	void SetName(const std::wstring& Name) { std::unique_lock Lock(m_Mutex); m_Name = Name; }
	std::wstring GetDescription() const { std::shared_lock Lock(m_Mutex); return m_Description; }
	void SetDescription(const std::wstring& Description) { std::unique_lock Lock(m_Mutex); m_Description = Description; }

	const std::list<CFlexGuid>& GetEnclaves() const { std::shared_lock Lock(m_Mutex); return m_Enclaves; }
	void SetEnclaves(const std::list<CFlexGuid>& Enclaves) { std::unique_lock Lock(m_Mutex); m_Enclaves = Enclaves; }
	bool HasEnclave(const CFlexGuid& Enclave) const { std::shared_lock Lock(m_Mutex); return std::find(m_Enclaves.begin(), m_Enclaves.end(), Enclave) != m_Enclaves.end(); }
	void AddEnclave(const CFlexGuid& Enclave) {
		std::unique_lock Lock(m_Mutex); if (std::find(m_Enclaves.begin(), m_Enclaves.end(), Enclave) == m_Enclaves.end()) m_Enclaves.push_back(Enclave);
	}
	void RemoveEnclave(const CFlexGuid& Enclave) {
		std::unique_lock Lock(m_Mutex); m_Enclaves.remove(Enclave);
	}
	
	const std::list<std::wstring>& GetCollections() const { std::shared_lock Lock(m_Mutex); return m_Collections; }
	void SetCollections(const std::list<std::wstring>& Collections) { std::unique_lock Lock(m_Mutex); m_Collections = Collections; }

	StVariant ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	NTSTATUS FromVariant(const class StVariant& Hash);

protected:
	friend class CHashEntryWnd;

	void WriteIVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	void WriteMVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	void ReadIValue(uint32 Index, const StVariant& Data);
	void ReadMValue(const SVarName& Name, const StVariant& Data);

	mutable std::shared_mutex m_Mutex;

	std::wstring			m_Name; // Filename or Subject
	std::wstring			m_Description;
	bool 					m_bEnabled = true;
	bool 					m_bTemporary = false;
	EHashType				m_Type = EHashType::eUnknown;
	CBuffer					m_Hash;

	std::list<CFlexGuid>	m_Enclaves; // Enclave Guids
	std::list<std::wstring>	m_Collections;

	StVariant m_Data;
};

typedef std::shared_ptr<CHash> CHashPtr;