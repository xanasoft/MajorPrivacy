

/////////////////////////////////////////////////////////////////////////////
// CProgramItem 
//

void CProgramItem::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	Data.Write(API_V_PROG_UID, m_UID);
	Data.Write(API_V_PROG_TYPE, (uint32)GetType());

	Data.WriteVariant(API_V_PROG_ID, m_ID.ToVariant(Opts));

	Data.WriteEx(API_V_NAME, TO_STR(m_Name));
	Data.WriteEx(API_V_ICON, TO_STR(m_IconFile));
	Data.WriteEx(API_V_INFO, TO_STR(m_Info));

	Data.Write(API_V_EXEC_TRACE, (int)m_ExecTrace);
	Data.Write(API_V_RES_TRACE, (int)m_ResTrace);
	Data.Write(API_V_NET_TRACE, (int)m_NetTrace);
	Data.Write(API_V_SAVE_TRACE, (int)m_SaveTrace);

#ifdef PROG_SVC
	Data.WriteVariant(API_V_FW_RULES, CollectFwRules());
	Data.WriteVariant(API_V_PROGRAM_RULES, CollectProgRules());
	Data.WriteVariant(API_V_ACCESS_RULES, CollectResRules());

	if (Opts.Flags & SVarWriteOpt::eSaveToFile)
	{
	}
	else
	{
		Data.Write(API_V_PROG_ITEM_MISSING, IsMissing());
	}

	Data.Write(API_V_LOG_MEM_USAGE, GetLogMemUsage());
#endif
}

void CProgramItem::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_PROG_UID: break; // dont read UID

	case API_V_PROG_ID:			m_ID.FromVariant(Data); break;

	case API_V_NAME:			m_Name = AS_STR(Data); break;
	case API_V_ICON:			m_IconFile = AS_STR(Data); break;
	case API_V_INFO:			m_Info = AS_STR(Data); break;

	case API_V_EXEC_TRACE:		m_ExecTrace = (ETracePreset)Data.To<int>(); break;
	case API_V_RES_TRACE:		m_ResTrace = (ETracePreset)Data.To<int>(); break;
	case API_V_NET_TRACE:		m_NetTrace = (ETracePreset)Data.To<int>(); break;
	case API_V_SAVE_TRACE:		m_SaveTrace = (ESavePreset)Data.To<int>(); break;

#ifdef PROG_GUI
	case API_V_FW_RULES:		m_FwRuleIDs = QFlexGuid::ReadVariantSet(Data); break;
	case API_V_PROGRAM_RULES:	m_ProgRuleIDs = QFlexGuid::ReadVariantSet(Data); break;
	case API_V_ACCESS_RULES:	m_ResRuleIDs = QFlexGuid::ReadVariantSet(Data); break;

	case API_V_PROG_ITEM_MISSING:	m_IsMissing = Data; break;

	case API_V_LOG_MEM_USAGE:		m_LogMemoryUsed = Data; break;
#else
	case API_V_FW_RULES: break;
	case API_V_PROGRAM_RULES: break;
	case API_V_ACCESS_RULES: break;
#endif

	default: break;
	}
}

/////////////////////////////////////////////////////////////////////////////

const char* TracePresetToStr(ETracePreset Trace)
{
	switch (Trace)
	{
	default:
	case ETracePreset::eDefault:		return API_S_TRACE_DEFAULT;
	case ETracePreset::eTrace:			return API_S_TRACE_ONLY;
	case ETracePreset::eNoTrace:		return API_S_TRACE_NONE;
	case ETracePreset::ePrivate:		return API_S_TRACE_PRIVATE;
	}
}

const char* SavePresetToStr(ESavePreset Trace)
{
	switch (Trace)
	{
	default:
	case ESavePreset::eDefault:			return API_S_SAVE_TRACE_DEFAULT;
	case ESavePreset::eSaveToDisk:		return API_S_SAVE_TRACE_TO_DISK;
	case ESavePreset::eDontSave:		return API_S_SAVE_TRACE_TO_RAM;
	}
}

