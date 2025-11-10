#include "pch.h"
#include <cstdarg>
#include "JSEngine.h"
#include "../../quickjspp/quickjspp.hpp"
#include "../../quickjspp/quickjs/quickjs-libc.h"
#include "../ServiceCore.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Processes/ProcessList.h"
#include "../Programs/ProgramLibrary.h"
#include "../Programs/ProgramManager.h"
#include "../Programs/ProgramRule.h"
#include "../Network/NetworkManager.h"
#include "../Network/Firewall/Firewall.h"
#include "../Network/Firewall/FirewallRule.h"
#include "../Network/DNS/DnsFilter.h"
#include "../Network/DNS/DnsRule.h"
#include "../Access/AccessManager.h"
#include "../Access/AccessRule.h"
#include "../Tweaks/TweakManager.h"
#include "../Tweaks/Tweak.h"
#include "../../Library/Helpers/NtUtil.h"
#include "../../Library/API/DriverAPI.h"
#include <regex>

struct SJSEngine
{
	SJSEngine() : context(runtime)
	{
	}

    StVariant JS2Var(const qjs::Value& Value)
    {
        auto C = Value.ctx;

        // 1) null/undefined -> empty variant
        if (Value.v.tag == JS_TAG_NULL
            || Value.v.tag == JS_TAG_UNDEFINED
            || Value.v.tag == JS_TAG_UNINITIALIZED)
        {
            return StVariant();
        }

        // 2) primitives
        switch (Value.v.tag) {
        case JS_TAG_INT:
            return StVariant(Value.as<int>());
        case JS_TAG_BOOL:
            return StVariant(Value.as<bool>());
        case JS_TAG_FLOAT64:
            return StVariant(Value.as<double>());
        case JS_TAG_STRING:
            return StVariant(Value.as<std::string>().c_str());
        default:
            break;
        }

        // 3) arrays
        if (JS_IsArray(C, Value.v)) {
            qjs::Value arr{C, JS_DupValue(C, Value.v)};
            uint32_t len = static_cast<uint32_t>(arr["length"].as<int>());  
            // create a list-variant
            CVariant listVar(nullptr, VAR_TYPE_LIST);
            for (uint32_t i = 0; i < len; ++i) {
                qjs::Value elem{C, JS_GetPropertyUint32(C, Value.v, i)};
                StVariant child = JS2Var(elem);
                listVar.Append(CVariant(child));
            }
            return StVariant(listVar);
        }

        // 4) plain objects -> map
        if (Value.v.tag == JS_TAG_OBJECT) {
            // 4a) get own property names
            JSPropertyEnum *ptab;
            uint32_t plen;
            int res = JS_GetOwnPropertyNames(C, &ptab, &plen, Value.v, 0);
            if (res < 0)
                return StVariant();  // on error, return empty

            // 4b) build a map-variant
            CVariant mapVar(nullptr, VAR_TYPE_MAP);
            for (uint32_t i = 0; i < plen; ++i) {
                // convert atom ? C-string
                const char *name = JS_AtomToCString(C, ptab[i].atom);
                if (!name) {
                    // skip this property on OOM
                    continue;
                }

                // read the property value
                JSValue propVal = JS_GetProperty(C, Value.v, ptab[i].atom);

                qjs::Value retVal(std::move(propVal));
                retVal.ctx = context.ctx;

                StVariant child = JS2Var(retVal);

                // insert into our map
                mapVar.Insert(name, CVariant(child));

                // cleanup
                JS_FreeCString(C, name);
            }

            // 4c) free the enumeration table
            js_free_rt(JS_GetRuntime(C), ptab);

            return StVariant(mapVar);
        }

        // 5) everything else
        return StVariant();
    }

    qjs::Value Var2JS(const StVariant& Variant)
    {
        auto C = context.ctx;

        switch (Variant.GetType()) {
            // strings
        case VAR_TYPE_ASCII:
        case VAR_TYPE_UTF8:
        case VAR_TYPE_UNICODE: {
            auto s = Variant.ToStringA();
            return qjs::Value{C, JS_NewString(C, s.IsEmpty() ? "" : s.ConstData())};
        }

            // unsigned int
        case VAR_TYPE_UINT: {
            uint64_t u = Variant.To<uint64_t>();
            return qjs::Value{C, JS_NewInt64(C, static_cast<int64_t>(u))};
        }

            // signed int
        case VAR_TYPE_SINT: {
            int64_t i = Variant.To<int64_t>();
            return qjs::Value{C, JS_NewInt64(C, i)};
        }

            // double
        case VAR_TYPE_DOUBLE: {
            double d = Variant.To<double>();
            return qjs::Value{C, JS_NewFloat64(C, d)};
        }

            // lists
        case VAR_TYPE_LIST: {
            CVariant const& list = static_cast<CVariant const&>(Variant);
            uint32_t cnt = list.Count();
            qjs::Value arr{C, JS_NewArray(C)};
            for (uint32_t i = 0; i < cnt; ++i) {
                CVariant child = list.Get(i);
                qjs::Value v = Var2JS(child);
                JS_SetPropertyUint32(C, arr.v, i, v.release());
            }
            return arr;
        }

            // maps
        case VAR_TYPE_MAP: {
            CVariant const& map = static_cast<CVariant const&>(Variant);
            uint32_t cnt = map.Count();
            qjs::Value obj{C, JS_NewObject(C)};
            for (uint32_t i = 0; i < cnt; ++i) {
                const char* key = map.Key(i);
                CVariant child = map.Get(key);
                qjs::Value v = Var2JS(child);
                JS_SetPropertyStr(C, obj.v, key, v.release());
            }
            return obj;
        }

            // fallback ? null
        default:
            return qjs::Value{C, JS_NULL};
        }
    }

	qjs::Runtime runtime;
	qjs::Context context;

    qjs::Value MakeString(const std::wstring& str) 
    { 
        auto utf8 = ToUtf8(nullptr, str.c_str());
        qjs::Value jsStr{ context.ctx, JS_NewString(context.ctx, utf8.IsEmpty() ? "" : utf8.ConstData()) };
        return jsStr;
	}

	// Process List
    qjs::Value MakeProcessList();
    qjs::Value MakeLibrary(const CProgramLibraryPtr& pLibrary);
    qjs::Value MakeProcess(const CProcessPtr& pProcess);

    // Firewall
    qjs::Value MakeFirewall();
    qjs::Value MakeFirewallRule(const CFirewallRulePtr& pRule);

    // DNS Filter
    qjs::Value MakeDnsFilter();
    qjs::Value MakeDnsRule(const FW::SharedPtr<CDnsRule>& pRule);

    // Program Manager
    qjs::Value MakeProgramManager();
    qjs::Value MakeProgramRule(const CProgramRulePtr& pRule);

    // Access Manager
    qjs::Value MakeAccessManager();
    qjs::Value MakeAccessRule(const CAccessRulePtr& pRule);

    // Tweak Manager
    qjs::Value MakeTweakManager();
    qjs::Value MakeTweak(const CTweakPtr& pTweak);
};

