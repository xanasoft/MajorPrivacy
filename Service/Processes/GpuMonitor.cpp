#include "pch.h"
#include "GpuMonitor.h"

#include <d3dkmthk.h>
#include <rpc.h>

//////////////////////////////////////////////////////////////////////////
// D3DKMT bits the SDK header does not have (from System Informer, phnt/include/ntd3dkmt.h)

typedef struct _MP_D3DKMT_GET_PROCESS_LIST
{
	LUID AdapterLuid;			// in
	ULONG DesiredAccess;		// in, must be PROCESS_QUERY_INFORMATION
	ULONG ProcessHandleCount;	// in/out, capacity, required capacity on STATUS_BUFFER_TOO_SMALL, returned count on success
	PHANDLE ProcessHandle;		// in, NULL queries the capacity
} MP_D3DKMT_GET_PROCESS_LIST;

typedef struct _MP_D3DKMT_ENUM_PROCESS_LIST
{
	LUID AdapterLuid;			// in
	PULONG ProcessIdBuffer;		// in, NULL queries the count
	SIZE_T ProcessIdCount;		// in/out
} MP_D3DKMT_ENUM_PROCESS_LIST;

typedef NTSTATUS(APIENTRY* P_D3DKMTEnumAdapters2)(D3DKMT_ENUMADAPTERS2*);
typedef NTSTATUS(APIENTRY* P_D3DKMTEnumAdapters3)(D3DKMT_ENUMADAPTERS3*);
typedef NTSTATUS(APIENTRY* P_D3DKMTQueryAdapterInfo)(const D3DKMT_QUERYADAPTERINFO*);
typedef NTSTATUS(APIENTRY* P_D3DKMTQueryStatistics)(const D3DKMT_QUERYSTATISTICS*);
typedef NTSTATUS(APIENTRY* P_D3DKMTCloseAdapter)(const D3DKMT_CLOSEADAPTER*);
typedef NTSTATUS(APIENTRY* P_D3DKMTEnumProcesses)(MP_D3DKMT_ENUM_PROCESS_LIST*);
typedef NTSTATUS(APIENTRY* P_D3DKMTGetProcessList)(MP_D3DKMT_GET_PROCESS_LIST*);

// DXCore adapter attributes, stored by dxgkrnl as a REG_MULTI_SZ of GUIDs in the adapter key
static const GUID MP_DXCORE_HARDWARE_TYPE_ATTRIBUTE_GPU					= { 0xb69eb219, 0x3ded, 0x4464, { 0x97, 0x9f, 0xa0, 0xb, 0xd4, 0x68, 0x70, 0x6 } };
static const GUID MP_DXCORE_HARDWARE_TYPE_ATTRIBUTE_COMPUTE_ACCELERATOR	= { 0xe0b195da, 0x58ef, 0x4a22, { 0x90, 0xf1, 0x1f, 0x28, 0x16, 0x9c, 0xab, 0x8d } };
static const GUID MP_DXCORE_HARDWARE_TYPE_ATTRIBUTE_NPU					= { 0xd46140c4, 0xadd7, 0x451b, { 0x9e, 0x56, 0x6, 0xfe, 0x8c, 0x3b, 0x58, 0xed } };
static const GUID MP_DXCORE_HARDWARE_TYPE_ATTRIBUTE_MEDIA_ACCELERATOR	= { 0x66bdb96a, 0x50b, 0x44c7, { 0xa4, 0xfd, 0xd1, 0x44, 0xce, 0xa, 0xb4, 0x43 } };
static const GUID MP_DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS				= { 0x8c47866b, 0x7583, 0x450d, { 0xf0, 0xf0, 0x6b, 0xad, 0xa8, 0x95, 0xaf, 0x4b } };
static const GUID MP_DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS				= { 0x0c9ece4d, 0x2f6e, 0x4f01, { 0x8c, 0x96, 0xe8, 0x9e, 0x33, 0x1b, 0x47, 0xb1 } };
static const GUID MP_DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE			= { 0x248e2800, 0xa793, 0x4724, { 0xab, 0xaa, 0x23, 0xa6, 0xde, 0x1b, 0xe0, 0x90 } };