void CProgramItem::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	Data.Write(API_S_PROG_UID, m_UID);
	Data.WriteEx(API_S_PROG_TYPE, CProgramID::TypeToStr(GetType()));

	Data.WriteVariant(API_S_PROG_ID, m_ID.ToVariant(Opts));

	Data.WriteEx(API_S_NAME, TO_STR(m_Name));
	Data.WriteEx(API_S_ICON, TO_STR(m_IconFile));
	Data.WriteEx(API_S_INFO, TO_STR(m_Info));

	Data.Write(API_S_EXEC_TRACE, TracePresetToStr(m_ExecTrace));
	Data.Write(API_S_RES_TRACE, TracePresetToStr(m_ResTrace));
	Data.Write(API_S_NET_TRACE, TracePresetToStr(m_NetTrace));
	Data.Write(API_S_SAVE_TRACE, SavePresetToStr(m_SaveTrace));

#ifdef PROG_SVC
	Data.WriteVariant(API_S_FW_RULES, CollectFwRules());
	Data.WriteVariant(API_S_PROGRAM_RULES, CollectProgRules());
	Data.WriteVariant(API_S_ACCESS_RULES, CollectResRules());

	if (Opts.Flags & SVarWriteOpt::eSaveToFile)
	{
	}
	else
	{
		Data.Write(API_S_ITEM_MISSING, IsMissing());
	}

	Data.Write(API_S_LOG_MEM_USAGE, GetLogMemUsage());
#endif
}

ETracePreset StrToTracePreset(const FW::StringA& Str)
{
	if (Str == API_S_TRACE_DEFAULT)				return ETracePreset::eDefault;
	else if (Str == API_S_TRACE_ONLY)			return ETracePreset::eTrace;
	else if (Str == API_S_TRACE_NONE)			return ETracePreset::eNoTrace;
	else if (Str == API_S_TRACE_PRIVATE)		return ETracePreset::ePrivate;
	return ETracePreset::eDefault;
}

ESavePreset StrToSavePreset(const FW::StringA& Str)
{
	if (Str == API_S_SAVE_TRACE_DEFAULT)		return ESavePreset::eDefault;
	else if (Str == API_S_SAVE_TRACE_TO_DISK)	return ESavePreset::eSaveToDisk;
	else if (Str == API_S_SAVE_TRACE_TO_RAM)	return ESavePreset::eDontSave;
	return ESavePreset::eDefault;
}

void CProgramItem::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_PROG_UID))				; // dont read UID
	else if (VAR_TEST_NAME(Name, API_S_PROG_ID))			m_ID.FromVariant(Data);

	else if (VAR_TEST_NAME(Name, API_S_NAME))				m_Name = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_ICON))				m_IconFile = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_INFO))				m_Info = AS_STR(Data);

	else if (VAR_TEST_NAME(Name, API_S_EXEC_TRACE))			m_ExecTrace = StrToTracePreset(Data);
	else if (VAR_TEST_NAME(Name, API_S_RES_TRACE))			m_ResTrace = StrToTracePreset(Data);
	else if (VAR_TEST_NAME(Name, API_S_NET_TRACE))			m_NetTrace = StrToTracePreset(Data);
	else if (VAR_TEST_NAME(Name, API_S_SAVE_TRACE))			m_SaveTrace = StrToSavePreset(Data);

#ifdef PROG_GUI
	else if (VAR_TEST_NAME(Name, API_S_FW_RULES))			m_FwRuleIDs = QFlexGuid::ReadVariantSet(Data);
	else if (VAR_TEST_NAME(Name, API_S_PROGRAM_RULES))		m_ProgRuleIDs = QFlexGuid::ReadVariantSet(Data);
	else if (VAR_TEST_NAME(Name, API_S_ACCESS_RULES))		m_ResRuleIDs = QFlexGuid::ReadVariantSet(Data);

	else if (VAR_TEST_NAME(Name, API_S_ITEM_MISSING))		m_IsMissing = Data;

	else if (VAR_TEST_NAME(Name, API_S_LOG_MEM_USAGE))		m_LogMemoryUsed = Data;