//
// MajorPrivacy JavaScript API Documentation
//
// =============================================================================
// GLOBAL OBJECTS
// =============================================================================
//
// processList             - Process management
//    Methods:
//      getProcess(pid)         - Get process object by PID
//      getAll()                - Get array of all process objects
//      getList()               - Get array of all PIDs
//      findByName(pattern)     - Find processes by name pattern (wildcards: * and ?)
//
// firewall                - Firewall rule management
//    Methods:
//      getAllRules()           - Get all firewall rules
//      getRule(guid)           - Get rule by GUID string
//      setRule(ruleData)       - Create or update a firewall rule
//      delRule(guid)           - Delete a firewall rule by GUID
//      setRuleEnabled(guid, enabled) - Enable/disable a firewall rule
//
// dnsFilter               - DNS filtering
//    Methods:
//      getAllEntries()         - Get all DNS filter entries
//      getEntry(id)            - Get DNS entry by ID
//      setEntry(entryData)     - Create or update DNS filter entry
//      delEntry(id)            - Delete DNS filter entry by ID
//
// programManager          - Program rule management
//    Methods:
//      getAllRules()           - Get all program rules
//      getRule(guid)           - Get rule by GUID string
//      addRule(ruleData)       - Add a new program rule
//      removeRule(guid)        - Remove a program rule by GUID
//
// accessManager           - File/registry access rule management
//    Methods:
//      getAllRules()           - Get all access rules
//      getRule(guid)           - Get rule by GUID string
//      addRule(ruleData)       - Add a new access rule
//      removeRule(guid)        - Remove an access rule by GUID
//
// tweakManager            - System tweaks management
//    Methods:
//      getAllTweaks()          - Get all tweaks
//      getTweak(id)            - Get tweak by ID
//      applyTweak(id)          - Apply a tweak
//      undoTweak(id)           - Undo a tweak
//      approveTweak(id)        - Approve a diverged tweak
//
// =============================================================================
// OBJECTS
// =============================================================================
//
// process                 - Process object
//    Properties:
//      pid                     - Process ID
//      parentPid               - Parent process ID
//      creatorPid              - Creator process ID
//      name                    - Process name
//      ntPath                  - NT file path
//      dosPath                 - DOS file path
//      workDir                 - Working directory
//      enclaveGuid             - Enclave GUID string
//      appContainerSid         - App container SID
//      appContainerName        - App container name
//      appPackageFullName      - App package full name
//      services                - Array of service names
//      signerInfo              - Signature information object
//      upload                  - Current upload rate
//      download                - Current download rate
//      uploaded                - Total uploaded bytes
//      downloaded              - Total downloaded bytes
//      handleCount             - Number of open handles
//      socketCount             - Number of open sockets
//    Methods:
//      getLibraries()          - Get loaded libraries array
//      terminate()             - Terminate the process
//
// library                 - Loaded library object
//    Properties:
//      ntPath                  - NT file path
//      dosPath                 - DOS file path
//      signerInfo              - Signature information object
//
// =============================================================================
// ENUMS
// =============================================================================
//
// Spawn                   - Process spawn action
//      Undefined, Allow, Block, Eject
//
// Load                    - Image load action
//      Undefined, Allow, Block, AllowUntrusted
//
// Access                  - Resource access action
//      Undefined, Allow, AllowRO, Enum, Protect, Block, Ignore
//
// =============================================================================
// CALLBACKS
// =============================================================================
//
// onStart(event)          - Called when a process is about to start
//    event.ntPath            - NT path of the executable
//    event.dosPath           - DOS path of the executable
//    event.commandLine       - Command line arguments
//    event.actorPid          - PID of the creating process
//    event.actorServiceTag   - Service tag of the creator
//    event.enclaveId         - Enclave ID string
//    event.trusted           - Whether the process is trusted
//    event.ci                - Code integrity information
//    Returns: Spawn action (Spawn.Allow, Spawn.Block, or Spawn.Eject)
//
// onLoad(event)           - Called when a DLL is about to be loaded
//    event.pid               - PID of the process loading the DLL
//    event.ntPath            - NT path of the DLL
//    event.dosPath           - DOS path of the DLL
//    event.enclaveId         - Enclave ID string
//    event.trusted           - Whether the DLL is trusted
//    event.ci                - Code integrity information
//    Returns: Load action (Load.Allow, Load.Block, or Load.AllowUntrusted)
//
// onAccess(event)         - Called when resource access is requested
//    event.ntPath            - NT path of the resource
//    event.dosPath           - DOS path of the resource
//    event.actorPid          - PID of the accessing process
//    event.actorServiceTag   - Service tag of the accessor
//    event.enclaveId         - Enclave ID string
//    event.accessMask        - Requested access mask
//    Returns: Access action (Access.Allow, Access.Block, etc.)
//
// onMount(event)          - Called when volume is about to be mounted
//    event.imagePath         - Image file path
//    event.devicePath        - Device path
//    event.mountPoint        - Mount point path
//    Returns: true to allow, false to block
//
// onDismount(event)       - Called when volume is dismounted
//    event.imagePath         - Image file path
//    event.devicePath        - Device path
//    event.mountPoint        - Mount point path
//
// onActivation(event)   - Called when a preset is being enabled/activated
//    event.callerPid         - PID of the process requesting activation
//
// onDeactivation(event)  - Called when a preset is being disabled/deactivated
//    event.callerPid         - PID of the process requesting deactivation
//
// =============================================================================
// LOGGING FUNCTIONS
// =============================================================================
//
// log(message)            - Log message (default level)
// logInfo(message)        - Log informational message
// logSuccess(message)     - Log success message
// logWarning(message)     - Log warning message
// logError(message)       - Log error message
//
// slogInfo(message)       - Log to system event log (information)
// slogSuccess(message)    - Log to system event log (success)
// slogWarning(message)    - Log to system event log (warning)
// slogError(message)      - Log to system event log (error)
//
// emitInfo(message)       - Emit event (information)
// emitSuccess(message)    - Emit event (success)
// emitWarning(message)    - Emit event (warning)
// emitError(message)      - Emit event (error)
//
// =============================================================================
// PROCESS EXECUTION (Only in onPresetEnable/onPresetDisable)
// =============================================================================
//
// shellExec(cmdLine, options) - Execute a program in the caller's session
//    IMPORTANT: Only available in onPresetEnable and onPresetDisable callbacks!
//               Will fail with error in all other contexts (onStart, onLoad, etc.)
//
//    Parameters:
//      cmdLine (string)      - Command line to execute
//      options (object)      - Optional configuration:
//        workDir (string)       - Working directory
//        hidden (bool)          - Run without window (default: false)
//        dropAdmin (bool)       - Drop admin privileges (default: false)
//        lowPrivilege (bool)    - Run with restricted token (default: false)
//    Returns: { success: bool, pid: number, error: string }
//
//    Security Notes:
//      - Runs in the caller's user session (not as SYSTEM)
//      - Automatically drops admin rights if caller is not admin
//      - Use dropAdmin to force drop admin even if available
//      - Use lowPrivilege for untrusted executables (sandboxed)
//
//    Examples:
//      function onPresetEnable(event) {
//        // Simple execution
//        let result = shellExec("notepad.exe");
//        if (result.success) logInfo("Started PID: " + result.pid);
//
//        // Run hidden without admin rights
//        shellExec("cmd.exe /c dir", { hidden: true, dropAdmin: true });
//
//        // Run with low privileges (sandboxed)
//        shellExec("untrusted.exe", { lowPrivilege: true, workDir: "C:\\Temp" });
//
//        return true;
//      }
//
//

/*struct STest
{
    STest() {}
    ~STest() {}

    int test = 0;
};*/

//static JSContext *JS_NewCustomContext(JSRuntime *rt)
//{
//    JSContext *ctx;
//    ctx = JS_NewContext(rt);
//    if (!ctx)
//        return NULL;
//#ifdef CONFIG_BIGNUM
//    if (bignum_ext) {
//        JS_AddIntrinsicBigFloat(ctx);
//        JS_AddIntrinsicBigDecimal(ctx);
//        JS_AddIntrinsicOperators(ctx);
//        JS_EnableBignumExt(ctx, true);
//    }
//#endif
//    js_init_module_std(ctx, "std");
//    js_init_module_os(ctx, "os");
//    return ctx;
//}

