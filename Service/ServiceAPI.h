/*
 * Copyright (c) 2023 David Xanatos, xanasoft.com
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

#include "../Library/API/PortMessage.h"

/////////////////////////////////////////////////////////////////////////////
// Service API
//

#define API_SERVICE_NAME        L"PrivacyAgent"

#define API_SERVICE_BINARY      API_SERVICE_NAME L".exe"

#define API_SERVICE_PORT	    L"\\RPC Control\\" API_SERVICE_NAME L"Port"

#define API_SERVICE_PIPE	    L"\\\\.\\pipe\\" API_SERVICE_NAME L"Pipe"

#define APP_NAME		        L"MajorPrivacy"

enum {
    SVC_API_FIRST                   = ('PS' << 16),

    SVC_API_GET_VERSION,

    SVC_API_GET_CONFIG,
    SVC_API_SET_CONFIG,

    SVC_API_GET_FW_RULES,
    SVC_API_SET_FW_RULE,
    SVC_API_GET_FW_RULE,
    SVC_API_DEL_FW_RULE,

    SVC_API_GET_FW_PROFILE,
    SVC_API_SET_FW_PROFILE,

    SVC_API_GET_PROCESSES,      // live runnign or recently terminated processes
    SVC_API_GET_PROCESS,

    SVC_API_GET_PROGRAMS,

    SVC_API_GET_SOCKETS,
    SVC_API_GET_TRAFFIC,
    SVC_API_GET_DNC_CACHE,

    SVC_API_GET_TRACE_LOG,

    SVC_API_GET_FS_TREE,
    SVC_API_GET_REG_TREE,

    SVC_API_EVENT_RULE_CHANGED,
    SVC_API_EVENT_NET_ACTIVITY,

    SVC_API_GET_TWEAKS,
    SVC_API_APPLY_TWEAK,
    SVC_API_UNDO_TWEAK,

    SVC_API_SHUTDOWN,

    SVC_API_LAST
};

#define SVC_API_ERR_CODE        "ErrCode"
#define SVC_API_ERR_MSG         "ErrMsg"

#define SVC_API_CONF_MAP		"Map"
#define SVC_API_CONF_NAME       "Name"
#define SVC_API_CONF_VALUE      "Value"

// SVC_API_GET_FW_RULES
#define SVC_API_PROG_IDS        "ProgIDs"
#define SVC_API_RULES           "Rules"

// SVC_API_GET_PROCESSE
#define SVC_API_PROCESSES       "Processes"

#define SVC_API_PROGRAMS        "Programs"

#define SVC_API_APPLICATIONS    "Applications"
#define SVC_API_SERVICES        "Services"
#define SVC_API_PACKAGES        "Packages"

// SVC_API_GET_SOCKETS
#define SVC_API_SOCKETS         "Sockets"

#define SVC_API_NET_TRAFFIC     "Traffic"
#define SVC_API_NET_HOST        "Host"
#define SVC_API_NET_UPLOADED    "Uploaded"
#define SVC_API_NET_DOWNLOADED  "Downloaded"
#define SVC_API_NET_LAST_ACT    "LastActivity"

// SVC_API_GET_DNC_CACHE
#define SVC_API_DNS_CACHE       "Cache"
#define SVC_API_DNS_CACHE_REF   "Ref"
#define SVC_API_DNS_HOST        "Host"
#define SVC_API_DNS_TYPE        "Type"
#define SVC_API_DNS_ADDR        "Addr"
#define SVC_API_DNS_DATA        "Data"
#define SVC_API_DNS_TTL         "TTL"
#define SVC_API_DNS_QUERY_COUNT "QCnt"


#define SVC_API_LOG_DATA        "LogData"

// Program ID
#define SVC_API_ID_PROG         "ProgID"
#define SVC_API_ID_TYPE         "Type"
#define SVC_API_ID_TYPE_FILE    "File"
#define SVC_API_ID_TYPE_PATTERN "Pattern"
#define SVC_API_ID_TYPE_INSTALL "Install"
#define SVC_API_ID_TYPE_SERVICE "Service"
#define SVC_API_ID_TYPE_PACKAGE "Package"
#define SVC_API_ID_TYPE_All     "All"
#define SVC_API_ID_TYPE_GROUP   "Group"
//
#define SVC_API_ID_FILE_PATH    "Path"
#define SVC_API_ID_REG_KEY      "RegKey"
#define SVC_API_ID_SVC_TAG      "SvcTag"
#define SVC_API_ID_APP_SID      "AppSID"

// Firewall Rule
#define SVC_API_FW_GUID         "GUID"
#define SVC_API_FW_INDEX        "Index"
//
#define SVC_API_FW_NAME         "Name"
#define SVC_API_FW_GROUP        "Group"
#define SVC_API_FW_DESCRIPTION  "Description"
//
#define SVC_API_FW_ENABLED      "Enabled"
#define SVC_API_FW_ACTION       "Action"
#define SVC_API_FW_ALLOW        "Allow"
#define SVC_API_FW_BLOCK        "Block"
#define SVC_API_FW_NONE         "None"
#define SVC_API_FW_DIRECTION    "Direction"
#define SVC_API_FW_BIDIRECTIONAL "Bidirectional"
#define SVC_API_FW_INBOUND      "Inbound"
#define SVC_API_FW_OUTBOUND     "Outbound"
#define SVC_API_FW_PROFILE      "Profile"
#define SVC_API_FW_ALL          "All"
#define SVC_API_FW_DOMAIN       "Domain"
#define SVC_API_FW_PRIVATE      "Private"
#define SVC_API_FW_PUBLIC       "Public"
//
#define SVC_API_FW_PROTOCOL     "Protocol"
#define SVC_API_FW_INTERFACE    "Interface"
#define SVC_API_FW_ANY          "ANY"
#define SVC_API_FW_LAN          "Lan"
#define SVC_API_FW_WIRELESS     "Wireless"
#define SVC_API_FW_REMOTEACCESS "RemoteAccess"
#define SVC_API_FW_LOCAL_ADDR   "LocalAddresses"
#define SVC_API_FW_LOCAL_PORT   "LocalPorts"
#define SVC_API_FW_REMOTE_ADDR  "RemoteAddresses"
#define SVC_API_FW_REMOTE_PORT  "RemotePorts"
#define SVC_API_FW_REMOTE_HOST  "RemoteHost"
#define SVC_API_FW_ICMP_OPT     "IcmpOpt"
//
#define SVC_API_FW_OS_P         "OsPlatform"
//
#define SVC_API_FW_EDGE_T       "EdgeTraversal"
//
#define SVC_API_FW_CHANGE       "Change"
#define SVC_API_FW_ADDED        "Added"
#define SVC_API_FW_CHANGED      "Changed"
#define SVC_API_FW_REMOVED      "Removed"
//

// Process
#define SVC_API_PROC_PID        "Pid"
#define SVC_API_PROC_CREATED    "Created"
#define SVC_API_PROC_PARENT     "ParentPid"
#define SVC_API_PROC_NAME       "Name"
#define SVC_API_PROC_PATH       "Path"
#define SVC_API_PROC_HASH       "Hash"
#define SVC_API_PROC_EID        "Eid"
#define SVC_API_PROC_SVCS       "Services"
#define SVC_API_PROC_APP_SID    "AppSid"
#define SVC_API_PROC_APP_NAME   "AppName"
//#define SVC_API_PROC_PACK_NAME  "PackName"
#define SVC_API_PROC_SOCKETS    "Sockets"

// Program
#define SVC_API_PROG_UID        "UID"
#define SVC_API_PROG_ID         "ID"
#define SVC_API_PROG_NAME       "Name"
#define SVC_API_PROG_ICON       "Icon"
#define SVC_API_PROG_INFO       "Info"
#define SVC_API_PROG_FILE       "File"
#define SVC_API_PROG_REG_KEY    "RegKey"
#define SVC_API_PROG_SVC_TAG    "SvcTag"
#define SVC_API_PROG_PROC_PID   "Pid"
#define SVC_API_PROG_APP_SID    "AppSID"
#define SVC_API_PROG_APP_NAME   "AppName"
#define SVC_API_PROG_APP_PATH   "AppPath"
#define SVC_API_PROG_ITEMS      "Items"
#define SVC_API_PROG_PROC_PIDS  "Pids"
#define SVC_API_PROG_PATTERN    "Pattern"
#define SVC_API_PROG_FW_RULES   "FwRules"
#define SVC_API_PROG_SOCKETS    "Sockets"

// Socket
#define SVC_API_SOCK_REF        "Ref"
#define SVC_API_SOCK_TYPE       "Type"
#define SVC_API_SOCK_LADDR      "LAddr"
#define SVC_API_SOCK_LPORT      "LPort"
#define SVC_API_SOCK_RADDR      "RAddr"
#define SVC_API_SOCK_RPORT      "RPort"
#define SVC_API_SOCK_STATE      "State"
#define SVC_API_SOCK_LSCOPE     "LScope"
#define SVC_API_SOCK_RSCOPE     "RScope"
#define SVC_API_SOCK_ACCESS     "Access"
#define SVC_API_SOCK_PID        "Pid"
#define SVC_API_SOCK_SVC_TAG    "SvcTag"
#define SVC_API_SOCK_RHOST      "RHost"
#define SVC_API_SOCK_CREATED    "Created"
#define SVC_API_SOCK_UPLOAD     "Upload"
#define SVC_API_SOCK_DOWNLOAD   "Download"
#define SVC_API_SOCK_UPLOADED   "Uploaded"
#define SVC_API_SOCK_DOWNLOADED "Downloaded"
#define SVC_API_SOCK_LAST_ACT   "LastActivity"

// Log
#define SVC_API_LOG_TYPE        "LogType"
#define SVC_API_EXEC_LOG        "Exec"
#define SVC_API_NET_LOG         "Net"
#define SVC_API_FS_LOG          "FS"
#define SVC_API_REG_LOG         "Reg"

// Events
#define SVC_API_EVENT_TYPE      "Type"
#define SVC_API_EVENT_FW_EVENT  "FwEvent"
#define SVC_API_EVENT_DATA      "Data"
#define SVC_API_EVENT_INDEX     "Index"
#define SVC_API_EVENT_UID       "UID"
#define SVC_API_EVENT_TIMESTAMP "TimeStamp"

#define SVC_API_EVENT_STATE     "State"
#define SVC_API_EVENT_FROM_LOG  "FromLog"
#define SVC_API_EVENT_UNRULED   "UnRuled"
#define SVC_API_EVENT_ALLOWED   "Allowed"
#define SVC_API_EVENT_BLOCKED   "Blocked"
#define SVC_API_EVENT_FAILED    "Failed"
#define SVC_API_EVENT_INVALID   "Invalid"

#define SVC_API_EVENT_REALM     "Realm"

#define SVC_API_TWEAKS          "Tweaks"

//#define SVC_API_TWEAK_REF       "Ref"
#define SVC_API_TWEAK_NAME      "Name"
#define SVC_API_TWEAK_TYPE      "Type"
#define SVC_API_TWEAK_TYPE_GROUP "Group"
#define SVC_API_TWEAK_TYPE_SET  "Set"
#define SVC_API_TWEAK_TYPE_REG  "Reg"
#define SVC_API_TWEAK_TYPE_GPO  "Gpo"
#define SVC_API_TWEAK_TYPE_SVC  "Svc"
#define SVC_API_TWEAK_TYPE_TASK "Task"
#define SVC_API_TWEAK_TYPE_FS   "FS"
#define SVC_API_TWEAK_TYPE_EXEC "Exec"
#define SVC_API_TWEAK_TYPE_FW   "Fw"
#define SVC_API_TWEAK_HINT      "Hint"
#define SVC_API_TWEAK_HINT_NONE "None"
#define SVC_API_TWEAK_HINT_RECOMMENDED "Recommended"
#define SVC_API_TWEAK_LIST      "List"
#define SVC_API_TWEAK_SET       "Set"
#define SVC_API_TWEAK_STATUS    "Status"
#define SVC_API_TWEAK_NOT_SET   "NotSet"
#define SVC_API_TWEAK_APPLIED   "Applied"
#define SVC_API_TWEAK_MISSING   "Missing"
#define SVC_API_TWEAK_KEY       "Key"
#define SVC_API_TWEAK_VALUE     "Value"
#define SVC_API_TWEAK_DATA      "Data"
#define SVC_API_TWEAK_SVC_TAG   "Tag"
#define SVC_API_TWEAK_FOLDER    "Folder"
#define SVC_API_TWEAK_ENTRY     "Entry"
#define SVC_API_TWEAK_PATH      "Path"
#define SVC_API_TWEAK_PROG_ID   "ProgID"



#define SVC_EVENT_DRIVER_FAILED 0x0101
#define SVC_EVENT_BLOCK_PROC    0x0102
#define SVC_EVENT_KILL_FAILED   0x0103