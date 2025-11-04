

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
		Enclave.WriteVariant(API_V_GUID, m_Guid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, Enclave.Allocator()));
#endif
	Enclave.Write(API_V_ENABLED, m_bEnabled);

	//Enclave.Write(API_S_LOCKDOWN, m_bLockdown);

	Enclave.WriteEx(API_V_NAME, TO_STR(m_Name));
	//Enclave.WriteEx(API_V_RULE_GROUP, TO_STR(m_Grouping));
	Enclave.WriteEx(API_V_RULE_DESCR, TO_STR(m_Description));

#ifdef KERNEL_MODE
	if(!IS_EMPTY(m_VolumeGuid))
		Enclave.Write(API_V_VOLUME_GUID, TO_STR(m_VolumeGuid));
#else
	if(!m_VolumeGuid.IsNull())
		Enclave.WriteVariant(API_V_VOLUME_GUID, m_VolumeGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, Enclave.Allocator()));
#endif

	Enclave.Write(API_V_USE_SCRIPT, m_bUseScript);
	Enclave.WriteEx(API_V_SCRIPT, TO_STR_A(m_Script));

	Enclave.Write(API_V_DLL_INJECT_MODE, (uint32)m_DllMode);

	Enclave.Write(API_V_EXEC_ALLOWED_SIGNERS, m_AllowedSignatures.Value);
	VariantWriter Collections(Enclave.Allocator());
	Collections.BeginList();
	for(const auto& Collection : m_AllowedCollections)
		Collections.WriteEx(TO_STR(Collection));
	Enclave.WriteVariant(API_V_EXEC_ALLOWED_COLLECTIONS, Collections.Finish());

	Enclave.Write(API_V_EXEC_ON_TRUSTED_SPAWN, (uint32)m_OnTrustedSpawn);
	Enclave.Write(API_V_EXEC_ON_SPAWN, (uint32)m_OnSpawn);
	Enclave.Write(API_V_IMAGE_LOAD_PROTECTION, m_ImageLoadProtection);
	Enclave.Write(API_V_IMAGE_COHERENCY_CHECKING, m_ImageCoherencyChecking);

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
	case API_V_GUID:		if (IS_EMPTY(m_Guid)) m_Guid = AS_STR(Data); break;
#else
	case API_V_GUID:		if (m_Guid.IsNull()) m_Guid.FromVariant(Data); break;
#endif
	case API_V_ENABLED:		m_bEnabled = Data.To<bool>(); break;

	//case API_V_LOCKDOWN:	m_bLockdown = Data.To<bool>(); break;

	case API_V_NAME:		m_Name = AS_STR(Data); break;
		//case API_V_RULE_GROUP:	m_Grouping = AS_STR(Data); break;
	case API_V_RULE_DESCR:	m_Description = AS_STR(Data); break;

#ifdef KERNEL_MODE
	case API_V_VOLUME_GUID:	m_VolumeGuid = AS_STR(Data); break;
#else
	case API_V_VOLUME_GUID:	m_VolumeGuid.FromVariant(Data); break;
#endif

	case API_V_USE_SCRIPT:	m_bUseScript = Data.To<bool>(); break;
	case API_V_SCRIPT:		AS_STR_A(m_Script, Data); break;

	case API_V_DLL_INJECT_MODE: m_DllMode = (EExecDllMode)Data.To<uint32>(); break;

	case API_V_EXEC_ALLOWED_SIGNERS: m_AllowedSignatures.Value = Data.To<uint32>(); break;
	case API_V_EXEC_ALLOWED_COLLECTIONS:
		LIST_CLEAR(m_AllowedCollections);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_AllowedCollections, AS_STR(Data[i]));
		break;
	
	case API_V_EXEC_ON_TRUSTED_SPAWN: m_OnTrustedSpawn = (EProgramOnSpawn)Data.To<uint32>(); break;
	case API_V_EXEC_ON_SPAWN: m_OnSpawn = (EProgramOnSpawn)Data.To<uint32>(); break;
	case API_V_IMAGE_LOAD_PROTECTION: m_ImageLoadProtection = Data.To<bool>(); break;
	case API_V_IMAGE_COHERENCY_CHECKING: m_ImageCoherencyChecking = Data.To<bool>(); break;

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
	if (!IS_EMPTY(m_Guid))
		Enclave.Write(API_S_GUID, TO_STR(m_Guid));