CJSEngine::CJSEngine(const std::string& Script, const CFlexGuid& Guid, EItemType Type)
{
    m_Script = Script;
    m_Guid = Guid;
    m_Type = Type;

	m = new SJSEngine();

#ifdef _DEBUG
    //js_std_set_worker_new_context_func(JS_NewCustomContext);
    js_std_init_handlers(m->runtime.rt);

    //JS_SetModuleLoaderFunc(m->runtime.rt, NULL, js_module_loader, NULL);

    js_std_add_helpers(m->context.ctx, 0, NULL);

    js_init_module_std(m->context.ctx, "std");
    js_init_module_os(m->context.ctx, "os");

    m->context.eval(R"xxx(
        import * as std from 'std';
        import * as os from 'os';
        globalThis.std = std;
        globalThis.os = os;
    )xxx", "<input>", JS_EVAL_TYPE_MODULE);
#endif

    //auto ctx = context.ctx;
    // 
    //std::shared_ptr<STest> pTest = std::make_shared<STest>();
    //
    //obj["test"] = [ctx, pProcess, pTest]()-> qjs::Value {
    //
    //std::wstring test = pProcess->GetNtFilePath();
    //test.empty();
    //
    //return qjs::Value {ctx, JS_NULL};
    //};

    //js_std_init_handlers(m->runtime.rt);
    //
    //JS_SetModuleLoaderFunc(m->runtime.rt, nullptr, js_module_loader, nullptr);
    //js_std_add_helpers(m->context.ctx, 0, nullptr);
    //
    //js_init_module_std(m->context.ctx, "std");
    //js_init_module_os(m->context.ctx, "os");
    //
    //m->context.eval(R"xxx(
    //    import * as std from 'std';
    //    import * as os from 'os';
    //    globalThis.std = std;
    //    globalThis.os = os;
    //)xxx", "<input>", JS_EVAL_TYPE_MODULE);
    
    //m->context.global()["println"] = [](const std::string& s) 
    //{ 
    //    std::cout << s << std::endl; 
    //};



    // Process List API
    m->context.global()["processList"] = m->MakeProcessList();

    // Firewall API
    m->context.global()["firewall"] = m->MakeFirewall();

    // DNS Filter API
    m->context.global()["dnsFilter"] = m->MakeDnsFilter();

    // Program Manager API
    m->context.global()["programManager"] = m->MakeProgramManager();

    // Access Manager API
    m->context.global()["accessManager"] = m->MakeAccessManager();

    // Tweak Manager API
    m->context.global()["tweakManager"] = m->MakeTweakManager();

    // Enums
    qjs::Value eSpawn = m->context.newObject();
    eSpawn["Undefined"] = EProgramOnSpawn::eUnknown;
    eSpawn["Allow"]     = EProgramOnSpawn::eAllow;
    eSpawn["Block"]     = EProgramOnSpawn::eBlock;
    eSpawn["Eject"]     = EProgramOnSpawn::eEject;
    m->context.global()["Spawn"] = eSpawn;

    qjs::Value eLoad = m->context.newObject();
    eLoad["Undefined"] = EImageOnLoad::eUnknown;
    eLoad["Allow"]     = EImageOnLoad::eAllow;
    eLoad["Block"]     = EImageOnLoad::eBlock;
    eLoad["AllowUntrusted"] = EImageOnLoad::eAllowUntrusted;
    m->context.global()["Load"] = eLoad;

    qjs::Value eAccess = m->context.newObject();
    eAccess["Undefined"]= EAccessRuleType::eNone;
    eAccess["Allow"]    = EAccessRuleType::eAllow;
    eAccess["AllowRO"]  = EAccessRuleType::eAllowRO;
    eAccess["Enum"]     = EAccessRuleType::eEnum;
    eAccess["Protect"]  = EAccessRuleType::eProtect;
    eAccess["Block"]    = EAccessRuleType::eBlock;
    eAccess["Ignore"]   = EAccessRuleType::eIgnore;
    m->context.global()["Access"] = eAccess;

    m->context.global()["log"] = [&](const std::string& s)          { Log(s); };
    m->context.global()["logInfo"] = [&](const std::string& s)      { Log(s, ELogLevels::eInfo); };
    m->context.global()["logSuccess"] = [&](const std::string& s)   { Log(s, ELogLevels::eSuccess); };
    m->context.global()["logWarning"] = [&](const std::string& s)   { Log(s, ELogLevels::eWarning); };
    m->context.global()["logError"] = [&](const std::string& s)     { Log(s, ELogLevels::eError); };

    m->context.global()["slogInfo"] = [&](const std::string& s)     { Log(s, ELogLevels::eInfo);    theCore->Log()->LogEvent(EVENTLOG_INFORMATION_TYPE, 0, SVC_EVENT_JSLOG_MSG, s2w(s)); };
    m->context.global()["slogSuccess"] = [&](const std::string& s)  { Log(s, ELogLevels::eSuccess); theCore->Log()->LogEvent(EVENTLOG_SUCCESS, 0, SVC_EVENT_JSLOG_MSG, s2w(s)); };
    m->context.global()["slogWarning"] = [&](const std::string& s)  { Log(s, ELogLevels::eWarning); theCore->Log()->LogEvent(EVENTLOG_WARNING_TYPE, 0, SVC_EVENT_JSLOG_MSG, s2w(s)); };
    m->context.global()["slogError"] = [&](const std::string& s)    { Log(s, ELogLevels::eError);   theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_JSLOG_MSG, s2w(s)); };

    m->context.global()["emitInfo"] = [&](const std::string& s)     { Log(s, ELogLevels::eInfo);    EmitEvent(s, ELogLevels::eInfo); };
    m->context.global()["emitSuccess"] = [&](const std::string& s)  { Log(s, ELogLevels::eSuccess); EmitEvent(s, ELogLevels::eSuccess); };
    m->context.global()["emitWarning"] = [&](const std::string& s)  { Log(s, ELogLevels::eWarning); EmitEvent(s, ELogLevels::eWarning); };
    m->context.global()["emitError"] = [&](const std::string& s)    { Log(s, ELogLevels::eError);   EmitEvent(s, ELogLevels::eError); };

    //m->context.global()["dump"] = [&](const qjs::Value& v)          { Dump(v); };

    // shellExec - Execute a program with various options
    // NOTE: Only available in activation/deactivation scripts (onPresetEnable/onPresetDisable)
    // Parameters:
    //   cmdLine (string) - Command line to execute
    //   options (object, optional) - Options object with:
    //     - workDir (string): Working directory
    //     - hidden (bool): Run hidden without window (default: false)
    //     - dropAdmin (bool): Drop admin privileges (default: false)
    //     - lowPrivilege (bool): Run with restricted low privilege token (default: false)
    // Returns: object with { success: bool, pid: number (if success), error: string (if failed) }
    m->context.global()["shellExec"] = [&](const std::string& cmdLineUtf8, const qjs::Value& optionsVal) -> qjs::Value {
        // Build result object
        qjs::Value result = m->context.newObject();

        // Check if caller PID is set (only available in activation/deactivation contexts)
        if (m_CallerPID == 0) {
            result["success"] = false;
            result["error"] = "shellExec is only available in callbacks with a CallerId";
            return result;
        }

        std::wstring cmdLine = FromUtf8(nullptr, cmdLineUtf8.c_str()).ConstData();
        std::wstring workDir;
        uint32 flags = CServiceCore::eExec_AsCallerUser;
        bool bAsync = false;
        bool bCapture = false;
        DWORD dwTimeout = INFINITE;

        // Parse options if provided
        if (optionsVal.v.tag == JS_TAG_OBJECT)
        {
            qjs::Value workDirVal{ m->context.ctx, JS_GetPropertyStr(m->context.ctx, optionsVal.v, "workDir") };
            if (workDirVal.v.tag == JS_TAG_STRING) {
                workDir = FromUtf8(nullptr, workDirVal.as<std::string>().c_str()).ConstData();
            }

            qjs::Value hiddenVal{ m->context.ctx, JS_GetPropertyStr(m->context.ctx, optionsVal.v, "hidden") };
            if (hiddenVal.v.tag == JS_TAG_BOOL && hiddenVal.as<bool>())
                flags |= CServiceCore::eExec_Hidden;

            qjs::Value dropAdminVal{ m->context.ctx, JS_GetPropertyStr(m->context.ctx, optionsVal.v, "dropAdmin") };
            if (dropAdminVal.v.tag == JS_TAG_BOOL && dropAdminVal.as<bool>())
                flags |= CServiceCore::eExec_DropAdmin;

            qjs::Value lowPrivVal{ m->context.ctx, JS_GetPropertyStr(m->context.ctx, optionsVal.v, "lowPrivilege") };
            if (lowPrivVal.v.tag == JS_TAG_BOOL && lowPrivVal.as<bool>())
                flags |= CServiceCore::eExec_LowPrivilege;

            //qjs::Value elevateVal{ m->context.ctx, JS_GetPropertyStr(m->context.ctx, optionsVal.v, "elevate") };
            //if (elevateVal.v.tag == JS_TAG_BOOL && elevateVal.as<bool>())
            //    flags |= CServiceCore::eExec_Elevate;

            //qjs::Value asSystemVal{ m->context.ctx, JS_GetPropertyStr(m->context.ctx, optionsVal.v, "asSystem") };
            //if (asSystemVal.v.tag == JS_TAG_BOOL && asSystemVal.as<bool>())
            //    flags |= CServiceCore::eExec_AsSystem;

            qjs::Value asyncVal{ m->context.ctx, JS_GetPropertyStr(m->context.ctx, optionsVal.v, "async") };
            if (asyncVal.v.tag == JS_TAG_BOOL && asyncVal.as<bool>())
                bAsync = true;

            qjs::Value captureVal{ m->context.ctx, JS_GetPropertyStr(m->context.ctx, optionsVal.v, "capture") };
            if (captureVal.v.tag == JS_TAG_BOOL && captureVal.as<bool>())
                bCapture = true;

            qjs::Value timeoutVal{ m->context.ctx, JS_GetPropertyStr(m->context.ctx, optionsVal.v, "timeout") };
            if (timeoutVal.v.tag == JS_TAG_INT) {
                int timeout = timeoutVal.as<int>();
                if (timeout > 0)
                    dwTimeout = (DWORD)timeout;
            }
        }

        // Capture requires synchronous mode
        if (bCapture && bAsync) {
            result["success"] = false;
            result["error"] = "Capture mode is not compatible with async mode";
            return result;
        }

        // Execute the process
        uint32 processId = 0;
        HANDLE hProcess = NULL;
        STATUS status = STATUS_SUCCESS;

        std::string stdoutData, stderrData;

        if (bCapture) {
            // Capture mode - create process with redirected stdout/stderr
            // We need to do manual process creation to set up pipes

            // For capture mode with complex token manipulation (asSystem, elevate, etc.),
            // we'll use a simpler approach: call CreateUserProcess but redirect output via cmd.exe wrapper
            // Or implement full inline creation

            // Simplified: Only support capture for basic execution (no asSystem/elevate in capture mode for now)
            if ((flags & (CServiceCore::eExec_AsSystem | CServiceCore::eExec_Elevate)) != 0) {
                result["success"] = false;
                result["error"] = "Capture mode does not support asSystem or elevate flags yet";
                return result;
            }

            SECURITY_ATTRIBUTES saAttr;
            saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
            saAttr.bInheritHandle = TRUE;
            saAttr.lpSecurityDescriptor = NULL;

            // Create pipes for stdout and stderr
            HANDLE hStdoutRead = NULL, hStdoutWrite = NULL;
            HANDLE hStderrRead = NULL, hStderrWrite = NULL;

            if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &saAttr, 0) ||
                !CreatePipe(&hStderrRead, &hStderrWrite, &saAttr, 0)) {
                result["success"] = false;
                result["error"] = "Failed to create pipes for output capture";
                if (hStdoutRead) CloseHandle(hStdoutRead);
                if (hStdoutWrite) CloseHandle(hStdoutWrite);
                if (hStderrRead) CloseHandle(hStderrRead);
                return result;
            }

            // Ensure the read handles are not inherited
            SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

            // Setup STARTUPINFO with redirected handles
            STARTUPINFOW si = { 0 };
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK;
            si.hStdOutput = hStdoutWrite;
            si.hStdError = hStderrWrite;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

            if (flags & CServiceCore::eExec_Hidden) {
                si.dwFlags |= STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_HIDE;
            }

            PROCESS_INFORMATION pi = { 0 };

            // Create process as caller user
            BOOL bSuccess = CreateProcessW(
                NULL,
                (wchar_t*)cmdLine.c_str(),
                NULL,
                NULL,
                TRUE,  // Inherit handles for pipes
                CREATE_UNICODE_ENVIRONMENT,
                NULL,
                workDir.empty() ? NULL : workDir.c_str(),
                &si,
                &pi);

            // Close write ends of pipes (child process has copies)
            CloseHandle(hStdoutWrite);
            CloseHandle(hStderrWrite);

            if (bSuccess) {
                processId = pi.dwProcessId;
                hProcess = pi.hProcess;
                CloseHandle(pi.hThread);

                // Wait for process to finish and read output
                DWORD waitResult = WaitForSingleObject(hProcess, dwTimeout);

                if (waitResult == WAIT_OBJECT_0 || waitResult == WAIT_TIMEOUT) {
                    // Read stdout
                    char buffer[4096];
                    DWORD bytesRead;
                    while (ReadFile(hStdoutRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                        buffer[bytesRead] = '\0';
                        stdoutData.append(buffer, bytesRead);
                    }

                    // Read stderr
                    while (ReadFile(hStderrRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
                        buffer[bytesRead] = '\0';
                        stderrData.append(buffer, bytesRead);
                    }

                    if (waitResult == WAIT_TIMEOUT) {
                        TerminateProcess(hProcess, 1);
                        result["timedOut"] = true;
                        result["exitCode"] = 1;
                    } else {
                        DWORD exitCode = 0;
                        if (GetExitCodeProcess(hProcess, &exitCode)) {
                            result["exitCode"] = (int)exitCode;
                        }
                    }

                    result["stdout"] = stdoutData;
                    result["stderr"] = stderrData;
                    result["pid"] = (int64_t)processId;
                    result["success"] = true;
                } else {
                    result["success"] = false;
                    result["error"] = "Failed to wait for process";
                }

                CloseHandle(hProcess);
                status = STATUS_SUCCESS;
            } else {
                status = ERR(GetLastWin32ErrorAsNtStatus());
            }

            CloseHandle(hStdoutRead);
            CloseHandle(hStderrRead);
        }
        else {
            // Non-capture mode - use CreateUserProcess
            status = theCore->CreateUserProcess(cmdLine, m_CallerPID, flags, workDir, &processId, bAsync ? nullptr : &hProcess);

            result["success"] = status.IsSuccess();

            if (status.IsSuccess()) {
                result["pid"] = (int64_t)processId;

                // If not async, wait for the process to finish
                if (!bAsync && hProcess) {
                    DWORD waitResult = WaitForSingleObject(hProcess, dwTimeout);

                    if (waitResult == WAIT_OBJECT_0) {
                        // Process finished normally - get exit code
                        DWORD exitCode = 0;
                        if (GetExitCodeProcess(hProcess, &exitCode)) {
                            result["exitCode"] = (int)exitCode;
                        }
                    }
                    else if (waitResult == WAIT_TIMEOUT) {
                        // Timeout - terminate the process
                        TerminateProcess(hProcess, 1);
                        result["timedOut"] = true;
                        result["exitCode"] = 1;
                    }
                    else {
                        // Wait failed
                        result["error"] = "Failed to wait for process";
                    }

                    CloseHandle(hProcess);
                }
            }
            else {
                std::wstring errorMsg = L"Failed to create process";
                if (status.GetStatus() != STATUS_UNSUCCESSFUL) {
                    wchar_t buf[256];
                    swprintf_s(buf, L" (0x%08X)", status.GetStatus());
                    errorMsg += buf;
                }
                result["error"] = ToUtf8(nullptr, errorMsg.c_str()).ConstData();
            }
        }

        return result;
    };

    ClearLog();
}

