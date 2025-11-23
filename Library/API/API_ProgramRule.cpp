

/////////////////////////////////////////////////////////////////////////////
// CProgramRule
//

void CProgramRule::WriteIVariant(VariantWriter& Rule, const SVarWriteOpt& Opts) const
{
	CGenericRule::WriteIVariant(Rule, Opts);

	Rule.Write(API_V_EXEC_RULE_ACTION, (uint32)m_Type);

	Rule.Write(API_V_USE_SCRIPT, m_bUseScript);
	Rule.WriteEx(API_V_SCRIPT, TO_STR_A(m_Script));

	Rule.Write(API_V_DLL_INJECT_MODE, (uint32)m_DllMode);

	Rule.WriteEx(API_V_FILE_PATH, TO_STR(m_ProgramPath));
#ifdef SAVE_NT_PATHS
	if(Opts.Flags & SVarWriteOpt::eSaveNtPaths) {
		Rule.Write(API_V_FILE_NT_PATH, TO_STR(m_PathPattern.Get()));
	}
#endif
	if (m_Type == EExecRuleType::eAllow || m_Type == EExecRuleType::eProtect)
	{
		Rule.Write(API_V_EXEC_ALLOWED_SIGNERS, m_AllowedSignatures.Value);
		VariantWriter Collections(Rule.Allocator());
		Collections.BeginList();
		for(const auto& Collection : m_AllowedCollections)
			Collections.WriteEx(TO_STR(Collection));
		Rule.WriteVariant(API_V_EXEC_ALLOWED_COLLECTIONS, Collections.Finish());
	}
	Rule.Write(API_V_IMAGE_LOAD_PROTECTION, m_ImageLoadProtection);
	Rule.Write(API_V_IMAGE_COHERENCY_CHECKING, m_ImageCoherencyChecking);

	VariantWriter Children(Rule.Allocator());
	Children.BeginList();
	for(const auto& Child : m_AllowedChildren)
		Children.WriteEx(TO_STR(Child));
	Rule.WriteVariant(API_V_EXEC_ALLOWED_CHILDREN, Children.Finish());

	VariantWriter Parents(Rule.Allocator());
	Parents.BeginList();
	for(const auto& Parent : m_AllowedParents)
		Parents.WriteEx(TO_STR(Parent));
	Rule.WriteVariant(API_V_EXEC_ALLOWED_PARENTS, Parents.Finish());

	VariantWriter BlockedChildren(Rule.Allocator());
	BlockedChildren.BeginList();
	for(const auto& Child : m_BlockedChildren)
		BlockedChildren.WriteEx(TO_STR(Child));
	Rule.WriteVariant(API_V_EXEC_BLOCKED_CHILDREN, BlockedChildren.Finish());

	VariantWriter BlockedParents(Rule.Allocator());
	BlockedParents.BeginList();
	for(const auto& Parent : m_BlockedParents)
		BlockedParents.WriteEx(TO_STR(Parent));
	Rule.WriteVariant(API_V_EXEC_BLOCKED_PARENTS, BlockedParents.Finish());

}

void CProgramRule::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_EXEC_RULE_ACTION: m_Type = (EExecRuleType)Data.To<uint32>(); break;
	
	case API_V_USE_SCRIPT: m_bUseScript = Data.To<bool>(); break;
	case API_V_SCRIPT: AS_STR_A(m_Script, Data); break;

	case API_V_DLL_INJECT_MODE: m_DllMode = (EExecDllMode)Data.To<uint32>(); break;

	case API_V_FILE_PATH: m_ProgramPath = AS_STR(Data); break;
#ifdef LOAD_NT_PATHS
	case API_V_FILE_NT_PATH: m_ProgramNtPath = AS_STR(Data); break;
#endif
	case API_V_EXEC_ALLOWED_SIGNERS: m_AllowedSignatures.Value = Data.To<uint32>(); break;
	case API_V_EXEC_ALLOWED_COLLECTIONS:
		LIST_CLEAR(m_AllowedCollections);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_AllowedCollections, AS_STR(Data[i]));
		break;
	case API_V_IMAGE_LOAD_PROTECTION: m_ImageLoadProtection = Data.To<bool>(); break;
	case API_V_IMAGE_COHERENCY_CHECKING: m_ImageCoherencyChecking = Data.To<bool>(); break;
	case API_V_EXEC_ALLOWED_CHILDREN:
		LIST_CLEAR(m_AllowedChildren);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_AllowedChildren, AS_STR(Data[i]));
		break;
	case API_V_EXEC_ALLOWED_PARENTS:
		LIST_CLEAR(m_AllowedParents);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_AllowedParents, AS_STR(Data[i]));
		break;
	case API_V_EXEC_BLOCKED_CHILDREN:
		LIST_CLEAR(m_BlockedChildren);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_BlockedChildren, AS_STR(Data[i]));
		break;
	case API_V_EXEC_BLOCKED_PARENTS:
		LIST_CLEAR(m_BlockedParents);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_BlockedParents, AS_STR(Data[i]));
		break;
	default: CGenericRule::ReadIValue(Index, Data);
	}
}

/////////////////////////////////////////////////////////////////////////////