#else
	if (!m_Guid.IsNull())
		Enclave.WriteVariant(API_S_GUID, m_Guid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, Enclave.Allocator()));
#endif
	Enclave.Write(API_S_ENABLED, m_bEnabled);

	//Enclave.Write(API_S_LOCKDOWN, m_bLockdown);

	Enclave.WriteEx(API_S_NAME, TO_STR(m_Name));
	//Enclave.WriteEx(API_S_RULE_GROUP, TO_STR(m_Grouping));
	Enclave.WriteEx(API_S_RULE_DESCR, TO_STR(m_Description));

#ifdef KERNEL_MODE
	if (!IS_EMPTY(m_VolumeGuid))
		Enclave.Write(API_S_VOLUME_GUID, TO_STR(m_VolumeGuid));
#else
	if (!m_VolumeGuid.IsNull())
		Enclave.WriteVariant(API_S_VOLUME_GUID, m_VolumeGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, Enclave.Allocator()));
#endif

	Enclave.Write(API_S_USE_SCRIPT, m_bUseScript);
	Enclave.WriteEx(API_S_SCRIPT, TO_STR_A(m_Script));

	switch (m_DllMode)
	{
	case EExecDllMode::eDefault:	Enclave.Write(API_S_DLL_INJECT_MODE, API_S_EXEC_DLL_MODE_DEFAULT); break;
	case EExecDllMode::eInjectLow:	Enclave.Write(API_S_DLL_INJECT_MODE, API_S_EXEC_DLL_MODE_INJECT_LOW); break;
	case EExecDllMode::eInjectHigh:	Enclave.Write(API_S_DLL_INJECT_MODE, API_S_EXEC_DLL_MODE_INJECT_HIGH); break;
	case EExecDllMode::eDisabled:	Enclave.Write(API_S_DLL_INJECT_MODE, API_S_EXEC_DLL_MODE_DISABLED); break;
	}

	VariantWriter Signers(Enclave.Allocator());
	Signers.BeginList();
	if (m_AllowedSignatures.Windows)
		Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_WINDOWS);
	if (m_AllowedSignatures.Microsoft)
		Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_MICROSOFT);
	if (m_AllowedSignatures.Antimalware)
		Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_ANTIMALWARE);
	if (m_AllowedSignatures.Authenticode)
		Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_AUTHENTICODE);
	if (m_AllowedSignatures.Store)
		Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_STORE);
	if (m_AllowedSignatures.Developer)
		Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_DEVELOPER);
	if (m_AllowedSignatures.UserSign)
		Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_USER_SIGN);
	if (m_AllowedSignatures.UserDB)
		Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_USER);
	if (m_AllowedSignatures.EnclaveDB)
		Signers.Write(API_S_EXEC_ALLOWED_SIGNERS_ENCLAVE);
	for (const auto& Collection : m_AllowedCollections)
		Signers.WriteEx(TO_STR(Collection));
	Enclave.WriteVariant(API_S_EXEC_ALLOWED_SIGNERS, Signers.Finish());

	switch (m_OnTrustedSpawn)
	{
	case EProgramOnSpawn::eAllow:	Enclave.Write(API_S_EXEC_ON_TRUSTED_SPAWN, API_S_EXEC_ON_SPAWN_ALLOW); break;
	case EProgramOnSpawn::eBlock:	Enclave.Write(API_S_EXEC_ON_TRUSTED_SPAWN, API_S_EXEC_ON_SPAWN_BLOCK); break;
	case EProgramOnSpawn::eEject:	Enclave.Write(API_S_EXEC_ON_TRUSTED_SPAWN, API_S_EXEC_ON_SPAWN_EJECT); break;
	}

	switch (m_OnSpawn)
	{
	case EProgramOnSpawn::eAllow:	Enclave.Write(API_S_EXEC_ON_SPAWN, API_S_EXEC_ON_SPAWN_ALLOW); break;
	case EProgramOnSpawn::eBlock:	Enclave.Write(API_S_EXEC_ON_SPAWN, API_S_EXEC_ON_SPAWN_BLOCK); break;
	case EProgramOnSpawn::eEject:	Enclave.Write(API_S_EXEC_ON_SPAWN, API_S_EXEC_ON_SPAWN_EJECT); break;
	}

	Enclave.Write(API_S_IMAGE_LOAD_PROTECTION, m_ImageLoadProtection);
	Enclave.Write(API_S_IMAGE_COHERENCY_CHECKING, m_ImageCoherencyChecking);

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

	//else if (VAR_TEST_NAME(Name, API_S_LOCKDOWN))		m_bLockdown = Data.To<bool>();

	else if (VAR_TEST_NAME(Name, API_S_NAME))			m_Name = AS_STR(Data);
	//else if (VAR_TEST_NAME(Name, API_S_RULE_GROUP))	m_Grouping = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_RULE_DESCR))		m_Description = AS_STR(Data);