CJSEngine::~CJSEngine()
{
	delete m;
}

RESULT(StVariant) CJSEngine::RunScript()
{
    std::unique_lock Lock(m_Mutex);

    try {
        auto Val = m->context.eval(m_Script);

        if (Val.v.tag == JS_TAG_EXCEPTION) {
            HandleException();
            return STATUS_UNSUCCESSFUL;
        }

        return m->JS2Var(Val);
    }
    catch (qjs::exception & ex) {
        HandleException();
        return STATUS_UNSUCCESSFUL;
    }
}

void CJSEngine::HandleException()
{
    auto exc = m->context.getException();
    auto str = (std::string)exc;
    if((bool)exc["stack"])
        str += "\n" + (std::string)exc["stack"];

	Log(str, ELogLevels::eError);

    theCore->Log()->LogEvent(EVENTLOG_ERROR_TYPE, 0, SVC_EVENT_JSLOG_MSG, s2w(str));
}

RESULT(StVariant) CJSEngine::CallFunc(const char* FuncName, int arg_count, ...)
{
    try {
        qjs::Value fn = m->context.global()[FuncName];

        if (fn.v.tag == JS_TAG_UNDEFINED)
            return STATUS_NOT_FOUND;

        std::vector<JSValue> jsArgs;
        jsArgs.reserve(arg_count);
        std::list<qjs::Value> jsTemp;

        va_list ap;
        va_start(ap, arg_count);
        for (int i = 0; i < arg_count; ++i) {
            StVariant var = va_arg(ap, StVariant);
            jsTemp.push_back(m->Var2JS(var));
            //auto test = jsTemp.back().toJSON();
            jsArgs.push_back(jsTemp.back().v);
        }
        va_end(ap);

        JSValue rawRet = JS_Call(m->context.ctx, fn.v, JS_UNDEFINED, arg_count, jsArgs.data());
        //auto test = rawRet.back().toJSON();

        qjs::Value retVal(std::move(rawRet));
        retVal.ctx = m->context.ctx;

        if (retVal.v.tag == JS_TAG_EXCEPTION) {
            HandleException();
            return STATUS_UNSUCCESSFUL;
        }

        return m->JS2Var(retVal);
    }
    catch (qjs::exception& ex) {
        HandleException();
        return STATUS_UNSUCCESSFUL;
    }
}