#else
	else if (VAR_TEST_NAME(Name, API_S_FW_RULES))			(void)0;
	else if (VAR_TEST_NAME(Name, API_S_PROGRAM_RULES))		(void)0;
	else if (VAR_TEST_NAME(Name, API_S_ACCESS_RULES))		(void)0;
#endif

	// else unknown tag
}

/////////////////////////////////////////////////////////////////////////////
// CWindowsService
//

void CWindowsService::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CProgramItem::WriteIVariant(Data, Opts);

	Data.WriteEx(API_V_SERVICE_TAG, TO_STR(m_ServiceTag));

#ifdef PROG_SVC
	Data.Write(API_V_PID, m_pProcess ? m_pProcess->GetProcessId() : 0);

	Data.Write(API_V_PROG_LAST_EXEC, m_LastExec);

	SStats Stats;
	CollectStats(Stats);

	if (Opts.Flags & SVarWriteOpt::eSaveToFile)
	{
	}
	else
	{
		Data.Write(API_V_PROG_ACCESS_COUNT, m_AccessTree.GetAccessCount());
		Data.WriteVariant(API_V_PROG_SOCKET_REFS, XVariant(Stats.SocketRefs));
	}

	Data.Write(API_V_SOCK_LAST_NET_ACT, Stats.LastNetActivity); 
	Data.Write(API_V_SOCK_LAST_ALLOW, m_LastFwAllowed);
	Data.Write(API_V_SOCK_LAST_BLOCK, m_LastFwBlocked);

	Data.Write(API_V_SOCK_UPLOAD, Stats.Upload);
	Data.Write(API_V_SOCK_DOWNLOAD, Stats.Download);
	Data.Write(API_V_SOCK_UPLOADED, Stats.Uploaded);
	Data.Write(API_V_SOCK_DOWNLOADED, Stats.Downloaded);
#endif
}

void CWindowsService::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_SERVICE_TAG:				m_ServiceTag = AS_STR(Data); break;

	case API_V_PROG_LAST_EXEC:			m_LastExec = Data; break;

#ifdef PROG_GUI
	case API_V_PID:					m_ProcessId = Data; break;

	case API_V_PROG_SOCKET_REFS:	m_SocketRefs = Data.AsQSet<quint64>(); break;
	case API_V_PROG_ACCESS_COUNT:	m_AccessCount = Data; break;

	case API_V_SOCK_LAST_NET_ACT:	m_Stats.LastNetActivity = Data; break;
	case API_V_SOCK_LAST_ALLOW:		m_Stats.LastFwAllowed = Data; break;
	case API_V_SOCK_LAST_BLOCK:		m_Stats.LastFwBlocked = Data; break;

	case API_V_SOCK_UPLOAD:			m_Stats.Upload = Data; break;
	case API_V_SOCK_DOWNLOAD:		m_Stats.Download = Data; break;
	case API_V_SOCK_UPLOADED:		m_Stats.Uploaded = Data; break;
	case API_V_SOCK_DOWNLOADED:		m_Stats.Downloaded = Data; break;
#else
	case API_V_PID: break;

	case API_V_PROG_ACCESS_COUNT: break;
	case API_V_PROG_SOCKET_REFS: break;

	case API_V_SOCK_LAST_NET_ACT:	m_TrafficLog.SetLastActivity(Data); break;
	case API_V_SOCK_LAST_ALLOW:		m_LastFwAllowed = Data; break;
	case API_V_SOCK_LAST_BLOCK:		m_LastFwBlocked = Data; break;

	case API_V_SOCK_UPLOAD: break;
	case API_V_SOCK_DOWNLOAD: break;
	case API_V_SOCK_UPLOADED:		m_TrafficLog.SetUploaded(Data); break;
	case API_V_SOCK_DOWNLOADED:		m_TrafficLog.SetDownloaded(Data); break;
