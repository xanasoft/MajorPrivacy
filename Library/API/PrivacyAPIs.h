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

/////////////////////////////////////////////////////////////////////////////
// Variant Field IDs
// 
// String based IDs are used when savong data to file or registry 
//


////////////////////////////
// Config
#define API_S_CONFIG						"Config"
#define API_S_VERSION						"Version"

#define API_S_VALUE							"Value"
#define API_S_DATA							"Data"
#define API_S_FOLDER						"Folder"
#define API_S_ENTRY							"Entry"

#define API_S_TYPE							"Type"
	#define API_S_HASH_TYPE_FILE			"FileHash"
	#define API_S_HASH_TYPE_CERT			"CertHash"
#define API_S_GUID							"Guid"
#define API_S_INDEX							"Index"
#define API_S_ENABLED						"Enabled"
#define API_S_TEMP							"Temporary"
#define API_S_TIMEOUT						"TimeOut"
#define API_S_TIME_STAMP					"TimeStamp"
#define API_S_LOCKDOWN						"Lockdown"

// Fields
#define API_S_GUI_CONFIG					"GuiConfig"
#define API_S_DRIVER_CONFIG					"DriverConfig"
#define API_S_USER_KEY						"UserKey"
#define API_S_SERVICE_CONFIG				"ServiceConfig"
#define API_S_PROGRAM_RULES					"ProgramRules"
#define API_S_ACCESS_RULES					"AccessRules"
#define API_S_FW_RULES						"FirewallRules"
#define API_S_PROGRAMS						"Programs"
#define API_S_LIBRARIES						"Libraries"
#define API_S_ENCLAVES						"Enclaves"
#define API_S_COLLECTIONS					"Collections"
#define API_S_HASH_DB						"HashDB"


////////////////////////////
// Crypto & Protection
#define API_S_PUB_KEY						"PublicKey"
#define API_S_PRIV_KEY						"PrivateKey"
#define API_S_HASH							"Hash"
#define API_S_KEY_BLOB						"KeyBlob"
#define API_S_LOCK							"Lock"
#define API_S_UNLOCK						"Unlock"
#define API_S_RANDOM						"Random"
#define API_S_SIGNATURE						"Signature"
#define API_S_SEQ_NUM						"SeqNumber"


////////////////////////////
// Programs
#define API_S_PROG_ITEMS					"Items"

#define API_S_ENCLAVE						"Enclave"

#define API_S_USER							"User"
#define API_S_USER_SID						"UserSID"

// Unique ID
#define API_S_PROG_UID						"UID"
#define API_S_PROG_UIDS						"UIDs"

// Program ID
#define API_S_ID						"ID"
#define API_S_IDS						"IDs"
#define API_S_PARENT					"Parent"

#define API_S_PROG_TYPE						"Type"
	#define API_S_PROG_TYPE_FILE				"File"
	#define API_S_PROG_TYPE_PATTERN				"Pattern"
	#define API_S_PROG_TYPE_INSTALL				"Install"
	#define API_S_PROG_TYPE_SERVICE				"Service"
	#define API_S_PROG_TYPE_PACKAGE				"Package"
	#define API_S_PROG_TYPE_GROUP				"Group"
	#define API_S_PROG_TYPE_All					"All"
	#define API_S_PROG_TYPE_ROOT				"Root"

#define API_S_FILE_PATH						"FilePath"
#define API_S_USE_SCRIPT					"UseScript"
#define API_S_SCRIPT						"Script"
#define API_S_DLL_INJECT_MODE				"DllInjectMode"
	#define API_S_EXEC_DLL_MODE_DEFAULT		"Default"
	#define API_S_EXEC_DLL_MODE_INJECT_LOW	"InjectLow"
	#define API_S_EXEC_DLL_MODE_INJECT_HIGH	"InjectHigh"
	#define API_S_EXEC_DLL_MODE_DISABLED	"Disabled"
#define API_S_DLL_INJECT_MODE_ENCLAVE		"Enclave"
#define API_S_INTERACTIVE					"Interactive"
#define API_S_FILE_NT_PATH					"FileNtPath"
#define API_S_SERVICE_TAG					"ServiceTag"
#define API_S_APP_SID						"AppSID"	
#define API_S_APP_NAME						"AppName"
#define API_S_PACK_NAME						"Package"
#define API_S_REG_KEY						"RegKey"
#define API_S_PROG_PATTERN					"Pattern"
#define API_S_OWNER							"Owner" // used by firewall rules

// Info
#define API_S_NAME							"Name"
#define API_S_ICON							"Icon"
#define API_S_INFO							"Info"	