RESULT(StVariant) CJSEngine::CallFunction(const char* FuncName, const CVariant& Param, uint32 CallerPID)
{
    std::unique_lock Lock(m_Mutex);

    m_CallerPID = CallerPID;

    RESULT(StVariant) Result = CallFunc(FuncName, 1, Param);

    m_CallerPID = 0;

	return Result;
}

void CJSEngine::EmitEvent(const std::string& Message, ELogLevels Level)
{
    StVariant Data;
	Data[API_V_GUID] = m_Guid.ToVariant(false);
    Data[API_V_TYPE] = (uint32)m_Type;
	Data[API_V_DATA] = Message;
    theCore->EmitEvent(Level, ELogEventType::eLogScriptEvent, Data);
}

//void CJSEngine::Dump(const qjs::Value& Value)
//{
//    
//}

void CJSEngine::Log(const std::string& Message, ELogLevels Level)
{
    std::unique_lock Lock(m_Mutex);

    while (m_Log.Count() >= 100) {
        m_BaseID++;
        m_Log.erase(m_Log.begin());
    }

    SLogEntry Entry;
    Entry.TimeStamp = GetCurrentTimeAsFileTime();
	Entry.Level = Level;
    Entry.Message = Message;
	m_Log.Append(Entry);
}

void CJSEngine::ClearLog()
{
    std::unique_lock Lock(m_Mutex);

    m_BaseID = rand();
    m_Log.Clear();
}

StVariant CJSEngine::DumpLog(uint32 LastID, FW::AbstractMemPool* pMemPool)
{
    std::unique_lock Lock(m_Mutex);

    int Index = 0;
    if(LastID > m_BaseID && LastID <= m_BaseID + m_Log.Count())
		Index = LastID - m_BaseID;
    
    StVariantWriter Log(pMemPool);
    Log.BeginList();
    for (; Index < m_Log.Count(); Index++)
    {
        StVariantWriter Entry(pMemPool);
        Entry.BeginIndex();
        Entry.Write(API_V_ID,  m_BaseID + Index + 1);
		Entry.Write(API_V_TYPE, (uint32)m_Log[Index].Level);
		Entry.Write(API_V_TIME_STAMP, m_Log[Index].TimeStamp);
		Entry.WriteEx(API_V_DATA, m_Log[Index].Message);
        Log.WriteVariant(Entry.Finish());
    }
	return Log.Finish();
}

FW::StringW CJSEngine__DumpHash(uint32 Algorithm, const std::vector<uint8>& Hash)
{
    FW::StringW Hex;
    ToHex(Hex, Hash.data(), Hash.size());

    switch (Algorithm)
    {
    case KphHashAlgorithmSha1:              return FW::StringW(nullptr, L"SHA1:") + Hex;
    case KphHashAlgorithmSha1Authenticode:  return FW::StringW(nullptr, L"SHA1ac:") + Hex;
    case KphHashAlgorithmSha256:            return FW::StringW(nullptr, L"SHA256:") + Hex;
    case KphHashAlgorithmSha256Authenticode:return FW::StringW(nullptr, L"SHA256ac:") + Hex;
    case KphHashAlgorithmSha384:            return FW::StringW(nullptr, L"SHA384:") + Hex;
    case KphHashAlgorithmSha512:            return FW::StringW(nullptr, L"SHA512:") + Hex;
    default: return Hex;
    }
}

StVariant CJSEngine__SumpSign(USignatures uSign)
{
    StVariant Sign;

    Sign["value"] = uSign.Value;
	Sign["windows"] = uSign.Windows != 0;
	Sign["microsoft"] = uSign.Microsoft != 0;
	Sign["antimalware"] = uSign.Antimalware != 0;
	Sign["authenticode"] = uSign.Authenticode != 0;
	Sign["ms_store"] = uSign.Store != 0;
	Sign["mp_dev_sign"] = uSign.Developer != 0;
    Sign["user_sign"] = uSign.UserSign != 0;
	Sign["user"] = uSign.UserDB != 0;
	Sign["enclave"] = uSign.EnclaveDB != 0;
    Sign["collection"] = uSign.Collection != 0;

	return Sign;
}

StVariant CJSEngine__FillVerifierInfo(const struct SVerifierInfo* pVerifyInfo)
{
    StVariant Verifier;

    if (!pVerifyInfo)
        return Verifier;

    StVariant Status(nullptr, VAR_TYPE_MAP);
    Status["signAuthority"] = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_SA) != 0;
    Status["codeIntegrity"] = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_CI) != 0;
    Status["signLevel"]     = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_SL) != 0;

    Status["imageIncoherent"] = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_INCOHERENT) != 0;

    Status["fileMismatch"]  = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_FILE_MISMATCH) != 0;
	Status["hashFailed"]    = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_HASH_FILED) != 0;
    Status["coherencyFail"] = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_COHERENCY_FAIL) != 0;
    Status["signatureFail"] = (pVerifyInfo->StatusFlags & MP_VERIFY_FLAG_SIGNATURE_FAIL) != 0;

    Verifier["statusFlags"] = Status;

	Verifier["signAuthority"] = (uint32)pVerifyInfo->PrivateAuthority;
	Verifier["signLevel"] = (uint32)pVerifyInfo->SignLevel;
	Verifier["codeIntegrity"] = (uint32)pVerifyInfo->SignPolicyBits;

    if(pVerifyInfo->FileHashAlgorithm)
	    Verifier["fileHash"] = CJSEngine__DumpHash(pVerifyInfo->FileHashAlgorithm, pVerifyInfo->FileHash);

    if (pVerifyInfo->SignerHashAlgorithm) {
        Verifier["signerHash"] = CJSEngine__DumpHash(pVerifyInfo->SignerHashAlgorithm, pVerifyInfo->SignerHash);
        Verifier["signerName"] = pVerifyInfo->SignerName;
    }

    if (pVerifyInfo->IssuerHashAlgorithm) {
        Verifier["issuerHash"] = CJSEngine__DumpHash(pVerifyInfo->IssuerHashAlgorithm, pVerifyInfo->IssuerHash);
        Verifier["issuerName"] = pVerifyInfo->IssuerName;
    }

	Verifier["foundSignatures"] = CJSEngine__SumpSign(pVerifyInfo->FoundSignatures);
	Verifier["allowedSignatures"] = CJSEngine__SumpSign(pVerifyInfo->AllowedSignatures);

    return Verifier;
}

