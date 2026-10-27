#pragma once

class CServiceList
{
	TRACK_OBJECT(CServiceList)
public:
	CServiceList(std::shared_mutex& Mutex);

	void EnumServices();

	struct SService
	{
		TRACK_OBJECT(SService)

		std::wstring ServiceTag;
		std::wstring Name;
		std::wstring BinaryPath;
		uint32 State;
		uint64 ProcessId;
	};

	typedef std::shared_ptr<SService> SServicePtr;

	std::map<std::wstring, SServicePtr> List() { std::shared_lock Lock(m_Mutex); return m_List;  };
	std::set<std::wstring> GetServicesByPid(uint64 Pid);
	SServicePtr GetService(const std::wstring& ServiceTag);
	SServicePtr GetServiceUnsafe(const std::wstring& ServiceTag);

protected:

	void SetProcessUnsafe(const SServicePtr& pService);
	void ClearProcessUnsafe(const SServicePtr& pService);

	std::shared_mutex& m_Mutex; // Note: we share one mutex with the process list

	std::map<std::wstring, SServicePtr> m_List;
	std::multimap<uint64, std::wstring> m_Pids;
};