#define API_S_EXEC_TRACE					"ExecTrace"
	#define API_S_TRACE_DEFAULT					"Default"
	#define API_S_TRACE_ONLY					"Trace"
	#define API_S_TRACE_NONE					"NoTrace"
	#define API_S_TRACE_PRIVATE					"Private"
#define API_S_RES_TRACE						"ResTrace"
#define API_S_NET_TRACE						"NetTrace"
#define API_S_SAVE_TRACE					"SaveTrace"
	#define API_S_SAVE_TRACE_DEFAULT			"Default"
	#define API_S_SAVE_TRACE_TO_DISK			"ToDisk"
	#define API_S_SAVE_TRACE_TO_RAM				"ToRam"

#define API_S_RULE_GROUP					"Group"
#define API_S_RULE_DESCR					"Description"

// Status
#define API_S_LAST_EXEC						"LastExec"
#define API_S_ITEM_MISSING					"Missing"
#define API_S_MEM_USED						"MemUsed"


////////////////////////////
// Libraries
#define API_S_LIB_REF						"LibRef"
#define API_S_LIB_LOAD_TIME					"LoadTime"
#define API_S_LIB_LOAD_COUNT				"LoadCount"
#define API_S_LIB_LOAD_LOG					"LoadLog"
#define API_S_SIGN_FLAGS					"SignFlags"
#define API_S_SIGN_INFO						"SignInfo"
#define API_S_SIGN_INFO_AUTH				"SignAuthority"
#define API_S_SIGN_INFO_LEVEL				"SignLevel"
#define API_S_SIGN_INFO_POLICY				"SignPolicy"
#define API_S_SIGN_SIGN_BITS				"SignBits"
#define API_S_CERT_STATUS					"CertStatus"
	#define API_S_CERT_STATUS_UNKNOWN			"Unknown"
	#define API_S_CERT_STATUS_OK				"OK"
	#define API_S_CERT_STATUS_ERROR 			"Error"
	#define API_S_CERT_STATUS_FAIL				"Fail"
	#define API_S_CERT_STATUS_DUMMY 			"Dummy"
	#define API_S_CERT_STATUS_NONE 				"None"
#define API_S_CERT_STATUS_UNVERIFIED		"Unverified"

#define API_S_FILE_HASH						"FileHash"
#define API_S_FILE_HASH_ALG					"FileHashAlg"
#define API_S_CERT_HASH_ALG					"SignerHashAlg"
#define API_S_CERT_HASH						"SignerHash"
#define API_S_SIGNER_NAME					"SignerName"
#define API_S_CA_HASH_ALG					"IssuerHashAlg"
#define API_S_CA_HASH						"IssuerHash"
#define API_S_CA_NAME						"IssuerName"
#define API_S_LIB_STATUS					"LoadStatus"


////////////////////////////
// Rules
#define API_S_RULE_REF_GUID					"RefGuid"
#define API_S_RULE_HIT_COUNT				"HitCount"

#define API_S_RULE_STATE					"RuleState"
	#define API_S_RULE_STATE_UNAPPROVED			"Unapproved"
	#define API_S_RULE_STATE_APPROVED			"Approved"
	#define API_S_RULE_STATE_BACKUP				"Backup"
	#define API_S_RULE_STATE_UNAPPROVED_DISABLED "Disabled"
	#define API_S_RULE_STATE_DIVERGED 			"Diverged"
	#define API_S_RULE_STATE_DELETED			"Deleted"

#define API_S_ORIGINAL_GUID					"OriginalGuid"
#define API_S_TEMPLATE_GUID					"TemplateGuid"
#define API_S_VOLUME_GUID					"VolumeGuid"

#define API_S_SOURCE						"Source"
	#define API_S_SOURCE_UNKNOWN				"Unknown"
	#define API_S_SOURCE_DEFAULT 				"WindowsDefault"
	#define API_S_SOURCE_STORE					"MicrosoftStore"
	#define API_S_SOURCE_MP 					"MajorPrivacy"
	#define API_S_SOURCE_TEMPLATE				"AutoTemplate"

////////////////////////////
// Access Rules
#define API_S_ACCESS_RULE_ACTION			"Access"
	#define API_S_ACCESS_RULE_ACTION_ALLOW		"Allow"
	#define API_S_ACCESS_RULE_ACTION_ALLOW_RO	"AllowRO"
	#define API_S_ACCESS_RULE_ACTION_ENUM		"Enum"
	#define API_S_ACCESS_RULE_ACTION_BLOCK		"Block"
	#define API_S_ACCESS_RULE_ACTION_PROTECT	"Protect"
	#define API_S_ACCESS_RULE_ACTION_IGNORE		"Ignore"