#define GPU_PROCESS_TIMEOUT		(2 * 60 * 1000)	// ms without being listed before a process state is dropped
#define GPU_ADAPTER_CHECK		(60 * 1000)		// ms between checks that the adapter LUIDs are still valid

//////////////////////////////////////////////////////////////////////////
// CGpuMonitor

CGpuMonitor::CGpuMonitor()
{
}

CGpuMonitor::~CGpuMonitor()
{
	Reset();
}

bool CGpuMonitor::ResolveFunctions()
{
	if (m_bResolved)
		return m_pQueryStatistics && m_pQueryAdapterInfo && m_pCloseAdapter && (m_pEnumAdapters2 || m_pEnumAdapters3);
	m_bResolved = true;

	HMODULE hGdi = LoadLibraryW(L"gdi32.dll");
	if (!hGdi)
		return false;

	m_pEnumAdapters2 = GetProcAddress(hGdi, "D3DKMTEnumAdapters2");
	m_pEnumAdapters3 = GetProcAddress(hGdi, "D3DKMTEnumAdapters3");
	m_pQueryAdapterInfo = GetProcAddress(hGdi, "D3DKMTQueryAdapterInfo");
	m_pQueryStatistics = GetProcAddress(hGdi, "D3DKMTQueryStatistics");
	m_pCloseAdapter = GetProcAddress(hGdi, "D3DKMTCloseAdapter");
	m_pEnumProcesses = GetProcAddress(hGdi, "D3DKMTEnumProcesses");		// Windows 11
	m_pGetProcessList = GetProcAddress(hGdi, "D3DKMTGetProcessList");

	return m_pQueryStatistics && m_pQueryAdapterInfo && m_pCloseAdapter && (m_pEnumAdapters2 || m_pEnumAdapters3);
}

void CGpuMonitor::Reset()
{
	m_Adapters.clear();
	m_TotalNodes = 0;
	for (int i = 0; i < eTypeMax; i++)
		m_NodeCount[i] = 0;

	for (auto& I : m_Processes) {
		if (I.second.bOwnHandle && I.second.hProcess)
			CloseHandle(I.second.hProcess);
	}
	m_Processes.clear();
}

uint32 CGpuMonitor::GetAdapterCount(EType Type) const
{
	uint32 Count = 0;
	for (const SAdapter& Adapter : m_Adapters) {
		if (Adapter.Type == Type)
			Count++;
	}
	return Count;
}

static NTSTATUS QueryAdapterInfo(void* pFunc, uint32 hAdapter, KMTQUERYADAPTERINFOTYPE Type, void* pData, uint32 Size)
{
	D3DKMT_QUERYADAPTERINFO Query;
	memset(&Query, 0, sizeof(Query));
	Query.hAdapter = hAdapter;
	Query.Type = Type;
	Query.pPrivateDriverData = pData;
	Query.PrivateDriverDataSize = Size;
	return ((P_D3DKMTQueryAdapterInfo)pFunc)(&Query);
}

void CGpuMonitor::CloseAdapter(uint32 hAdapter)
{
	if (!hAdapter || !m_pCloseAdapter)
		return;
	D3DKMT_CLOSEADAPTER Close;
	memset(&Close, 0, sizeof(Close));
	Close.hAdapter = hAdapter;
	((P_D3DKMTCloseAdapter)m_pCloseAdapter)(&Close);
}

bool CGpuMonitor::IsSoftwareDevice(uint32 hAdapter)
{
	D3DKMT_ADAPTERTYPE Type;
	memset(&Type, 0, sizeof(Type));
	if (NT_SUCCESS(QueryAdapterInfo(m_pQueryAdapterInfo, hAdapter, KMTQAITYPE_ADAPTERTYPE, &Type, sizeof(Type))))
		return Type.SoftwareDevice != 0;
	return false;
}

std::wstring CGpuMonitor::QueryName(uint32 hAdapter)
{
	D3DKMT_ADAPTERREGISTRYINFO Info;
	memset(&Info, 0, sizeof(Info));
	if (NT_SUCCESS(QueryAdapterInfo(m_pQueryAdapterInfo, hAdapter, KMTQAITYPE_ADAPTERREGISTRYINFO, &Info, sizeof(Info)))) {
		Info.AdapterString[MAX_PATH - 1] = 0;
		return Info.AdapterString;
	}
	return std::wstring();
}

