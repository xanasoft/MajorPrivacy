#pragma once


/////////////////////////////////////////////////////////////////////////////
// Program ID
//

enum class EProgramType // API_S_PROG_TYPE
{
	eUnknown = 0,

	eProgramFile		= 'exe',
	eFilePattern		= 'pat',
	eAppInstallation	= 'inst',
	eWindowsService		= 'svc',
	eAppPackage			= 'app',
	eProgramSet			= 'set',
	eProgramList		= 'list',
	eProgramGroup		= 'grp',
	eAllPrograms		= 'all',
	eProgramRoot		= 'root',
};

/////////////////////////////////////////////////////////////////////////////
// Program Rules
//

enum class EExecRuleType // API_S_EXEC_RULE_ACTION
{
	eUnknown = 0,
	eAllow,
	eBlock,
	eProtect, // put in a secrure enclave
	eIsolate, // sandbox
	eAudit,
	eMax
};

enum class EProgramOnSpawn // API_S_EXEC_ON_SPAWN
{
	eUnknown = 0,
	eAllow,
	eBlock,
	eEject
};

#ifndef KERNEL_MODE // todo
enum KPH_VERIFY_AUTHORITY // API_S_... TODO
{
	KphUntestedAuthority = 0,
	KphNoAuthority = 1,
	KphUnkAuthority = 2,
	// 3
	// 4
	KphMsCodeAuthority = 5,
	//KphStoreAuthority = 6,
	// 7
	KphAvAuthority = 8,
	KphMsAuthority = 9,
	// 10
	// 11
	// 12
	// 13
	KphUserAuthority = 14,
	KphDevAuthority = 15,
	// inly 4 bit max is 15
};

enum KPH_PROTECTION_LEVEL // API_S_... TODO
{
	KphNoProtection = 0,
	KphFullProtection,
	KphLiteProtection
	// max is 3
};

union KPH_PROCESS_FLAGS // API_S_... TODO
{
	ULONG Flags;
	struct
	{
		ULONG CreateNotification : 1;
		ULONG ExitNotification : 1;
		ULONG VerifiedProcess : 1;
		ULONG SecurelyCreated : 2;
		ULONG Protected : 1;
		ULONG IsWow64 : 1;
		ULONG IsSubsystemProcess : 1;
		ULONG AllocatedImageName : 1;
		ULONG SystemAllocatedImageFileName : 1;
		ULONG Reserved : 22;
	};
};

union KPH_PROCESS_SFLAGS // API_S_... TODO
{
	ULONG SecFlags;
	struct
	{
		ULONG DenyStart : 1;
		ULONG AllowDebugging : 1;
		ULONG DisableImageLoadProtection : 1;
		ULONG SignatureAuthority : 4;
		ULONG SignatureRequirement : 4;
		ULONG ProtectionLevel : 2;
		ULONG EjectFromEnclave : 1;
		ULONG UntrustedSpawn: 1;
		ULONG AuditEnabled : 1;
		//ULONG HasVerdict : 1;
		//ULONG WasTainted : 1;
		ULONG SecReserved : 16;
	};
};

#define KPH_VERIFY_FLAG_FILE_SIG_OK     0x00000001
#define KPH_VERIFY_FLAG_CERT_SIG_OK     0x00000002
#define KPH_VERIFY_FLAG_FILE_SIG_FAIL   0x00000004
#define KPH_VERIFY_FLAG_CERT_SIG_FAIL   0x00000008
#define KPH_VERIFY_FLAG_SIG_MASK        0x0000000F
#define KPH_VERIFY_FLAG_FILE_SIG_DB     0x00000010
#define KPH_VERIFY_FLAG_CI_SIG_DUMMY    0x00000020
#define KPH_VERIFY_FLAG_CI_SIG_OK       0x00000040
#define KPH_VERIFY_FLAG_CI_SIG_FAIL     0x00000080
#define KPH_VERIFY_FLAG_COHERENCY_FAIL  0x00000100

#endif


/////////////////////////////////////////////////////////////////////////////
// Access Rules 
//