void CProgramRule::WriteMVariant(VariantWriter& Rule, const SVarWriteOpt& Opts) const
{
	CGenericRule::WriteMVariant(Rule, Opts);

	switch (m_Type)
	{
	case EExecRuleType::eAllow:		Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_ALLOW); break;
	case EExecRuleType::eBlock:		Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_BLOCK); break;
	case EExecRuleType::eProtect:	Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_PROTECT); break;
	case EExecRuleType::eIsolate:	Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_ISOLATE); break;
	case EExecRuleType::eAudit:		Rule.Write(API_S_EXEC_RULE_ACTION, API_S_EXEC_RULE_ACTION_AUDIT); break;
		// todo other:
	}


	Rule.Write(API_S_USE_SCRIPT, m_bUseScript);
	Rule.WriteEx(API_S_SCRIPT, TO_STR(m_Script));

	switch (m_DllMode)
	{
	case EExecDllMode::eDefault:	Rule.Write(API_S_DLL_INJECT_MODE, API_S_EXEC_DLL_MODE_DEFAULT); break;
	case EExecDllMode::eInjectLow:	Rule.Write(API_S_DLL_INJECT_MODE, API_S_EXEC_DLL_MODE_INJECT_LOW); break;
	case EExecDllMode::eInjectHigh:	Rule.Write(API_S_DLL_INJECT_MODE, API_S_EXEC_DLL_MODE_INJECT_HIGH); break;
	case EExecDllMode::eDisabled:	Rule.Write(API_S_DLL_INJECT_MODE, API_S_EXEC_DLL_MODE_DISABLED); break;
	}

	Rule.WriteEx(API_S_FILE_PATH, TO_STR(m_ProgramPath));
#ifdef SAVE_NT_PATHS
	if (Opts.Flags & SVarWriteOpt::eSaveNtPaths) {
		Rule.Write(API_S_FILE_NT_PATH, TO_STR(m_PathPattern.Get()));
	}
#endif

	if (m_Type == EExecRuleType::eAllow || m_Type == EExecRuleType::eProtect)
	{
		VariantWriter Signers(Rule.Allocator());
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
		Rule.WriteVariant(API_S_EXEC_ALLOWED_SIGNERS, Signers.Finish());
	}

	Rule.Write(API_S_IMAGE_LOAD_PROTECTION, m_ImageLoadProtection);
	Rule.Write(API_S_IMAGE_COHERENCY_CHECKING, m_ImageCoherencyChecking);

	VariantWriter Children(Rule.Allocator());
	Children.BeginList();
	for(const auto& Child : m_AllowedChildren)
		Children.WriteEx(TO_STR(Child));
	Rule.WriteVariant(API_S_EXEC_ALLOWED_CHILDREN, Children.Finish());

	VariantWriter Parents(Rule.Allocator());
	Parents.BeginList();
	for(const auto& Parent : m_AllowedParents)
		Parents.WriteEx(TO_STR(Parent));
	Rule.WriteVariant(API_S_EXEC_ALLOWED_PARENTS, Parents.Finish());

	VariantWriter BlockedChildren(Rule.Allocator());
	BlockedChildren.BeginList();
	for(const auto& Child : m_BlockedChildren)
		BlockedChildren.WriteEx(TO_STR(Child));
	Rule.WriteVariant(API_S_EXEC_BLOCKED_CHILDREN, BlockedChildren.Finish());

	VariantWriter BlockedParents(Rule.Allocator());
	BlockedParents.BeginList();
	for(const auto& Parent : m_BlockedParents)
		BlockedParents.WriteEx(TO_STR(Parent));
	Rule.WriteVariant(API_S_EXEC_BLOCKED_PARENTS, BlockedParents.Finish());
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
		else if (Type == API_S_EXEC_RULE_ACTION_AUDIT)
			m_Type = EExecRuleType::eAudit;
		//else // todo other
		//	return STATUS_INVALID_PARAMETER;
	}

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

	else if (VAR_TEST_NAME(Name, API_S_FILE_PATH))
	{
		m_ProgramPath = AS_STR(Data);
	}
#ifdef LOAD_NT_PATHS
	else if (VAR_TEST_NAME(Name, API_S_FILE_NT_PATH))	m_ProgramNtPath = AS_STR(Data);
#endif

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

	else if (VAR_TEST_NAME(Name, API_S_IMAGE_LOAD_PROTECTION))	m_ImageLoadProtection = Data.To<bool>();
	else if (VAR_TEST_NAME(Name, API_S_IMAGE_COHERENCY_CHECKING))	m_ImageCoherencyChecking = Data.To<bool>();

	else if (VAR_TEST_NAME(Name, API_S_EXEC_ALLOWED_CHILDREN))
	{
		LIST_CLEAR(m_AllowedChildren);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_AllowedChildren, AS_STR(Data[i]));
	}

	else if (VAR_TEST_NAME(Name, API_S_EXEC_ALLOWED_PARENTS))
	{
		LIST_CLEAR(m_AllowedParents);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_AllowedParents, AS_STR(Data[i]));
	}

	else if (VAR_TEST_NAME(Name, API_S_EXEC_BLOCKED_CHILDREN))
	{
		LIST_CLEAR(m_BlockedChildren);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_BlockedChildren, AS_STR(Data[i]));
	}

	else if (VAR_TEST_NAME(Name, API_S_EXEC_BLOCKED_PARENTS))
	{
		LIST_CLEAR(m_BlockedParents);
		for (uint32 i = 0; i < Data.Count(); i++)
			LIST_APPEND(m_BlockedParents, AS_STR(Data[i]));
	}

	else
		CGenericRule::ReadMValue(Name, Data);
}