bool CGpuMonitor::QueryAttributes(uint32 hAdapter, SAttributes& Attributes)
{
	//
	// System Informer, plugins/ExtendedTools/utils.c EtQueryAdapterAttributes: the adapter
	// registry key holds the DXCore attribute GUIDs, "DXCoreAttributes" on current systems,
	// "DXAttributes" on older ones.
	//
	static const wchar_t* Names[] = { L"DXCoreAttributes", L"DXAttributes" };

	for (const wchar_t* Name : Names)
	{
		std::vector<BYTE> Buffer(FIELD_OFFSET(D3DDDI_QUERYREGISTRY_INFO, OutputString) + MAX_PATH * sizeof(WCHAR));
		D3DDDI_QUERYREGISTRY_INFO* pInfo = (D3DDDI_QUERYREGISTRY_INFO*)Buffer.data();
		pInfo->QueryType = D3DDDI_QUERYREGISTRY_ADAPTERKEY;
		pInfo->QueryFlags.TranslatePath = 1;
		pInfo->ValueType = REG_MULTI_SZ;
		wcscpy_s(pInfo->ValueName, Name);

		if (!NT_SUCCESS(QueryAdapterInfo(m_pQueryAdapterInfo, hAdapter, KMTQAITYPE_QUERYREGISTRY, pInfo, (uint32)Buffer.size())))
			continue;

		if (pInfo->Status == D3DDDI_QUERYREGISTRY_STATUS_BUFFER_OVERFLOW)
		{
			Buffer.resize(FIELD_OFFSET(D3DDDI_QUERYREGISTRY_INFO, OutputString) + pInfo->OutputValueSize + sizeof(WCHAR));
			pInfo = (D3DDDI_QUERYREGISTRY_INFO*)Buffer.data();
			pInfo->Status = D3DDDI_QUERYREGISTRY_STATUS_SUCCESS;
			if (!NT_SUCCESS(QueryAdapterInfo(m_pQueryAdapterInfo, hAdapter, KMTQAITYPE_QUERYREGISTRY, pInfo, (uint32)Buffer.size())))
				continue;
		}

		if (pInfo->Status != D3DDDI_QUERYREGISTRY_STATUS_SUCCESS)
			continue;

		// a multi string of GUIDs, with or without braces
		const WCHAR* pStr = pInfo->OutputString;
		const WCHAR* pEnd = (const WCHAR*)((const BYTE*)pInfo->OutputString + pInfo->OutputValueSize);
		while (pStr < pEnd && *pStr)
		{
			size_t Len = wcsnlen(pStr, pEnd - pStr);
			std::wstring Str(pStr, Len);
			pStr += Len + 1;

			if (!Str.empty() && Str.front() == L'{') Str.erase(0, 1);
			if (!Str.empty() && Str.back() == L'}') Str.pop_back();

			GUID Guid;
			if (UuidFromStringW((RPC_WSTR)Str.c_str(), &Guid) != RPC_S_OK)
				continue;

			if (Guid == MP_DXCORE_HARDWARE_TYPE_ATTRIBUTE_GPU)						Attributes.TypeGpu = true;
			else if (Guid == MP_DXCORE_HARDWARE_TYPE_ATTRIBUTE_COMPUTE_ACCELERATOR)	Attributes.TypeComputeAccelerator = true;
			else if (Guid == MP_DXCORE_HARDWARE_TYPE_ATTRIBUTE_NPU)					Attributes.TypeNpu = true;
			else if (Guid == MP_DXCORE_HARDWARE_TYPE_ATTRIBUTE_MEDIA_ACCELERATOR)	Attributes.TypeMediaAccelerator = true;
			else if (Guid == MP_DXCORE_ADAPTER_ATTRIBUTE_D3D11_GRAPHICS)			Attributes.D3D11Graphics = true;
			else if (Guid == MP_DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS)			Attributes.D3D12Graphics = true;
			else if (Guid == MP_DXCORE_ADAPTER_ATTRIBUTE_D3D12_CORE_COMPUTE)		Attributes.D3D12CoreCompute = true;
		}
		return true;
	}
	return false;
}

