

/////////////////////////////////////////////////////////////////////////////
// CProgramItem 
//

void CProgramItem::WriteIVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	Data.Write(API_V_PROG_UID, m_UID);
	Data.Write(API_V_PROG_TYPE, (uint32)GetType());

	Data.WriteVariant(API_V_ID, m_ID.ToVariant(Opts, Data.Allocator()));

	Data.WriteEx(API_V_NAME, TO_STR(m_Name));
	Data.WriteEx(API_V_ICON, TO_STR(m_IconFile));
	Data.WriteEx(API_V_INFO, TO_STR(m_Info));

	Data.Write(API_V_EXEC_TRACE, (int)m_ExecTrace);
	Data.Write(API_V_RES_TRACE, (int)m_ResTrace);
	Data.Write(API_V_NET_TRACE, (int)m_NetTrace);
	Data.Write(API_V_SAVE_TRACE, (int)m_SaveTrace);

#ifdef PROG_SVC
	Data.WriteVariant(API_V_FW_RULES, CollectFwRules(Data.Allocator()));
	Data.WriteVariant(API_V_PROGRAM_RULES, CollectProgRules(Data.Allocator()));
	Data.WriteVariant(API_V_ACCESS_RULES, CollectResRules(Data.Allocator()));

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

	case API_V_ID:			m_ID.FromVariant(Data); break;

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

	Data.WriteVariant(API_S_ID, m_ID.ToVariant(Opts, Data.Allocator()));

	Data.WriteEx(API_S_NAME, TO_STR(m_Name));
	Data.WriteEx(API_S_ICON, TO_STR(m_IconFile));
	Data.WriteEx(API_S_INFO, TO_STR(m_Info));

	Data.Write(API_S_EXEC_TRACE, TracePresetToStr(m_ExecTrace));
	Data.Write(API_S_RES_TRACE, TracePresetToStr(m_ResTrace));
	Data.Write(API_S_NET_TRACE, TracePresetToStr(m_NetTrace));
	Data.Write(API_S_SAVE_TRACE, SavePresetToStr(m_SaveTrace));

