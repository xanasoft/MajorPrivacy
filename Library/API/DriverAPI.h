#pragma once
#include "../Types.h"
#include "../Status.h"
#include "../lib_global.h"
#include "../Common/Buffer.h"
#include "../Common/Variant.h"
#include "../../Library/API/PrivacyDefs.h"

struct SProcessInfo
{
	std::wstring ImageName;
	std::wstring FileName;
	uint64 CreateTime = 0;
	//uint64 SeqNr = 0;
	uint64 ParentPid = 0;
	//uint64 ParentSeqN = 0r;
	//std::vector<uint8> FileHash;
	uint64 EnclaveId = 0;
	uint32 SecState = 0;
	uint32 Flags = 0;
	uint32 SecFlags = 0;
	uint32 NumberOfImageLoads = 0;
	uint32 NumberOfMicrosoftImageLoads = 0;
	uint32 NumberOfAntimalwareImageLoads = 0;
	uint32 NumberOfVerifiedImageLoads = 0;
	uint32 NumberOfSignedImageLoads = 0;
	uint32 NumberOfUntrustedImageLoads = 0;
};

typedef std::shared_ptr<SProcessInfo> SProcessInfoPtr;

struct SHandleInfo
{
	std::wstring Path;
	std::wstring Type;
};

typedef std::shared_ptr<SHandleInfo> SHandleInfoPtr;

/*struct SEnclaveInfo
{
	uint64 EnclaveID;
};

typedef std::shared_ptr<SEnclaveInfo> SEnclaveInfoPtr;*/

struct SProcessEvent
{
	enum class EType {
		Unknown = 0,
		ProcessStarted,
		ProcessStopped,
		ImageLoad,
		UntrustedLoad,
		ProcessAccess,
		ThreadAccess,
		ResourceAccess,
	};

	EType Type = EType::Unknown;
	uint64 TimeStamp = 0;
};

struct SProcessEventEx : public SProcessEvent
{
	uint64 ActorProcessId = 0;
	uint64 ActorThreadId = 0;
	std::wstring ActorServiceTag;
};

struct SProcessStartEvent : public SProcessEventEx
{
	uint64 ProcessId = 0;
	uint64 ParentId = 0;
	std::wstring FileName;
	std::wstring CommandLine;
	sint32 CreationStatus = 0;
	EEventStatus Status = EEventStatus::eUndefined;
};

struct SProcessStopEvent : public SProcessEvent
{
	uint64 ProcessId = 0;
	uint32 ExitCode = 0;
};

struct SProcessImageEvent : public SProcessEventEx
{
	uint64 ProcessId = 0;
	std::wstring FileName;
	bool bLoadPrevented = false;
	uint32 ImageProperties = 0;
	uint64 ImageBase = 0;
	uint32 ImageSelector = 0;
	uint32 ImageSectionNumber = 0;
	KPH_VERIFY_AUTHORITY SignAuthority = KphUntestedAuthority;
	uint32 SignLevel = 0;
	uint32 SignPolicy = 0;
};

struct SProcessAccessEvent : public SProcessEventEx
{
	uint64 ProcessId = 0;
	//bool bThread = false;
	uint32 AccessMask = 0;
	EEventStatus Status = EEventStatus::eUndefined;
};

struct SResourceAccessEvent : public SProcessEventEx
{
	std::wstring Path;
	uint32 AccessMask = 0;
	EEventStatus Status = EEventStatus::eUndefined;
	NTSTATUS NtStatus = 0;
};

typedef std::shared_ptr<SProcessEvent> SProcessEventPtr;

struct SUserKeyInfo
{
	CBuffer PubKey;
	CBuffer EncryptedBlob;
	bool bLocked = false;
};

typedef std::shared_ptr<SUserKeyInfo> SUserKeyInfoPtr;

class LIBRARY_EXPORT CDriverAPI
{
public:
	enum EInterface {
		eFltPort = 0,
		eDevice,
	};
	CDriverAPI(EInterface Interface = eFltPort);
	virtual ~CDriverAPI();

	STATUS InstallDrv(uint32 TraceLogLevel = 0);
	STATUS ConnectDrv();
	bool IsConnected();
	STATUS Reconnect();
	void Disconnect();

	RESULT(CVariant) Call(uint32 MessageId, const CVariant& Message);

	uint32 GetABIVersion();

	RESULT(std::shared_ptr<std::vector<uint64>>) EnumProcesses();
	RESULT(SProcessInfoPtr) GetProcessInfo(uint64 pid);

	RESULT(SHandleInfoPtr) GetHandleInfo(ULONG_PTR UniqueProcessId, ULONG_PTR HandleValue);

	RESULT(std::shared_ptr<std::vector<uint64>>) EnumEnclaves();
	//RESULT(SEnclaveInfoPtr) GetEnclaveInfo(uint64 eid);

	STATUS StartProcessInEnvlave(const std::wstring& path, uint64 EnclaveId = 0);

	STATUS SetUserKey(const CBuffer& PubKey, const CBuffer& EncryptedBlob, bool bLock = false);
	RESULT(SUserKeyInfoPtr) GetUserKey();
	STATUS ClearUserKey(const CBuffer& Signature);

	STATUS GetChallenge(CBuffer& Challenge);

	//STATUS SetupRuleAlias(const std::wstring& PrefixPath, const std::wstring& DevicePath);
	//STATUS ClearRuleAlias(const std::wstring& DevicePath);

	STATUS RegisterForProcesses(bool bRegister = true);
	void RegisterProcessHandler(const std::function<sint32(const SProcessEvent* pEvent)>& Handler);
    template<typename T, class C>
    void RegisterProcessHandler(T Handler, C This) { RegisterProcessHandler(std::bind(Handler, This, std::placeholders::_1)); }

	STATUS RegisterForRuleEvents(ERuleType Type, bool bRegister = true);
	void RegisterRuleEventHandler(ERuleType Type, const std::function<void(const std::wstring& Guid, ERuleEvent Event, ERuleType Type, uint64 PID)>& Handler);
	template<typename T, class C>
	void RegisterRuleEventHandler(ERuleType Type, T Handler, C This) { RegisterRuleEventHandler(Type, std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)); }

	void TestDrv();

protected:
	friend sint32 CDriverAPI__EmitProcess(CDriverAPI* This, const SProcessEvent* pEvent);
	friend void CDriverAPI__EmitRuleEvent(CDriverAPI* This, ERuleType Type, const std::wstring& Guid, ERuleEvent Event, uint64 PID);

	std::mutex m_HandlersMutex;
	std::vector<std::function<sint32(const SProcessEvent* pEvent)>> m_ProcessHandlers;
	std::map<ERuleType, std::function<void(const std::wstring& Guid, ERuleEvent Event, ERuleType Type, uint64 PID)>> m_RuleEventHandlers;

	EInterface m_Interface;
	class CAbstractClient* m_pClient;
};