qjs::Value SJSEngine::MakeLibrary(const CProgramLibraryPtr& pLibrary)
{
    qjs::Value obj = context.newObject();

    auto path = pLibrary->GetPath();

    obj["ntPath"] = MakeString(path.c_str());
    obj["dosPath"] = MakeString(theCore->NormalizePath(path));

    SVarWriteOpt Opts;
    obj["signerInfo"] = Var2JS(pLibrary->GetSignInfo().ToVariant(Opts));

    return obj;
};

qjs::Value SJSEngine::MakeProcess(const CProcessPtr& pProcess) 
{
    qjs::Value obj = context.newObject();

    auto path = pProcess->GetNtFilePath();

    obj["pid"] = (int64_t)pProcess->GetProcessId();
    obj["parentPid"] = (int64_t)pProcess->GetParentId();
    obj["creatorPid"] = (int64_t)pProcess->GetCreatorId();
    obj["name"] = MakeString(pProcess->GetName());
    obj["ntPath"] = MakeString(path);
    obj["dosPath"] = MakeString(theCore->NormalizePath(path));
    obj["workDir"] = MakeString(pProcess->GetWorkDir());

    obj["enclaveGuid"] = pProcess->GetEnclave().ToString();

    obj["appContainerSid"] = MakeString(pProcess->GetAppContainerSid());
    obj["appContainerName"] = MakeString(pProcess->GetAppContainerName());
    obj["appPackageFullName"] = MakeString(pProcess->GetAppPackageName());

    auto Services = pProcess->GetServices();
    if (!Services.empty())
    {
        qjs::Value arr{ context.ctx, JS_NewArray(context.ctx) };
        uint32_t idx = 0;
        for (auto& I : Services)
        {
            JS_SetPropertyUint32(context.ctx, arr.v, idx++, MakeString(I));
        }
        obj["services"] = arr;
    }

#if 0
    auto Libraries = pProcess->GetLibraries();
    if (!Libraries.empty())
    {
        qjs::Value arr{ context.ctx, JS_NewArray(context.ctx) };
        uint32_t idx = 0;
        for (auto& I : Libraries)
        {
            qjs::Value lib = MakeLibrary(I);
            JS_SetPropertyUint32(context.ctx, arr.v, idx++, lib.release());
        }
        obj["libraries"] = arr;
    }
#else
    auto getLibraries = [&]() -> qjs::Value {

        auto Libraries = pProcess->GetLibraries();

        qjs::Value arr = context.newObject();
        uint32_t idx = 0;
        for (auto& I : Libraries)
        {
            qjs::Value lib = MakeLibrary(I);
            JS_SetPropertyUint32(context.ctx, arr.v, idx++, lib.release());
        }

        return arr;
    };
    obj["getLibraries"] = getLibraries;
#endif

    SVarWriteOpt Opts;
    obj["signerInfo"] = Var2JS(pProcess->GetSignInfo().ToVariant(Opts));

    // Network stats
    obj["upload"] = (int64_t)pProcess->GetUpload();
    obj["download"] = (int64_t)pProcess->GetDownload();
    obj["uploaded"] = (int64_t)pProcess->GetUploaded();
    obj["downloaded"] = (int64_t)pProcess->GetDownloaded();

    // Process control methods
    auto terminateProcess = [pid = pProcess->GetProcessId()]() -> bool {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
        if (hProcess) {
            BOOL result = TerminateProcess(hProcess, 1);
            CloseHandle(hProcess);
            return result != FALSE;
        }
        return false;
    };
    obj["terminate"] = terminateProcess;

    // Handle and socket counts
    obj["handleCount"] = pProcess->GetHandleCount();
    obj["socketCount"] = pProcess->GetSocketCount();

    //auto meta = context.newObject();
    //meta["createdAt"] = std::time(nullptr);
    //obj["meta"] = meta;

    return obj;
};

qjs::Value SJSEngine::MakeProcessList()
{
    auto processList = context.newObject();

    auto getList = [&]() -> qjs::Value {

        qjs::Value arr{ context.ctx, JS_NewArray(context.ctx) };
        uint32_t idx = 0;

        for (auto& I : theCore->ProcessList()->List())
        {
            JS_SetPropertyUint32(context.ctx, arr.v, idx++, JS_NewInt64(context.ctx, (int64_t)I.second->GetProcessId()));
        }

        return arr;
    };
    processList["getList"] = getList;

    auto getAll = [&]() -> qjs::Value {

        qjs::Value arr{ context.ctx, JS_NewArray(context.ctx) };
        uint32_t idx = 0;

        for (auto& I : theCore->ProcessList()->List())
        {
            qjs::Value obj = MakeProcess(I.second);

            JS_SetPropertyUint32(context.ctx, arr.v, idx++, obj.release());
        }

        return arr;
    };
    processList["getAll"] = getAll;

    auto findByName = [&](const std::string& NameUtf8) -> qjs::Value {

        qjs::Value arr{ context.ctx, JS_NewArray(context.ctx) };

        // Build regex from DOS pattern (escape then translate * and ?)
        std::wstring pattern = FromUtf8(nullptr, NameUtf8.c_str()).ConstData();

        try {

            std::wstring rx = pattern;
            static const std::wregex specialChars(L"[.^$|()\\[\\]{}*+?\\\\]");
            rx = std::regex_replace(rx, specialChars, L"\\$&");      // escape regex meta
            static const std::wregex asterisk(L"\\\\\\*");
            rx = std::regex_replace(rx, asterisk, L".*");            // * -> .*
            static const std::wregex questionMark(L"\\\\\\?");
            rx = std::regex_replace(rx, questionMark, L".");         // ? -> .

            const std::wregex re(L"^" + rx + L"$", std::regex_constants::icase);

            uint32_t idx = 0;
            for (auto& it : theCore->ProcessList()->List()) {
                const CProcessPtr& pProcess = it.second;
                std::wstring DosPath = theCore->NormalizePath(pProcess->GetNtFilePath());
                if (std::regex_search(DosPath, re)) {
                    qjs::Value obj = MakeProcess(pProcess);
                    JS_SetPropertyUint32(context.ctx, arr.v, idx++, obj.release());
                    //JS_SetPropertyUint32(ctx, arr.v, idx++, JS_NewInt64(ctx, (int64_t)pProcess->GetProcessId()));
                }
            }
        }
        catch (...) {
            // On any regex error, just return empty array
        }

        return arr;
    };
    processList["findByName"] = findByName;

    auto getProcess = [&](int PID) -> qjs::Value {

        CProcessPtr pProcess = theCore->ProcessList()->GetProcessEx(PID, CProcessList::eCanAdd);
        if (!pProcess)
            return qjs::Value{ context.ctx, JS_NULL };

        return MakeProcess(pProcess);
    };
    processList["getProcess"] = getProcess;

    return processList;
}

//////////////////////////////////////////////////////////////////////////
// Firewall API

