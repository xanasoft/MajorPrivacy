#pragma once
#include "ProgramItem.h"
#include "../Processes/Process.h"
#include "../Network/TrafficLog.h"

class CWindowsService: public CProgramItem
{
public:
	CWindowsService(const TServiceId& Id);

	virtual TServiceId GetSvcTag() const						{ std::unique_lock lock(m_Mutex); return m_ServiceId; }

	virtual void SetProcess(const CProcessPtr& pProcess);

	virtual CTrafficLog* TrafficLog()							{ return &m_TraceLog; }

protected:

	virtual void WriteVariant(CVariant& Data) const;

	TServiceId		m_ServiceId;

	//
	// Note: A service can have only one process,
	// howeever a process can host multiple services
	//

	CProcessPtr		m_pProcess;


	CTrafficLog		m_TraceLog;
};

typedef std::shared_ptr<CWindowsService> CWindowsServicePtr;
typedef std::weak_ptr<CWindowsService> CWindowsServiceRef;