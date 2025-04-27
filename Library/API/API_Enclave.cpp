

/////////////////////////////////////////////////////////////////////////////
// CEnclave
//

void CEnclave::WriteIVariant(VariantWriter& Enclave, const SVarWriteOpt& Opts) const
{
#ifdef KERNEL_MODE
	if(!IS_EMPTY(m_Guid))
		Enclave.Write(API_V_GUID, TO_STR(m_Guid));
#else
	if(!m_Guid.IsNull())
		Enclave.WriteVariant(API_V_GUID, m_Guid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids));
#endif
	Enclave.Write(API_V_ENABLED, m_bEnabled);

	Enclave.WriteEx(API_V_NAME, TO_STR(m_Name));
	//Enclave.WriteEx(API_V_RULE_GROUP, TO_STR(m_Grouping));
	Enclave.WriteEx(API_V_RULE_DESCR, TO_STR(m_Description));

	Enclave.Write(API_V_EXEC_SIGN_REQ, (uint32)m_SignatureLevel);
	Enclave.Write(API_V_EXEC_ON_TRUSTED_SPAWN, (uint32)m_OnTrustedSpawn);
	Enclave.Write(API_V_EXEC_ON_SPAWN, (uint32)m_OnSpawn);
	Enclave.Write(API_V_IMAGE_LOAD_PROTECTION, m_ImageLoadProtection);

	Enclave.Write(API_V_INTEGRITY_LEVEL, (uint32)m_IntegrityLevel);

	Enclave.Write(API_V_ALLOW_DEBUGGING, m_AllowDebugging);
	Enclave.Write(API_V_KEEP_ALIVE, m_KeepAlive);

	Enclave.WriteVariant(API_V_DATA, m_Data);
}

void CEnclave::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
#ifdef KERNEL_MODE
	case API_V_GUID:		if(IS_EMPTY(m_Guid)) m_Guid = AS_STR(Data); break;
#else
	case API_V_GUID:		if(m_Guid.IsNull()) m_Guid.FromVariant(Data); break;
#endif
	case API_V_ENABLED:		m_bEnabled = Data.To<bool>(); break;

	case API_V_NAME:		m_Name = AS_STR(Data); break;
	//case API_V_RULE_GROUP:	m_Grouping = AS_STR(Data); break;
	case API_V_RULE_DESCR:	m_Description = AS_STR(Data); break;

	case API_V_EXEC_SIGN_REQ: m_SignatureLevel = (KPH_VERIFY_AUTHORITY)Data.To<uint32>(); break;
	case API_V_EXEC_ON_TRUSTED_SPAWN: m_OnTrustedSpawn = (EProgramOnSpawn)Data.To<uint32>(); break;
	case API_V_EXEC_ON_SPAWN: m_OnSpawn = (EProgramOnSpawn)Data.To<uint32>(); break;
	case API_V_IMAGE_LOAD_PROTECTION: m_ImageLoadProtection = Data.To<bool>(); break;

	case API_V_INTEGRITY_LEVEL: m_IntegrityLevel = (EIntegrityLevel)Data.To<uint32>(); break;

	case API_V_ALLOW_DEBUGGING:	m_AllowDebugging = Data.To<bool>(); break;
	case API_V_KEEP_ALIVE:	m_KeepAlive = Data.To<bool>(); break;

	case API_V_DATA:		m_Data = Data.Clone(); break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CEnclave::WriteMVariant(VariantWriter& Enclave, const SVarWriteOpt& Opts) const
{
#ifdef KERNEL_MODE
	if(!IS_EMPTY(m_Guid))
		Enclave.Write(API_S_GUID, TO_STR(m_Guid));
#else
	if(!m_Guid.IsNull())
		Enclave.WriteVariant(API_S_GUID, m_Guid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids));