#endif
	default: CProgramItem::ReadIValue(Index, Data);
	}
}

/////////////////////////////////////////////////////////////////////////////

void CWindowsService::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CProgramItem::WriteMVariant(Data, Opts);

	Data.WriteEx(API_S_SERVICE_TAG, TO_STR(m_ServiceTag));

#ifdef PROG_SVC
	Data.Write(API_S_PID, m_pProcess ? m_pProcess->GetProcessId() : 0);

	Data.Write(API_S_LAST_EXEC, m_LastExec);

	SStats Stats;
	CollectStats(Stats);

	if (Opts.Flags & SVarWriteOpt::eSaveToFile)
	{
	}
	else
	{
		Data.WriteVariant(API_S_PROG_SOCKETS, XVariant(Stats.SocketRefs));
	}

	Data.Write(API_S_SOCK_LAST_ACT, Stats.LastNetActivity);

	Data.Write(API_S_SOCK_LAST_ALLOW, m_LastFwAllowed);
	Data.Write(API_S_SOCK_LAST_BLOCK, m_LastFwBlocked);

	Data.Write(API_S_SOCK_UPLOAD, Stats.Upload);
	Data.Write(API_S_SOCK_DOWNLOAD, Stats.Download);
	Data.Write(API_S_SOCK_UPLOADED, Stats.Uploaded);
	Data.Write(API_S_SOCK_DOWNLOADED, Stats.Downloaded);
#endif
}

void CWindowsService::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_SERVICE_TAG))					m_ServiceTag = AS_STR(Data);

	else if (VAR_TEST_NAME(Name, API_S_LAST_EXEC))			m_LastExec = Data;

#ifdef PROG_GUI
	else if (VAR_TEST_NAME(Name, API_S_PID))				m_ProcessId = Data;

	else if (VAR_TEST_NAME(Name, API_S_PROG_SOCKETS))		m_SocketRefs = Data.AsQSet<quint64>();

	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ACT))		m_Stats.LastNetActivity = Data;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ALLOW))	m_Stats.LastFwAllowed = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_BLOCK))	m_Stats.LastFwBlocked = Data;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOAD))		m_Stats.Upload = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOAD))		m_Stats.Download = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOADED))		m_Stats.Uploaded = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOADED))	m_Stats.Downloaded = Data;
#else
	else if (VAR_TEST_NAME(Name, API_S_PID))				(void)0;

	else if (VAR_TEST_NAME(Name, API_S_PROG_SOCKETS))		(void)0;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ACT))		m_TrafficLog.SetLastActivity(Data);

	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ALLOW))	m_LastFwAllowed = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_BLOCK))	m_LastFwBlocked = Data;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOAD))		(void)0;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOAD))		(void)0;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOADED))		m_TrafficLog.SetUploaded(Data);
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOADED))	m_TrafficLog.SetDownloaded(Data);
#endif

	else
		CProgramItem::ReadMValue(Name, Data);
}

/////////////////////////////////////////////////////////////////////////////
// CProgramSet 
//

void CProgramSet::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CProgramItem::WriteIVariant(Data, Opts);

	VariantWriter Items;
	Items.BeginList();
	for (auto pItem : m_Nodes) {
		//Items.Write(pItem->ToVariant());
		XVariant Item;
		Item[API_V_PROG_UID] = pItem->GetUID();
		Item[API_V_PROG_ID] = pItem->GetID().ToVariant(Opts);	
		Items.WriteVariant(Item);
	}
	Data.WriteVariant(API_V_PROG_ITEMS, Items.Finish());
}

void CProgramSet::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index) {
	case API_V_PROG_ITEMS: break;
	default: CProgramItem::ReadIValue(Index, Data);
	}
}

/////////////////////////////////////////////////////////////////////////////