bool CGpuMonitor::Init()
{
	Reset();

	if (!ResolveFunctions())
		return false;

	//
	// D3DKMTEnumAdapters3 (Windows 10 2004+) is the only enumeration that lists compute only
	// adapters, which is what NPUs are. The handles it hands out are ours to close.
	//
	std::vector<D3DKMT_ADAPTERINFO> Infos;
	if (m_pEnumAdapters3)
	{
		D3DKMT_ENUMADAPTERS3 Enum;
		memset(&Enum, 0, sizeof(Enum));
		Enum.Filter.IncludeComputeOnly = 1;
		Enum.Filter.IncludeDisplayOnly = 1;
		if (NT_SUCCESS(((P_D3DKMTEnumAdapters3)m_pEnumAdapters3)(&Enum)) && Enum.NumAdapters > 0)
		{
			Infos.resize(Enum.NumAdapters + 4);
			Enum.NumAdapters = (ULONG)Infos.size();
			Enum.pAdapters = Infos.data();
			if (NT_SUCCESS(((P_D3DKMTEnumAdapters3)m_pEnumAdapters3)(&Enum)))
				Infos.resize(Enum.NumAdapters);
			else
				Infos.clear();
		}
	}
	if (Infos.empty() && m_pEnumAdapters2)
	{
		D3DKMT_ENUMADAPTERS2 Enum;
		memset(&Enum, 0, sizeof(Enum));
		if (NT_SUCCESS(((P_D3DKMTEnumAdapters2)m_pEnumAdapters2)(&Enum)) && Enum.NumAdapters > 0)
		{
			Infos.resize(Enum.NumAdapters + 4);
			Enum.NumAdapters = (ULONG)Infos.size();
			Enum.pAdapters = Infos.data();
			if (NT_SUCCESS(((P_D3DKMTEnumAdapters2)m_pEnumAdapters2)(&Enum)))
				Infos.resize(Enum.NumAdapters);
			else
				Infos.clear();
		}
	}

	for (const D3DKMT_ADAPTERINFO& Info : Infos)
	{
		uint32 hAdapter = Info.hAdapter;

		bool bDuplicate = false;
		for (const SAdapter& Known : m_Adapters) {
			if (Known.Luid.LowPart == Info.AdapterLuid.LowPart && Known.Luid.HighPart == Info.AdapterLuid.HighPart)
				bDuplicate = true;
		}

		if (!bDuplicate && !IsSoftwareDevice(hAdapter))
		{
			//
			// Classification as in System Informer: an adapter is an NPU when it says so and
			// nothing else, a GPU unless it is positively NPU or media only. Older drivers
			// do not carry the attributes at all, those count as GPUs.
			//
			SAttributes Attr;
			bool bValid = QueryAttributes(hAdapter, Attr);
			bool bGraphics = Attr.TypeGpu || Attr.TypeComputeAccelerator || Attr.D3D11Graphics || Attr.D3D12Graphics || Attr.D3D12CoreCompute;

			EType Type = eTypeMax;
			if (bValid && Attr.TypeNpu && !Attr.TypeGpu && !Attr.TypeComputeAccelerator)
				Type = eNpu;
			else if (!bValid || bGraphics || !(Attr.TypeNpu || Attr.TypeMediaAccelerator))
				Type = eGpu;

			if (Type != eTypeMax)
			{
				D3DKMT_QUERYSTATISTICS Stats;
				memset(&Stats, 0, sizeof(Stats));
				Stats.Type = D3DKMT_QUERYSTATISTICS_ADAPTER;
				Stats.AdapterLuid = Info.AdapterLuid;
				if (NT_SUCCESS(((P_D3DKMTQueryStatistics)m_pQueryStatistics)(&Stats)) && Stats.QueryResult.AdapterInformation.NodeCount > 0)
				{
					SAdapter Adapter;
					Adapter.Luid = Info.AdapterLuid;
					Adapter.Type = Type;
					Adapter.NodeCount = Stats.QueryResult.AdapterInformation.NodeCount;
					Adapter.FirstNode = m_TotalNodes;
					Adapter.Name = QueryName(hAdapter);
					m_Adapters.push_back(Adapter);

					m_TotalNodes += Adapter.NodeCount;
					m_NodeCount[Type] += Adapter.NodeCount;
				}
			}
		}

		CloseAdapter(hAdapter);
	}

	if (m_Logger) {
		for (const SAdapter& Adapter : m_Adapters) {
			wchar_t Msg[512];
			swprintf_s(Msg, L"Usage statistics: monitoring %s \"%s\" with %u node(s)", Adapter.Type == eNpu ? L"NPU" : L"GPU", Adapter.Name.c_str(), Adapter.NodeCount);
			m_Logger(Msg);
		}
		if (m_Adapters.empty())
			m_Logger(L"Usage statistics: no GPU or NPU adapter found, GPU usage is not recorded");
	}

	m_LastCheck = GetTickCount64();
	return !m_Adapters.empty();
}