#ifdef KERNEL_MODE
	if (VAR_TEST_NAME(Name, API_S_VOLUME_GUID))			m_VolumeGuid = AS_STR(Data);
#else
	if (VAR_TEST_NAME(Name, API_S_VOLUME_GUID))			m_VolumeGuid.FromVariant(Data);
#endif

	else if (VAR_TEST_NAME(Name, API_S_USE_SCRIPT))		m_bUseScript = Data.To<bool>();
	else if (VAR_TEST_NAME(Name, API_S_SCRIPT))			AS_STR_A(m_Script, Data);

	else if (VAR_TEST_NAME(Name, API_S_DLL_INJECT_MODE))
	{
		ASTR DllMode = Data;
		if (DllMode == API_S_EXEC_DLL_MODE_DEFAULT)
			m_DllMode = EExecDllMode::eDefault;
		else if (DllMode == API_S_EXEC_DLL_MODE_INJECT_LOW)
			m_DllMode = EExecDllMode::eInjectLow;
		else if (DllMode == API_S_EXEC_DLL_MODE_INJECT_HIGH)
			m_DllMode = EExecDllMode::eInjectHigh;
		else if (DllMode == API_S_EXEC_DLL_MODE_DISABLED)
			m_DllMode = EExecDllMode::eDisabled;
		//else // todo other
		//	return STATUS_INVALID_PARAMETER;
	}

	else if (VAR_TEST_NAME(Name, API_S_EXEC_SIGN_REQ))
	{
		ASTR SignReq = Data;
		if (SignReq == API_S_EXEC_SIGN_REQ_VERIFIED)
			m_AllowedSignatures.UserDB = m_AllowedSignatures.UserSign = TRUE;
		else if (SignReq == API_S_EXEC_SIGN_REQ_MICROSOFT || SignReq == API_S_EXEC_SIGN_REQ_TRUSTED)
			m_AllowedSignatures.UserDB = m_AllowedSignatures.UserSign = m_AllowedSignatures.Windows = m_AllowedSignatures.Microsoft = m_AllowedSignatures.Antimalware = TRUE;
	}

	else if (VAR_TEST_NAME(Name, API_S_EXEC_ALLOWED_SIGNERS))
	{
		m_AllowedSignatures.Value = 0;
		LIST_CLEAR(m_AllowedCollections);
		for (uint32 i = 0; i < Data.Count(); i++) {
			ASTR SignReq = Data[i];
			if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_WINDOWS || SignReq == "[MS]")			m_AllowedSignatures.Windows = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_MICROSOFT)	m_AllowedSignatures.Microsoft = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_ANTIMALWARE)	m_AllowedSignatures.Antimalware = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_AUTHENTICODE)	m_AllowedSignatures.Authenticode = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_STORE)		m_AllowedSignatures.Store = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_DEVELOPER)	m_AllowedSignatures.Developer = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_USER_SIGN)	m_AllowedSignatures.UserSign = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_USER)			m_AllowedSignatures.UserDB = TRUE;
			else if(SignReq == API_S_EXEC_ALLOWED_SIGNERS_ENCLAVE)		m_AllowedSignatures.EnclaveDB = TRUE;
			else
				LIST_APPEND(m_AllowedCollections, AS_STR(Data[i]));
		}
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
	else if (VAR_TEST_NAME(Name, API_S_IMAGE_COHERENCY_CHECKING))	m_ImageCoherencyChecking = Data.To<bool>();

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
