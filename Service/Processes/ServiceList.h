#pragma once

class CServiceList
{
public:
	CServiceList(std::recursive_mutex& Mutex);

	void EnumServices();

	struct SService
	{
		std::wstring Id;
		std::wstring Name;
		std::wstring BinaryPath;
		uint32 State;
		uint64 ProcessId;
	};

	typedef std::shared_ptr<SService> SServicePtr;

	std::map<std::wstring, SServicePtr> List() { 
		std::unique_lock Lock(m_Mutex);
		return m_List; 
	};
	std::set<std::wstring> GetServicesByPid(uint64 Pid) { 
		std::unique_lock Lock(m_Mutex);
		std::set<std::wstring> Services;
		for (auto I = m_Pids.find(Pid); I != m_Pids.end() && I->first == Pid; ++I)
			Services.insert(I->second);
		return Services;
	}
	SServicePtr GetService(const std::wstring& Id) { 
		std::unique_lock Lock(m_Mutex);
		auto F = m_List.find(Id); 
		return F != m_List.end() ? F->second : SServicePtr(); 
	}

protected:

	std::recursive_mutex& m_Mutex; // Note: we need to sharte a recursive mutex with the process list

	std::map<std::wstring, SServicePtr> m_List;
	std::multimap<uint64, std::wstring> m_Pids;
};