void CProgramSet::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CProgramItem::WriteMVariant(Data, Opts);

	VariantWriter Items;
	Items.BeginList();
	for (auto pItem : m_Nodes) {
		//Items.Write(pItem->ToVariant());
		XVariant Item;
		Item[API_S_PROG_UID] = pItem->GetUID();
		Item[API_S_PROG_ID] = pItem->GetID().ToVariant(Opts);	
		Items.WriteVariant(Item);
	}
	Data.WriteVariant(API_S_PROG_ITEMS, Items.Finish());
}

void CProgramSet::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_PROG_ITEMS)) 
		(void)0;
	else
		CProgramItem::ReadMValue(Name, Data);
}

/////////////////////////////////////////////////////////////////////////////
// CProgramPattern
//

void CProgramPattern::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CProgramList::WriteIVariant(Data, Opts);

	Data.WriteEx(API_V_PROG_PATTERN, GET_PATH(m_Pattern));
}

void CProgramPattern::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_PROG_PATTERN: SET_PATH(m_Pattern,Data); break;
	default: CProgramList::ReadIValue(Index, Data);
	}
}

/////////////////////////////////////////////////////////////////////////////

void CProgramPattern::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CProgramList::WriteMVariant(Data, Opts);

	Data.WriteEx(API_S_PROG_PATTERN, GET_PATH(m_Pattern));
}

void CProgramPattern::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_PROG_PATTERN)) SET_PATH(m_Pattern, Data);
	else
		CProgramList::ReadMValue(Name, Data);
}

/////////////////////////////////////////////////////////////////////////////
// CProgramFile
//

void CProgramFile::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CProgramSet::WriteIVariant(Data, Opts);

	Data.WriteEx(API_V_FILE_PATH, GET_PATH(m_Path));

	Data.WriteVariant(API_V_SIGN_INFO, m_SignInfo.ToVariant(Opts));

#ifdef PROG_SVC
	SStats Stats;
	CollectStats(Stats);

	Data.WriteVariant(API_V_PIDS, XVariant(Stats.Pids));

	Data.Write(API_V_PROG_LAST_EXEC, m_LastExec);

	if (Opts.Flags & SVarWriteOpt::eSaveToFile)
	{
		Data.WriteVariant(API_V_LIBRARIES, StoreLibraries(Opts));
	}
	else
	{
		Data.Write(API_V_PROG_ACCESS_COUNT, m_AccessTree.GetAccessCount());
		Data.WriteVariant(API_V_PROG_SOCKET_REFS, XVariant(Stats.SocketRefs));
	}

	Data.Write(API_V_SOCK_LAST_NET_ACT, Stats.LastNetActivity);
	Data.Write(API_V_SOCK_LAST_ALLOW, m_LastFwAllowed);
	Data.Write(API_V_SOCK_LAST_BLOCK, m_LastFwBlocked);

	Data.Write(API_V_SOCK_UPLOAD, Stats.Upload);
	Data.Write(API_V_SOCK_DOWNLOAD, Stats.Download);
	Data.Write(API_V_SOCK_UPLOADED, Stats.Uploaded);
	Data.Write(API_V_SOCK_DOWNLOADED, Stats.Downloaded);
#endif
}

void CProgramFile::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_FILE_PATH:		SET_PATH(m_Path, Data); break;

	case API_V_SIGN_INFO:		m_SignInfo.FromVariant(Data); break;

	case API_V_PROG_LAST_EXEC:		m_LastExec = Data; break;

#ifdef PROG_GUI
	case API_V_PIDS:			m_ProcessPids = Data.AsQSet<quint64>(); break;

	case API_V_PROG_ACCESS_COUNT: m_AccessCount = Data; break;
	case API_V_PROG_SOCKET_REFS: m_SocketRefs = Data.AsQSet<quint64>(); break;

	case API_V_LIBRARIES:		break;

	case API_V_SOCK_LAST_NET_ACT: m_Stats.LastNetActivity = Data; break;
	case API_V_SOCK_LAST_ALLOW:	m_Stats.LastFwAllowed = Data; break;
	case API_V_SOCK_LAST_BLOCK:	m_Stats.LastFwBlocked = Data; break;

	case API_V_SOCK_UPLOAD:		m_Stats.Upload = Data; break;
	case API_V_SOCK_DOWNLOAD:	m_Stats.Download = Data; break;
	case API_V_SOCK_UPLOADED:	m_Stats.Uploaded = Data; break;
	case API_V_SOCK_DOWNLOADED:	m_Stats.Downloaded = Data; break;
