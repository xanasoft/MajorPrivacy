/*
* Copyright (c) 2023-2024 David Xanatos, xanasoft.com
* All rights reserved.
*
* This file is part of MajorPrivacy.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
*/

#pragma once

#define MY_ABI_VERSION  0x009601

/////////////////////////////////////////////////////////////////////////////
// Driver
//

#define API_DRIVER_NAME         L"KernelIsolator"

#define API_DRIVER_BINARY       API_DRIVER_NAME L".sys"

#define API_DEVICE_NAME         L"\\Device\\" API_DRIVER_NAME L"Api"

#define API_DRIVER_PORT         L"\\" API_DRIVER_NAME L"Port"

#define API_DRIVER_ALTITUDE      L"385490"

#define API_DEVICE_CTLCODE      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_NEITHER, FILE_ANY_ACCESS)

#define API_NUM_ARGS            8

enum {
	API_FIRST                   = ('PD' << 16),

	API_GET_VERSION,
	API_GET_PROCESS_INFO,
	API_SET_PROCESS_INFO,
	API_ENUM_PROCESSES,
	API_GET_HANDLE_INFO,
	API_ENUM_ENCLAVES,
	API_GET_ENCLAVE_INFO,
	API_PREPARE_ENCLAVE,
	API_REGISTER_FOR_EVENT,

	API_SET_USER_KEY,
	API_GET_USER_KEY,
	API_UNLOCK_USER_KEY,
	API_LOCK_USER_KEY,
	API_CLEAR_USER_KEY,

	API_GET_CHALLENGE,

	API_SET_PROGRAM_RULES,
	API_GET_PROGRAM_RULES,
	API_SET_PROGRAM_RULE,
	API_GET_PROGRAM_RULE,
	API_DEL_PROGRAM_RULE,

	API_SET_ACCESS_RULES,
	API_GET_ACCESS_RULES,
	API_SET_ACCESS_RULE,
	API_GET_ACCESS_RULE,
	API_DEL_ACCESS_RULE,

	API_SETUP_ACCESS_RULE_ALIAS,
	API_CLEAR_ACCESS_RULE_ALIAS,

	API_SET_CONFIG_VALUE,
	API_GET_CONFIG_VALUE,

	API_GET_SUPPORT_INFO,

	API_LAST
};

/////////////////////////////////////////////////////////////////////////////
// Service
//

#define API_SERVICE_NAME        L"PrivacyAgent"

#define API_SERVICE_BINARY      API_SERVICE_NAME L".exe"

#define API_SERVICE_PORT	    L"\\RPC Control\\" API_SERVICE_NAME L"Port"

#define API_SERVICE_PIPE	    L"\\\\.\\pipe\\" API_SERVICE_NAME L"Pipe"

enum {
	SVC_API_FIRST                   = ('PS' << 16),

	SVC_API_GET_VERSION,

	SVC_API_GET_CONFIG_DIR,
	SVC_API_GET_CONFIG,
	SVC_API_SET_CONFIG,

	// Network Manager
	SVC_API_GET_FW_RULES,
	SVC_API_SET_FW_RULE,
	SVC_API_GET_FW_RULE,
	SVC_API_DEL_FW_RULE,

	SVC_API_GET_FW_PROFILE,
	SVC_API_SET_FW_PROFILE,

	SVC_API_GET_FW_AUDIT_MODE,
	SVC_API_SET_FW_AUDIT_MODE,

	SVC_API_GET_SOCKETS,
	SVC_API_GET_TRAFFIC,
	SVC_API_GET_DNC_CACHE,
	SVC_API_FLUSH_DNS_CACHE,

	// Program Manager
	SVC_API_GET_PROCESSES,      // live runnign or recently terminated processes
	SVC_API_GET_PROCESS,

	SVC_API_GET_PROGRAMS,
	SVC_API_GET_LIBRARIES,
	SVC_API_START_SECURE,

