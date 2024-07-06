

/////////////////////////////////////////////////////////////////////////////
// Index Serializer
//

#ifndef TWEAKS_RO

void CAbstractTweak::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
    Data.Write(API_V_NAME, m_Name);

    Data.Write(API_V_TWEAK_HINT, (uint32)m_Hint);

    ETweakStatus Status = GetStatus();
    Data.Write(API_V_TWEAK_STATUS, (uint32)Status);
}

void CTweakList::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CAbstractTweak::WriteIVariant(Data, Opts);

	CVariant List;
	List.BeginList();
	for (auto I : m_List)
		List.WriteVariant(I->ToVariant(Opts));
	List.Finish();
	Data.WriteVariant(API_V_TWEAK_LIST, List);
}

void CTweakGroup::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweakList::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eGroup);
}

void CTweakSet::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweakList::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eSet);
}

void CTweak::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CAbstractTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_IS_SET, m_Set);
}

void CRegTweak::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eReg);

	Data.Write(API_V_TWEAK_KEY, m_Key);
	Data.Write(API_V_TWEAK_VALUE, m_Value);
	Data.WriteVariant(API_V_TWEAK_DATA, m_Data);
}

void CGpoTweak::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eGpo);

	Data.Write(API_V_TWEAK_KEY, m_Key);
	Data.Write(API_V_TWEAK_VALUE, m_Value);
	Data.WriteVariant(API_V_TWEAK_DATA, m_Data);
}

void CSvcTweak::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eSvc);

	Data.Write(API_V_SVC_TAG, m_SvcTag);
}

void CTaskTweak::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eTask);

	Data.Write(API_V_TWEAK_FOLDER, m_Folder);
	Data.Write(API_V_TWEAK_ENTRY, m_Entry);
}

void CFSTweak::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eFS);

	Data.Write(API_V_TWEAK_PATH, m_PathPattern);
}

void CExecTweak::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eExec);

	Data.Write(API_V_TWEAK_PATH, m_PathPattern);
}

void CFwTweak::WriteIVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteIVariant(Data, Opts);

	Data.Write(API_V_TWEAK_TYPE, (uint32)ETweakType::eFw);

	Data.WriteVariant(API_V_TWEAK_PROG_ID, m_ProgID.ToVariant(Opts));
}

#endif

#ifndef TWEAKS_WO

void CAbstractTweak::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_NAME:			m_Name = Data.AsQStr(); break;
	case API_V_TWEAK_HINT:		m_Hint = (ETweakHint)Data.To<uint32>(); break;
	case API_V_TWEAK_STATUS:	m_Status = (ETweakStatus)Data.To<uint32>(); break;
	}
}

void CTweakList::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_TWEAK_LIST:	ReadList(Data); break;
	default: CAbstractTweak::ReadIValue(Index, Data);
	}
}

void CTweak::ReadIValue(uint32 Index, const XVariant& Data)
{
	CAbstractTweak::ReadIValue(Index, Data);
}

#endif


/////////////////////////////////////////////////////////////////////////////
// Map Serializer
//

#ifndef TWEAKS_RO

void CAbstractTweak::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	Data.Write(API_S_NAME, m_Name);

	switch (m_Hint)
	{
	case ETweakHint::eNone:         Data.Write(API_S_TWEAK_HINT, API_S_TWEAK_HINT_NONE); break;
	case ETweakHint::eRecommended:  Data.Write(API_S_TWEAK_HINT, API_S_TWEAK_HINT_RECOMMENDED); break;
	}

	ETweakStatus Status = GetStatus();
	switch (Status)
	{
	case ETweakStatus::eNotSet:     Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_NOT_SET); break;
	case ETweakStatus::eApplied:    Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_APPLIED); break;
	case ETweakStatus::eSet:        Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_SET); break;
	case ETweakStatus::eMissing:    Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_MISSING); break;
	default:                        Data.Write(API_S_TWEAK_STATUS, API_S_TWEAK_STATUS_GROUP); 
	}
}