#else
	case API_V_PIDS: break;

	case API_V_PROG_ACCESS_COUNT: break;
	case API_V_PROG_SOCKET_REFS: break;

	case API_V_LIBRARIES:		LoadLibraries(Data); break;

	case API_V_SOCK_LAST_NET_ACT: m_TrafficLog.SetLastActivity(Data); break;
	case API_V_SOCK_LAST_ALLOW:	m_LastFwAllowed = Data; break;
	case API_V_SOCK_LAST_BLOCK:	m_LastFwBlocked = Data; break;

	case API_V_SOCK_UPLOAD: break;
	case API_V_SOCK_DOWNLOAD: break;
	case API_V_SOCK_UPLOADED:	m_TrafficLog.SetUploaded(Data); break;
	case API_V_SOCK_DOWNLOADED: m_TrafficLog.SetDownloaded(Data); break;
#endif
	default: CProgramSet::ReadIValue(Index, Data);
	}
}

/////////////////////////////////////////////////////////////////////////////

void CProgramFile::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CProgramSet::WriteMVariant(Data, Opts);

	Data.WriteEx(API_S_FILE_PATH, GET_PATH(m_Path));

	Data.WriteVariant(API_S_SIGN_INFO, m_SignInfo.ToVariant(Opts));

#ifdef PROG_SVC
	SStats Stats;
	CollectStats(Stats);

	Data.WriteVariant(API_S_PIDS, XVariant(Stats.Pids));

	Data.Write(API_S_LAST_EXEC, m_LastExec);

	if (Opts.Flags & SVarWriteOpt::eSaveToFile)
	{
		Data.WriteVariant(API_S_LIBRARIES, StoreLibraries(Opts));
	}
	else
	{
		Data.WriteVariant(API_S_PROG_SOCKETS, XVariant(Stats.SocketRefs));
	}

	Data.Write(API_S_SOCK_LAST_ACT, Stats.LastNetActivity);

	Data.Write(API_S_SOCK_LAST_ALLOW, m_LastFwAllowed);
	Data.Write(API_S_SOCK_LAST_BLOCK, m_LastFwBlocked);

	Data.Write(API_S_SOCK_UPLOAD, Stats.Upload);
	Data.Write(API_S_SOCK_DOWNLOAD, Stats.Download);
	Data.Write(API_S_SOCK_UPLOADED, Stats.Uploaded);
	Data.Write(API_S_SOCK_DOWNLOADED, Stats.Downloaded);
#endif
}

void CProgramFile::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_FILE_PATH)) SET_PATH(m_Path, Data);

	else if (VAR_TEST_NAME(Name, API_S_SIGN_INFO))			m_SignInfo.FromVariant(Data);

	else if (VAR_TEST_NAME(Name, API_S_LAST_EXEC))			m_LastExec = Data;

#ifdef PROG_GUI
	else if (VAR_TEST_NAME(Name, API_S_PIDS))				m_ProcessPids = Data.AsQSet<quint64>();

	else if (VAR_TEST_NAME(Name, API_S_PROG_SOCKETS))		m_SocketRefs = Data.AsQSet<quint64>();

	else if (VAR_TEST_NAME(Name, API_S_LIBRARIES))			(void)0;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ACT))		m_Stats.LastNetActivity = Data;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ALLOW))	m_Stats.LastFwAllowed = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_BLOCK))	m_Stats.LastFwBlocked = Data;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOAD))		m_Stats.Upload = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOAD))		m_Stats.Download = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOADED))		m_Stats.Uploaded = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOADED))	m_Stats.Downloaded = Data;
