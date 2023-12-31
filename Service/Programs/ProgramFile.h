#pragma once
#include "AppPackage.h"
#include "WindowsService.h"

#include "../Processes/Process.h"
#include "TraceLogEntry.h"
#include "../Network/Firewall/FirewallDefs.h"
#include "../Network/TrafficLog.h"

class CProgramFile: public CProgramSet
{
public:
	CProgramFile(const std::wstring& FileName);

	virtual void AddProcess(const CProcessPtr& pProcess);
	virtual void RemoveProcess(const CProcessPtr& pProcess);

	virtual std::map<uint64, CProcessPtr> GetProcesses() const	{ std::shared_lock lock(m_Mutex); return m_Processes; }
	virtual std::wstring GetPath() const						{ std::shared_lock lock(m_Mutex); return m_FileName; }

	virtual CAppPackagePtr GetAppPackage() const;
	virtual std::list<CWindowsServicePtr> GetAllServices() const;
	virtual CWindowsServicePtr GetService(const std::wstring& SvcTag) const;

	virtual CTrafficLog* TrafficLog()							{ return &m_TraceLog; }

	virtual uint32 AddTraceLogEntry(const CTraceLogEntryPtr& pLogEntry, ETraceLogs Log);
	virtual std::vector<CTraceLogEntryPtr> GetTraceLog(ETraceLogs Log) const;

protected:
	friend class CProgramManager;

	virtual void WriteVariant(CVariant& Data) const;

	std::wstring					m_FileName;

	//
	// Note: A program file can have multiple running processes
	// but a process always has one unique program file, 
	// see CProcess::m_pFileRef for details
	//

	std::map<uint64, CProcessPtr>	m_Processes;

	CTrafficLog						m_TraceLog;

	//
	// Note: log entries don't have mutexes, we thread them as read only objects
	// thay must not be modificed after being added to the list !!!
	//

	std::vector<CTraceLogEntryPtr>	m_TraceLogs[(int)ETraceLogs::eLogMax];
};

typedef std::shared_ptr<CProgramFile> CProgramFilePtr;
typedef std::weak_ptr<CProgramFile> CProgramFileRef;