void CTweakList::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CAbstractTweak::WriteMVariant(Data, Opts);

	CVariant List;
	List.BeginList();
	for (auto I : m_List)
		List.WriteVariant(I->ToVariant(Opts));
	List.Finish();
	Data.WriteVariant(API_S_TWEAK_LIST, List);
}

void CTweakGroup::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweakList::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_GROUP);
}

void CTweakSet::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweakList::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_SET);
}

void CTweak::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CAbstractTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_IS_SET, m_Set);
}

void CRegTweak::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_REG);

	Data.Write(API_S_TWEAK_KEY, m_Key);
	Data.Write(API_S_TWEAK_VALUE, m_Value);
	Data.WriteVariant(API_S_TWEAK_DATA, m_Data);
}

void CGpoTweak::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_GPO);

	Data.Write(API_S_TWEAK_KEY, m_Key);
	Data.Write(API_S_TWEAK_VALUE, m_Value);
	Data.WriteVariant(API_S_TWEAK_DATA, m_Data);
}

void CSvcTweak::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_SVC);

	Data.Write(API_S_TWEAK_SVC_TAG, m_SvcTag);
}

void CTaskTweak::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_TASK);

	Data.Write(API_S_TWEAK_FOLDER, m_Folder);
	Data.Write(API_S_TWEAK_ENTRY, m_Entry);
}

void CFSTweak::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_FS);

	Data.Write(API_S_TWEAK_PATH, m_PathPattern);
}

void CExecTweak::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_EXEC);

	Data.Write(API_S_TWEAK_PATH, m_PathPattern);
}

void CFwTweak::WriteMVariant(XVariant& Data, const SVarWriteOpt& Opts) const
{
	CTweak::WriteMVariant(Data, Opts);

	Data.Write(API_S_TWEAK_TYPE, API_S_TWEAK_TYPE_FW);

	Data.WriteVariant(API_S_TWEAK_PROG_ID, m_ProgID.ToVariant(Opts));
}

#endif

#ifndef TWEAKS_WO

void CAbstractTweak::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_NAME))					m_Name = Data.AsQStr();

	else if (VAR_TEST_NAME(Name, API_S_TWEAK_HINT))
	{
		QString Hint = Data.AsQStr();
		if (Hint == API_S_TWEAK_HINT_RECOMMENDED)			m_Hint = ETweakHint::eRecommended;
		else												m_Hint = ETweakHint::eNone;
	}

	else if (VAR_TEST_NAME(Name, API_S_TWEAK_STATUS))
	{
		QString Status = Data.AsQStr();
		if (Status == API_S_TWEAK_STATUS_NOT_SET)			m_Status = ETweakStatus::eNotSet;
		else if (Status == API_S_TWEAK_STATUS_SET)			m_Status = ETweakStatus::eSet;
		else if (Status == API_S_TWEAK_STATUS_APPLIED)		m_Status = ETweakStatus::eApplied;
		else if (Status == API_S_TWEAK_STATUS_MISSING)		m_Status = ETweakStatus::eMissing;
		else if (Status == API_S_TWEAK_STATUS_GROUP)		m_Status = ETweakStatus::eGroup;
		else m_Status = ETweakStatus::eNotSet;
	}
}

void CTweakList::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_TWEAK_LIST))
		ReadList(Data);
	else
		CAbstractTweak::ReadMValue(Name, Data);
}

void CTweak::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	CAbstractTweak::ReadMValue(Name, Data);
}

