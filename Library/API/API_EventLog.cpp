

/////////////////////////////////////////////////////////////////////////////
// CEventLog
//

void CEventLogEntry::WriteIVariant(VariantWriter& Entry, const SVarWriteOpt& Opts) const
{
	if((Opts.Flags & SVarWriteOpt::eSaveToFile) == 0)
		Entry.Write(API_V_EVENT_REF, (uint64)this);
	Entry.Write(API_V_EVENT_TIME_STAMP, m_TimeStamp);
	Entry.Write(API_V_EVENT_LEVEL, (int)m_EventLevel);
	Entry.Write(API_V_EVENT_TYPE, m_EventType);
	Entry.WriteVariant(API_V_EVENT_DATA, m_EventData);
}

void CEventLogEntry::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
#ifdef LOG_GUI
	case API_V_EVENT_REF:			m_UID = Data.To<uint64>(); break;	
#endif
	case API_V_EVENT_TIME_STAMP:	m_TimeStamp = Data.To<uint64>(); break;
	case API_V_EVENT_LEVEL:			m_EventLevel = (ELogLevels)Data.To<sint32>(); break;
	case API_V_EVENT_TYPE:			m_EventType = Data.To<sint32>(); break;
	case API_V_EVENT_DATA:			m_EventData = Data; break;
	}
}

/////////////////////////////////////////////////////////////////////////////

//void CEventLogEntry::WriteMVariant(VariantWriter& Entry, const SVarWriteOpt& Opts) const
//{
//	Entry.Write(API_S_EVENT_TIME_STAMP, m_TimeStamp);
//	Entry.Write(API_S_EVENT_LEVEL, m_EventLevel);
//	Entry.Write(API_S_EVENT_TYPE, m_EventType);
//	Entry.WriteVariant(API_S_EVENT_DATA, m_EventData);
//}
//
//void CEventLogEntry::ReadMValue(const SVarName& Name, const XVariant& Data)
//{
//	if (VAR_TEST_NAME(Name, API_S_EVENT_TIME_STAMP))
//		m_TimeStamp = Data.To<uint64>();
//	else if (VAR_TEST_NAME(Name, API_S_EVENT_LEVEL))
//		m_EventLevel = Data.To<sint32>();
//	else if (VAR_TEST_NAME(Name, API_S_EVENT_TYPE))
//		m_EventType = Data.To<sint32>();
//	else if (VAR_TEST_NAME(Name, API_S_EVENT_DATA))
//		m_EventData = Data;
//}