#ifdef PROG_SVC
	Data.WriteVariant(API_S_FW_RULES, CollectFwRules(Data.Allocator()));
	Data.WriteVariant(API_S_PROGRAM_RULES, CollectProgRules(Data.Allocator()));
	Data.WriteVariant(API_S_ACCESS_RULES, CollectResRules(Data.Allocator()));

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
	else if (VAR_TEST_NAME(Name, API_S_ID))			m_ID.FromVariant(Data);

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
		Data.WriteVariant(API_S_PROG_SOCKETS, XVariant(Data.Allocator(), Stats.SocketRefs));
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

	VariantWriter Items(Data.Allocator());
	Items.BeginList();
	for (auto pItem : m_Nodes) {
		//Items.Write(pItem->ToVariant(Data.Allocator()));
		XVariant Item;
		Item[API_V_PROG_UID] = pItem->GetUID();
		Item[API_V_ID] = pItem->GetID().ToVariant(Opts, Data.Allocator());	
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

	VariantWriter Items(Data.Allocator());
	Items.BeginList();
	for (auto pItem : m_Nodes) {
		//Items.Write(pItem->ToVariant(Data.Allocator()));
		XVariant Item;
		Item[API_S_PROG_UID] = pItem->GetUID();
		Item[API_S_ID] = pItem->GetID().ToVariant(Opts, Data.Allocator());	
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

	Data.WriteVariant(API_V_SIGN_INFO, m_SignInfo.ToVariant(Opts, Data.Allocator()));

#ifdef PROG_SVC
	SStats Stats;
	CollectStats(Stats);

	Data.WriteVariant(API_V_PIDS, XVariant(Data.Allocator(), Stats.Pids));

	Data.Write(API_V_PROG_LAST_EXEC, m_LastExec);

	if (Opts.Flags & SVarWriteOpt::eSaveToFile)
	{
		Data.WriteVariant(API_V_LIBRARIES, StoreLibraries(Opts, Data.Allocator()));
	}
	else
	{
		Data.Write(API_V_PROG_ACCESS_COUNT, m_AccessTree.GetAccessCount());
		Data.WriteVariant(API_V_PROG_SOCKET_REFS, XVariant(Data.Allocator(), Stats.SocketRefs));
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

	Data.WriteVariant(API_S_SIGN_INFO, m_SignInfo.ToVariant(Opts, Data.Allocator()));

#ifdef PROG_SVC
	SStats Stats;
	CollectStats(Stats);

	Data.WriteVariant(API_S_PIDS, XVariant(Data.Allocator(), Stats.Pids));

	Data.Write(API_S_LAST_EXEC, m_LastExec);

	if (Opts.Flags & SVarWriteOpt::eSaveToFile)
	{
		Data.WriteVariant(API_S_LIBRARIES, StoreLibraries(Opts, Data.Allocator()));
	}
	else
	{
		Data.WriteVariant(API_S_PROG_SOCKETS, XVariant(Data.Allocator(), Stats.SocketRefs));
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
	Data.WriteEx(API_V_PACK_NAME, TO_STR(m_AppPackageName));
	Data.WriteEx(API_V_FILE_PATH, GET_PATH(m_Path));
}

void CAppPackage::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_APP_SID: m_AppContainerSid = AS_STR(Data); break;
	case API_V_APP_NAME: m_AppContainerName = AS_STR(Data); break;
	case API_V_PACK_NAME: m_AppPackageName = AS_STR(Data); break;
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
	Data.WriteEx(API_S_PACK_NAME, TO_STR(m_AppPackageName));
	Data.WriteEx(API_S_FILE_PATH, GET_PATH(m_Path));
}

void CAppPackage::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_APP_SID))			m_AppContainerSid = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_APP_NAME))	m_AppContainerName = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_PACK_NAME))	m_AppPackageName = AS_STR(Data);
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
	Entry.WriteVariant(API_V_SIGN_INFO, m_SignInfo.ToVariant(Opts, Entry.Allocator()));
}

void CProgramLibrary::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_PROG_UID: m_UID = Data.To<uint64>(); break;
	case API_V_FILE_PATH: SET_PATH(m_Path, Data); break;
	case API_V_SIGN_INFO: m_SignInfo.FromVariant(Data); break;
	default: break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CProgramLibrary::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	Data.Write(API_S_PROG_UID, m_UID);
	Data.WriteEx(API_S_FILE_PATH, GET_PATH(m_Path));
	Data.WriteVariant(API_S_SIGN_INFO, m_SignInfo.ToVariant(Opts, Data.Allocator()));
}

void CProgramLibrary::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_PROG_UID))				m_UID = Data.To<uint64>();
	else if (VAR_TEST_NAME(Name, API_S_FILE_PATH))			SET_PATH(m_Path, Data);
	else if (VAR_TEST_NAME(Name, API_S_SIGN_INFO))			m_SignInfo.FromVariant(Data);
}

/////////////////////////////////////////////////////////////////////////////
// CImageSignInfo
//

void CImageSignInfo::WriteIVariant(VariantWriter& Entry, const SVarWriteOpt& Opts) const
{
	Entry.Write(API_V_SIGN_FLAGS, m_StatusFlags);

	Entry.Write(API_V_IMG_SIGN_AUTH, m_PrivateAuthority);
	Entry.Write(API_V_IMG_SIGN_LEVEL, m_SignLevel);
	Entry.Write(API_V_IMG_SIGN_POLICY, m_SignPolicyBits);

	Entry.Write(API_V_IMG_SIGN_BITS, m_Signatures.Value);

	if (m_FileHashAlgorithm)
	{
		Entry.Write(API_V_FILE_HASH_ALG, (uint32)m_FileHashAlgorithm);
		Entry.WriteVariant(API_V_FILE_HASH, XVariant(Entry.Allocator(), m_FileHash));
	}

	//Entry.Write(API_V_CERT_STATUS, (uint8)m_HashStatus);

	if (m_SignerHashAlgorithm) 
	{
		Entry.Write(API_V_IMG_CERT_ALG, (uint32)m_SignerHashAlgorithm);
		Entry.WriteVariant(API_V_IMG_CERT_HASH, XVariant(Entry.Allocator(), m_SignerHash));
		Entry.WriteEx(API_V_IMG_SIGN_NAME, TO_STR(m_SignerName));
	}

	if (m_IssuerHashAlgorithm) 
	{
		Entry.Write(API_V_IMG_CA_ALG, (uint32)m_IssuerHashAlgorithm);
		Entry.WriteVariant(API_V_IMG_CA_HASH, XVariant(Entry.Allocator(), m_IssuerHash));
		Entry.WriteEx(API_V_IMG_CA_NAME, TO_STR(m_IssuerName));
	}

	Entry.Write(API_V_TIME_STAMP, m_TimeStamp);
}