qjs::Value SJSEngine::MakeFirewallRule(const CFirewallRulePtr& pRule)
{
    if (!pRule)
        return qjs::Value{ context.ctx, JS_NULL };

    qjs::Value obj = context.newObject();

    SVarWriteOpt Opts;
    StVariant ruleData = pRule->ToVariant(Opts);

    // Convert the full rule data to JS
    return Var2JS(ruleData);
}

qjs::Value SJSEngine::MakeFirewall()
{
    auto firewall = context.newObject();

    // Get all firewall rules
    auto getAllRules = [&]() -> qjs::Value {
        auto rules = theCore->NetworkManager()->Firewall()->GetAllRules();

        qjs::Value arr{ context.ctx, JS_NewArray(context.ctx) };
        uint32_t idx = 0;
        for (auto& I : rules) {
            qjs::Value ruleObj = MakeFirewallRule(I.second);
            JS_SetPropertyUint32(context.ctx, arr.v, idx++, ruleObj.release());
        }
        return arr;
    };
    firewall["getAllRules"] = getAllRules;

    // Get rule by GUID
    auto getRule = [&](const std::string& guidStr) -> qjs::Value {
        CFlexGuid guid;
        guid.FromString(guidStr);
        auto pRule = theCore->NetworkManager()->Firewall()->GetRule(guid);
        return MakeFirewallRule(pRule);
    };
    firewall["getRule"] = getRule;

    // Create or update a firewall rule
    auto setRule = [&](const qjs::Value& ruleData) -> qjs::Value {
        StVariant ruleVariant = JS2Var(ruleData);

        auto pRule = std::make_shared<CFirewallRule>();
        if (!pRule->FromVariant(ruleVariant)) {
            return MakeString(L"Failed to parse rule data");
        }

        STATUS status = theCore->NetworkManager()->Firewall()->SetRule(pRule);
        if (status.IsError()) {
            return MakeString(L"Failed to set rule");
        }

        return MakeString(pRule->GetGuidStr());
    };
    firewall["setRule"] = setRule;

    // Delete a firewall rule
    auto delRule = [&](const std::string& guidStr) -> bool {
        CFlexGuid guid;
        guid.FromString(guidStr);
        STATUS status = theCore->NetworkManager()->Firewall()->DelRule(guid);
        return status.IsSuccess();
    };
    firewall["delRule"] = delRule;

    // Enable/disable a rule
    auto setRuleEnabled = [&](const std::string& guidStr, bool enabled) -> bool {
        CFlexGuid guid;
        guid.FromString(guidStr);
        auto pRule = theCore->NetworkManager()->Firewall()->GetRule(guid);
        if (!pRule)
            return false;

        pRule->SetEnabled(enabled);
        STATUS status = theCore->NetworkManager()->Firewall()->SetRule(pRule);
        return status.IsSuccess();
    };
    firewall["setRuleEnabled"] = setRuleEnabled;

    return firewall;
}

//////////////////////////////////////////////////////////////////////////
// DNS Filter API

qjs::Value SJSEngine::MakeDnsRule(const FW::SharedPtr<CDnsRule>& pRule)
{
    if (!pRule)
        return qjs::Value{ context.ctx, JS_NULL };

    SVarWriteOpt Opts;
    StVariant ruleData = pRule->ToVariant(Opts);
    return Var2JS(ruleData);
}

qjs::Value SJSEngine::MakeDnsFilter()
{
    auto dnsFilter = context.newObject();

    // Get DNS filter entry by ID
    auto getEntry = [&](const std::string& entryId) -> qjs::Value {
        SVarWriteOpt Opts;
        auto result = theCore->NetworkManager()->DnsFilter()->GetEntry(s2w(entryId), Opts);
        if (result.IsError())
            return qjs::Value{ context.ctx, JS_NULL };
        return Var2JS(result.GetValue());
    };
    dnsFilter["getEntry"] = getEntry;

    // Set (create or update) DNS filter entry
    auto setEntry = [&](const qjs::Value& entryData) -> qjs::Value {
        StVariant entryVariant = JS2Var(entryData);
        auto result = theCore->NetworkManager()->DnsFilter()->SetEntry(entryVariant);
        if (result.IsError())
            return MakeString(L"Failed to set DNS entry");
        return Var2JS(result.GetValue());
    };
    dnsFilter["setEntry"] = setEntry;

    // Delete DNS filter entry
    auto delEntry = [&](const std::string& entryId) -> bool {
        STATUS status = theCore->NetworkManager()->DnsFilter()->DelEntry(s2w(entryId));
        return status.IsSuccess();
    };
    dnsFilter["delEntry"] = delEntry;

    // Get all DNS filter entries
    auto getAllEntries = [&]() -> qjs::Value {
        SVarWriteOpt Opts;
        StVariant entries = theCore->NetworkManager()->DnsFilter()->SaveEntries(Opts);
        return Var2JS(entries);
    };
    dnsFilter["getAllEntries"] = getAllEntries;

    return dnsFilter;
}

//////////////////////////////////////////////////////////////////////////
// Program Manager API

qjs::Value SJSEngine::MakeProgramRule(const CProgramRulePtr& pRule)
{
    if (!pRule)
        return qjs::Value{ context.ctx, JS_NULL };

    SVarWriteOpt Opts;
    StVariant ruleData = pRule->ToVariant(Opts);
    return Var2JS(ruleData);
}

qjs::Value SJSEngine::MakeProgramManager()
{
    auto programManager = context.newObject();

    // Get all program rules
    auto getAllRules = [&]() -> qjs::Value {
        auto rules = theCore->ProgramManager()->GetAllRules();

        qjs::Value arr{ context.ctx, JS_NewArray(context.ctx) };
        uint32_t idx = 0;
        for (auto& I : rules) {
            qjs::Value ruleObj = MakeProgramRule(I.second);
            JS_SetPropertyUint32(context.ctx, arr.v, idx++, ruleObj.release());
        }
        return arr;
    };
    programManager["getAllRules"] = getAllRules;

    // Get rule by GUID
    auto getRule = [&](const std::string& guidStr) -> qjs::Value {
        CFlexGuid guid;
        guid.FromString(guidStr);
        auto pRule = theCore->ProgramManager()->GetRule(guid);
        return MakeProgramRule(pRule);
    };
    programManager["getRule"] = getRule;

    // Add a new program rule
    auto addRule = [&](const qjs::Value& ruleData) -> qjs::Value {
        StVariant ruleVariant = JS2Var(ruleData);

        auto pRule = std::make_shared<CProgramRule>(CProgramID());
        if (pRule->FromVariant(ruleVariant) != STATUS_SUCCESS) {
            return MakeString(L"Failed to parse rule data");
        }

        auto result = theCore->ProgramManager()->AddRule(pRule);
        if (result.IsError())
            return MakeString(L"Failed to add rule");

        return MakeString(result.GetValue());
    };
    programManager["addRule"] = addRule;

    // Remove a program rule
    auto removeRule = [&](const std::string& guidStr) -> bool {
        CFlexGuid guid;
        guid.FromString(guidStr);
        STATUS status = theCore->ProgramManager()->RemoveRule(guid);
        return status.IsSuccess();
    };
    programManager["removeRule"] = removeRule;

    return programManager;
}

//////////////////////////////////////////////////////////////////////////
// Access Manager API

qjs::Value SJSEngine::MakeAccessRule(const CAccessRulePtr& pRule)
{
    if (!pRule)
        return qjs::Value{ context.ctx, JS_NULL };

    SVarWriteOpt Opts;
    StVariant ruleData = pRule->ToVariant(Opts);
    return Var2JS(ruleData);
}

