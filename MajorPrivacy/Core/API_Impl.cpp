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
#include "pch.h"

#include "../../Library/API/PrivacyAPI.h"
#include "Access/AccessRule.h"
#include "Programs/ProgramID.h"
#include "Programs/ProgramRule.h"
#include "Network/FwRule.h"
#include "Tweaks/Tweak.h"
#include "Programs/ProgramLibrary.h"
#include "Programs/ProgramItem.h"
#include "Programs/WindowsService.h"
#include "Programs/AppPackage.h"
#include "Programs/ProgramPattern.h"
#include "Programs/AppInstallation.h"

#define TO_STR(x) (x).toStdWString()
#define AS_STR(x) (x).AsQStr()
#define AS_LIST(x) (x).AsQList()
#define AS_ALIST(x) (x).AsQList()
#define IS_EMPTY(x) (x).isEmpty()
#define GET_PATH(x) (x).Get(EPathType::eNative).toStdWString()
#define SET_PATH(x, y) (x).Set(y.AsQStr(), EPathType::eNative)
#define ASTR std::string

#define LOAD_NT_PATHS

#include "../../Library/API/API_GenericRule.cpp"

#include "../../Library/API/API_ProgramRule.cpp"

#include "../../Library/API/API_AccessRule.cpp"

#include "../../Library/API/API_FwRule.cpp"

#include "../../Library/API/API_ProgramID.cpp"

#define PROG_GUI
#include "../../Library/API/API_Programs.cpp"

#define TWEAKS_RO
#include "../../Library/API/API_Tweak.cpp"