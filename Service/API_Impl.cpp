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
#include "pch.h"

#include "../../Library/API/PrivacyAPI.h"
#include "Access/AccessRule.h"
#include "Programs/ProgramID.h"
#include "Programs/ProgramRule.h"
#include "Network/Firewall/FirewallRule.h"
#include "Network/Dns/DnsFilter.h"
#include "Tweaks/Tweak.h"
#include "Programs/ProgramLibrary.h"
#include "Programs/ProgramItem.h"
#include "Programs/WindowsService.h"
#include "Programs/ProgramPattern.h"
#include "Programs/AppInstallation.h"
#include "Programs/ImageSignInfo.h"
#include "Enclaves/Enclave.h"
#include "Common/EventLog.h"
#include "HashDB/HashDB.h"

#define XVariant StVariant
#define VariantWriter StVariantWriter
#define VariantReader StVariantReader
#define TO_STR(x) (x)
#define AS_STR(x) (x).AsStr()
#define TO_STR_A(x) (x)
#define AS_STR_A(s, x) (s = (x))
#define TO_STR_W(x) (x)
#define AS_STR_W(s, x) (s = (x))
#define AS_ASTR(x) (x).To<std::string>()
#define TO_BYTES(x) (x).AsBytes()
#define AS_LIST(x) (x).AsList<std::wstring>()
#define AS_ALIST(x) (x).AsList<std::string>()
#define IS_EMPTY(x) (x).empty()
#define GET_BYTES(x) (StVariant(x))
#define GET_PATH TO_STR
#define SET_PATH(x, y) (x) = (y)
#define ASTR std::string
#define WSTR std::wstring
#define ASTR_VECTOR std::vector<std::string>
#define LIST_CLEAR(l) l.clear()
#define LIST_APPEND(l, e) l.push_back(e)
#define CFwRule CFirewallRule

#include "../../Library/API/API_GenericRule.cpp"

#include "../../Library/API/API_ProgramRule.cpp"

#include "../../Library/API/API_AccessRule.cpp"

#include "../../Library/API/API_Enclave.cpp"

#include "../../Library/API/API_HashDB.cpp"

#include "../../Library/API/API_DnsRule.cpp"

#include "../../Library/API/API_EventLog.cpp"

#include "Network/Firewall/WindowsFirewall.h"
#include "../Library/Common/Strings.h"
#include "../../Library/API/API_FwRule.cpp"

#include "../../Library/API/API_ProgramID.cpp"

#define PROG_SVC
#include "../../Library/API/API_Programs.cpp"

#define TWEAK_SVC
#include "../../Library/API/API_Tweak.cpp"