	SVC_API_SET_PROGRAM,
	SVC_API_ADD_PROGRAM,
	SVC_API_REMOVE_PROGRAM,
	SVC_API_CLEANUP_PROGRAMS,
	SVC_API_REGROUP_PROGRAMS,

	SVC_API_GET_TRACE_LOG,

	SVC_API_GET_LIBRARY_STATS,
	SVC_API_GET_EXEC_STATS,
	SVC_API_GET_INGRESS_STATS,

	SVC_API_GET_ACCESS_STATS,

	// Access Manager
	SVC_API_GET_HANDLES,
	SVC_API_CLEAR_LOGS,
	SVC_API_CLEANUP_ACCESS_TREE,

	// Volume Manager
	SVC_API_VOL_CREATE_IMAGE,
	SVC_API_VOL_CHANGE_PASSWORD,
	//SVC_API_VOL_DELETE_IMAGE,

	SVC_API_VOL_MOUNT_IMAGE,
	SVC_API_VOL_DISMOUNT_VOLUME,
	SVC_API_VOL_DISMOUNT_ALL,

	//SVC_API_VOL_GET_VOLUME_LIST,
	SVC_API_VOL_GET_ALL_VOLUMES,
	SVC_API_VOL_GET_VOLUME,

	SVC_API_GET_TWEAKS,
	SVC_API_APPLY_TWEAK,
	SVC_API_UNDO_TWEAK,

	SVC_API_SET_WATCHED_PROG,

	// events
	SVC_API_EVENT_PROG_ITEM_CHANGED,

	SVC_API_EVENT_FW_RULE_CHANGED,
	SVC_API_EVENT_EXEC_RULE_CHANGED,
	SVC_API_EVENT_RES_RULE_CHANGED,

	SVC_API_EVENT_NET_ACTIVITY,
	SVC_API_EVENT_EXEC_ACTIVITY,
	SVC_API_EVENT_RES_ACTIVITY,

	SVC_API_EVENT_CLEANUP_PROGRESS,

	// shutdown
	SVC_API_SHUTDOWN,

	SVC_API_LAST
};

// for ini file
#define GROUP_NAME		        L"Xanasoft"
#define APP_NAME		        L"MajorPrivacy"


/////////////////////////////////////////////////////////////////////////////
// My Error Coded
// 
// NT Status coded are 32 bit values, success values are 0 or positive, 
// failure values are negative, startin with 0xC0000001.
// For our own status codes we set the third highest bit to 1, hence
// our status codes are 0xE0000001 and so on.
// We can also use special success codes starting at 0x20000001
// 
//

#define IS_MAJOR_STATUS(x)					((x) & 0x20000000)


#define STATUS_ERR_PROG_NOT_FOUND			0xE0000001

#define STATUS_ERR_DUPLICATE_PROG			0xE0000002

#define STATUS_ERR_PROG_HAS_RULES			0xE0000003

#define STATUS_ERR_PROG_PARENT_NOT_FOUND	0xE0000004

#define STATUS_ERR_CANT_REMOVE_FROM_PATTERN	0xE0000005

#define STATUS_ERR_PROG_PARENT_NOT_VALID	0xE0000006

#define STATUS_ERR_CANT_REMOVE_AUTO_ITEM	0xE0000007

#define STATUS_ERR_NO_USER_KEY				0xE0000008

#define STATUS_ERR_WRONG_PASSWORD			0xE0000009

#define STATUS_ERR_PROG_ALREADY_IN_GROUP	0xE000000A

#define STATUS_OK							0x00000000 // same as STATUS_SUCCESS

#define STATUS_OK_1							0x20000001


/////////////////////////////////////////////////////////////////////////////
// Variant Field IDs
//
// Designated static field ID's are used in variants which are saved to disk/registry
// Dynamic field ID's are used in variants which are passed between components
//
//