#endif
	Enclave.Write(API_S_ENABLED, m_bEnabled);

	Enclave.WriteEx(API_S_NAME, TO_STR(m_Name));
	//Enclave.WriteEx(API_S_RULE_GROUP, TO_STR(m_Grouping));
	Enclave.WriteEx(API_S_RULE_DESCR, TO_STR(m_Description));

	switch (m_SignatureLevel) 
	{
	case KphDevAuthority:
	case KphUserAuthority:	Enclave.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_VERIFIED); break;

	case KphMsAuthority:
	case KphAvAuthority:	Enclave.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_MICROSOFT); break;

	case KphMsCodeAuthority:Enclave.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_TRUSTED); break;

	case KphUnkAuthority:	Enclave.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_OTHER); break;

	case KphUntestedAuthority:
	case KphNoAuthority:	Enclave.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_NONE); break;
	}

	switch (m_OnTrustedSpawn)
	{
	case EProgramOnSpawn::eAllow:	Enclave.Write(API_S_EXEC_ON_TRUSTED_SPAWN, API_S_EXEC_ON_SPAWN_ALLOW); break;
	case EProgramOnSpawn::eBlock:	Enclave.Write(API_S_EXEC_ON_TRUSTED_SPAWN, API_S_EXEC_ON_SPAWN_BLOCK); break;
	case EProgramOnSpawn::eEject:	Enclave.Write(API_S_EXEC_ON_TRUSTED_SPAWN, API_S_EXEC_ON_SPAWN_EJECT); break;
	}

	switch(m_OnSpawn)
	{
	case EProgramOnSpawn::eAllow:	Enclave.Write(API_S_EXEC_ON_SPAWN, API_S_EXEC_ON_SPAWN_ALLOW); break;
	case EProgramOnSpawn::eBlock:	Enclave.Write(API_S_EXEC_ON_SPAWN, API_S_EXEC_ON_SPAWN_BLOCK); break;
	case EProgramOnSpawn::eEject:	Enclave.Write(API_S_EXEC_ON_SPAWN, API_S_EXEC_ON_SPAWN_EJECT); break;
	}

	Enclave.Write(API_S_IMAGE_LOAD_PROTECTION, m_ImageLoadProtection);

	switch (m_IntegrityLevel)
	{
	case EIntegrityLevel::eUntrusted:	Enclave.Write(API_S_INTEGRITY_LEVEL, API_S_INTEGRITY_LEVEL_UNTRUSTED); break;
	case EIntegrityLevel::eLow:			Enclave.Write(API_S_INTEGRITY_LEVEL, API_S_INTEGRITY_LEVEL_LOW); break;
	case EIntegrityLevel::eMedium:		Enclave.Write(API_S_INTEGRITY_LEVEL, API_S_INTEGRITY_LEVEL_MEDIUM); break;
	case EIntegrityLevel::eMediumPlus:	Enclave.Write(API_S_INTEGRITY_LEVEL, API_S_INTEGRITY_LEVEL_MEDIUM_PLUS); break;
	case EIntegrityLevel::eHigh:		Enclave.Write(API_S_INTEGRITY_LEVEL, API_S_INTEGRITY_LEVEL_HIGH); break;
	case EIntegrityLevel::eSystem:		Enclave.Write(API_S_INTEGRITY_LEVEL, API_S_INTEGRITY_LEVEL_SYSTEM); break;
	}

	Enclave.Write(API_S_ALLOW_DEBUGGING, m_AllowDebugging);
	Enclave.Write(API_S_KEEP_ALIVE, m_KeepAlive);

	Enclave.WriteVariant(API_S_DATA, m_Data);
}

