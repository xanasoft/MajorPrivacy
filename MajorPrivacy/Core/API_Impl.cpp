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
#include "Enclaves/Enclave.h"
#include "HashDB/HashEntry.h"
#include "Access/AccessRule.h"
#include "Programs/ProgramID.h"
#include "Programs/ProgramRule.h"
#include "Network/FwRule.h"
#include "Network/DnsRule.h"
#include "Tweaks/Tweak.h"
#include "Programs/ProgramLibrary.h"
#include "Programs/ProgramItem.h"
#include "Programs/WindowsService.h"
#include "Programs/AppPackage.h"
#include "Programs/ProgramPattern.h"
#include "Programs/AppInstallation.h"
#include "EventLog.h"
#include "Presets/Preset.h"

#define XVariant QtVariant
#define VariantWriter QtVariantWriter
#define VariantReader QtVariantReader
#define TO_STR(x) (x)
#define AS_STR(x) (x).AsQStr()
#define AS_ASTR AS_STR
#define TO_STR_A(x) (x)
#define AS_STR_A(s, x) s = (x).AsQStr()
#define TO_STR_W(x) (x)
#define AS_STR_W(s, x) s = (x).AsQStr()
#define TO_BYTES(x) (x).AsQBytes()
#define AS_LIST(x) (x).AsQList()
#define AS_ALIST(x) (x).AsQList()
#define IS_EMPTY(x) (x).isEmpty()
#define GET_BYTES(x) (QtVariant(x))
#define GET_PATH(x) (x)
#define SET_PATH(x, y) x = y.AsQStr()
#define ASTR QString // todo switch to QByteArray or soemthing
#define WSTR QString
#define ASTR_VECTOR QStringList
#define LIST_CLEAR(l) l.clear()
#define LIST_APPEND(l, e) l.append(e)

#define LOAD_NT_PATHS

#include "../../Library/API/API_Enclave.cpp"

#define HASHDB_GUI
#include "../../Library/API/API_HashDB.cpp"

#include "../../Library/API/API_GenericRule.cpp"

#include "../../Library/API/API_ProgramRule.cpp"

#include "../../Library/API/API_AccessRule.cpp"

#include "../../Library/API/API_FwRule.cpp"

#include "../../Library/API/API_DnsRule.cpp"

#define LOG_GUI
#include "../../Library/API/API_EventLog.cpp"

#include "../../Library/API/API_ProgramID.cpp"

#define PROG_GUI
#include "../../Library/API/API_Programs.cpp"

#define TWEAK_GUI
#include "../../Library/API/API_Tweak.cpp"

#include "../../Library/API/API_Preset.cpp"