enum
#ifdef __cplusplus
API_V_STATIC : unsigned long
#endif
{
	API_V_STATIC_FIRST			= 0,

	API_V_CONFIG				= 'conf',

	API_V_PROGRAM_RULES			= 'exrz',
	API_V_ACCESS_RULES			= 'rarz',
	API_V_FW_RULES				= 'fwrz',	// CVariant

	// Programs
	API_V_PID					= 'pid',	// uint64
	API_V_PIDS					= 'pids',

	API_V_PROG_UID				= 'uid',	// uint64
	API_V_PROG_UIDS				= 'uids',	// CVariant

	API_V_PROG_ID				= 'prog',	// CProgramID
	API_V_PROG_IDS				= 'prgs',	// CVariant

	API_V_PROG_TYPE				= 'prtp',	// EProgramType

	API_V_FILE_PATH				= 'path',	// String
	API_V_FILE_PATH2			= 'pat2',	// String
	API_V_SVC_TAG				= 'stag',	// String
	API_V_APP_SID				= 'asid',	// String
	API_V_APP_NAME				= 'appn',	// String
	API_V_PACK_NAME				= 'pkgn',	// String
	API_V_REG_KEY				= 'rkey',	// String
	API_V_PROG_PATTERN			= 'ppat',	// String

	API_V_CMD_LINE				= 'cmdl',	// String
	API_V_OWNER					= 'ownr',	// String  // used by firewall rules

	API_V_PROG_ITEMS			= 'pris',	// CVariant

	API_V_PROG_LAST_EXEC		= 'plec',	// uint64	

	// Info
	API_V_NAME					= 'name',	// String
	API_V_ICON					= 'icon',	// String
	API_V_INFO					= 'info',	// String
	API_V_RULE_GROUP			= 'grp',	// String
	API_V_RULE_DESCR			= 'dscr',	// String

	//

	// Libraries
	API_V_LIB_REF				= 'lref',	// uint64
	API_V_LIB_LOAD_TIME			= 'lltm',	// uint64
	API_V_LIB_LOAD_COUNT		= 'llct',	// uint64
	API_V_SIGN_INFO				= 'sigi',	// SLibraryInfo::USign
	API_V_SIGN_INFO_AUTH		= 'lsau',	// KPH_VERIFY_AUTHORITY
	API_V_SIGN_INFO_LEVEL		= 'lsle',	// uint32
	API_V_SIGN_INFO_POLICY		= 'lspl',	// uint32
	API_V_LIB_STATUS			= 'lbst',	// CExecLogEntry

	// Rules
	API_V_RULE_ENABLED			= 'en',		// bool
	API_V_RULE_TEMP				= 'tmp',	// bool

	API_V_RULE_GUID				= 'guid',	// GUID
	API_V_RULE_INDEX			= 'idx',	// int

	API_V_RULE_DATA				= 'data',	// Variant

	API_V_RULE_DATA_REF_GUID	= 'rgid',

// Access Rules
	API_V_ACCESS_RULE_ACTION	= 'atyp',	// EAccessRuleType
	API_V_ACCESS_PATH			= 'afnp',	// String
	API_V_ACCESS_PATH2			= 'afn2',	// String
	API_V_VOL_RULE				= 'vrul',	// bool

// Execution Rules
	API_V_EXEC_RULE_ACTION		= 'xtyp',	// CProgramRule::EType
	API_V_EXEC_SIGN_REQ			= 'sreq',	// KPH_VERIFY_AUTHORITY
	API_V_EXEC_ON_TRUSTED_SPAWN	= 'tsp',	// EProgramOnSpawn
	API_V_EXEC_ON_SPAWN			= 'usp',	// EProgramOnSpawn
	API_V_IMAGE_LOAD_PROTECTION	= 'ilp',	// bool

// Firewall Rules
	API_V_FW_RULE_ACTION		= 'fwa',	// String
	API_V_FW_RULE_DIRECTION		= 'fwd',	// String
	API_V_FW_RULE_PROFILE		= 'fwpf',
	API_V_FW_RULE_PROTOCOL		= 'fwpt',	// int
	API_V_FW_RULE_INTERFACE		= 'fwif',
	API_V_FW_RULE_LOCAL_ADDR	= 'fwla',
	API_V_FW_RULE_REMOTE_ADDR	= 'fwra',
	API_V_FW_RULE_LOCAL_PORT	= 'fwlp',
	API_V_FW_RULE_REMOTE_PORT	= 'fwrp',
	API_V_FW_RULE_REMOTE_HOST	= 'fwrh',
	API_V_FW_RULE_ICMP			= 'icmp',
	API_V_FW_RULE_OS			= 'fwos',
	API_V_FW_RULE_EDGE			= 'fwet',

	// Processes
	API_V_PROG_EXEC_ACTORS		= 'peas',
	API_V_PROG_EXEC_TARGETS		= 'pets',
	API_V_PROG_INGRESS_ACTORS	= 'pias',
	API_V_PROG_INGRESS_TARGETS	= 'pits',
	API_V_PROC_REF				= 'pref',	// uint64
	API_V_PROC_EVENT_ACTOR		= 'peac',	// uint64
	API_V_PROC_EVENT_TARGET		= 'petg',	// uint64
	API_V_PROC_EVENT_LAST_EXEC	= 'pele',	// uint64
	API_V_PROC_EVENT_BLOCKED	= 'pebl',	// bool
	API_V_PROC_EVENT_CMD_LINE	= 'pecl',	// String
	API_V_PROG_SVC_TAG			= 'psvt',	// String
	API_V_PROC_EVENT_LAST_ACT	= 'pela',	// uint64
	API_V_THREAD_ACCESS_MASK	= 'tham',	// uint32
	API_V_PROCESS_ACCESS_MASK	= 'pram',	// uint32

	API_V_PROC_TARGETS			= 'ptgs',	// CVariant
	API_V_PROC_ACTORS			= 'pacs',	// CVariant

	// Access
	//API_V_ACCESS_TREE			= 'actr',
	API_V_PROG_RESOURCE_ACCESS	= 'prac',	// CVariant
	API_V_ACCESS_REF			= 'acrf',	// uint64
	API_V_ACCESS_NAME			= 'acnm',	// String
	API_V_ACCESS_TIME			= 'actm',	// uint64
	API_V_ACCESS_BLOCKED		= 'acbl',	// bool
	API_V_ACCESS_MASK			= 'acms',	// uint32
	API_V_ACCESS_STATUS			= 'acst',	// uint32
	API_V_ACCESS_IS_DIR			= 'acdr',	// bool	
	API_V_ACCESS_NODES			= 'acns',	// CVariant
	API_V_ACCESS_LAST_ACT		= 'acac',	// uint64
	API_V_ACCESS_COUNT			= 'acnt',	// uint32

	// Sockets
	API_V_PROG_TRAFFIC			= 'ptrf',
	API_V_PROG_SOCKETS			= 'psck',
	API_V_SOCK_LAST_ACT			= 'slac',
	API_V_SOCK_LAST_ALLOW		= 'slal',
	API_V_SOCK_LAST_BLOCK		= 'slbl',
	API_V_SOCK_UPLOAD			= 'supl',
	API_V_SOCK_DOWNLOAD			= 'sdlw',
	API_V_SOCK_UPLOADED			= 'supd',
	API_V_SOCK_DOWNLOADED		= 'sddw',
	API_V_SOCK_RHOST			= 'srhs',

	// Tweaks
	API_V_TWEAKS				= 'twks',	// CVariant
	API_V_TWEAK_HINT			= 'twhn',	// ETweakHint
	API_V_TWEAK_STATUS			= 'twst',	// ETweakStatus
	API_V_TWEAK_LIST			= 'twls',	// CVariant
	API_V_TWEAK_TYPE			= 'twtp',	// ETweakType
	API_V_TWEAK_IS_SET			= 'twse',	// CVariant

	API_V_TWEAK_KEY				= 'twky',	// String
	API_V_TWEAK_VALUE			= 'twvl',	// CVariant
	API_V_TWEAK_DATA			= 'twda',	// CVariant
	API_V_TWEAK_FOLDER			= 'twfl',	// String
	API_V_TWEAK_ENTRY			= 'twen',	// String
	API_V_TWEAK_SVC_TAG			= 'twsv',	// String
	API_V_TWEAK_PATH			= 'twpa',	// String
	API_V_TWEAK_PROG_ID			= 'twpi',	// CProgramID

	API_V_STATIC_LAST			= 'zzzz'
};

