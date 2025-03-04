#pragma once
#include "../Types.h"
#include "../Status.h"
#include "../lib_global.h"
#include "../Common/Buffer.h"
#include "../Common/Variant.h"
#include "../../Library/API/PrivacyDefs.h"
#include "../../Library/Common/FlexGuid.h"

struct SProcessInfo
{
	std::wstring ImageName;
	std::wstring FileName;
	uint64 CreateTime = 0;
	//uint64 SeqNr = 0;
	uint64 ParentPid = 0;
	//uint64 ParentSeqN = 0r;
	//std::vector<uint8> FileHash;
	std::wstring Enclave;
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

		InjectionRequest,
	};

	EType Type = EType::Unknown;
	uint64 TimeStamp = 0;
};

struct SInjectionRequest : public SProcessEvent
{
	uint64 ProcessId = 0;
};


struct SProcessEventEx : public SProcessEvent
{
	uint64 ActorProcessId = 0;
	uint64 ActorThreadId = 0;
	std::wstring ActorServiceTag;
};

struct SVerifierInfo
{
	uint32 VerificationFlags = 0;
	KPH_VERIFY_AUTHORITY SignAuthority = KphUntestedAuthority;
	uint32 SignLevel = 0;
	uint32 SignPolicy = 0;
	uint32 FileHashAlgorithm = 0;
	std::vector<uint8> FileHash;
	uint32 SignerHashAlgorithm = 0;
	std::vector<uint8> SignerHash;
	std::string SignerName;

	void ReadFromEvent(const CVariant& Event);
};

struct SProcessStartEvent : public SProcessEventEx
{
	uint64 ProcessId = 0;
	uint64 ParentId = 0;
	std::wstring EnclaveId;
	std::wstring FileName;
	std::wstring CommandLine;
	SVerifierInfo VerifierInfo;
	sint32 CreationStatus = 0;
	EEventStatus Status = EEventStatus::eUndefined;
};

struct SProcessStopEvent : public SProcessEvent
{
	uint64 ProcessId = 0;
	std::wstring EnclaveId;
	uint32 ExitCode = 0;
};

struct SProcessImageEvent : public SProcessEventEx
{
	uint64 ProcessId = 0;
	std::wstring EnclaveId;
	std::wstring FileName;
	bool bLoadPrevented = false;
	//uint32 ImageProperties = 0;
	uint64 ImageBase = 0;
	//uint32 ImageSelector = 0;
	//uint32 ImageSectionNumber = 0;
	SVerifierInfo VerifierInfo;
};

struct SProcessAccessEvent : public SProcessEventEx
{
	uint64 ProcessId = 0;
	std::wstring EnclaveId;
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
	bool IsDirectory = false;
};

typedef std::shared_ptr<SProcessEvent> SProcessEventPtr;

struct SUserKeyInfo
{
	CBuffer PubKey;
	CBuffer EncryptedBlob;
	bool bLocked = false;
};

typedef std::shared_ptr<SUserKeyInfo> SUserKeyInfoPtr;

enum class EConfigGroup
{
	eUndefined = 0,
	eEnclaves,
	eProgramRules,
	eAccessRules,
	eFirewallRules
};

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
	void Disconnect();

	RESULT(CVariant) Call(uint32 MessageId, const CVariant& Message);

	uint32 GetABIVersion();

	RESULT(std::shared_ptr<std::vector<uint64>>) EnumProcesses();
	RESULT(SProcessInfoPtr) GetProcessInfo(uint64 pid);

	RESULT(SHandleInfoPtr) GetHandleInfo(ULONG_PTR UniqueProcessId, ULONG_PTR HandleValue);

	STATUS PrepareEnclave(const CFlexGuid& EnclaveGuid);

	STATUS SetUserKey(const CBuffer& PubKey, const CBuffer& EncryptedBlob, bool bLock = false);
	RESULT(SUserKeyInfoPtr) GetUserKey();
	STATUS ClearUserKey(const CBuffer& ChallengeResponse);

	STATUS ProtectConfig(const CBuffer& ConfigSignature, bool bHardLock = false);
	STATUS UnprotectConfig(const CBuffer& ChallengeResponse);
	uint32 GetConfigStatus();
	STATUS UnlockConfig(const CBuffer& ChallengeResponse);
	STATUS CommitConfigChanges(const CBuffer& ConfigSignature = CBuffer());
	STATUS DiscardConfigChanges();

	STATUS GetChallenge(CBuffer& Challenge);
	STATUS GetConfigHash(CBuffer& ConfigHash, const CVariant& Data = CVariant());

	//STATUS SetupRuleAlias(const std::wstring& PrefixPath, const std::wstring& DevicePath);
	//STATUS ClearRuleAlias(const std::wstring& DevicePath);

	STATUS RegisterForProcesses(uint32 uEvents, bool bRegister = true);
	void RegisterProcessHandler(const std::function<sint32(const SProcessEvent* pEvent)>& Handler);
    template<typename T, class C>
    void RegisterProcessHandler(T Handler, C This) { RegisterProcessHandler(std::bind(Handler, This, std::placeholders::_1)); }

	STATUS RegisterForConfigEvents(EConfigGroup Config, bool bRegister = true);
	void RegisterConfigEventHandler(EConfigGroup Config, const std::function<void(const std::wstring& Guid, EConfigEvent Event, EConfigGroup Config, uint64 PID)>& Handler);
	template<typename T, class C>
	void RegisterConfigEventHandler(EConfigGroup Config, T Handler, C This) { RegisterConfigEventHandler(Config, std::bind(Handler, This, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4)); }

protected:
	friend sint32 CDriverAPI__EmitProcess(CDriverAPI* This, const SProcessEvent* pEvent);
	friend void CDriverAPI__EmitConfigEvent(CDriverAPI* This, EConfigGroup Config, const std::wstring& Guid, EConfigEvent Event, uint64 PID);

	std::mutex m_HandlersMutex;
	std::vector<std::function<sint32(const SProcessEvent* pEvent)>> m_ProcessHandlers;
	std::map<EConfigGroup, std::function<void(const std::wstring& Guid, EConfigEvent Event, EConfigGroup Config, uint64 PID)>> m_ConfigEventHandlers;

	EInterface m_Interface;
	class CAbstractClient* m_pClient;
};