bool CGpuMonitor::CheckAdapters()
{
	//
	// The kernel assigns a new LUID when an adapter restarts (driver update, disable and
	// enable), statistics for the old one fail from then on. (System Informer, #3115)
	//
	for (const SAdapter& Adapter : m_Adapters)
	{
		D3DKMT_QUERYSTATISTICS Stats;
		memset(&Stats, 0, sizeof(Stats));
		Stats.Type = D3DKMT_QUERYSTATISTICS_ADAPTER;
		Stats.AdapterLuid = Adapter.Luid;
		if (!NT_SUCCESS(((P_D3DKMTQueryStatistics)m_pQueryStatistics)(&Stats)))
			return false;
	}
	return true;
}

HANDLE CGpuMonitor::GetProcessHandle(uint32 Pid, uint64 Now)
{
	SProcessState& State = m_Processes[Pid];
	State.LastSeen = Now;
	if (!State.hProcess)
	{
		State.hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, Pid);
		if (!State.hProcess)
			State.hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, Pid);
		State.bOwnHandle = State.hProcess != NULL;
	}
	return State.hProcess;
}

void CGpuMonitor::ListProcesses(SAdapter& Adapter, std::vector<std::pair<uint32, HANDLE>>& Processes, std::vector<HANDLE>& Temporary)
{
	uint64 Now = GetTickCount64();
	NTSTATUS status = STATUS_PROCEDURE_NOT_FOUND;

	//
	// Windows 11 lists the ids, we keep a handle per process. Before that dxgkrnl hands
	// out fresh handles with every call, those are used for this sample and closed.
	//
	if (m_pEnumProcesses)
	{
		std::vector<ULONG> Ids(Adapter.ProcessHint + 16);
		for (int Attempt = 0; Attempt < 4; Attempt++)
		{
			MP_D3DKMT_ENUM_PROCESS_LIST Request;
			memset(&Request, 0, sizeof(Request));
			Request.AdapterLuid = Adapter.Luid;
			Request.ProcessIdBuffer = Ids.data();
			Request.ProcessIdCount = Ids.size();
			status = ((P_D3DKMTEnumProcesses)m_pEnumProcesses)(&Request);
			if (status == STATUS_BUFFER_TOO_SMALL) {
				Ids.resize(Request.ProcessIdCount + 16);
				continue;
			}
			if (NT_SUCCESS(status)) {
				Ids.resize(Request.ProcessIdCount);
				Adapter.ProcessHint = Ids.size();
				for (ULONG Pid : Ids) {
					if (HANDLE hProcess = GetProcessHandle(Pid, Now))
						Processes.push_back(std::make_pair((uint32)Pid, hProcess));
				}
			}
			break;
		}
		if (NT_SUCCESS(status))
			return;
	}

	if (m_pGetProcessList)
	{
		std::vector<HANDLE> Handles(Adapter.ProcessHint + 16);
		for (int Attempt = 0; Attempt < 4; Attempt++)
		{
			MP_D3DKMT_GET_PROCESS_LIST Request;
			memset(&Request, 0, sizeof(Request));
			Request.AdapterLuid = Adapter.Luid;
			Request.DesiredAccess = PROCESS_QUERY_INFORMATION;
			Request.ProcessHandleCount = (ULONG)Handles.size();
			Request.ProcessHandle = Handles.data();
			status = ((P_D3DKMTGetProcessList)m_pGetProcessList)(&Request);
			if (status == STATUS_BUFFER_TOO_SMALL) {
				Handles.resize(Request.ProcessHandleCount + 16);
				continue;
			}
			if (NT_SUCCESS(status)) {
				Handles.resize(Request.ProcessHandleCount);
				Adapter.ProcessHint = Handles.size();
				for (HANDLE hProcess : Handles) {
					if (!hProcess) continue;
					Temporary.push_back(hProcess);
					uint32 Pid = GetProcessId(hProcess);
					if (Pid) {
						m_Processes[Pid].LastSeen = Now;
						Processes.push_back(std::make_pair(Pid, hProcess));
					}
				}
			}
			break;
		}
	}
}