enum
#ifdef __cplusplus
API_V_DYNAMIC : unsigned long
#endif
{
	API_V_DYNAMIC_FIRST			= 0x80000000,
	
	API_V_ERR_CODE,
	API_V_ERR_MSG,

	API_V_EVENT,
	API_V_RULE_TYPE,

	API_V_CONF_KEY,
	API_V_CONF_VALUE,
	API_V_CONF_DATA, //API_V_CONF_MAP,

	API_V_PROG_PARENT,

	// Process Info
	API_V_CREATE_TIME,
	API_V_PARENT_PID,
	API_V_CREATOR_PID,
	API_V_CREATOR_TID,

	API_V_KPP_STATE,

	API_V_N_IMG,
	API_V_FLAGS,
	API_V_SFLAGS,

	API_V_NUM_THREADS,
	API_V_HANDLES,
	API_V_SOCKETS,

	API_V_EID,
	API_V_EIDS,
	API_V_SEC,

	API_V_USER_SID,

	API_V_N_MS_IMG,
	API_V_N_AV_IMG,
	API_V_N_V_IMG,
	API_V_N_S_IMG,
	API_V_N_U_IMG,

	API_V_UW_REFS,

	API_V_SVCS,


	// Enclave Info
	API_V_ENCLAVE_ID,						// uint64	
	API_V_ENCLAVE_NAME,						// String
	API_V_ENCLAVE_SIGN_LEVEL,				// KPH_VERIFY_AUTHORITY
	API_V_ENCLAVE_ON_TRUSTED_SPAWN,			// EProgramOnSpawn
	API_V_ENCLAVE_ON_SPAWN,					// EProgramOnSpawn
	API_V_ENCLAVE_IMAGE_LOAD_PROTECTION,	// bool

	// Processes

	API_V_PROG_PROC_PIDS,

	API_V_HANDLE,
	API_V_HANDLE_TYPE,

	// Access Rules
	API_V_PATH_PREFIX,
	API_V_DEVICE_PATH,

	// Event Generic 
	API_V_EVENT_UID,
	API_V_EVENT_ACTOR_PID,
	API_V_EVENT_ACTOR_TID,
	API_V_EVENT_ACTOR_SVC,
	API_V_EVENT_PID,
	API_V_EVENT_TIME_STAMP,
	API_V_EVENT_PATH,
	API_V_EVENT_OPERATION,
	API_V_EVENT_ACCESS,
	API_V_EVENT_ACCESS_STATUS,
	API_V_EVENT_STATUS,
	API_V_EVENT_IS_DIR,

	API_V_EVENT_INDEX,
	API_V_EVENT_DATA,

	API_V_FW_AUDIT_MODE,					// FwAuditPolicy
	API_V_FW_RULE_FILTER_MODE,				// FwFilteringModes
	API_V_FW_EVENT_STATE,					// EFwEventStates

	// Process Event Specific
	API_V_EVENT_WAS_LP,
	API_V_EVENT_IMG_BASE,
	API_V_EVENT_IMG_SIGN_AUTH,
	API_V_EVENT_IMG_SIGN_LEVEL,
	API_V_EVENT_IMG_SIGN_POLICY,
	API_V_EVENT_IS_P,
	API_V_EVENT_NO_PROTECT,
	API_V_EVENT_IMG_PROPS,
	API_V_EVENT_IMG_SEL,
	API_V_EVENT_IMG_SECT_NR,
	API_V_EVENT_PARENT_PID,
	API_V_EVENT_CMD,
	API_V_EVENT_CS,
	API_V_EVENT_ECODE,

	// Access
	API_V_ACCESS_PID,

	// DNS
	API_V_DNS_CACHE_REF,
	API_V_DNS_HOST,
	API_V_DNS_TYPE,
	API_V_DNS_ADDR,
	API_V_DNS_DATA,
	API_V_DNS_TTL,
	API_V_DNS_QUERY_COUNT,

	// Sockets
	API_V_SOCK_REF,
	API_V_SOCK_TYPE,
	API_V_SOCK_LADDR,
	API_V_SOCK_LPORT,
	API_V_SOCK_RADDR,
	API_V_SOCK_RPORT,
	API_V_SOCK_STATE,
	API_V_SOCK_LSCOPE,
	API_V_SOCK_RSCOPE,
	API_V_SOCK_PID,
	API_V_SOCK_SVC_TAG,
	API_V_SOCK_CREATED,

	// Process Events
	API_V_PROC_EVENT_ROLE,
	API_V_PROC_EVENT_TYPE,
	API_V_PROC_EVENT_STATUS,
	API_V_PROC_EVENT_MISC,
	API_V_PROC_EVENT_ACCESS_MASK,

	// Volume
	API_V_VOL_REF,
	API_V_VOL_PATH,
	API_V_VOL_DEVICE_PATH,
	API_V_VOL_MOUNT_POINT,
	API_V_VOL_SIZE,
	API_V_VOL_PASSWORD,
	API_V_VOL_PROTECT,
	API_V_VOL_CIPHER,
	API_V_VOL_OLD_PASS,
	API_V_VOL_NEW_PASS,
	API_V_VOLUMES,

	//
	API_V_NET_TRAFFIC,
	API_V_DNS_CACHE,
	API_V_PROCESSES,
	API_V_PROGRAMS,
	API_V_LIBRARIES,
	API_V_COMMAND,

	// Trace Log
	API_V_LOG_TYPE, // ETraceLogs
	API_V_LOG_OFFSET,
	API_V_LOG_DATA,

	// Crypto
	API_V_PUB_KEY,
	API_V_PRIV_KEY,
	API_V_HASH,
	API_V_PRIV_KEY_BLOB,
	API_V_LOCK,
	API_V_UNLOCK,
	API_V_RAND,
	API_V_SIGNATURE,

	// Other
	API_V_ENUM_ALL,
	API_V_COUNT,
	API_V_VERSION,

	API_V_SET_NOTIFY_BITMASK,
	API_V_CLEAR_NOTIFY_BITMASK,
 
	// Support
	API_V_SUPPORT_NAME,
	API_V_SUPPORT_STATUS,
	API_V_SUPPORT_HWID,

	// cache
	API_V_CACHE_TOKEN,

	// todo move up with next driver update
	API_V_PURGE_RULES,
	API_V_ITEM_MISSING,

	// otehre events
	API_V_PROGRESS_FINISHED,
	API_V_PROGRESS_TOTAL,
	API_V_PROGRESS_DONE,


	API_V_DEL_WITH_RULES
};


/////////////////////////////////////////////////////////////////////////////
// Event log ID's
//

#define SVC_EVENT_DRIVER_FAILED		0x0101
#define SVC_EVENT_BLOCK_PROC		0x0102
#define SVC_EVENT_KILL_FAILED		0x0103
#define SVC_EVENT_SVC_INIT_FAILED	0x0104
#define SVC_EVENT_SVC_STATUS_MSG	0x0105
#define SVC_EVENT_VOL_PROTECT_ERROR	0x0106
#define SVC_EVENT_PROG_CLEANUP		0x0107


/////////////////////////////////////////////////////////////////////////////
//
//

#define DEF_MP_SYS_FILE L"\\$mpsys$"

#include "PrivacyAPIs.h"