#else
	else if (VAR_TEST_NAME(Name, API_S_PIDS))				(void)0;

	else if (VAR_TEST_NAME(Name, API_S_PROG_SOCKETS))		(void)0;

	else if (VAR_TEST_NAME(Name, API_S_LIBRARIES))			LoadLibraries(Data);

	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ACT))		m_TrafficLog.SetLastActivity(Data);

	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_ALLOW))	m_LastFwAllowed = Data;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_LAST_BLOCK))	m_LastFwBlocked = Data;

	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOAD))		(void)0;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOAD))		(void)0;
	else if (VAR_TEST_NAME(Name, API_S_SOCK_UPLOADED))		m_TrafficLog.SetUploaded(Data);
	else if (VAR_TEST_NAME(Name, API_S_SOCK_DOWNLOADED))	m_TrafficLog.SetDownloaded(Data);
#endif
	else
		CProgramSet::ReadMValue(Name, Data);
}

/////////////////////////////////////////////////////////////////////////////
// CAppPackage
//

void CAppPackage::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CProgramList::WriteIVariant(Data, Opts);

	Data.WriteEx(API_V_APP_SID, TO_STR(m_AppContainerSid));
	Data.WriteEx(API_V_APP_NAME, TO_STR(m_AppContainerName));
	Data.WriteEx(API_V_FILE_PATH, GET_PATH(m_Path));
}

void CAppPackage::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_APP_SID: m_AppContainerSid = AS_STR(Data); break;
	case API_V_APP_NAME: m_AppContainerName = AS_STR(Data); break;
	case API_V_FILE_PATH: SET_PATH(m_Path, Data); break;
	default: CProgramList::ReadIValue(Index, Data);
	}
}

/////////////////////////////////////////////////////////////////////////////

void CAppPackage::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CProgramList::WriteMVariant(Data, Opts);

	Data.WriteEx(API_S_APP_SID, TO_STR(m_AppContainerSid));
	Data.WriteEx(API_S_APP_NAME, TO_STR(m_AppContainerName));
	Data.WriteEx(API_S_FILE_PATH, GET_PATH(m_Path));
}

void CAppPackage::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_APP_SID))			m_AppContainerSid = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_APP_NAME))	m_AppContainerName = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_FILE_PATH))	SET_PATH(m_Path, Data);
	else
		CProgramList::ReadMValue(Name, Data);
}


/////////////////////////////////////////////////////////////////////////////
// SAppInstallation
//

void CAppInstallation::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CProgramList::WriteIVariant(Data, Opts);

	Data.WriteEx(API_V_REG_KEY, TO_STR(m_RegKey));
	Data.WriteEx(API_V_FILE_PATH, GET_PATH(m_Path));
}

void CAppInstallation::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index) {
	case API_V_REG_KEY: m_RegKey = AS_STR(Data); break;
	case API_V_FILE_PATH: SET_PATH(m_Path, Data); break;
	default: CProgramList::ReadIValue(Index, Data);
	}
}

/////////////////////////////////////////////////////////////////////////////

void CAppInstallation::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	CProgramList::WriteMVariant(Data, Opts);

	Data.WriteEx(API_S_REG_KEY, TO_STR(m_RegKey));
	Data.WriteEx(API_S_FILE_PATH, GET_PATH(m_Path));
}

void CAppInstallation::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_REG_KEY))			m_RegKey = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_FILE_PATH))	SET_PATH(m_Path, Data);
	else
		CProgramList::ReadMValue(Name, Data);
}

/////////////////////////////////////////////////////////////////////////////
// CProgramLibrary
//

void CProgramLibrary::WriteIVariant(VariantWriter& Entry, const SVarWriteOpt& Opts) const
{
	Entry.Write(API_V_PROG_UID, m_UID);
	Entry.WriteEx(API_V_FILE_PATH, GET_PATH(m_Path));
}

void CProgramLibrary::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_PROG_UID: m_UID = Data.To<uint64>(); break;
	case API_V_FILE_PATH: SET_PATH(m_Path, Data); break;
	default: break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CProgramLibrary::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	Data.Write(API_S_PROG_UID, m_UID);
	Data.WriteEx(API_S_FILE_PATH, GET_PATH(m_Path));
}