qjs::Value SJSEngine::MakeAccessManager()
{
    auto accessManager = context.newObject();

    // Get all access rules
    auto getAllRules = [&]() -> qjs::Value {
        auto rules = theCore->AccessManager()->GetAllRules();

        qjs::Value arr{ context.ctx, JS_NewArray(context.ctx) };
        uint32_t idx = 0;
        for (auto& I : rules) {
            qjs::Value ruleObj = MakeAccessRule(I.second);
            JS_SetPropertyUint32(context.ctx, arr.v, idx++, ruleObj.release());
        }
        return arr;
    };
    accessManager["getAllRules"] = getAllRules;

    // Get rule by GUID
    auto getRule = [&](const std::string& guidStr) -> qjs::Value {
        CFlexGuid guid;
        guid.FromString(guidStr);
        auto pRule = theCore->AccessManager()->GetRule(guid);
        return MakeAccessRule(pRule);
    };
    accessManager["getRule"] = getRule;

    // Add a new access rule
    auto addRule = [&](const qjs::Value& ruleData) -> qjs::Value {
        StVariant ruleVariant = JS2Var(ruleData);

        auto pRule = std::make_shared<CAccessRule>(CProgramID());
        if (pRule->FromVariant(ruleVariant) != STATUS_SUCCESS) {
            return MakeString(L"Failed to parse rule data");
        }

        auto result = theCore->AccessManager()->AddRule(pRule);
        if (result.IsError())
            return MakeString(L"Failed to add rule");

        return MakeString(result.GetValue());
    };
    accessManager["addRule"] = addRule;

    // Remove an access rule
    auto removeRule = [&](const std::string& guidStr) -> bool {
        CFlexGuid guid;
        guid.FromString(guidStr);
        STATUS status = theCore->AccessManager()->RemoveRule(guid);
        return status.IsSuccess();
    };
    accessManager["removeRule"] = removeRule;

    return accessManager;
}

//////////////////////////////////////////////////////////////////////////
// Tweak Manager API

qjs::Value SJSEngine::MakeTweak(const CTweakPtr& pTweak)
{
    if (!pTweak)
        return qjs::Value{ context.ctx, JS_NULL };

    SVarWriteOpt Opts;
    StVariant tweakData = pTweak->ToVariant(Opts);
    return Var2JS(tweakData);
}

qjs::Value SJSEngine::MakeTweakManager()
{
    auto tweakManager = context.newObject();

    // Get all tweaks
    auto getAllTweaks = [&]() -> qjs::Value {
        SVarWriteOpt Opts;
        uint32 callerPID = GetCurrentProcessId();
        StVariant tweaks = theCore->TweakManager()->GetTweaks(callerPID, Opts);
        return Var2JS(tweaks);
    };
    tweakManager["getAllTweaks"] = getAllTweaks;

    // Get tweak by ID
    auto getTweak = [&](const std::string& tweakId) -> qjs::Value {
        auto pTweak = theCore->TweakManager()->GetTweakById(s2w(tweakId));
        return MakeTweak(pTweak);
    };
    tweakManager["getTweak"] = getTweak;

    // Apply a tweak
    auto applyTweak = [&](const std::string& tweakId) -> bool {
        uint32 callerPID = GetCurrentProcessId();
        STATUS status = theCore->TweakManager()->ApplyTweak(s2w(tweakId), callerPID);
        return status.IsSuccess();
    };
    tweakManager["applyTweak"] = applyTweak;

    // Undo a tweak
    auto undoTweak = [&](const std::string& tweakId) -> bool {
        uint32 callerPID = GetCurrentProcessId();
        STATUS status = theCore->TweakManager()->UndoTweak(s2w(tweakId), callerPID);
        return status.IsSuccess();
    };
    tweakManager["undoTweak"] = undoTweak;

    // Approve a tweak (for diverged tweaks)
    auto approveTweak = [&](const std::string& tweakId) -> bool {
        uint32 callerPID = GetCurrentProcessId();
        STATUS status = theCore->TweakManager()->ApproveTweak(s2w(tweakId), callerPID);
        return status.IsSuccess();
    };
    tweakManager["approveTweak"] = approveTweak;

    return tweakManager;
}

//////////////////////////////////////////////////////////////////////////
// Script Runners

EProgramOnSpawn CJSEngine::RunStartScript(const std::wstring& NtPath, const std::wstring& CommandLine, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, bool bTrusted, const struct SVerifierInfo* pVerifierInfo)
{
    //std::unique_lock Lock(m_Mutex);

    StVariant Event;
    Event["ntPath"] = NtPath;
    Event["dosPath"] = theCore->NormalizePath(NtPath);
    Event["commandLine"] = CommandLine;
    Event["actorPid"] = ActorPid;
    Event["actorServiceTag"] = ActorServiceTag;
    Event["enclaveId"] = EnclaveId;
    Event["trusted"] = bTrusted;
    Event["ci"] = CJSEngine__FillVerifierInfo(pVerifierInfo);

    auto Ret = CallFunction("onStart", Event);
    if(Ret.IsError())
        return EProgramOnSpawn::eUnknown;
    EProgramOnSpawn Action = (EProgramOnSpawn)Ret.GetValue().To<int>();
    return Action;
}

EImageOnLoad CJSEngine::RunLoadScript(uint64 Pid, const std::wstring& NtPath, const std::wstring& EnclaveId, bool bTrusted, const struct SVerifierInfo* pVerifierInfo)
{
    //std::unique_lock Lock(m_Mutex);

    StVariant Event;
    Event["pid"] = Pid;
    Event["ntPath"] = NtPath;
    Event["dosPath"] = theCore->NormalizePath(NtPath);
    Event["enclaveId"] = EnclaveId;
    Event["trusted"] = bTrusted;
    Event["ci"] = CJSEngine__FillVerifierInfo(pVerifierInfo);
    
    auto Ret = CallFunction("onLoad", Event);
    if(Ret.IsError())
        return EImageOnLoad::eUnknown;
    EImageOnLoad Action = (EImageOnLoad)Ret.GetValue().To<int>();
    return Action;
}

EAccessRuleType CJSEngine::RunAccessScript(const std::wstring& NtPath, uint64 ActorPid, const std::wstring& ActorServiceTag, const std::wstring& EnclaveId, uint32 AccessMask)
{
    //std::unique_lock Lock(m_Mutex);

    StVariant Event;
    Event["ntPath"] = NtPath;
    Event["dosPath"] = theCore->NormalizePath(NtPath);
    Event["actorPid"] = ActorPid;
    Event["actorServiceTag"] = ActorServiceTag;
    Event["enclaveId"] = EnclaveId;
    Event["accessMask"] = AccessMask;

    auto Ret = CallFunction("onAccess", Event);
    if(Ret.IsError())
        return EAccessRuleType::eNone;
    EAccessRuleType Action = (EAccessRuleType)Ret.GetValue().To<int>();
    return Action;
}

bool CJSEngine::RunMountScript(const std::wstring& ImagePath, const std::wstring& DevicePath, const std::wstring& MountPoint)
{
    StVariant Data;
	Data["imagePath"] = ImagePath;
	Data["devicePath"] = DevicePath;
    Data["mountPoint"] = MountPoint;
    auto Ret = CallFunction("onMount", Data);
    if (Ret.IsError()) {
        if(Ret.GetStatus() == STATUS_NOT_FOUND)
			return true; // allow if no function
		return false; // block on error
    }
    if(!Ret.GetValue().IsValid())
		return true; // allow if no return value
	return Ret.GetValue().To<bool>(); // allow/block
}

void CJSEngine::RunDismountScript(const std::wstring& ImagePath, const std::wstring& DevicePath, const std::wstring& MountPoint)
{
    StVariant Data;
    Data["imagePath"] = ImagePath;
    Data["devicePath"] = DevicePath;
    Data["mountPoint"] = MountPoint;
    CallFunction("onDismount", Data);
}

void CJSEngine::RunActivationScript(uint32 CallerPID)
{
    StVariant Data;
    CallFunction("onActivation", Data, CallerPID);
}

void CJSEngine::RunDeactivationScript(uint32 CallerPID)
{
    StVariant Data;
    CallFunction("onDeactivation", Data, CallerPID);
}