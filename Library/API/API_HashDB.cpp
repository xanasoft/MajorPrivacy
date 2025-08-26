

/////////////////////////////////////////////////////////////////////////////
// CHash
//

void CHash::WriteIVariant(VariantWriter& Hash, const SVarWriteOpt& Opts) const
{
	Hash.WriteEx(API_V_NAME, TO_STR(m_Name));
	Hash.WriteEx(API_V_RULE_DESCR, TO_STR(m_Description));
	Hash.Write(API_V_ENABLED, m_bEnabled);
	Hash.Write(API_V_TYPE, (uint32)m_Type);
	Hash.WriteEx(API_V_HASH, m_Hash);

	VariantWriter Enclaves(Hash.Allocator());
	Enclaves.BeginList();
	for(const auto& Enclave : m_Enclaves)
#ifdef HASHDB_GUI
		Enclaves.WriteEx(Enclave.ToQS());
#else
		Enclaves.WriteEx(TO_STR(Enclave));
#endif
	Hash.WriteVariant(API_V_ENCLAVES, Enclaves.Finish());

	VariantWriter Collections(Hash.Allocator());
	Collections.BeginList();
	for(const auto& Collection : m_Collections)
		Collections.WriteEx(TO_STR(Collection));
	Hash.WriteVariant(API_V_COLLECTIONS, Collections.Finish());

	Hash.WriteVariant(API_V_DATA, m_Data);
}

void CHash::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_NAME:		m_Name = AS_STR(Data); break;
	case API_V_RULE_DESCR:	m_Description = AS_STR(Data); break;
	case API_V_ENABLED:		m_bEnabled = Data.To<bool>(); break;
	case API_V_TYPE:		m_Type = (EHashType)Data.To<uint32>(); break;
#ifdef HASHDB_GUI
	case API_V_HASH:		m_Hash = Data.AsQBytes(); break;
#else
	case API_V_HASH:		m_Hash = Data.ToBuffer(); break;
#endif

	case API_V_ENCLAVES:
		LIST_CLEAR(m_Enclaves);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_Enclaves, AS_STR(Data[i]));
		break;

	case API_V_COLLECTIONS:
		LIST_CLEAR(m_Collections);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_Collections, AS_STR(Data[i]));
		break;

	case API_V_DATA:		m_Data = Data.Clone(); break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CHash::WriteMVariant(VariantWriter& Hash, const SVarWriteOpt& Opts) const
{
	Hash.WriteEx(API_S_NAME, TO_STR(m_Name));
	Hash.WriteEx(API_S_RULE_DESCR, TO_STR(m_Description));
	Hash.Write(API_S_ENABLED, m_bEnabled);
	switch (m_Type)
	{
	case EHashType::eFileHash:    Hash.Write(API_S_TYPE, API_S_HASH_TYPE_FILE); break;
	case EHashType::eCertHash: Hash.Write(API_S_TYPE, API_S_HASH_TYPE_CERT); break;
	}
	Hash.WriteEx(API_S_HASH, m_Hash);

	VariantWriter Enclaves(Hash.Allocator());
	Enclaves.BeginList();
	for(const auto& Enclave : m_Enclaves)
#ifdef HASHDB_GUI
		Enclaves.WriteEx(Enclave.ToQS());
#else
		Enclaves.WriteEx(TO_STR(Enclave));
#endif
	Hash.WriteVariant(API_S_ENCLAVES, Enclaves.Finish());

	VariantWriter Collections(Hash.Allocator());
	Collections.BeginList();
	for(const auto& Collection : m_Collections)
		Collections.WriteEx(TO_STR(Collection));
	Hash.WriteVariant(API_S_COLLECTIONS, Collections.Finish());

	Hash.WriteVariant(API_S_DATA, m_Data);
}

void CHash::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_NAME))				m_Name = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_RULE_DESCR))		m_Description = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_ENABLED))		m_bEnabled = Data.To<bool>();
	else if (VAR_TEST_NAME(Name, API_S_TYPE))
	{
		ASTR Type = Data;
		if (Type == API_S_HASH_TYPE_FILE)				m_Type = EHashType::eFileHash;
		else if (Type == API_S_HASH_TYPE_CERT)			m_Type = EHashType::eCertHash;
		else m_Type = EHashType::eUnknown;
	}
#ifdef HASHDB_GUI
	else if (VAR_TEST_NAME(Name, API_S_HASH))			m_Hash = Data.AsQBytes();
#else
	else if (VAR_TEST_NAME(Name, API_S_HASH))			m_Hash = Data.ToBuffer();
#endif

	else if (VAR_TEST_NAME(Name, API_S_ENCLAVES))
	{
		LIST_CLEAR(m_Enclaves);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_Enclaves, AS_STR(Data[i]));
	}

	else if (VAR_TEST_NAME(Name, API_S_COLLECTIONS))
	{
		LIST_CLEAR(m_Collections);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_Collections, AS_STR(Data[i]));
	}
	

	else if (VAR_TEST_NAME(Name, API_S_DATA))	        m_Data = Data.Clone();
}