void CProgramLibrary::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_PROG_UID))				m_UID = Data.To<uint64>();
	else if (VAR_TEST_NAME(Name, API_S_FILE_PATH))			SET_PATH(m_Path, Data);
}

/////////////////////////////////////////////////////////////////////////////
// CImageSignInfo
//

void CImageSignInfo::WriteIVariant(VariantWriter& Entry, const SVarWriteOpt& Opts) const
{
	Entry.Write(API_V_IMG_SIGN_AUTH, m_SignInfo.Authority);
	Entry.Write(API_V_IMG_SIGN_LEVEL, m_SignInfo.Level);
	Entry.Write(API_V_IMG_SIGN_POLICY, m_SignInfo.Policy);

	Entry.WriteVariant(API_V_FILE_HASH, XVariant(m_FileHash));

	Entry.Write(API_V_CERT_STATUS, (uint8)m_HashStatus);
	if (m_HashStatus != EHashStatus::eHashNone && m_HashStatus != EHashStatus::eHashUnknown) {
		Entry.WriteVariant(API_V_IMG_CERT_ALG, XVariant(m_SignerHash));
		Entry.WriteEx(API_V_IMG_SIGN_NAME, TO_STR(m_SignerName));
	}
}

void CImageSignInfo::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_IMG_SIGN_AUTH: m_SignInfo.Authority = Data.To<uint8>(); break;
	case API_V_IMG_SIGN_LEVEL: m_SignInfo.Level = Data.To<uint8>(); break;
	case API_V_IMG_SIGN_POLICY: m_SignInfo.Policy = Data.To<uint32>(); break;

	case API_V_FILE_HASH: m_FileHash = TO_BYTES(Data); break;

	case API_V_CERT_STATUS: m_HashStatus = (EHashStatus)Data.To<uint8>(); break;
	case API_V_IMG_CERT_ALG: m_SignerHash = TO_BYTES(Data); break;
	case API_V_IMG_SIGN_NAME: m_SignerName = AS_ASTR(Data); break;
	default: break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CImageSignInfo::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	Data.Write(API_S_SIGN_INFO_AUTH, m_SignInfo.Authority);
	Data.Write(API_S_SIGN_INFO_LEVEL, m_SignInfo.Level);
	Data.Write(API_S_SIGN_INFO_POLICY, m_SignInfo.Policy);

	Data.WriteVariant(API_S_FILE_HASH, XVariant(m_FileHash));

	Data.Write(API_S_CERT_STATUS, (uint8)m_HashStatus);
	if (m_HashStatus != EHashStatus::eHashNone && m_HashStatus != EHashStatus::eHashUnknown) {
		Data.WriteVariant(API_S_CERT_HASH, XVariant(m_SignerHash));
		Data.WriteEx(API_S_SIGNER_NAME, TO_STR(m_SignerName));
	}
}

void CImageSignInfo::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_SIGN_INFO_AUTH))			m_SignInfo.Authority = Data.To<uint8>();
	else if (VAR_TEST_NAME(Name, API_S_SIGN_INFO_LEVEL))	m_SignInfo.Level = Data.To<uint8>();
	else if (VAR_TEST_NAME(Name, API_S_SIGN_INFO_POLICY))	m_SignInfo.Policy = Data.To<uint32>();

	else if (VAR_TEST_NAME(Name, API_S_FILE_HASH))			m_FileHash = TO_BYTES(Data);

	else if (VAR_TEST_NAME(Name, API_S_CERT_STATUS))		m_HashStatus = (EHashStatus)Data.To<uint8>();
	else if (VAR_TEST_NAME(Name, API_S_CERT_HASH))			m_SignerHash = TO_BYTES(Data);
	else if (VAR_TEST_NAME(Name, API_S_SIGNER_NAME))		m_SignerName = AS_ASTR(Data);
}