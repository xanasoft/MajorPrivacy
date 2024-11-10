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

/////////////////////////////////////////////////////////////////////////////
// Variant Field IDs
// 
// String based IDs are used when savong data to file or registry 
//


#define API_S_CONFIG						"Config"
#define API_S_VERSION						"Version"

#define API_S_ACCESS_LOG					"AccessLog"
#define API_S_TRAFFIC_LOG					"TrafficLog"

#define API_S_GUI_CONFIG					"GuiConfig"
#define API_S_DRIVER_CONFIG					"DriverConfig"
#define API_S_USER_KEY						"UserKey"
#define API_S_SERVICE_CONFIG				"ServiceConfig"
#define API_S_PROGRAM_RULES					"ProgramRules"
#define API_S_ACCESS_RULES					"AccessRules"
#define API_S_FW_RULES						"FirewallRules"
#define API_S_PROGRAMS						"Programs"
#define API_S_LIBRARIES						"Libraries"
#define API_S_TRACELOG						"TraceLog"

// Programs
#define API_S_PID							"PID"
#define	API_S_PIDS							"PIDs"

#define API_S_PROG_UID						"UID"
#define API_S_PROG_UIDS						"UIDs"

#define API_S_PROG_ID						"ID"
#define API_S_PROG_IDS						"IDs"

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
#define API_S_FILE_PATH2					"FileNtPath"
#define API_S_SVC_TAG						"ServiceTag"
#define API_S_APP_SID						"AppSID"	
#define API_S_APP_NAME						"AppName"
#define API_S_PACK_NAME						"Package"
#define API_S_REG_KEY						"RegKey"
#define API_S_PROG_PATTERN					"Pattern"

#define API_S_OWNER							"Owner" // used by firewall rules

#define API_S_PROG_ITEMS					"Items"

#define API_S_PROG_LAST_EXEC				"LastExec"

// Info
#define API_S_NAME							"Name"
#define API_S_ICON							"Icon"
#define API_S_INFO							"Info"	
#define API_S_RULE_GROUP					"Group"
#define API_S_RULE_DESCR					"Description"

//

// Libraries
#define API_S_LIB_REF						"LibRef"
#define API_S_LIB_LOAD_TIME					"LoadTime"
#define API_S_LIB_LOAD_COUNT				"LoadCount"
#define API_S_SIGN_INFO						"SignInfo" // SLibraryInfo::USign
#define API_S_SIGN_INFO_AUTH					"SignAuthority" // KPH_VERIFY_AUTHORITY
#define API_S_SIGN_INFO_LEVEL				"SignLevel"
#define API_S_SIGN_INFO_POLICY				"SignPolicy"
#define API_S_LIB_STATUS					"LoadStatus"

// Rules
#define API_S_RULE_ENABLED					"Enabled"
#define API_S_RULE_TEMP						"Temporary"

#define API_S_RULE_GUID						"Guid"
#define API_S_RULE_INDEX					"Index"

#define API_S_RULE_DATA						"Data"

// Access Rules
#define API_S_ACCESS_RULE_ACTION			"Access"
	#define API_S_ACCESS_RULE_ACTION_ALLOW		"Allow"
	#define API_S_ACCESS_RULE_ACTION_ALLOW_RO	"AllowRO"
	#define API_S_ACCESS_RULE_ACTION_ENUM		"Enum"
	#define API_S_ACCESS_RULE_ACTION_BLOCK		"Block"
	#define API_S_ACCESS_RULE_ACTION_PROTECT	"Protect"
#define API_S_ACCESS_PATH					"AccessPath"
#define API_S_ACCESS_PATH2					"AccessNtPath"
#define API_S_VOL_RULE						"VolumeRule"	

// Execution Rules
#define API_S_EXEC_RULE_ACTION				"Execution"
	#define API_S_EXEC_RULE_ACTION_ALLOW		"Allow"
	#define API_S_EXEC_RULE_ACTION_BLOCK		"Block"
	#define API_S_EXEC_RULE_ACTION_PROTECT		"Protect"
	#define API_S_EXEC_RULE_ACTION_ISOLATE		"Isolate"
