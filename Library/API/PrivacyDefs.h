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

union USignatures
{
	uint32 Value;
	struct {
		uint32 Windows : 1;		// Signed windows component
		uint32 Microsoft : 1;	// Signed by microsoft MS VS, Edge, Office etc
		uint32 Antimalware : 1;	// 3rd party Antimalware signed my Microsoft
		uint32 Authenticode : 1; // 3rd party signed
		uint32 Store : 1;		// 3rd from the windows store
		uint32 Developer : 1;	// Developer signed part of MajorPrivacy
		uint32 User : 1;		// Signet with User Key by User
		uint32 Enclave : 1;		// Signet with User Key by User for Enclave
		uint32 Reserved : 23;
		uint32 Collection : 1;	// for internal use indicates a collection tag match
	};
};

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

enum class EImageOnLoad
{
	eUnknown = 0,
	eAllow,
	eBlock,
	eAllowUntrusted
};


enum class EIntegrityLevel
{
	eUnknown = -1,
	eUntrusted = 0,
	eLow,
	eMedium,
	eMediumPlus,
	eHigh,
	eSystem
};

enum class EHashType {
	eUnknown = 0,
	eFileHash,
	eCertHash,
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
		ULONG DisableImageCoherencyChecking : 1;
		ULONG SignatureAuthority : 4;
		ULONG ProtectionLevel : 2;
		ULONG EjectFromEnclave : 1;
		ULONG UntrustedSpawn: 1;
		//ULONG AuditEnabled : 1;
		//ULONG HasVerdict : 1;
		//ULONG WasTainted : 1;
		ULONG SecReserved : 20;
	};
};

typedef enum _KPH_HASH_ALGORITHM
{
	KphHashAlgorithmMd5,
	KphHashAlgorithmSha1,
	KphHashAlgorithmSha1Authenticode,
	KphHashAlgorithmSha256,
	KphHashAlgorithmSha256Authenticode,
	KphHashAlgorithmSha384,
	KphHashAlgorithmSha512,
	MaxKphHashAlgorithm,
} KPH_HASH_ALGORITHM, *PKPH_HASH_ALGORITHM;

#define KPH_PROCESS_SECURELY_CREATED                     0x00000001ul
#define KPH_PROCESS_VERIFIED_PROCESS                     0x00000002ul
#define KPH_PROCESS_PROTECTED_PROCESS                    0x00000004ul
#define KPH_PROCESS_NO_UNTRUSTED_IMAGES                  0x00000008ul
#define KPH_PROCESS_HAS_FILE_OBJECT                      0x00000010ul
#define KPH_PROCESS_HAS_SECTION_OBJECT_POINTERS          0x00000020ul
#define KPH_PROCESS_NO_USER_WRITABLE_REFERENCES          0x00000040ul
#define KPH_PROCESS_NO_FILE_TRANSACTION                  0x00000080ul
#define KPH_PROCESS_NOT_BEING_DEBUGGED                   0x00000100ul
#define KPH_PROCESS_NO_WRITABLE_FILE_OBJECT              0x00000200ul

#define KPH_PROCESS_STATE_MAXIMUM (KPH_PROCESS_SECURELY_CREATED               |\
                                   KPH_PROCESS_VERIFIED_PROCESS               |\
                                   KPH_PROCESS_PROTECTED_PROCESS              |\
                                   KPH_PROCESS_NO_UNTRUSTED_IMAGES            |\
                                   KPH_PROCESS_HAS_FILE_OBJECT                |\
                                   KPH_PROCESS_HAS_SECTION_OBJECT_POINTERS    |\
                                   KPH_PROCESS_NO_USER_WRITABLE_REFERENCES    |\
                                   KPH_PROCESS_NO_FILE_TRANSACTION            |\
                                   KPH_PROCESS_NOT_BEING_DEBUGGED             |\
                                   KPH_PROCESS_NO_WRITABLE_FILE_OBJECT)