void CGpuMonitor::EvictProcesses(uint64 Now)
{
	for (auto I = m_Processes.begin(); I != m_Processes.end(); )
	{
		if (Now - I->second.LastSeen > GPU_PROCESS_TIMEOUT) {
			if (I->second.bOwnHandle && I->second.hProcess)
				CloseHandle(I->second.hProcess);
			I = m_Processes.erase(I);
		}
		else
			++I;
	}
}

void CGpuMonitor::Sample(std::map<uint32, SProcessUsage>& Usage)
{
	if (m_Adapters.empty())
		return;

	uint64 Now = GetTickCount64();
	if (Now - m_LastCheck > GPU_ADAPTER_CHECK)
	{
		m_LastCheck = Now;
		if (!CheckAdapters()) {
			Init(); // re-enumerate, the process states start over
			if (m_Adapters.empty())
				return;
		}
		EvictProcesses(Now);
	}

	for (SAdapter& Adapter : m_Adapters)
	{
		std::vector<std::pair<uint32, HANDLE>> Processes;
		std::vector<HANDLE> Temporary;
		ListProcesses(Adapter, Processes, Temporary);

		for (const auto& Process : Processes)
		{
			SProcessState& State = m_Processes[Process.first];
			if (State.NodeTime.size() != m_TotalNodes) {
				State.NodeTime.assign(m_TotalNodes, 0);
				State.bInitialized = false;
			}

			uint64 Delta = 0;
			for (uint32 Node = 0; Node < Adapter.NodeCount; Node++)
			{
				D3DKMT_QUERYSTATISTICS Stats;
				memset(&Stats, 0, sizeof(Stats));
				Stats.Type = D3DKMT_QUERYSTATISTICS_PROCESS_NODE;
				Stats.AdapterLuid = Adapter.Luid;
				Stats.hProcess = Process.second;
				Stats.QueryProcessNode.NodeId = Node;
				if (!NT_SUCCESS(((P_D3DKMTQueryStatistics)m_pQueryStatistics)(&Stats)))
					continue;

				uint64 RunningTime = (uint64)Stats.QueryResult.ProcessNodeInformation.RunningTime.QuadPart;
				uint64& Last = State.NodeTime[Adapter.FirstNode + Node];
				if (State.bInitialized) {
					// a smaller value means the id was reused by a new process, its time so far is the delta
					Delta += RunningTime >= Last ? RunningTime - Last : RunningTime;
				}
				Last = RunningTime;
			}

			//
			// The first sample of a process only records the base values, so a process that
			// is seen for the first time does not get its whole lifetime booked into one bucket.
			// Initialization completes once every adapter was visited at least once; a process
			// that uses only one adapter is initialized by that adapter alone.
			//
			if (State.bInitialized && Delta && m_NodeCount[Adapter.Type])
				Usage[Process.first].Time[Adapter.Type] += Delta / m_NodeCount[Adapter.Type];
		}

		for (HANDLE hProcess : Temporary)
			CloseHandle(hProcess);
	}

	for (auto& I : m_Processes) {
		if (!I.second.NodeTime.empty())
			I.second.bInitialized = true;
	}
}
