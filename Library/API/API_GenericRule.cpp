

/////////////////////////////////////////////////////////////////////////////
// Index Serializer
//

void CGenericRule::WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const
{
	if(!IS_EMPTY(m_Guid))
		Rule.Write(API_V_RULE_GUID, TO_STR(m_Guid));

	Rule.Write(API_V_RULE_ENABLED, m_bEnabled);
	Rule.Write(API_V_RULE_TEMP, m_bTemporary);

	Rule.Write(API_V_NAME, TO_STR(m_Name));
	//Rule.Write(API_V_RULE_GROUP, TO_STR(m_Grouping));
	Rule.Write(API_V_RULE_DESCR, TO_STR(m_Description));

	Rule.WriteVariant(API_V_RULE_DATA, m_Data);
}

void CGenericRule::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch(Index)
	{
	case API_V_RULE_GUID:		m_Guid = AS_STR(Data); break;

	case API_V_RULE_ENABLED:	m_bEnabled = Data.To<bool>(); break;
	case API_V_RULE_TEMP:		m_bTemporary = Data.To<bool>(); break;

	case API_V_NAME:			m_Name = AS_STR(Data); break;
	//case API_V_RULE_GROUP:		m_Grouping = AS_STR(Data); break;
	case API_V_RULE_DESCR:		m_Description = AS_STR(Data); break;

	case API_V_RULE_DATA:		m_Data = Data; break;

	default: break;
	}
}


/////////////////////////////////////////////////////////////////////////////
// Map Serializer
//

void CGenericRule::WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const
{
	if(!IS_EMPTY(m_Guid))
		Rule.Write(API_S_RULE_GUID, TO_STR(m_Guid));

	Rule.Write(API_S_RULE_ENABLED, m_bEnabled);
	Rule.Write(API_S_RULE_TEMP, m_bTemporary);

	Rule.Write(API_S_NAME, TO_STR(m_Name));
	//Rule.Write(API_S_RULE_GROUP, TO_STR(m_Grouping));
	Rule.Write(API_S_RULE_DESCR, TO_STR(m_Description));

	Rule.WriteVariant(API_S_RULE_DATA, m_Data);
}

void CGenericRule::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_RULE_GUID))		        m_Guid = AS_STR(Data);

	else if (VAR_TEST_NAME(Name, API_S_RULE_ENABLED))		m_bEnabled = Data.To<bool>();
	else if (VAR_TEST_NAME(Name, API_S_RULE_TEMP))			m_bTemporary = Data.To<bool>();

	else if (VAR_TEST_NAME(Name, API_S_NAME))				m_Name = AS_STR(Data);
	//else if (VAR_TEST_NAME(Name, API_S_RULE_GROUP))		m_Grouping = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_RULE_DESCR))			m_Description = AS_STR(Data);

	else if (VAR_TEST_NAME(Name, API_S_RULE_DATA))	        m_Data = Data;

	// else unknown tag
}