void CTweak::FromVariant(const class XVariant& Tweak)
{
	CAbstractTweak::FromVariant(Tweak);

	if (Tweak.GetType() == VAR_TYPE_MAP)
	{
		ETweakType Type = (ETweakType)Tweak.Find(API_S_TWEAK_TYPE).To<uint32>();
		if (Type == ETweakType::eReg)
		{
			m_Info = GetTypeStr() + ":\n" +
				Tweak.Find(API_S_TWEAK_KEY).AsQStr() + "\n" +
				Tweak.Find(API_S_TWEAK_VALUE).AsQStr() + " = " + Tweak.Find(API_S_TWEAK_DATA).AsQStr();
		}
		else if (m_Type == ETweakType::eGpo)
		{
			m_Info = GetTypeStr() + ":\n" +
				Tweak.Find(API_S_TWEAK_KEY).AsQStr() + "\n" +
				Tweak.Find(API_S_TWEAK_VALUE).AsQStr() + " = " + Tweak.Find(API_S_TWEAK_DATA).AsQStr();
		}
		else if (m_Type == ETweakType::eSvc)
		{
			m_Info = GetTypeStr() + ":\n" +
				Tweak.Find(API_S_TWEAK_SVC_TAG).AsQStr();
		}
		else if (m_Type == ETweakType::eTask)
		{
			m_Info = GetTypeStr() + ":\n" +
				Tweak.Find(API_S_TWEAK_FOLDER).AsQStr() + "\\" + Tweak.Find(API_S_TWEAK_ENTRY).AsQStr();
		}
		else if (m_Type == ETweakType::eFS)
		{
			m_Info = GetTypeStr() + ":\n" +
				Tweak.Find(API_S_TWEAK_PATH).AsQStr();
		}
		else if (m_Type == ETweakType::eExec)
		{
			m_Info = GetTypeStr() + ":\n" +
				Tweak.Find(API_S_TWEAK_PATH).AsQStr();
		}
		else if (m_Type == ETweakType::eFw)
		{
			CProgramID ID;
			ID.FromVariant(Tweak.Find(API_S_TWEAK_PROG_ID));
			m_Info = GetTypeStr() + ":\n" +
				ID.ToString();
		}
	}
	else if (Tweak.GetType() == VAR_TYPE_INDEX)
	{
		ETweakType Type = (ETweakType)Tweak.Find(API_V_TWEAK_TYPE).To<uint32>();
		if (Type == ETweakType::eReg)
		{
			m_Info = GetTypeStr() + ":\n" +
				Tweak.Find(API_V_TWEAK_KEY).AsQStr() + "\n" +
				Tweak.Find(API_V_TWEAK_VALUE).AsQStr() + " = " + Tweak.Find(API_V_TWEAK_DATA).AsQStr();
		}
		else if (m_Type == ETweakType::eGpo)
		{
			m_Info = GetTypeStr() + ":\n" +
				Tweak.Find(API_V_TWEAK_KEY).AsQStr() + "\n" +
				Tweak.Find(API_V_TWEAK_VALUE).AsQStr() + " = " + Tweak.Find(API_V_TWEAK_DATA).AsQStr();
		}
		else if (m_Type == ETweakType::eSvc)
		{
			m_Info = GetTypeStr() + ":\n" +
				Tweak.Find(API_V_TWEAK_SVC_TAG).AsQStr();
		}
		else if (m_Type == ETweakType::eTask)
		{
			m_Info = GetTypeStr() + ":\n" +
				Tweak.Find(API_V_TWEAK_FOLDER).AsQStr() + "\\" + Tweak.Find(API_V_TWEAK_ENTRY).AsQStr();
		}
		else if (m_Type == ETweakType::eFS)
		{
			m_Info = GetTypeStr() + ":\n" +
				Tweak.Find(API_V_TWEAK_PATH).AsQStr();
		}
		else if (m_Type == ETweakType::eExec)
		{
			m_Info = GetTypeStr() + ":\n" +
				Tweak.Find(API_V_TWEAK_PATH).AsQStr();
		}
		else if (m_Type == ETweakType::eFw)
		{
			CProgramID ID;
			ID.FromVariant(Tweak.Find(API_V_TWEAK_PROG_ID));
			m_Info = GetTypeStr() + ":\n" +
				ID.ToString();
		}
	}
}

#endif