#define KPH_PROCESS_STATE_HIGH    (KPH_PROCESS_VERIFIED_PROCESS               |\
                                   KPH_PROCESS_PROTECTED_PROCESS              |\
                                   KPH_PROCESS_NO_UNTRUSTED_IMAGES            |\
                                   KPH_PROCESS_HAS_FILE_OBJECT                |\
                                   KPH_PROCESS_HAS_SECTION_OBJECT_POINTERS    |\
                                   KPH_PROCESS_NO_USER_WRITABLE_REFERENCES    |\
                                   KPH_PROCESS_NO_FILE_TRANSACTION            |\
                                   KPH_PROCESS_NOT_BEING_DEBUGGED             |\
                                   KPH_PROCESS_NO_WRITABLE_FILE_OBJECT)

#define KPH_PROCESS_STATE_MEDIUM  (KPH_PROCESS_VERIFIED_PROCESS               |\
                                   KPH_PROCESS_PROTECTED_PROCESS              |\
                                   KPH_PROCESS_HAS_FILE_OBJECT                |\
                                   KPH_PROCESS_HAS_SECTION_OBJECT_POINTERS    |\
                                   KPH_PROCESS_NO_USER_WRITABLE_REFERENCES    |\
                                   KPH_PROCESS_NO_FILE_TRANSACTION            |\
                                   KPH_PROCESS_NO_WRITABLE_FILE_OBJECT)

#define KPH_PROCESS_STATE_LOW     (KPH_PROCESS_VERIFIED_PROCESS               |\
                                   KPH_PROCESS_HAS_FILE_OBJECT                |\
                                   KPH_PROCESS_HAS_SECTION_OBJECT_POINTERS    |\
                                   KPH_PROCESS_NO_USER_WRITABLE_REFERENCES    |\
                                   KPH_PROCESS_NO_FILE_TRANSACTION            |\
                                   KPH_PROCESS_NO_WRITABLE_FILE_OBJECT)

#define KPH_PROCESS_STATE_MINIMUM (KPH_PROCESS_HAS_FILE_OBJECT                |\
                                   KPH_PROCESS_HAS_SECTION_OBJECT_POINTERS    |\
                                   KPH_PROCESS_NO_USER_WRITABLE_REFERENCES    |\
                                   KPH_PROCESS_NO_FILE_TRANSACTION            |\
                                   KPH_PROCESS_NO_WRITABLE_FILE_OBJECT)

#endif
#define MP_VERIFY_FLAG_SA				0x00000001
#define MP_VERIFY_FLAG_CI				0x00000002
#define MP_VERIFY_FLAG_SL				0x00000004
//#define MP_VERIFY_FLAG_...			0x00000008

#define MP_VERIFY_FLAG_FILE_MISMATCH	0x00010000
#define MP_VERIFY_FLAG_HASH_FILED		0x00020000
#define MP_VERIFY_FLAG_COHERENCY_FAIL	0x00040000
#define MP_VERIFY_FLAG_SIGNATURE_FAIL	0x00080000



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

enum class EAccessProtectType
{
	eNone = 0,
	eProtect,
	eSupress,
};

/////////////////////////////////////////////////////////////////////////////
// ExecLog
//

enum class EExecLogRole // API_S_... TODO
{
	eUndefined = 0,
	eActor,
	eTarget,
	eBoth,
};