void CImageSignInfo::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_SIGN_FLAGS: m_StatusFlags = Data.To<uint32>(); break;

	case API_V_IMG_SIGN_AUTH: m_PrivateAuthority = (KPH_VERIFY_AUTHORITY)Data.To<uint32>(); break;
	case API_V_IMG_SIGN_LEVEL: m_SignLevel = Data.To<uint32>(); break;
	case API_V_IMG_SIGN_POLICY: m_SignPolicyBits = Data.To<uint32>(); break;

	case API_V_IMG_SIGN_BITS: m_Signatures.Value = Data.To<uint32>(); break;

	case API_V_FILE_HASH_ALG: m_FileHashAlgorithm = Data.To<uint32>(); break;
	case API_V_FILE_HASH: m_FileHash = TO_BYTES(Data); break;

	//case API_V_CERT_STATUS: m_HashStatus = (EHashStatus)Data.To<uint8>(); break;

	case API_V_IMG_CERT_ALG: m_SignerHashAlgorithm = Data.To<uint32>(); break;
	case API_V_IMG_CERT_HASH: m_SignerHash = TO_BYTES(Data); break;
	case API_V_IMG_SIGN_NAME: m_SignerName = AS_ASTR(Data); break;

	case API_V_IMG_CA_ALG: m_IssuerHashAlgorithm = Data.To<uint32>(); break;
	case API_V_IMG_CA_HASH: m_IssuerHash = TO_BYTES(Data); break;
	case API_V_IMG_CA_NAME: m_IssuerName = AS_ASTR(Data); break;

	case API_V_TIME_STAMP: m_TimeStamp = Data; break;
	default: break;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CImageSignInfo::WriteMVariant(VariantWriter& Data, const SVarWriteOpt& Opts) const
{
	Data.Write(API_S_SIGN_FLAGS, m_StatusFlags);

	Data.Write(API_S_SIGN_INFO_AUTH, m_PrivateAuthority);
	Data.Write(API_S_SIGN_INFO_LEVEL, m_SignLevel);
	Data.Write(API_S_SIGN_INFO_POLICY, m_SignPolicyBits);

	Data.Write(API_S_SIGN_SIGN_BITS, m_Signatures.Value);

	if (m_FileHashAlgorithm)
	{
		Data.Write(API_S_FILE_HASH_ALG, (uint32)m_FileHashAlgorithm);
		Data.WriteVariant(API_S_FILE_HASH, XVariant(Data.Allocator(), m_FileHash));
	}

	//switch (m_HashStatus) 
	//{
	//case EHashStatus::eHashUnknown:	Data.Write(API_S_CERT_STATUS, API_S_CERT_STATUS_UNKNOWN); break;
	//case EHashStatus::eHashOk: 		Data.Write(API_S_CERT_STATUS, API_S_CERT_STATUS_OK); break;
	////case EHashStatus::eHashError:	Data.Write(API_S_CERT_STATUS, API_S_CERT_STATUS_ERROR); break;
	//case EHashStatus::eHashFail:	Data.Write(API_S_CERT_STATUS, API_S_CERT_STATUS_FAIL); break;
	//case EHashStatus::eHashDummy:	Data.Write(API_S_CERT_STATUS, API_S_CERT_STATUS_DUMMY); break;
	//case EHashStatus::eHashNone:	Data.Write(API_S_CERT_STATUS, API_S_CERT_STATUS_NONE); break;	
	//}

	if (m_SignerHashAlgorithm)
	{
		Data.Write(API_S_CERT_HASH_ALG, (uint32)m_SignerHashAlgorithm);
		Data.WriteVariant(API_S_CERT_HASH, XVariant(Data.Allocator(), m_SignerHash));
		Data.WriteEx(API_S_SIGNER_NAME, TO_STR(m_SignerName));
	}

	if(m_IssuerHashAlgorithm)
	{
		Data.Write(API_S_CA_HASH_ALG, (uint32)m_IssuerHashAlgorithm);
		Data.WriteVariant(API_S_CA_HASH, XVariant(Data.Allocator(), m_IssuerHash));
		Data.WriteEx(API_S_CA_NAME, TO_STR(m_IssuerName));
	}

	Data.Write(API_S_TIME_STAMP, m_TimeStamp);
}

