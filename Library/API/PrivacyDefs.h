#pragma once


/////////////////////////////////////////////////////////////////////////////
// Program ID
//

enum class EProgramType
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

enum class EExecRuleType
{
	eUnknown = 0,
	eAllow,
	eBlock,
	eProtect,
	eIsolate,
	eSpecial,
	eMax
};

enum class EProgramOnSpawn
{
	eUnknown = 0,
	eAllow,
	eBlock,
	eEject
};

#ifndef KERNEL_MODE // todo
enum KPH_VERIFY_AUTHORITY
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

enum KPH_PROTECTION_LEVEL
{
	KphNoProtection = 0,
	KphFullProtection,
	KphLiteProtection
	// max is 3
};

union KPH_PROCESS_FLAGS
{
	ULONG Flags;
	struct
	{
		ULONG CreateNotification : 1;
		ULONG ExitNotification : 1;
		ULONG VerifiedProcess : 1;
		ULONG SecurelyCreated : 1;
		ULONG Protected : 1;
		ULONG IsLsass : 1;
		ULONG IsWow64 : 1;
		ULONG IsSubsystemProcess : 1;
		ULONG Reserved : 24;
	};
};

union KPH_PROCESS_SFLAGS
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
		ULONG SecReserved : 19;
	};
};

typedef struct _KPH_VERIFY_INFO
{
	UCHAR SigningLevel;
	ULONG PolicyBits;
} KPH_VERIFY_INFO, *PKPH_VERIFY_INFO;

#endif


/////////////////////////////////////////////////////////////////////////////
// Access Rules 
//

enum class EAccessRuleType
{
	eNone = 0,
	eAllow,
	eAllowRO,
	eEnum,
	eProtect, // same as eBlock but shows a prompt
	eBlock,
};

/////////////////////////////////////////////////////////////////////////////
// ExecLog
//

enum class EExecLogRole
{
	eUndefined = 0,
	eActor,
	eTarget,
	eBooth,
};

enum class EExecLogType
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

enum class EEventStatus
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

struct SLibraryInfo
{
	uint64						LastLoadTime = 0;
	uint32						TotalLoadCount = 0;
	union USign {
		uint64					Data = 0;
		struct {
			uint8				Authority;	// 3 bits - KPH_VERIFY_AUTHORITY
			uint8				Level;		// 4 bits
			uint8				Reserved1;
			uint8				Reserved2;
			uint32				Policy;
		};
	} Sign;
	EEventStatus				LastStatus = EEventStatus::eUndefined;
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

enum class ETweakStatus
{
	eGroup = -1,    // tweak is group
	eNotSet = 0,    // tweak is not set
	eApplied,       // tweak was not set by user but is applied
	eSet,           // tweak is set
	eMissing        // tweak was set but is not applied
};

enum class ETweakHint
{
	eNone = 0,
	eRecommended,

	eMax
};

enum class ETweakMode
{
	eDefault    = 0,
	eAll        = 1,
	eDontSet    = 2,

	eMax
};

enum class ETweakType
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

enum class ERuleType
{
	eUnknown = 0,
	eProgram,
	eAccess,
	eFirewall,
	eMax,
};

enum class ERuleEvent
{
	eUnknown = 0,
	eAdded,
	eModified,
	eRemoved,
	eAllChanged,
};

enum class ETraceLogs
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
	enum EFormat : uint32
	{
		eInvalid = 0,
		eIndex,
		eMap,
	} Format = eIndex;

	enum EFlags : uint32
	{
		eNone = 0,
		eSaveAll,
		eSaveNtPaths,
	};
	uint32 Flags = eNone;
};


/////////////////////////////////////////////////////////////////////////////
// 
//


#define SYSTEM_ENCLAVE_ID	1
