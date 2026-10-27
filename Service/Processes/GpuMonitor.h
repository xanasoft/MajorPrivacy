#pragma once
#include <map>
#include <vector>
#include <string>
#include <functional>

//
// GPU / NPU usage sampling through the D3DKMT statistics of dxgkrnl, the same source
// Task Manager and System Informer (plugins/ExtendedTools/gpumon.c, npumon.c) use.
//
// Every adapter has a number of nodes (engines: 3D, copy, video decode, ...). For each
// process dxgkrnl keeps the running time per node. The monitor takes the running time
// delta of every node a process used since the last sample and hands out, per process,
// the sum over the nodes of all GPU adapters and of all NPU adapters, divided by the
// number of nodes of that kind, so 100% means every node of every adapter was busy for
// the whole sample. Only the processes dxgkrnl lists as users of an adapter are queried.
//

class CGpuMonitor
{
public:
	enum EType
	{
		eGpu = 0,
		eNpu,
		eTypeMax
	};

	struct SProcessUsage
	{
		uint64 Time[eTypeMax] = { 0 };	// 100ns of node running time since the last sample, normalized to one node
	};

	CGpuMonitor();
	~CGpuMonitor();

	// where to report the adapters found (the service event log); the monitor itself has no dependencies
	void		SetLogger(const std::function<void(const std::wstring&)>& Logger) { m_Logger = Logger; }

	// enumerates the adapters, false when there is nothing to monitor (no adapter, no D3DKMT)
	bool		Init();
	void		Reset();

	bool		IsReady() const					{ return !m_Adapters.empty(); }
	uint32		GetNodeCount(EType Type) const	{ return Type < eTypeMax ? m_NodeCount[Type] : 0; }
	uint32		GetAdapterCount(EType Type) const;

	// one sample: the running time deltas of every process that used an adapter, by process id
	void		Sample(std::map<uint32, SProcessUsage>& Usage);

private:
	struct SAdapter
	{
		LUID		Luid = { 0, 0 };
		EType		Type = eGpu;
		uint32		NodeCount = 0;
		uint32		FirstNode = 0;	// index into SProcessState::NodeTime
		size_t		ProcessHint = 0;	// size of the last process list, a buffer hint
		std::wstring Name;
	};

	struct SProcessState
	{
		HANDLE		hProcess = NULL;
		bool		bOwnHandle = false;
		bool		bInitialized = false;		// the first sample only records the base values
		uint64		LastSeen = 0;
		std::vector<uint64> NodeTime;			// running time per node, 100ns, last seen value
	};

	struct SAttributes
	{
		bool TypeGpu = false, TypeComputeAccelerator = false, TypeNpu = false, TypeMediaAccelerator = false;
		bool D3D11Graphics = false, D3D12Graphics = false, D3D12CoreCompute = false;
	};

	bool		ResolveFunctions();
	bool		CheckAdapters();
	bool		QueryAttributes(uint32 hAdapter, SAttributes& Attributes);
	bool		IsSoftwareDevice(uint32 hAdapter);
	std::wstring QueryName(uint32 hAdapter);
	void		CloseAdapter(uint32 hAdapter);

	// the processes using an adapter: id and a handle good for D3DKMTQueryStatistics
	void		ListProcesses(SAdapter& Adapter, std::vector<std::pair<uint32, HANDLE>>& Processes, std::vector<HANDLE>& Temporary);
	HANDLE		GetProcessHandle(uint32 Pid, uint64 Now);
	void		EvictProcesses(uint64 Now);

	std::vector<SAdapter>				m_Adapters;
	uint32								m_NodeCount[eTypeMax] = { 0 };
	uint32								m_TotalNodes = 0;
	std::map<uint32, SProcessState>		m_Processes;
	uint64								m_LastCheck = 0;
	std::function<void(const std::wstring&)> m_Logger;

	// gdi32 entry points, resolved at runtime (D3DKMTEnumProcesses is Windows 11 only)
	bool								m_bResolved = false;
	void*								m_pEnumAdapters2 = nullptr;
	void*								m_pEnumAdapters3 = nullptr;
	void*								m_pQueryAdapterInfo = nullptr;
	void*								m_pQueryStatistics = nullptr;
	void*								m_pCloseAdapter = nullptr;
	void*								m_pEnumProcesses = nullptr;
	void*								m_pGetProcessList = nullptr;
};