#define API_S_EXEC_SIGN_REQ					"SignReq"
	#define API_S_EXEC_SIGN_REQ_VERIFIED		"Verified"
	#define API_S_EXEC_SIGN_REQ_MICROSOFT		"MSVerified"
	#define API_S_EXEC_SIGN_REQ_TRUSTED			"MSTrusted"
	#define API_S_EXEC_SIGN_REQ_OTHER 			"Any"
	#define API_S_EXEC_SIGN_REQ_NONE			"None"
#define API_S_EXEC_ON_TRUSTED_SPAWN			"TrustedSpawn"
#define API_S_EXEC_ON_SPAWN					"ProcessSpawn"
	#define API_S_EXEC_ON_SPAWN_ALLOW			"Allow"
	#define API_S_EXEC_ON_SPAWN_BLOCK			"Block"
	#define API_S_EXEC_ON_SPAWN_EJECT			"Eject"
#define API_S_IMAGE_LOAD_PROTECTION			"ImageProtection"

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

// Sockets
#define API_S_PROG_SOCKETS					"Sockets"
#define API_S_SOCK_LAST_ACT					"LastNetActivity"
#define API_S_SOCK_LAST_ALLOW				"LastFwAllow"
#define API_S_SOCK_LAST_BLOCK				"LastFwBlock"
#define API_S_SOCK_UPLOAD					"Upload"
#define API_S_SOCK_DOWNLOAD					"Download"
#define API_S_SOCK_UPLOADED					"Uploaded"
#define API_S_SOCK_DOWNLOADED				"Downloaded"

// Tweaks
#define API_S_TWEAKS						"Tweaks"
#define API_S_TWEAK_HINT					"TweakHint"
	#define API_S_TWEAK_HINT_NONE				"None"
	#define API_S_TWEAK_HINT_RECOMMENDED		"Recommended"
#define API_S_TWEAK_STATUS					"TweakStatus"
	#define API_S_TWEAK_STATUS_NOT_SET			"NotSet"
	#define API_S_TWEAK_STATUS_APPLIED			"Applied"
	#define API_S_TWEAK_STATUS_SET				"Set"
	#define API_S_TWEAK_STATUS_MISSING			"Missing"
	#define API_S_TWEAK_STATUS_GROUP			"TypeGroup"
#define API_S_TWEAK_LIST					"TweakList"
#define API_S_TWEAK_TYPE					"TweakType"
	#define API_S_TWEAK_TYPE_GROUP				"Group"
	#define API_S_TWEAK_TYPE_SET				"Set"
	#define API_S_TWEAK_TYPE_REG				"Reg"
	#define API_S_TWEAK_TYPE_GPO				"Gpo"
	#define API_S_TWEAK_TYPE_SVC				"Svc"
	#define API_S_TWEAK_TYPE_TASK				"Task"
	#define API_S_TWEAK_TYPE_FS					"Fs"
	#define API_S_TWEAK_TYPE_EXEC				"Exec"
	#define API_S_TWEAK_TYPE_FW					"Fw"
#define API_S_TWEAK_IS_SET					"IsSet"

#define API_S_TWEAK_KEY						"Key"
#define API_S_TWEAK_VALUE					"Value"
#define API_S_TWEAK_DATA					"Data"
#define API_S_TWEAK_FOLDER					"Folder"
#define API_S_TWEAK_ENTRY					"Entry"
#define API_S_TWEAK_SVC_TAG					"SvcTag"
#define API_S_TWEAK_PATH					"Path"
#define API_S_TWEAK_PROG_ID					"ProgId"


// Crypto
#define API_S_PUB_KEY						"PublicKey"
#define API_S_PRIV_KEY						"PrivateKey"
#define API_S_KEY_BLOB						"KeyBlob"
#define API_S_HASH							"Hash"