#define API_S_ACCESS_PATH					"AccessPath"
#define API_S_ACCESS_NT_PATH				"AccessNtPath"
#define API_S_VOL_RULE						"VolumeRule"	

////////////////////////////
// Execution Rules
#define API_S_EXEC_RULE_ACTION				"Execution"
	#define API_S_EXEC_RULE_ACTION_ALLOW		"Allow"
	#define API_S_EXEC_RULE_ACTION_BLOCK		"Block"
	#define API_S_EXEC_RULE_ACTION_PROTECT		"Protect"
	#define API_S_EXEC_RULE_ACTION_ISOLATE		"Isolate"
	#define API_S_EXEC_RULE_ACTION_AUDIT		"Audit"
#define API_S_EXEC_SIGN_REQ					"SignReq"
	#define API_S_EXEC_SIGN_REQ_VERIFIED		"Verified"
	#define API_S_EXEC_SIGN_REQ_MICROSOFT		"MSVerified"
	#define API_S_EXEC_SIGN_REQ_TRUSTED			"MSTrusted"
	#define API_S_EXEC_SIGN_REQ_OTHER 			"Any"
	#define API_S_EXEC_SIGN_REQ_NONE			"None"
#define API_S_EXEC_ALLOWED_SIGNERS			"AllowedSigners"
	#define API_S_EXEC_ALLOWED_SIGNERS_WINDOWS	"[WIN]"
	#define API_S_EXEC_ALLOWED_SIGNERS_MICROSOFT "[MSFT]"
	#define API_S_EXEC_ALLOWED_SIGNERS_ANTIMALWARE	"[AV]"
	#define API_S_EXEC_ALLOWED_SIGNERS_AUTHENTICODE	"[AC]"
	#define API_S_EXEC_ALLOWED_SIGNERS_STORE	"[Store]"
	#define API_S_EXEC_ALLOWED_SIGNERS_DEVELOPER "[MPDev]"
	#define API_S_EXEC_ALLOWED_SIGNERS_USER_SIGN "[UserSig]"
	#define API_S_EXEC_ALLOWED_SIGNERS_USER		"[User]"
	#define API_S_EXEC_ALLOWED_SIGNERS_ENCLAVE	"[Enclave]"
#define API_S_EXEC_ON_TRUSTED_SPAWN			"TrustedSpawn"
	// ...
#define API_S_EXEC_ON_SPAWN					"ProcessSpawn"
	#define API_S_EXEC_ON_SPAWN_ALLOW			"Allow"
	#define API_S_EXEC_ON_SPAWN_BLOCK			"Block"
	#define API_S_EXEC_ON_SPAWN_EJECT			"Eject"
#define API_S_INTEGRITY_LEVEL				"IntegrityLevel"
	#define API_S_INTEGRITY_LEVEL_UNTRUSTED		"Untrusted"
	#define API_S_INTEGRITY_LEVEL_LOW			"Low"
	#define API_S_INTEGRITY_LEVEL_MEDIUM		"Medium"
	#define API_S_INTEGRITY_LEVEL_MEDIUM_PLUS	"MediumPlus"
	#define API_S_INTEGRITY_LEVEL_HIGH			"High"
	#define API_S_INTEGRITY_LEVEL_SYSTEM		"System"
#define API_S_IMAGE_LOAD_PROTECTION			"ImageLoadProtection"
#define API_S_IMAGE_COHERENCY_CHECKING		"ImageCoherencyChecking"
#define API_S_ALLOW_DEBUGGING 				"AllowDebugging"
#define API_S_KEEP_ALIVE 					"KeepAlive"

////////////////////////////
// Firewall Rules
#define API_S_FW_RULE_ACTION				"FwAction"
	#define API_S_FW_RULE_ACTION_ALLOW			"Allow"
	#define API_S_FW_RULE_ACTION_BLOCK			"Block"
#define API_S_FW_RULE_DIRECTION				"Direction"
	#define API_S_FW_RULE_DIRECTION_BIDIRECTIONAL "Bidirectional"
	#define API_S_FW_RULE_DIRECTION_INBOUND		"Inbound"
	#define API_S_FW_RULE_DIRECTION_OUTBOUND	"Outbound"
#define API_S_FW_RULE_PROFILE				"FwProfile"
	#define API_S_FW_RULE_PROFILE_ALL			"All"
	#define API_S_FW_RULE_PROFILE_DOMAIN		"Domain"
	#define API_S_FW_RULE_PROFILE_PRIVATE		"Private"
	#define API_S_FW_RULE_PROFILE_PUBLIC		"Public"