void CImageSignInfo::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_SIGN_FLAGS))				m_StatusFlags = Data.To<uint32>();
	else if (VAR_TEST_NAME(Name, API_S_SIGN_INFO_AUTH))		m_PrivateAuthority = (KPH_VERIFY_AUTHORITY)Data.To<uint32>();
	else if (VAR_TEST_NAME(Name, API_S_SIGN_INFO_LEVEL))	m_SignLevel = Data.To<uint32>();
	else if (VAR_TEST_NAME(Name, API_S_SIGN_INFO_POLICY))	m_SignPolicyBits = Data.To<uint32>();

	else if (VAR_TEST_NAME(Name, API_S_SIGN_SIGN_BITS))		m_Signatures.Value = Data.To<uint32>();

	else if (VAR_TEST_NAME(Name, API_S_FILE_HASH_ALG))		m_FileHashAlgorithm = Data.To<uint32>();
	else if (VAR_TEST_NAME(Name, API_S_FILE_HASH))			m_FileHash = TO_BYTES(Data);

	//else if (VAR_TEST_NAME(Name, API_S_CERT_STATUS))
	//{
	//	ASTR HashStatus = Data;
	//	if (HashStatus == API_S_CERT_STATUS_UNKNOWN)		m_HashStatus = EHashStatus::eHashUnknown;
	//	else if (HashStatus == API_S_CERT_STATUS_OK)			m_HashStatus = EHashStatus::eHashOk;
	//	//else if (HashStatus == API_S_CERT_STATUS_ERROR)		m_HashStatus = EHashStatus::eHashError;
	//	else if (HashStatus == API_S_CERT_STATUS_FAIL)		m_HashStatus = EHashStatus::eHashFail;
	//	else if (HashStatus == API_S_CERT_STATUS_DUMMY)		m_HashStatus = EHashStatus::eHashDummy;
	//	else if (HashStatus == API_S_CERT_STATUS_NONE)		m_HashStatus = EHashStatus::eHashNone;
	//	else // fallback todo remove
	//		m_HashStatus = (EHashStatus)Data.To<uint8>();
	//}

	else if (VAR_TEST_NAME(Name, API_S_CERT_HASH_ALG))		m_SignerHashAlgorithm = Data.To<uint32>();
	else if (VAR_TEST_NAME(Name, API_S_CERT_HASH))			m_SignerHash = TO_BYTES(Data);
	else if (VAR_TEST_NAME(Name, API_S_SIGNER_NAME))		m_SignerName = AS_ASTR(Data);

	else if (VAR_TEST_NAME(Name, API_S_CA_HASH_ALG))		m_IssuerHashAlgorithm = Data.To<uint32>();
	else if (VAR_TEST_NAME(Name, API_S_CA_HASH))			m_IssuerHash = TO_BYTES(Data);
	else if (VAR_TEST_NAME(Name, API_S_CA_NAME))			m_IssuerName = AS_ASTR(Data);

	else if (VAR_TEST_NAME(Name, API_S_TIME_STAMP))			m_TimeStamp = Data;
}