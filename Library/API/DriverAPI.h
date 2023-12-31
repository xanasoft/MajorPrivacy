#pragma once
#include "../Types.h"
#include "../Status.h"
#include "../lib_global.h"
#include "../Common/Buffer.h"

struct SProcessInfo
{
	std::wstring ImageName;
	std::wstring FileName;
	uint64 CreateTime;
	uint64 ParentPid;
	std::vector<uint8> FileHash;
	uint32 EnclaveId;
};

typedef std::shared_ptr<SProcessInfo> SProcessInfoPtr;

struct SProcessEvent
{
	enum class EType {
		Unknown = 0,
		Started,
		Stopped,
	};

	EType Type;
	uint64 ProcessId;
	uint64 CreateTime;
	uint64 ParentId;
	std::wstring FileName;
	std::wstring CommandLine;
	uint32 ExitCode;
};

typedef std::shared_ptr<SProcessEvent> SProcessEventPtr;

class LIBRARY_EXPORT CDriverAPI
{
public:
	CDriverAPI();
	virtual ~CDriverAPI();

	STATUS ConnectDrv();
	bool IsDrvConnected();
	void DisconnectDrv();

	RESULT(std::shared_ptr<std::vector<uint64>>) EnumProcesses();

	RESULT(SProcessInfoPtr) GetProcessInfo(uint64 pid);

	STATUS StartProcessInEnvlave(const std::wstring& path, uint32 EnclaveId = 0);

	STATUS RegisterForProcesses(bool bRegister = true);

	void RegisterProcessHandler(const std::function<sint32(const SProcessEvent* pEvent)>& Handler);
    template<typename T, class C>
    void RegisterProcessHandler(T Handler, C This) { RegisterProcessHandler(std::bind(Handler, This, std::placeholders::_1)); }

	void TestDrv();

protected:
	friend sint32 CDriverAPI__EmitProcess(CDriverAPI* This, const SProcessEvent* pEvent);

	std::vector<std::function<sint32(const SProcessEvent* pEvent)>> m_ProcessHandlers;
	std::mutex m_ProcessHandlersMutex;

	class CAbstractClient* m_pClient;
};