#define API_S_FW_RULE_PROTOCOL				"Protocol"
#define API_S_FW_RULE_INTERFACE				"Interface"
	#define API_S_FW_RULE_INTERFACE_ANY			"Any"
	#define API_S_FW_RULE_INTERFACE_LAN			"Lan"
	#define API_S_FW_RULE_INTERFACE_WIRELESS	"Wireless"
	#define API_S_FW_RULE_INTERFACE_REMOTEACCESS "RemoteAccess"
#define API_S_FW_RULE_LOCAL_ADDR			"LocalAddr"
#define API_S_FW_RULE_REMOTE_ADDR			"RemoteAddr"
#define API_S_FW_RULE_LOCAL_PORT			"LocalPort"
#define API_S_FW_RULE_REMOTE_PORT			"RemotePort"
#define API_S_FW_RULE_REMOTE_HOST			"RemoteHost"
#define API_S_FW_RULE_ICMP					"Icmp"
#define API_S_FW_RULE_OS					"Os"
#define API_S_FW_RULE_EDGE					"Edge"


////////////////////////////
// Processes
#define API_S_PID							"PID"
#define	API_S_PIDS							"PIDs"

////////////////////////////
// DNS
#define API_S_DNS_RULES						"DnsRules"
#define API_S_DNS_HOST						"Host"
#define API_S_DNS_RULE_ACTION				"Action"
	#define API_S_DNS_RULE_ACTION_BLOCK			"Block"
	#define API_S_DNS_RULE_ACTION_ALLOW			"Allow"	

////////////////////////////
// Logs
#define API_S_ACCESS_LOG					"AccessLog"
#define API_S_INGRESS_LOG					"IngressLog"
#define API_S_TRAFFIC_LOG					"TrafficLog"
#define API_S_PROG_SOCKETS					"Sockets"
#define API_S_EVENT_LOG						"EventLog"

////////////////////////////
// Sockets Stats
#define API_S_SOCK_LAST_ACT					"LastNetActivity"
#define API_S_SOCK_LAST_ALLOW				"LastFwAllow"
#define API_S_SOCK_LAST_BLOCK				"LastFwBlock"
#define API_S_SOCK_UPLOAD					"Upload"
#define API_S_SOCK_DOWNLOAD					"Download"
#define API_S_SOCK_UPLOADED					"Uploaded"
#define API_S_SOCK_DOWNLOADED				"Downloaded"
#define API_S_SOCK_RHOST					"RemoteHost"


////////////////////////////
// Tweaks
#define API_S_TWEAKS						"Tweaks"
#define API_S_TWEAK_HINT					"Hint"
	#define API_S_TWEAK_HINT_NONE				"None"
	#define API_S_TWEAK_HINT_RECOMMENDED		"Recommended"
	#define API_S_TWEAK_HINT_NOT_RECOMMENDED	"NotRecommended"
	#define API_S_TWEAK_HINT_BREAKING			"Breaking"
#define API_S_TWEAK_STATUS					"Status"
	#define API_S_TWEAK_STATUS_NOT_SET			"NotSet"
	#define API_S_TWEAK_STATUS_APPLIED			"Applied"
	#define API_S_TWEAK_STATUS_SET				"Set"
	#define API_S_TWEAK_STATUS_MISSING			"Missing"
	#define API_S_TWEAK_STATUS_GROUP			"TypeGroup"
#define API_S_TWEAK_LIST					"List"
#define API_S_TWEAK_TYPE					"Type"
	#define API_S_TWEAK_TYPE_GROUP				"Group"
	#define API_S_TWEAK_TYPE_SET				"Bundle"
	#define API_S_TWEAK_TYPE_REG				"Registry"
	#define API_S_TWEAK_TYPE_GPO				"GroupPolicy"
	#define API_S_TWEAK_TYPE_SVC				"SystemService"
	#define API_S_TWEAK_TYPE_TASK				"TaskScheduler"
	#define API_S_TWEAK_TYPE_RES				"ResourceAccess"
	#define API_S_TWEAK_TYPE_EXEC				"ExecutionControl"
	#define API_S_TWEAK_TYPE_FW					"Firewall"

#define API_S_TWEAK_ID						"ID"
#define API_S_TWEAK_NAME					"Name"
#define API_S_TWEAK_INDEX					"Index"
#define API_S_TWEAK_WIN_VER					"WinVer"
#define API_S_TWEAK_IS_SET					"IsSet"
#define API_S_TWEAK_IS_APPLIED				"IsApplied"

////////////////////////////
// Other
#define API_S_LOG_MEM_USAGE					"LogMemUsage"