void CEnclave::ReadMValue(const SVarName& Name, const XVariant& Data)
{
#ifdef KERNEL_MODE
	if (VAR_TEST_NAME(Name, API_S_GUID))				{ if(IS_EMPTY(m_Guid)) m_Guid = AS_STR(Data); }
#else
	if (VAR_TEST_NAME(Name, API_S_GUID))				{ if(m_Guid.IsNull()) m_Guid.FromVariant(Data); }
#endif
	else if (VAR_TEST_NAME(Name, API_S_ENABLED))		m_bEnabled = Data.To<bool>();

	else if (VAR_TEST_NAME(Name, API_S_NAME))			m_Name = AS_STR(Data);
	//else if (VAR_TEST_NAME(Name, API_S_RULE_GROUP))	m_Grouping = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_RULE_DESCR))		m_Description = AS_STR(Data);

	else if (VAR_TEST_NAME(Name, API_S_EXEC_SIGN_REQ))
	{
		ASTR SignReq = Data;
		if (SignReq == API_S_EXEC_SIGN_REQ_VERIFIED)
			m_SignatureLevel = KphUserAuthority;
		else if (SignReq == API_S_EXEC_SIGN_REQ_MICROSOFT)
			m_SignatureLevel = KphAvAuthority;
		else if (SignReq == API_S_EXEC_SIGN_REQ_TRUSTED)
			m_SignatureLevel = KphMsCodeAuthority;
		else if (SignReq == API_S_EXEC_SIGN_REQ_OTHER)
			m_SignatureLevel = KphUnkAuthority;
		//else if (SignReq == "None")
		//	m_SignatureLevel = KphNoAuthority; // dont require signature
		//else
		//	return STATUS_INVALID_PARAMETER; // todo
	}

	else if (VAR_TEST_NAME(Name, API_S_EXEC_ON_TRUSTED_SPAWN))
	{
		ASTR OnTrustedSpawn = Data;
		if (OnTrustedSpawn == API_S_EXEC_ON_SPAWN_ALLOW)
			m_OnTrustedSpawn = EProgramOnSpawn::eAllow;
		else if (OnTrustedSpawn == API_S_EXEC_ON_SPAWN_BLOCK)
			m_OnTrustedSpawn = EProgramOnSpawn::eBlock;
		else if (OnTrustedSpawn == API_S_EXEC_ON_SPAWN_EJECT)
			m_OnTrustedSpawn = EProgramOnSpawn::eEject;
		//else // todo other
		//	return STATUS_INVALID_PARAMETER;
	}

	else if (VAR_TEST_NAME(Name, API_S_EXEC_ON_SPAWN))
	{
		ASTR OnSpawn = Data;
		if (OnSpawn == API_S_EXEC_ON_SPAWN_ALLOW)
			m_OnSpawn = EProgramOnSpawn::eAllow;
		else if (OnSpawn == API_S_EXEC_ON_SPAWN_BLOCK)
			m_OnSpawn = EProgramOnSpawn::eBlock;
		else if (OnSpawn == API_S_EXEC_ON_SPAWN_EJECT)
			m_OnSpawn = EProgramOnSpawn::eEject;
		//else // todo other
		//	return STATUS_INVALID_PARAMETER;
	}

	else if (VAR_TEST_NAME(Name, API_S_IMAGE_LOAD_PROTECTION))	m_ImageLoadProtection = Data.To<bool>();

	else if (VAR_TEST_NAME(Name, API_S_INTEGRITY_LEVEL))
	{
		ASTR IntegrityLevel = Data;
		if (IntegrityLevel == API_S_INTEGRITY_LEVEL_UNTRUSTED)			m_IntegrityLevel = EIntegrityLevel::eUntrusted;
		else if (IntegrityLevel == API_S_INTEGRITY_LEVEL_LOW)			m_IntegrityLevel = EIntegrityLevel::eLow;
		else if (IntegrityLevel == API_S_INTEGRITY_LEVEL_MEDIUM)		m_IntegrityLevel = EIntegrityLevel::eMedium;
		else if (IntegrityLevel == API_S_INTEGRITY_LEVEL_MEDIUM_PLUS)	m_IntegrityLevel = EIntegrityLevel::eMediumPlus;
		else if (IntegrityLevel == API_S_INTEGRITY_LEVEL_HIGH)			m_IntegrityLevel = EIntegrityLevel::eHigh;
		else if (IntegrityLevel == API_S_INTEGRITY_LEVEL_SYSTEM)		m_IntegrityLevel = EIntegrityLevel::eSystem;
	}

	else if (VAR_TEST_NAME(Name, API_S_ALLOW_DEBUGGING))		m_AllowDebugging = Data.To<bool>();
	else if (VAR_TEST_NAME(Name, API_S_KEEP_ALIVE))			m_KeepAlive = Data.To<bool>();

	else if (VAR_TEST_NAME(Name, API_S_DATA))	        m_Data = Data.Clone();
}
