

/////////////////////////////////////////////////////////////////////////////
// Index Serializer
//

void CProgramRule::WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const
{
	CGenericRule::WriteIVariant(Rule, Opts);

	Rule.Write(API_V_EXEC_RULE_ACTION, (uint32)m_Type);
	Rule.Write(API_V_FILE_PATH, TO_STR(m_ProgramPath));
	if (m_Type == EExecRuleType::eAllow || m_Type == EExecRuleType::eProtect)
		Rule.Write(API_V_EXEC_SIGN_REQ, (uint32)m_SignatureLevel);
	Rule.Write(API_V_EXEC_ON_TRUSTED_SPAWN, (uint32)m_OnTrustedSpawn);
	Rule.Write(API_V_EXEC_ON_SPAWN, (uint32)m_OnSpawn);
	Rule.Write(API_V_IMAGE_LOAD_PROTECTION, m_ImageLoadProtection);
}

void CProgramRule::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_EXEC_RULE_ACTION: m_Type = (EExecRuleType)Data.To<uint32>(); break;
	case API_V_FILE_PATH: m_ProgramPath = AS_STR(Data); break;
	case API_V_EXEC_SIGN_REQ: m_SignatureLevel = (KPH_VERIFY_AUTHORITY)Data.To<uint32>(); break;
	case API_V_EXEC_ON_TRUSTED_SPAWN: m_OnTrustedSpawn = (EProgramOnSpawn)Data.To<uint32>(); break;
	case API_V_EXEC_ON_SPAWN: m_OnSpawn = (EProgramOnSpawn)Data.To<uint32>(); break;
	case API_V_IMAGE_LOAD_PROTECTION: m_ImageLoadProtection = Data.To<bool>(); break;
	default: CGenericRule::ReadIValue(Index, Data);
	}
}


/////////////////////////////////////////////////////////////////////////////
// Map Serializer
//

void CProgramRule::WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const
{
	CGenericRule::WriteMVariant(Rule, Opts);

	switch (m_Type)
	{
	case EExecRuleType::eAllow:		Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_ALLOW); break;
	case EExecRuleType::eBlock:		Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_BLOCK); break;
	case EExecRuleType::eProtect:	Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_PROTECT); break;
	case EExecRuleType::eIsolate:	Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_ISOLATE); break;
		// todo other:
	}

	Rule.Write(API_S_FILE_PATH, TO_STR(m_ProgramPath));

	if (m_Type == EExecRuleType::eAllow || m_Type == EExecRuleType::eProtect)
	{
		switch (m_SignatureLevel) 
		{
		case KphDevAuthority:
		case KphUserAuthority:	Rule.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_VERIFIED); break;

		case KphMsAuthority:
		case KphAvAuthority:	Rule.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_MICROSOFT); break;

		case KphMsCodeAuthority:Rule.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_TRUSTED); break;

		case KphUnkAuthority:	Rule.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_OTHER); break;

		case KphUntestedAuthority:
		case KphNoAuthority:	Rule.Write(API_S_EXEC_SIGN_REQ, API_S_EXEC_SIGN_REQ_NONE); break;
		}
	}

	switch (m_OnTrustedSpawn)
	{
	case EProgramOnSpawn::eAllow:	Rule.Write(API_S_EXEC_ON_TRUSTED_SPAWN, API_S_EXEC_ON_SPAWN_ALLOW); break;
	case EProgramOnSpawn::eBlock:	Rule.Write(API_S_EXEC_ON_TRUSTED_SPAWN, API_S_EXEC_ON_SPAWN_BLOCK); break;
	case EProgramOnSpawn::eEject:	Rule.Write(API_S_EXEC_ON_TRUSTED_SPAWN, API_S_EXEC_ON_SPAWN_EJECT); break;
	}

	switch(m_OnSpawn)
	{
	case EProgramOnSpawn::eAllow:	Rule.Write(API_S_EXEC_ON_SPAWN, API_S_EXEC_ON_SPAWN_ALLOW); break;
	case EProgramOnSpawn::eBlock:	Rule.Write(API_S_EXEC_ON_SPAWN, API_S_EXEC_ON_SPAWN_BLOCK); break;
	case EProgramOnSpawn::eEject:	Rule.Write(API_S_EXEC_ON_SPAWN, API_S_EXEC_ON_SPAWN_EJECT); break;
	}

	Rule.Write(API_S_IMAGE_LOAD_PROTECTION, m_ImageLoadProtection);
}

void CProgramRule::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_EXEC_RULE_ACTION))
	{
		ASTR Type = Data;
		if (Type == API_S_EXEC_RULE_ACTION_ALLOW)
			m_Type = EExecRuleType::eAllow;
		else if (Type == API_S_EXEC_RULE_ACTION_BLOCK)
			m_Type = EExecRuleType::eBlock;
		else if (Type == API_S_EXEC_RULE_ACTION_PROTECT)
			m_Type = EExecRuleType::eProtect;
		else if (Type == API_S_EXEC_RULE_ACTION_ISOLATE)
			m_Type = EExecRuleType::eIsolate;
		//else // todo other
		//	return STATUS_INVALID_PARAMETER;
	}

	else if (VAR_TEST_NAME(Name, API_S_FILE_PATH))
	{
		m_ProgramPath = AS_STR(Data);
	}

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

	else 
		CGenericRule::ReadMValue(Name, Data);
}