enum class EAccessRuleType // API_S_ACCESS_RULE_ACTION
{
	eNone = 0,
	eAllow,
	eAllowRO,
	eEnum,
	eProtect, // same as eBlock but shows a prompt
	eBlock,
	eIgnore, // same as eNone but dont log 
};

/////////////////////////////////////////////////////////////////////////////
// ExecLog
//

enum class EExecLogRole // API_S_... TODO
{
	eUndefined = 0,
	eActor,
	eTarget,
	eBooth,
};

enum class EExecLogType // API_S_... TODO
{
	eUnknown = 0,
	eProcessStarted,
	eImageLoad,
	eProcessAccess,
	eThreadAccess,
};

/////////////////////////////////////////////////////////////////////////////
// Event Status
//

enum class EEventStatus // API_S_... TODO
{
	eUndefined = -1,
	eAllowed = 0,
	eUntrusted, // but allowed
	eEjected,	// but allowed bur ejected from enclave
	eBlocked,
	eProtected, // blocked but shows a prompt
};

/////////////////////////////////////////////////////////////////////////////
// Program Info
//

union UCISignInfo // API_S_... TODO
{
	uint64					Data = 0;
	struct {
		uint8				Authority;	// 3 bits - KPH_VERIFY_AUTHORITY
		uint8				Level;		// 4 bits
		uint8				Reserved1;
		uint8				Reserved2;
		uint32				Policy;
	};
};

enum class EHashStatus // API_S_... TODO
{
	eHashUnknown = 0,
	eHashOk,
	eHashFail,
	eHashDummy,
	eHashNone
};


/////////////////////////////////////////////////////////////////////////////
// Tweaks
//

//enum class ETweakPreset
//{
//    eGroup = -1,    // tweak is group
//    eNotSet = 0,    // not set
//    eSet,           // recomended
//    eSetCustom,     // group has custom preset
//    eSetAll         // all in group set
//};

enum class ETweakStatus // API_S_TWEAK_STATUS
{
	eGroup = -1,    // tweak is group
	eNotSet = 0,    // tweak is not set
	eApplied,       // tweak was not set by user but is applied
	eSet,           // tweak is set
	eMissing        // tweak was set but is not applied
};

enum class ETweakHint // API_S_TWEAK_HINT
{
	eNone = 0,
	eRecommended,

	eMax
};

enum class ETweakMode // API_S_... TODO
{
	eDefault    = 0,
	eAll        = 1,
	eDontSet    = 2,

	eMax
};

enum class ETweakType // API_S_TWEAK_TYPE
{
	eUnknown,

	eGroup,
	eSet,

	eReg,
	eGpo,
	eSvc,
	eTask,
	eFS,
	eExec,
	eFw,

	eMax
};


/////////////////////////////////////////////////////////////////////////////
// 
//

enum class ERuleType // API_S_... TODO
{
	eUnknown = 0,
	eProgram,
	eAccess,
	eFirewall,
	eMax,
};

enum class EConfigEvent // API_S_... TODO
{
	eUnknown = 0,
	eAdded,
	eModified,
	eRemoved,
	eAllChanged,
};

enum class ETraceLogs // API_S_... TODO
{
	eExecLog = 0,
	eNetLog,
	eResLog,
	eLogMax
};


/////////////////////////////////////////////////////////////////////////////
// Serializer Options
//

struct SVarWriteOpt
{
	enum EFormat : uint8
	{
		eInvalid = 0,
		eIndex,
		eMap,
	} Format = eIndex;

	uint8 Reserved1 = 0;

	enum EFlags : uint16
	{
		eNone		= 0x00,
		eSaveAll	= 0x01,
		eSaveNtPaths = 0x02,
		eSaveToFile = 0x04,
		//			= 0x08,
		eTextGuids	= 0x10,
		//			= 0x20,
		//			= 0x40,
		//			= 0x80,
	};
	uint16 Flags = eNone;
};


/////////////////////////////////////////////////////////////////////////////
// 
//


#define SYSTEM_ENCLAVE	"{FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF}"
#define NULL_GUID		"{00000000-0000-0000-0000-000000000000}"