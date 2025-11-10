/*
* Copyright (c) 2023-2025 David Xanatos, xanasoft.com
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

#define MY_ABI_VERSION  0x009910

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
	API_GET_VERSION = 'VERS',
	API_GET_PROCESS_INFO = 'PRCS',
	API_SET_PROCESS_INFO = 'SPRC',
	API_ENUM_PROCESSES = 'EPRC',
	API_ENUM_THREADS = 'ETHD',
	API_GET_HANDLE_INFO = 'HNDL',
	API_REGISTER_FOR_EVENT = 'REVT',

	API_SET_USER_KEY = 'SUKY',
	API_GET_USER_KEY = 'GUKY',
	API_CLEAR_USER_KEY = 'CUKY',
	API_GET_CHALLENGE = 'CHAL',

	API_PROTECT_CONFIG = 'PCFG',
	API_UNPROTECT_CONFIG = 'UCFG',
	API_GET_CONFIG_STATUS = 'GCFS',
	API_UNLOCK_CONFIG = 'ULCK',
	API_COMMIT_CONFIG = 'CCFG',
	API_DISCARD_CHANGES = 'DCFG',
	API_STORE_CONFIG = 'WCFG',
	API_GET_CONFIG_HASH = 'HCFG',

	API_SET_ENCLAVES = 'LENC',
	API_GET_ENCLAVES = 'AENC',
	API_SET_ENCLAVE = 'SENC',
	API_GET_ENCLAVE = 'GENC',
	API_DEL_ENCLAVE = 'DENC',
	API_PREP_ENCLAVE = 'PENC',

	API_SET_HASHES = 'LHAS',
	API_GET_HASHES = 'AHAS',
	API_SET_HASH = 'SHAS',
	API_GET_HASH = 'GHAS',
	API_DEL_HASH = 'DHAS',

	API_SET_PROGRAM_RULES = 'LPRG',
	API_GET_PROGRAM_RULES = 'APRG',
	API_SET_PROGRAM_RULE = 'SPRG',
	API_GET_PROGRAM_RULE = 'GPRG',
	API_DEL_PROGRAM_RULE = 'DPRG',

	API_SET_ACCESS_RULES = 'LACS',
	API_GET_ACCESS_RULES = 'AACS',
	API_SET_ACCESS_RULE = 'SACS',
	API_GET_ACCESS_RULE = 'GACS',
	API_DEL_ACCESS_RULE = 'DACS',
	//API_SETUP_ACCESS_RULE_ALIAS = 'SARA',
	//API_CLEAR_ACCESS_RULE_ALIAS = 'CARA',

	API_SET_VOLUME_LOCKDOWN = 'SVLK',

	API_SET_CONFIG_VALUE = 'SCFG',
	API_GET_CONFIG_VALUE = 'GCFG',

	API_ACQUIRE_NO_UNLOAD = 'ANUL',
	API_RELEASE_NO_UNLOAD = 'RNUL',

	API_ACQUIRE_NO_HIBERNATION = 'ANHB',
	API_RELEASE_NO_HIBERNATION = 'RNHB',

	API_IGNORE_PENDING_IMAGE_LOAD = 'IPIL',

	API_GET_SUPPORT_INFO = 'SUPP',

	API_GET_SECURE_PARAM = 'GSPM',
	API_SET_SECURE_PARAM = 'SSPM',

	API_TEST_DEV_AUTHORITY = 'TDAT',

	API_VERIFY_SIGNATURE = 'VRSN',
};

/////////////////////////////////////////////////////////////////////////////
// Service
//

#define API_SERVICE_NAME        L"PrivacyAgent"

#define API_SERVICE_BINARY      API_SERVICE_NAME L".exe"

#define API_SERVICE_PORT	    L"\\RPC Control\\" API_SERVICE_NAME L"Port"

#define API_SERVICE_PIPE	    L"\\\\.\\pipe\\" API_SERVICE_NAME L"Pipe"

enum {
	SVC_API_GET_VERSION = 'VERS',

	SVC_API_GET_CONFIG_DIR = 'GCDR',
	SVC_API_GET_CONFIG = 'GCFG',
	SVC_API_SET_CONFIG = 'SCFG',

	SVC_API_CHECK_CONFIG_FILE = 'CCFF',
	SVC_API_GET_CONFIG_FILE = 'GCFF',
	SVC_API_SET_CONFIG_FILE = 'SCFF',
	SVC_API_DEL_CONFIG_FILE = 'DCFF',
	SVC_API_LIST_CONFIG_FILES = 'LCFF',

	SVC_API_GET_CONFIG_STATUS = 'GCFS',
	SVC_API_COMMIT_CONFIG = 'CCFG',
	SVC_API_DISCARD_CHANGES = 'DCFG',

	// Network Manager
	SVC_API_GET_FW_RULES = 'GFRS',
	SVC_API_SET_FW_RULE = 'SFRU',
	SVC_API_GET_FW_RULE = 'GFRU',
	SVC_API_DEL_FW_RULE = 'DFRU',

	SVC_API_GET_FW_PROFILE = 'GFPR',
	SVC_API_SET_FW_PROFILE = 'SFPR',

	SVC_API_GET_FW_AUDIT_MODE = 'GFAM',
	SVC_API_SET_FW_AUDIT_MODE = 'SFAM',

	SVC_API_GET_SOCKETS = 'GSKT',
	SVC_API_GET_TRAFFIC = 'GTRF',
	SVC_API_GET_DNC_CACHE = 'GDNS',
	SVC_API_FLUSH_DNS_CACHE = 'FDNS',

	SVC_API_GET_DNS_RULES = 'GADR',
	SVC_API_GET_DNS_RULE = 'GDFR',
	SVC_API_SET_DNS_RULE = 'SDFR',
	SVC_API_DEL_DNS_RULE = 'DDFR',

	SVC_API_GET_DNS_LIST_INFO = 'DBLI',

	// Program Manager
	SVC_API_GET_PROCESSES = 'GPRC', // live runnign or recently terminated processes
	SVC_API_GET_PROCESS = 'GPRX',

	SVC_API_GET_PROGRAMS = 'GPRO',
	SVC_API_GET_PROGRAM = 'GPRG',
	SVC_API_GET_LIBRARIES = 'GLIB',
	SVC_API_GET_LIBRARY = 'GLBR',
	SVC_API_START_SECURE = 'SSEC',

	SVC_API_RUN_UPDATER = 'RUPD',

	SVC_API_SET_PROGRAM = 'SPRO',
	SVC_API_ADD_PROGRAM = 'APRO',
	SVC_API_REMOVE_PROGRAM = 'RPRO',
	SVC_API_REFRESH_PROGRAMS = 'RPRG',
	SVC_API_CLEANUP_PROGRAMS = 'CPRG',
	SVC_API_REGROUP_PROGRAMS = 'RGRP',

	SVC_API_GET_TRACE_LOG = 'GTRC',

	SVC_API_GET_LIBRARY_STATS = 'GLST',
	SVC_API_GET_EXEC_STATS = 'GEST',
	SVC_API_GET_INGRESS_STATS = 'GIST',
	SVC_API_CLEANUP_LIBS = 'CLIB',

	SVC_API_GET_ACCESS_STATS = 'GAST',

	// Access Manager
	SVC_API_GET_HANDLES = 'GHND',
	SVC_API_CLEAR_LOGS = 'CLOG',
	SVC_API_CLEAR_RECORDS = 'CREC',
	SVC_API_CLEANUP_ACCESS_TREE = 'CTRE',
	SVC_API_SET_ACCESS_EVENT_ACTION = 'SAEA',

	// Volume Manager
	SVC_API_VOL_CREATE_IMAGE = 'CVOL',
	SVC_API_VOL_CHANGE_PASSWORD = 'CPWD',
	//SVC_API_VOL_DELETE_IMAGE = 'RVOL',

	SVC_API_VOL_MOUNT_IMAGE = 'MVOL',
	SVC_API_VOL_DISMOUNT_VOLUME = 'DVOL',
	SVC_API_VOL_DISMOUNT_ALL = 'DVLA',

	//SVC_API_VOL_GET_VOLUME_LIST = 'GVL',
	SVC_API_VOL_GET_ALL_VOLUMES = 'GAVL',
	SVC_API_VOL_GET_VOLUME = 'GVOL',
	SVC_API_VOL_SET_VOLUME = 'SVOL',

	SVC_API_GET_TWEAKS = 'GTWK',
	SVC_API_APPLY_TWEAK = 'ATWK',
	SVC_API_UNDO_TWEAK = 'UTWK',
	SVC_API_APPROVE_TWEAK = 'CTWK',

	SVC_API_GET_PRESETS = 'GPRS',
	SVC_API_SET_PRESETS = 'SPRS',
	SVC_API_GET_PRESET = 'GPRT',
	SVC_API_SET_PRESET = 'SPRT',
	SVC_API_DEL_PRESET = 'DPRT',
	SVC_API_ACTIVATE_PRESET = 'APRT',
	SVC_API_DEACTIVATE_PRESET = 'UPRT',

	SVC_API_SET_WATCHED_PROG = 'SWAT',

	SVC_API_SET_DAT_FILE = 'SDAT',
	SVC_API_GET_DAT_FILE = 'GDAT',

	// events
	SVC_API_EVENT_PROG_ITEM_CHANGED = 'EPRG',

	SVC_API_EVENT_LOG_ENTRY = 'ELOG',

	SVC_API_EVENT_ENCLAVE_CHANGED = 'EENC',
	SVC_API_EVENT_HASHDB_CHANGED = 'EHDB',
	SVC_API_EVENT_FW_RULE_CHANGED = 'EFRU',
	SVC_API_EVENT_DNS_RULE_CHANGED = 'EDFR',
	SVC_API_EVENT_EXEC_RULE_CHANGED = 'EEXR',
	SVC_API_EVENT_RES_RULE_CHANGED = 'ERER',
	SVC_API_EVENT_PRESET_CHANGED = 'EPST',

	SVC_API_EVENT_NET_ACTIVITY = 'ENET',
	SVC_API_EVENT_EXEC_ACTIVITY = 'EEXC',
	SVC_API_EVENT_RES_ACTIVITY = 'ERES',

	SVC_API_EVENT_CLEANUP_PROGRESS = 'ECLN',

	// other
	SVC_API_GET_EVENT_LOG = 'GELG',
	SVC_API_CLEAR_EVENT_LOG = 'CELG',
	SVC_API_GET_SVC_STATS = 'GSST',

	SVC_API_GET_SCRIPT_LOG = 'GSLG',
	SVC_API_CLEAR_SCRIPT_LOG = 'CSLG',
	SVC_API_CALL_SCRIPT_FUNC = 'CSCF',

	SVC_API_SHOW_SECURE_PROMPT = 'SSPT',

	// shutdown
	SVC_API_SHUTDOWN = 'SHUT',
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

#define STATUS_ERR_PROC_EJECTED				0xE000000B

#define STATUS_ERR_RULE_DIVERGENT			0xE000000C

#define STATUS_OK							0x00000000 // same as STATUS_SUCCESS

#define STATUS_OK_CNCELED					0x20000001


/////////////////////////////////////////////////////////////////////////////
// Variant Field IDs
//
// Designated static field ID's are used in variants which are saved to disk/registry
// Dynamic field ID's are used in variants which are passed between components
//
//

enum
#ifdef __cplusplus
API_V_VALUES : unsigned long
#endif
{
	API_V_FIRST			= 0,

	////////////////////////////
	// Config
	API_V_CONFIG = 'cfg',
	API_V_VERSION = 'ver',

	API_V_KEY = 'key',
	API_V_VALUE = 'val',
	API_V_DATA = 'data',
	API_V_FOLDER = 'fldr',
	API_V_ENTRY  = 'entr',
	API_V_ENTRIES = 'ents',
	API_V_RULE = 'rule',
	API_V_RULES = 'ruls',
	API_V_LIST = 'list',
	API_V_FILES = 'flst',

	API_V_TYPE = 'type',
	API_V_GUID = 'guid',
	API_V_INDEX = 'indx',
	API_V_ENABLED = 'enbl',
	API_V_TEMP = 'temp',
	API_V_TIMEOUT = 'tmot',
	API_V_TIME_STAMP = 'tmst',
	API_V_LOCKDOWN = 'lckd',
	API_V_FORCE = 'frce',

	//
	API_V_GUI_CONFIG = 'gcfg',
	API_V_DRIVER_CONFIG = 'dcfg',
	API_V_USER_KEY = 'uky',
	API_V_SERVICE_CONFIG = 'scfg',
	API_V_PROGRAM_RULES = 'prul',
	API_V_ACCESS_RULES = 'arul',
	API_V_FW_RULES = 'frul',
	API_V_PROGRAMS = 'prog',
	API_V_LIBRARIES = 'libs',


	////////////////////////////
	// Crypto & Protection
	API_V_PUB_KEY = 'pkey',
	API_V_PRIV_KEY = 'skey',
	API_V_HASH = 'hash',
	API_V_KEY_BLOB = 'kblo',
	API_V_LOCK = 'lock',
	API_V_UNLOCK = 'unlk',
	API_V_RAND = 'rand',
	API_V_SIGNATURE = 'sign',
	API_V_ENCLAVES = 'encs',
	API_V_COLLECTIONS = 'cols',
	API_V_TOKEN = 'tokn',


	////////////////////////////
	// Generic Info

	API_V_NAME = 'name',
	API_V_ICON = 'icon',
	API_V_INFO = 'info',

	API_V_EXEC_TRACE = 'etrc',
	API_V_RES_TRACE = 'rtrc',
	API_V_NET_TRACE = 'ntrc',
	API_V_SAVE_TRACE = 'strc',

	API_V_RULE_GROUP = 'grup',
	API_V_RULE_DESCR = 'desc',
	API_V_CMD_LINE = 'cmdl',

	API_V_RULE_STATE = 'rste',
	API_V_ORIGINAL_GUID = 'ogid',
	API_V_SOURCE = 'src',
	API_V_TEMPLATE_GUID = 'tgid',
	API_V_VOLUME_GUID = 'vgid',

	API_V_SHUTDOWN = 'shdn',


	////////////////////////////
	// Generic Fields
	API_V_ENUM_ALL = 'eall',
	API_V_URL = 'url',
	API_V_COUNT = 'cnt',
	API_V_REFRESH = 'refr',
	API_V_RELOAD = 'rld',
	API_V_GET_DATA = 'gdat',
	API_V_VERIFY = 'vfy',
	API_V_BLOCKING = 'blk',
	API_V_PARAMS = 'prms',
	API_V_WAIT = 'wait',
	API_V_ELEVATE = 'elvt',
	API_V_REVISION = 'revs',

	////////////////////////////
	// Program ID
	API_V_ID = 'id',
	API_V_IDS = 'ids',
	API_V_PARENT = 'prnt',

	API_V_PROG_TYPE = 'ptyp',
		// ...

	API_V_FILE_PATH = 'fpth',
	API_V_USE_SCRIPT = 'uscr', // use script instead of file path
	API_V_SCRIPT = 'scpt',
	API_V_DLL_INJECT_MODE = 'dlim',
	API_V_INTERACTIVE = 'intr', // used by access rules
	API_V_FILE_NT_PATH = 'fnt',
	API_V_SERVICE_TAG = 'svc',
	API_V_APP_SID = 'asid', // App Container SI
	API_V_APP_NAME = 'appn', // App Container Name
	API_V_PACK_NAME = 'pckg', // Package Name
	API_V_REG_KEY = 'rkey',
	API_V_PROG_PATTERN = 'patt', // like API_V_FILE_PATH but with wildcards
	API_V_OWNER = 'ownr', // used by firewall rules


	////////////////////////////
	// Programs
	API_V_PROG_ITEMS = 'prgs',

	API_V_PROG_UID = 'uid',
	API_V_PROG_UIDS = 'uids',

	// Status
	API_V_PROG_ACCESS_COUNT = 'pacn',
	API_V_PROG_HANDLE_COUNT = 'phnd',
	API_V_PROG_SOCKET_REFS = 'psok',
	API_V_PROG_LAST_EXEC = 'plst',
	API_V_PROG_ITEM_MISSING = 'pims',
	API_V_MEM_USED = 'muse',

	// Mgmt Fields
	API_V_PROG_PARENT = 'ppar',
	API_V_PURGE_RULES = 'prl',
	API_V_DEL_WITH_RULES = 'drl',
	API_V_KEEP_ONE = 'keep',


	////////////////////////////
	// Libraries
	API_V_LIB_REF = 'lref',
	API_V_LIB_LOAD_TIME = 'ltim',
	API_V_LIB_LOAD_COUNT = 'lcnt',
	API_V_LIB_LOAD_LOG = 'llog',
	API_V_LIB_STATUS = 'lsts',


	////////////////////////////
	// Generic Rules
	API_V_RULE_REF_GUID = 'rgid',
	API_V_RULE_HIT_COUNT = 'rhct',

	API_V_ITEMS = 'itms',
	API_V_ACTION = 'actn',
		// ...

	////////////////////////////
	// Access Rules
	API_V_ACCESS_RULE_ACTION = 'acac',
		// ...
	API_V_ACCESS_PATH = 'acpt',
	API_V_ACCESS_NT_PATH = 'acnt',
	API_V_VOL_RULE = 'vorl',

	////////////////////////////
	// Execution Rules
	API_V_EXEC_RULE_ACTION = 'exac',
		// ...
	API_V_EXEC_SIGN_REQ = 'exsr',
	API_V_EXEC_ALLOWED_SIGNERS = 'exas',
	API_V_EXEC_ALLOWED_COLLECTIONS = 'excl',
		// ...
	API_V_EXEC_ON_TRUSTED_SPAWN = 'exot',
		// ...
	API_V_EXEC_ON_SPAWN = 'exos',
		// ...
	API_V_IMAGE_LOAD_PROTECTION = 'ilpr',
	API_V_IMAGE_COHERENCY_CHECKING = 'icck',
	API_V_INTEGRITY_LEVEL = 'ilvl',
	//API_V_PATH_PREFIX,
	//API_V_DEVICE_PATH,
	API_V_ALLOW_DEBUGGING = 'adbg',
	API_V_KEEP_ALIVE = 'kpal',

	////////////////////////////
	// Firewall Rules
	API_V_FW_RULE_ACTION = 'fwac',
		// ...
	API_V_FW_RULE_DIRECTION = 'fwdi',
		// ...
	API_V_FW_RULE_PROFILE = 'fwpr',
		// ...
	API_V_FW_RULE_PROTOCOL = 'fwpt',
	API_V_FW_RULE_INTERFACE = 'fwif',
		// ...
	API_V_FW_RULE_LOCAL_ADDR = 'fwla',
	API_V_FW_RULE_REMOTE_ADDR = 'fwra',
	API_V_FW_RULE_LOCAL_PORT = 'fwlp',
	API_V_FW_RULE_REMOTE_PORT = 'fwrp',
	API_V_FW_RULE_REMOTE_HOST = 'fwhs',
	API_V_FW_RULE_ICMP = 'fwic',
	API_V_FW_RULE_OS = 'fwos',
	API_V_FW_RULE_EDGE = 'fwed',


	////////////////////////////
	// Firewall Config
	API_V_FW_AUDIT_MODE = 'fwam',			// FwAuditPolicy
	API_V_FW_RULE_FILTER_MODE = 'fwfm',		// FwFilterMode
	API_V_FW_EVENT_STATE = 'fwes',			// FwEventState


	////////////////////////////
	// Process Info
	API_V_PROCESSES = 'prcs', // Process List
	API_V_PROCESS_REF = 'pref', // Inernal Process Reference

	API_V_PID = 'pid',
	API_V_PIDS = 'pids',
	API_V_TID = 'tid',
	API_V_TIDS = 'tids',
	API_V_ENCLAVE = 'encl',

	API_V_CREATE_TIME = 'ctim',
	API_V_PARENT_PID = 'ppid',
	API_V_CREATOR_PID = 'cpid', // API_V_EVENT_ACTOR_PID
	API_V_CREATOR_TID = 'ctid', // API_V_EVENT_ACTOR_TID

	API_V_KPP_STATE = 'kpps',
	API_V_FLAGS = 'fgs',
	API_V_SFLAGS = 'sfgs',

	API_V_NUM_THREADS = 'nthr',
	API_V_SERVICES = 'svcs',
	API_V_HANDLES = 'hnds',
	API_V_SOCKETS = 'skts',

	API_V_USER = 'user',
	API_V_USER_SID = 'usid',

	API_V_NUM_IMG = 'nimg',
	API_V_NUM_MS_IMG = 'nmsi',
	API_V_NUM_AV_IMG = 'navi',
	API_V_NUM_V_IMG = 'nvfi',
	API_V_NUM_S_IMG = 'nsig',
	API_V_NUM_U_IMG = 'nuns', 

	API_V_UW_REFS = 'uwrf',


	////////////////////////////
	// Handle Info
	API_V_HANDLE = 'hndl',
	API_V_HANDLE_TYPE = 'htyp',


	////////////////////////////
	// Event Logging
	API_V_EVENT_LOG = 'elog',

	// Program Access Log
	API_V_ACCESS_LOG = 'alog',

	API_V_PROG_RESOURCE_ACCESS = 'racc',
	API_V_PROG_PROCESS_ACCESS = 'pacc',
	API_V_PROG_EXEC_PARENTS = 'epnt',
	API_V_PROG_EXEC_CHILDREN = 'echd',
	API_V_PROG_INGRESS_ACTORS = 'iact',
	API_V_PROG_INGRESS_TARGETS = 'itgt',

	// Resource Access
	API_V_ACCESS_REF = 'aref',
	API_V_ACCESS_NAME = 'anam',
	API_V_ACCESS_NODES = 'anod',

	// Network Traffic Log
	API_V_TRAFFIC_LOG = 'tlog',

	API_V_SOCK_REF = 'sref',		// Internal Socket Reference
	API_V_SOCK_TYPE = 'styp',
	API_V_SOCK_LADDR = 'slad',
	API_V_SOCK_LPORT = 'slpt',
	API_V_SOCK_RADDR = 'srad',
	API_V_SOCK_RPORT = 'srpt',
	API_V_SOCK_STATE = 'ssta',
	API_V_SOCK_LSCOPE = 'lsco',
	API_V_SOCK_RSCOPE = 'rsco',
	API_V_SOCK_LAST_NET_ACT = 'lnet',
	API_V_SOCK_LAST_ALLOW = 'lall',
	API_V_SOCK_LAST_BLOCK = 'lblo',
	API_V_SOCK_UPLOAD = 'upld',
	API_V_SOCK_DOWNLOAD = 'dwnl',
	API_V_SOCK_UPLOADED = 'uplt',
	API_V_SOCK_DOWNLOADED = 'dwlt',
	API_V_SOCK_RHOST = 'rhst',


	////////////////////////////
	// Event Info
	API_V_EVENT_REF = 'eref',
	API_V_EVENT_LEVEL = 'elev',
	API_V_EVENT_TYPE = 'etyp',
	API_V_EVENT_SUB_TYPE = 'estp',
	API_V_EVENT_EXPECTED = 'eexp',
	API_V_EVENT_INDEX = 'eidx',
	API_V_EVENT_TIMEOUT = 'etmo',
	API_V_EVENT_DATA = 'edat',

	API_V_EVENT_ACTOR_PID = 'apid',
	API_V_EVENT_ACTOR_TID = 'atid',
	API_V_EVENT_ACTOR_UID = 'acid',		// UID
	API_V_EVENT_ACTOR_EID = 'aeid',		// Enclave Guid
	API_V_EVENT_ACTOR_SVC = 'asvc',		// uint32 tag
	API_V_EVENT_TARGET_PID = 'tpid',
	API_V_EVENT_TARGET_TID = 'ttid',
	API_V_EVENT_TARGET_UID = 'taid',	// UID
	API_V_EVENT_TARGET_EID = 'teid',	// Enclave Guid
	API_V_EVENT_TIME_STAMP = 'etim',

	API_V_OPERATION = 'oper',
	API_V_ACCESS_MASK = 'amsk',  // desired access
	API_V_STATUS = 'stat', // NTSTATUS
	API_V_NT_STATUS = 'ntst', // NTSTATUS
	API_V_EVENT_STATUS = 'ests', // EEventStatus
	API_V_EVENT_ACTION = 'eact', // EEventAction
	API_V_IS_DIRECTORY = 'isdr',
	API_V_WAS_BLOCKED = 'wblk', // <- API_V_EVENT_STATUS

	API_V_LAST_ACTIVITY = 'lact', // <-  API_V_EVENT_TIME_STAMP

	// Process Termination Event
	API_V_EVENT_ECODE = 'evid',

	// Process Ingress Event
	API_V_EVENT_ROLE = 'role', // EExecLogRole
	API_V_PROC_MISC_ID = 'pmid', // Program UID or Library UID
	API_V_PROC_MISC_ENCLAVE = 'pmen', // Enclave UID
	API_V_THREAD_ACCESS_MASK = 'tmsk', // desired access
	API_V_PROCESS_ACCESS_MASK = 'pmsk', // desired access

	// Image Load Event
	API_V_EVENT_IMG_BASE = 'ibas',
	API_V_EVENT_NO_PROTECT = 'nprt',
	API_V_EVENT_CREATING = 'crtn',
	API_V_EVENT_IMG_PROPS = 'iprp',
	API_V_EVENT_IMG_SEL = 'isel',
	API_V_EVENT_IMG_SECT_NR = 'isnr',

	// Network Firewall Event
	API_V_FW_ALLOW_RULES = 'fwar',
	API_V_FW_BLOCK_RULES = 'fwbr',

	// Event Configuration
	API_V_SET_NOTIFY_BITMASK = 'snbm',
	API_V_CLEAR_NOTIFY_BITMASK = 'cnbm',

	////////////////////////////
	// Event Trace Log
	API_V_LOG_TYPE = 'ltyp', // ETraceLogs
	API_V_LOG_OFFSET = 'loff',
	API_V_LOG_DATA = 'ldat',


	////////////////////////////
	// CI Info
	API_V_SIGN_INFO = 'sinf',
	API_V_SIGN_FLAGS = 'sflg', // --> API_V_CERT_STATUS
	API_V_IMG_SIGN_AUTH = 'saut',
	API_V_IMG_SIGN_BITS = 'sbit',
	API_V_IMG_SIGN_LEVEL = 'slvl',
	API_V_IMG_SIGN_POLICY = 'spol',
	API_V_FILE_HASH = 'fhas',
	API_V_FILE_HASH_ALG = 'falg',
	API_V_FILE_TMBP = 'ftmb',
	API_V_FILE_TMBP_ALG = 'talg',
	API_V_CERT_STATUS = 'csts',
	API_V_IMG_SIGN_NAME = 'snam',
	API_V_IMG_CERT_ALG = 'calg',
	API_V_IMG_CERT_HASH = 'chas',
	API_V_IMG_CA_NAME = 'canm',
	API_V_IMG_CA_ALG = 'caal',
	API_V_IMG_CA_HASH = 'cahs',


	////////////////////////////
	// DNS Cache
	API_V_DNS_CACHE = 'dcch',
	API_V_DNS_CACHE_REF = 'dcre',
	API_V_DNS_HOST = 'dhst',
	API_V_DNS_TYPE = 'dtyp',
	API_V_DNS_ADDR = 'dadr',
	API_V_DNS_DATA = 'ddat',
	API_V_DNS_TTL = 'dttl',
	API_V_DNS_STATUS = 'dsts',
	API_V_DNS_QUERY_COUNT = 'dqc',
	API_V_DNS_RULE_ACTION = 'drac',
	API_V_DNS_LIST_INFO = 'dlin',


	////////////////////////////
	// Volume
	API_V_VOLUMES = 'vols',
	API_V_VOL_REF = 'vref',
	API_V_VOL_PATH = 'vpth',
	API_V_VOL_DEVICE_PATH = 'vdev',
	API_V_VOL_MOUNT_POINT = 'vmpo',
	API_V_VOL_SIZE = 'vsiz',
	API_V_VOL_PASSWORD = 'vpwd',
	API_V_VOL_PROTECT = 'vprt',
	API_V_VOL_LOCKDOWN = 'vlck',
	API_V_VOL_CIPHER = 'vcip',
	API_V_VOL_OLD_PASS = 'vold',
	API_V_VOL_NEW_PASS = 'vnew',


	////////////////////////////
	// Tweaks
	API_V_TWEAKS = 'twks',
	API_V_TWEAK_HINT = 'thnt',
		// ...
	API_V_TWEAK_STATUS = 'tsts',
		// ...
	API_V_TWEAK_LIST = 'twkl',
	API_V_TWEAK_TYPE = 'ttyp',
	API_V_TWEAK_ID = 'twid',
	API_V_TWEAK_NAME = 'twnm',
	API_V_TWEAK_INDEX = 'twix',
		// ...
	API_V_TWEAK_WIN_VER = 'twvw',
	API_V_TWEAK_IS_SET = 'tset',
	API_V_TWEAK_IS_APPLIED = 'tapp',

	////////////////////////////
	// Presets
	API_V_PRESETS = 'psts',
	API_V_PRESET = 'pset',
	API_V_IS_ACTIVE = 'isac',


	////////////////////////////
	// Support
	API_V_SUPPORT_NAME = 'snme',
	API_V_SUPPORT_STATE = 'sstt',
	API_V_SUPPORT_STATUS = 'ssts',
	API_V_SUPPORT_HWID = 'shwi',


	////////////////////////////
	// Progress Info
	API_V_PROGRESS_FINISHED = 'pfin',
	API_V_PROGRESS_TOTAL = 'ptot',
	API_V_PROGRESS_DONE = 'pdon',


	////////////////////////////
	// Basic
	API_V_CACHE_TOKEN = 'ctok',
	API_V_ERR_CODE = 'ecod',
	API_V_ERR_MSG = 'emsg',

	////////////////////////////
	// Other
	API_V_LOG_MEM_USAGE = 'lmus',
	API_V_SVC_MEM_WS = 'smws',
	API_V_SVC_MEM_PB = 'smpb',

	API_V_MB_TEXT = 'mbtx',
	API_V_MB_TITLE = 'mbti',
	API_V_MB_TYPE = 'mbty',
	API_V_MB_CODE = 'mbec',

	API_V_LAST = 0x80000000
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
#define SVC_EVENT_DNS_INIT_FAILED	0x0108
#define SVC_EVENT_GPO_FAILED		0x0109
#define SVC_EVENT_RULE_NOT_FOUND	0x010A
#define SVC_EVENT_JSLOG_MSG			0x010B


/////////////////////////////////////////////////////////////////////////////
// Driver Config Status
//

#define CONFIG_STATUS_DIRTY		0x00000001 // config has been altered
#define CONFIG_STATUS_PROTECTED	0x00000002 // config is signature protected
#define CONFIG_STATUS_LOCKED	0x00000004 // config is locked and can not be changed
#define CONFIG_STATUS_KEYLOCKED	0x00000008 // the registry key is locked and can not be changed without a reboot

#define CONFIG_STATUS_BAD		0x00FF0000
#define CONFIG_STATUS_NO_SIG	0x00010000 // config is not signed
#define CONFIG_STATUS_BAD_SIG	0x00020000 // config signature is invalid
#define CONFIG_STATUS_BAD_SEQ	0x00040000 // config has invalid sequence number (replay atack?)
#define CONFIG_STATUS_CORRUPT	0x00080000 // config is corrupted and can not be parsed

#define CONFIG_STATUS_WARNINGS	0xFF000000
#define CONFIG_STATUS_RESTORED	0x01000000 // config was restored
#define CONFIG_STATUS_REVERTED	0x02000000 // config was reverted
#define CONFIG_STATUS_SUPRESSED	0x04000000 // config was supressed



/////////////////////////////////////////////////////////////////////////////
//
//

#define DEF_MP_SYS_FILE L"\\$mpsys$"

#define DEF_MP_SIG_VERSION 1

#include "PrivacyAPIs.h"