enum class EExecLogType // API_S_... TODO
{
	eUnknown = 0,
	eProcessStarted = 0x01,
	eImageLoad = 0x02,
	eProcessAccess = 0x04,
	eThreadAccess = 0x08,
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

//enum class EHashStatus // API_S_... TODO
//{
//	eHashUnknown = 0,
//	eHashOk,
//	//eHashError,
//	eHashFail,
//	eHashDummy,
//	eHashNone
//};

enum class ETracePreset
{
	eDefault = 0,
	eTrace,
	eNoTrace,
	ePrivate
};

enum class ESavePreset
{
	eDefault = 0,
	eSaveToDisk,
	eDontSave,
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
	eNotAvailable = -2, // tweak is not available
	eGroup = -1,    // tweak is group
	eNotSet = 0,    // tweak is not set and not applied
	eApplied,       // tweak was not set by user but is applied
	eSet,           // tweak is set and applied
	eMissing,       // tweak was set but is not applied
	eIneffective,	// tweak is applied but is not effective
	eCustom,		// tweak is not applied and the current value is not the default
};

enum class ETweakHint // API_S_TWEAK_HINT
{
	eNone = 0,
	eRecommended,
	eNotRecommended,
	eBreaking,
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
	eRes,
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

enum class EFwRuleState
{
	eUnapproved = 0,
	eUnapprovedDisabled,
	eBackup, // was approved but somethign happened
	eDiverged,
	eApproved,
	eMax,
};

enum class EFwRuleSource
{
	eUnknown = 0,
	eWindowsDefault,
	eWindowsStore,
	eMajorPrivacy,
	eAutoTemplate,
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

enum class EPrivacyEvent
{
	eUnknown = 0,
	eFwRuleAltered, // created changed or removed
	eFwTemplateApplied,
	eTweakBroken,
	eExecBlocked,
	eAccessBlocked, // when auditing for said resource is enabled

	eMax
};

enum class ETraceLogs // API_S_... TODO
{
	eExecLog = 0,
	eNetLog,
	eResLog,
	eLogMax
};

enum class EScriptTypes // API_S_... TODO
{
	eNone = 0,
	eExecRule,
	eEnclave,
	eResRule,
	eVolume,
	eMax
};

enum class ELogLevels // API_S_... TODO
{
	eNone = 0,
	eInfo,
	eSuccess,
	eWarning,
	eError,
	eCritical,
	eMax
};

enum ELogEventType
{
	eLogUnknown = 0,

	eLogFwModeChanged = 'fwmc',		// FireWall Mode Changed
	eLogFwRuleAdded = 'fwra',		// FireWall Rule Added
	eLogFwRuleModified = 'fwrm',	// FireWall Rule Modified
	eLogFwRuleRemoved = 'fwrd',		// FireWall Rule Deleted
	eLogFwRuleGenerated = 'fwrg',	// FireWall Rule Generated
	eLogFwRuleApproved = 'fwrc',	// FireWall Rule Confirmed
	eLogFwRuleRestored = 'fwrf',	// FireWall Rule Fixed
	eLogFwRuleRejected = 'fwrr',	// FireWall Rule Rejected
	//eLogFwRuleScriptEvent = 'fwse',	// FireWall rule Script Event

	eLogResRuleAdded = 'rara',		// Resource Access Rule Added
	eLogResRuleModified = 'rarm',	// Resource Access Rule Modified
	eLogResRuleRemoved = 'rard',	// Resource Access Rule Deleted
	//eLogResRuleScriptEvent = 'rase',// Resource Access rule Script Event

	eLogExecRuleAdded = 'exra',		// EXecution Rule Added
	eLogExecRuleModified = 'exrm',	// EXecution Rule Modified
	eLogExecRuleRemoved = 'exrd',	// EXecution Rule Deleted
	eLogExecStartBlocked = 'exbl',	// EXecution Start Blocked
	//eLogExecRuleScriptEvent = 'exse',// EXecution rule Script Event

	//eLogEnclaveScriptEvent = 'ense',// ENclave Script Event
	eLogScriptEvent = 'lsce',		// Log SCript Event

	eLogProgramAdded = 'prea',		// PRogram Entry Added
	eLogProgramModified = 'prem',	// PRogram Entry Modified
	eLogProgramRemoved = 'pred',	// PRogram Entry Removed
	eLogProgramMissing = 'prim',	// PRogram Item Missing
	eLogProgramCleanedUp = 'prec',	// PRogram Entry Cleaned
	eLogProgramBlocked = 'prbl',	// PRogram BLocked
};

enum ELogEventSubType
{
	eLogSubUnknown = 0,
	eLogFirewall,
	eLogAccess,
	eLogProcess,
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