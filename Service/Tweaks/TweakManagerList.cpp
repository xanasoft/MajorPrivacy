#include "pch.h"
#include "Tweak.h"
#include "../Library/Common/DbgHelp.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/ConfigIni.h"


#define V(x) StVariant(x)

// Windows Versions
#define WinVer_Win2k        SWinVer(2195)
#define WinVer_WinXP        SWinVer(2600)
#define WinVer_WinXPonly    SWinVer(2600, 3790)
#define WinVer_WinXPto7     SWinVer(2600, 7601)
#define WinVer_WinXPto8     SWinVer(2600, 9600)
#define WinVer_WinXPto11    SWinVer(2600, 22000)
#define WinVer_Win6         SWinVer(6002)
#define WinVer_Win6to7      SWinVer(6002, 7601)
#define WinVer_Win7         SWinVer(7601)
#define WinVer_Win7to81     SWinVer(7601, 9600)
#define WinVer_Win7to11     SWinVer(7601, 22000)
#define WinVer_Win8         SWinVer(9200)
#define WinVer_Win81        SWinVer(9600)
#define WinVer_Win81only    SWinVer(9600, 9600)
#define WinVer_Win8to11     SWinVer(9200, 22000)
// Windows 10
#define WinVer_Win10        SWinVer(10000)
#define WinVer_Win10to1709  SWinVer(10000, 16299)
#define WinVer_Win10EE      SWinVer(10000, EWinEd::Enterprise)
#define WinVer_Win1507      SWinVer(10240)
#define WinVer_Win1511      SWinVer(10586)
#define WinVer_Win1607      SWinVer(14393)
#define WinVer_Win1703      SWinVer(15063)
#define WinVer_Win1709      SWinVer(16299)
#define WinVer_Win1803      SWinVer(17134)
#define WinVer_Win1809      SWinVer(17763)
#define WinVer_Win19H1      SWinVer(18362)
#define WinVer_Win19H2      SWinVer(18363)
#define WinVer_Win20H1      SWinVer(19041)
#define WinVer_Win20H2      SWinVer(19042)
#define WinVer_Win21H1      SWinVer(19043)
#define WinVer_Win10_21H2   SWinVer(19044)
#define WinVer_Win10_22H2   SWinVer(19045)
#define WinVer_Win10to11    SWinVer(10000, 22000)
// Windows 11
#define WinVer_Win11        SWinVer(22000)
#define WinVer_Win11_22H2   SWinVer(22621)
#define WinVer_Win23H2      SWinVer(22631)


//#ifdef _DEBUG
//#define DUMP_TO_INI
//#endif

#ifdef DUMP_TO_INI
CConfigIni* g_TweaksIni = NULL;
#endif

template <class _Ty, class... _Types>
CTweakPtr MakeTweakObject(const CTweakPtr& pParent, std::list<CTweakPtr>* pList, _Types&&... _Args)
{
    CTweakPtr pTweak = std::make_shared<_Ty>(std::forward<_Types>(_Args)...);
    //std::dynamic_pointer_cast<CTweakList>(pParent)->AddTweak(pTweak);
    //pTweak->SetParent(pParent);
	pTweak->SetParentId(pParent->GetId());
    if (pList) {
        //std::wstring id = pTweak->GetId();
		////DbgPrint(L"[%s] %s\n", id.c_str(), pTweak->GetName().c_str());
        //bool bInserted = pList->insert(std::make_pair(id, pTweak)).second;
        //ASSERT(bInserted); // names must be unique
#if 0 //#ifdef _DEBUG
		for (auto& _Tweak : *pList) {
			ASSERT(_Tweak->GetId() != pTweak->GetId()); // names must be unique
			if ((_Tweak->GetType() == ETweakType::eReg || _Tweak->GetType() == ETweakType::eGpo) &&
				(pTweak->GetType() == ETweakType::eReg || pTweak->GetType() == ETweakType::eGpo)) {
				
                std::wstring Key1;
                std::wstring Key2;
                std::wstring Value1;
                std::wstring Value2;
                if (_Tweak->GetType() == ETweakType::eReg) {
                    Key1 = std::dynamic_pointer_cast<CRegTweak>(_Tweak)->GetKey();
					Value1 = std::dynamic_pointer_cast<CRegTweak>(_Tweak)->GetValue();
                }
                else if (_Tweak->GetType() == ETweakType::eGpo) {
                    Key1 = std::dynamic_pointer_cast<CGpoTweak>(_Tweak)->GetKey();
					Value1 = std::dynamic_pointer_cast<CGpoTweak>(_Tweak)->GetValue();
                }
                if (pTweak->GetType() == ETweakType::eReg) {
                    Key2 = std::dynamic_pointer_cast<CRegTweak>(pTweak)->GetKey();
					Value2 = std::dynamic_pointer_cast<CRegTweak>(pTweak)->GetValue();
                }
                else if (pTweak->GetType() == ETweakType::eGpo) {
                    Key2 = std::dynamic_pointer_cast<CGpoTweak>(pTweak)->GetKey();
					Value2 = std::dynamic_pointer_cast<CGpoTweak>(pTweak)->GetValue();
                }

                if (_wcsicmp(Key1.c_str(), Key2.c_str()) == 0 && _wcsicmp(Value1.c_str(), Value2.c_str()) == 0) {
                    //DbgPrint(L"[%s] %s\n", pTweak->GetId().c_str(), pTweak->GetName().c_str());
                    //DbgPrint(L"[%s] %s\n", _Tweak->GetId().c_str(), _Tweak->GetName().c_str());
                    if(Key1 == L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU")
                        continue;
                    ASSERT(0);
                }
			}
		}
#endif
        pList->push_back(pTweak);
		pTweak->SetIndex((int)pList->size());
    }
    //DbgPrint(L"Names[\"%s\"] = tr(\"%s\");\n", pTweak->GetName().c_str(), pTweak->GetName().c_str());
#ifdef DUMP_TO_INI
	pTweak->Dump(g_TweaksIni);
#endif
    return pTweak;
}

std::shared_ptr<CTweakList> InitKnownTweaks(std::list<CTweakPtr>* pList)
{
    std::shared_ptr<CTweakList> pRoot = std::make_shared<CTweakGroup>(L"", L"");
    
#ifdef DUMP_TO_INI
    g_TweaksIni = new CConfigIni(GetApplicationDirectory() + L"\\Tweaks.ini");
#endif

    /*  
    *  #########################################
    *       Telemetry & Error reporting
    *  #########################################
    */

    CTweakPtr telemetryCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Telemetry & Error Reporting", L"windows_telemetry");


    // *** Telemetry ***
    CTweakPtr telemetry = MakeTweakObject<CTweakSet>(telemetryCat, pList, L"Disable Telemetry", L"disable_telemetry", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(telemetry, pList, L"Disallow Telemetry", L"disallow_telemetry", WinVer_Win10EE, // only available in enterprise and education
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
        L"AllowTelemetry",
        V(0));
    MakeTweakObject<CGpoTweak>(telemetry, pList, L"Disable Telemetry Opt-In Notification", L"disable_optin_notify", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", 
        L"DisableTelemetryOptInChangeNotification", 
        V(1));
    MakeTweakObject<CSvcTweak>(telemetry, pList, L"Disable Telemetry Service (DiagTrack-Listener)", L"disable_diagtrack_service", WinVer_Win7,
        L"DiagTrack");
    MakeTweakObject<CRegTweak>(telemetry, pList, L"Disable AutoLogger-Diagtrack-Listener", L"disable_diagtrack_autologger", WinVer_Win10,
        L"HKLM\\SYSTEM\\CurrentControlSet\\Control\\WMI\\Autologger\\AutoLogger-Diagtrack-Listener",
        L"Start",
        V(0));
    MakeTweakObject<CSvcTweak>(telemetry, pList, L"Disable Push Service", L"disable_push_service", WinVer_Win10,
        L"dmwappushservice",
        ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(telemetry, pList, L"Don't AllowDeviceNameInTelemetry", L"disable_device_name_in_telemetry", WinVer_Win1803,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
        L"AllowDeviceNameInTelemetry",
        V(0));
    MakeTweakObject<CResTweak>(telemetry, pList, L"Block Diagnosis/AutoLogger Data Folder", L"block_diagnosis_data", WinVer_Win10,
        L"%ProgramData%\\Microsoft\\Diagnosis\\**"); // //%ProgramData%\Microsoft\Diagnosis\ETLLogs\AutoLogger\AutoLogger-Diagtrack-Listener.etl
    //Disable file Diagtrack-Listener.etl
    //(Disable file diagtrack.dll)
    //(Disable file BthTelemetry.dll)
    MakeTweakObject<CGpoTweak>(telemetry, pList, L"Limit DiagTrack Telemetry", L"limit_diagtrack", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Diagnostics\\DiagTrack",
        L"DiagTrackAuthorization",
        V(7));
    MakeTweakObject<CGpoTweak>(telemetry, pList, L"Limit DiagTrack Telemetry (32-bit)", L"limit_diagtrack_32", WinVer_Win10,
        L"HKLM\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Diagnostics\\DiagTrack",
        L"DiagTrackAuthorization",
        V(7));
    MakeTweakObject<CRegTweak>(telemetry, pList, L"Disable DataCollection", L"disable_data_collection", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\DataCollection",
        L"AllowTelemetry",
        V(0));
    MakeTweakObject<CGpoTweak>(telemetry, pList, L"Limit Diagnostic Log Collection", L"limit_diagnostic_log_collection", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
        L"LimitDiagnosticLogCollection",
        V(1));
    MakeTweakObject<CTaskTweak>(telemetry, pList, L"Device Census", L"disable_device_census_task", WinVer_Win7,
        L"\\Microsoft\\Windows\\Device Information",
        L"*");
    //L"Device"
    //L"Device User"
    MakeTweakObject<CFwTweak>(telemetry, pList, L"Firewall Device Census", L"fw_block_device_census", WinVer_Win10,
        L"%SystemRoot%\\system32\\DeviceCensus.exe");
    MakeTweakObject<CGpoTweak>(telemetry, pList, L"Disable OneSettings Downloads", L"disable_one_settings_downloads", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
        L"DisableOneSettingsDownloads",
        V(1));


    // *** AppCompat ***
    CTweakPtr appExp = MakeTweakObject<CTweakSet>(telemetryCat, pList, L"Disable Application Telemetry", L"disable_app_telemetry", ETweakHint::eRecommended);
    MakeTweakObject<CTaskTweak>(appExp, pList, L"Disable Application Experience Tasks", L"disable_app_experience_tasks", WinVer_Win6,
        L"\\Microsoft\\Windows\\Application Experience",
        L"*");
        // L"Microsoft Compatibility Appraiser"
        // L"Microsoft Compatibility Appraiser Exp"
    MakeTweakObject<CSvcTweak>(appExp, pList, L"Disable Application Experience Service", L"disable_app_experience_service", WinVer_Win7to81,
        L"AeLookupSvc");
    //MakeTweakObject<CSvcTweak>(appExp, pList, L"Disable Application Information Service", WinVer_Win7, // this breaks UAC
    //    L"Appinfo");
    MakeTweakObject<CGpoTweak>(appExp, pList, L"Disable Application Telemetry", L"disable_appcompat_telemetry", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat", 
        L"AllowTelemetry", 
        V(0));
    MakeTweakObject<CGpoTweak>(appExp, pList, L"Disable Application Impact Telemetry (AIT)", L"disable_app_compat_telemetry", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat",
        L"AITEnable",
        V(0));
    MakeTweakObject<CGpoTweak>(appExp, pList, L"Disable Steps Recorder (UAR)", L"disable_steps_recorder", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\windows\\AppCompat",
        L"DisableUAR",
        V(1));
    MakeTweakObject<CGpoTweak>(appExp, pList, L"Disable Inventory Collector Task", L"disable_inventory_collector", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\windows\\AppCompat",
        L"DisableInventory",
        V(1));
    MakeTweakObject<CExecTweak>(appExp, pList, L"Disable CompatTelRunner.exe", L"disable_compattelrunner", WinVer_Win6,
        L"%SystemRoot%\\System32\\CompatTelRunner.exe");
    MakeTweakObject<CFwTweak>(appExp, pList, L"Firewall CompatTelRunner.exe", L"fw_block_compattelrunner", WinVer_Win10,
        L"%SystemRoot%\\system32\\compattelrunner.exe");
    MakeTweakObject<CExecTweak>(appExp, pList, L"Disable DiagTrackRunner.exe", L"disable_diagtrackrunner", WinVer_Win7to81,
        L"%SystemRoot%\\CompatTel\\diagtrackrunner.exe");


    // *** AppCompat ***
    //CTweakPtr appCompat = MakeTweakObject<CTweakSet>(telemetryCat, pList, L"Disable Application Compatybility", L"disable_app_compatybility", ETweakHint::eNotRecommended);
    //MakeTweakObject<CGpoTweak>(appCompat, pList, L"Disable Program Compatibility Assistant", L"disable_pca_feature", WinVer_Win6,
    //    L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat",
    //    L"DisablePCA",
    //    V(1),
    //    ETweakHint::eBreaking);
    //MakeTweakObject<CGpoTweak>(appCompat, pList, L"Disable Application Compatibility Engine", L"disable_appcompat_engine", WinVer_Win6,
    //    L"HKLM\\Software\\Policies\\Microsoft\\Windows\\AppCompat",
    //    L"DisableEngine",
    //    V(1),
    //    ETweakHint::eBreaking);
    //MakeTweakObject<CGpoTweak>(appCompat, pList, L"Remove Compatibility Tab from File Properties", L"disable_compatibility_tab", WinVer_Win6,
    //    L"HKLM\\Software\\Policies\\Microsoft\\Windows\\AppCompat",
    //    L"DisablePropPage",
    //    V(1),
    //    ETweakHint::eBreaking);
    //MakeTweakObject<CSvcTweak>(appCompat, pList, L"Disable Program Compatibility Assistant Service", L"disable_pca_service", WinVer_Win6,
    //    L"PcaSvc",
    //    ETweakHint::eBreaking);


    // *** CEIP ***
    CTweakPtr ceip = MakeTweakObject<CTweakSet>(telemetryCat, pList, L"Disable CEIP", L"disable_ceip", ETweakHint::eRecommended);
    // Disable CEIP via Policy
    MakeTweakObject<CGpoTweak>(ceip, pList, L"Turn off CEIP", L"disable_ceip_policy_hklm", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\SQMClient\\Windows",
        L"CEIPEnable",
        V(0));
    // Disable CEIP
    MakeTweakObject<CRegTweak>(ceip, pList, L"Disable CEIP", L"turn_off_ceip", WinVer_Win6,
        L"HKLM\\Software\\Microsoft\\SQMClient\\Windows",
        L"CEIPEnable",
        V(0));
    // Disable CEIP uploads
    MakeTweakObject<CRegTweak>(ceip, pList, L"Disable CEIP Uploads", L"disable_ceip_uploads", WinVer_Win6,
        L"HKLM\\Software\\Microsoft\\SQMClient",
        L"UploadDisableFlag",
        V(0));
    MakeTweakObject<CTaskTweak>(ceip, pList, L"Disable all CEIP Tasks", L"disable_ceip_tasks", WinVer_Win6,
        L"\\Microsoft\\Windows\\Customer Experience Improvement Program",
        V(L"*"));
	    // L"Server\ServerCeipAssistant"
        // L"Server\ServerRoleCollector"
        // L"Server\ServerRoleUsageCollector"
        // L"KernelCeipTask"
        // L"BthSQM"
        // L"UsbCeip"
        // L"Consolidator"
        // L"Uploader"
        // L"1JsbCetp"
    MakeTweakObject<CTaskTweak>(ceip, pList, L"Disable autochk SQM upload task", L"disable_autochk_proxy", WinVer_Win6,
        L"\\Microsoft\\Windows\\Autochk",
        L"Proxy");
    MakeTweakObject<CFwTweak>(ceip, pList, L"Firewall SQM-Konsolidator", L"fw_wsqmcons", WinVer_Win10,
        L"%SystemRoot%\\system32\\wsqmcons.exe");
    MakeTweakObject<CGpoTweak>(ceip, pList, L"Disable IE CEIP", L"disable_ie_ceip", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\SQM",
        L"DisableCustomerImprovementProgram",
        V(0),  // must be 0 according to https://getadmx.com/?Category=Windows_10_2016&Policy=Microsoft.Policies.InternetExplorer::SQM_DisableCEIP
        ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(ceip, pList, L"Disable Live Messenger CEIP", L"disable_messenger_ceip", WinVer_WinXPto7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Messenger\\Client",
        L"CEIP",
        V(2),
        ETweakHint::eNotRecommended);
    // Set USER GPO "SOFTWARE\\Policies\\Microsoft\\Messenger\\Client" "CEIP" "2" (deprecated)
    MakeTweakObject<CRegTweak>(ceip, pList, L"Disable SIUF Collection (User)", L"disable_siuf_collection_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Siuf\\Rules",
        L"NumberOfSIUFInPeriod",
        V(0));
    MakeTweakObject<CRegTweak>(ceip, pList, L"Disable SIUF Period (User)", L"disable_siuf_period_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Siuf\\Rules",
        L"PeriodInNanoSeconds",
        V(0));
    MakeTweakObject<CTaskTweak>(ceip, pList, L"Disable Feadback Task", L"disable_siuf_tasks", WinVer_Win10,
        L"\\Microsoft\\Windows\\Feedback\\Siuf",
        V(L"*"));
    // L"DmClient"
    // L"DmClientOnScenarioDownload"
    MakeTweakObject<CFwTweak>(ceip, pList, L"Firewall Feedback SIUF", L"fw_block_dmclient", WinVer_Win10,
        L"%SystemRoot%\\system32\\dmclient.exe");
    MakeTweakObject<CGpoTweak>(ceip, pList, L"Do not Show Feedback Notifications", L"disable_feedback_notifications", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
        L"DoNotShowFeedbackNotifications",
        V(1));


    // *** Error Reporting ***
    CTweakPtr werr = MakeTweakObject<CTweakSet>(telemetryCat, pList, L"Disable Error Reporting", L"disable_error_reporting", ETweakHint::eRecommended);
    MakeTweakObject<CRegTweak>(werr, pList, L"Turn off Windows Error Reporting", L"turn_off_error_reporting", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting",
        L"Disabled",
        V(1));
    MakeTweakObject<CGpoTweak>(werr, pList, L"Block Auto Approval of Crash Dumps", L"block_crashdump_autoapprove", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Error Reporting", 
        L"AutoApproveOSDumps", 
        V(0));
    MakeTweakObject<CGpoTweak>(werr, pList, L"Block Additional Error Reporting Data", L"block_errorreport_additionaldata", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Error Reporting", 
        L"DontSendAdditionalData", 
        V(1));
    MakeTweakObject<CGpoTweak>(werr, pList, L"Turn off Windows Error Reporting (GPO)", L"turn_off_error_reporting_gpo", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Error Reporting",
        L"Disabled",
        V(1));
    MakeTweakObject<CGpoTweak>(werr, pList, L"Do not Report Errors", L"do_not_report_errors", WinVer_WinXPonly,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\PCHealth\\ErrorReporting",
        L"DoReport",
        V(0));
    MakeTweakObject<CSvcTweak>(werr, pList, L"Disable Error Reporting Service", L"disable_error_reporting_service", WinVer_Win6,
        L"WerSvc");
    MakeTweakObject<CTaskTweak>(werr, pList, L"Disable Error Reporting Tasks", L"disable_error_reporting_tasks", WinVer_Win6,
        L"\\Microsoft\\Windows\\Windows Error Reporting",
        V(L"*"));
        //L"QueueReporting"
    // (Disable file WerSvc.dll)
    // (Disable file wer*.exe)
    MakeTweakObject<CRegTweak>(werr, pList, L"Disable Error Report UI", L"disable_errorreport_ui", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting",
        L"DontShowUI",
        V(1),
        ETweakHint::eNotRecommended);
    MakeTweakObject<CFwTweak>(werr, pList, L"Firewall Problem Reporting", L"fw_block_wermgr", WinVer_Win10,
        L"%SystemRoot%\\system32\\wermgr.exe");


    // *** Kernel Crash Dumps ***
    CTweakPtr kdmp = MakeTweakObject<CTweakSet>(telemetryCat, pList, L"Disable Kernel Crash Dumps", L"disable_kernel_crash_dumps", ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(kdmp, pList, L"Disable System Crash Dumps", L"disable_system_crash_dumps", WinVer_Win2k,
        L"HKLM\\SYSTEM\\CurrentControlSet\\Control\\CrashControl",
        L"CrashDumpEnabled",
        V(0),
        ETweakHint::eNotRecommended);


    // *** Other Diagnostics ***
    CTweakPtr diag = MakeTweakObject<CTweakSet>(telemetryCat, pList, L"Other Diagnostics", L"other_diagnostics");
    MakeTweakObject<CGpoTweak>(diag, pList, L"Turn off MSDT", L"turn_off_msdt", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\ScriptedDiagnosticsProvider\\Policy",
        L"DisableQueryRemoteServer",
        V(0)); // must be 0 according to https://getadmx.com/?Category=Windows_10_2016&Policy=Microsoft.Policies.MSDT::MsdtSupportProvider
    MakeTweakObject<CGpoTweak>(diag, pList, L"Turn off Online Assist", L"turn_off_online_assist", WinVer_Win6to7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Assistance\\Client\\1.0",
        L"NoOnlineAssist",
        V(1));
    // Set USER GPO @"Software\Policies\Microsoft\Assistance\Client\1.0" "NoOnlineAssist" "1" (deprecated)
    // Set USER GPO @"Software\Policies\Microsoft\Assistance\Client\1.0" "NoExplicitFeedback" "1" (deprecated)
    // Set USER GPO @"Software\Policies\Microsoft\Assistance\Client\1.0" "NoImplicitFeedback" "1" (deprecated)
    //MakeTweakObject<CSvcTweak>(diag, pList, L"Disable diagnosticshub Service", L"disable_diagnosticshub_service", WinVer_Win10,
    //    L"diagnosticshub.standardcollector.service");
    MakeTweakObject<CGpoTweak>(diag, pList, L"Do not Update Disk Health Model", L"disable_disk_health_model_updates", WinVer_Win1709,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\StorageHealth",
        L"AllowDiskHealthModelUpdates",
        V(0),
        ETweakHint::eNotRecommended);
    MakeTweakObject<CTaskTweak>(diag, pList, L"Disable DiskDiagnosticDataCollector", L"disable_diskdiagnostic_datacollector", WinVer_Win6,
        L"\\Microsoft\\Windows\\DiskDiagnostic",
        L"Microsoft-Windows-DiskDiagnosticDataCollector",
        ETweakHint::eNotRecommended);
    MakeTweakObject<CTaskTweak>(diag, pList, L"Disable DiskDiagnosticResolver", L"disable_diskdiagnostic_resolver", WinVer_Win6,
        L"\\Microsoft\\Windows\\DiskDiagnostic",
        L"Microsoft-Windows-DiskDiagnosticResolver",
        ETweakHint::eNotRecommended);


    /*  
    *  #########################################
    *              Cortana & search
    *  #########################################
    */

    CTweakPtr searchCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Search / Cortana & Copilot", L"windows_search");

    // *** Windows AI ***
    CTweakPtr ai = MakeTweakObject<CTweakSet>(searchCat, pList, L"Disable Windows AI (Copilot)", L"disable_windows_ai");
    MakeTweakObject<CGpoTweak>(ai, pList, L"Disable AI Data Analysis", L"disable_ai_data_analysis", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsAI",
        L"DisableAIDataAnalysis",
        V(1));
    //MakeTweakObject<CGpoTweak>(ai, pList, L"Disable AI Data Analysis (User)", L"disable_ai_data_analysis_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsAI",
    //    L"DisableAIDataAnalysis",
    //    V(1));
    MakeTweakObject<CGpoTweak>(ai, pList, L"Disable Windows Copilot", L"disable_windows_copilot", WinVer_Win11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsCopilot",
        L"TurnOffWindowsCopilot",
        V(1));
    //MakeTweakObject<CGpoTweak>(ai, pList, L"Disable Windows Copilot (User)", L"disable_windows_copilot_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsCopilot",
    //    L"TurnOffWindowsCopilot",
    //    V(1));
    MakeTweakObject<CRegTweak>(ai, pList, L"Disable Copilot Button in File Explorer (User)", L"disable_copilot_button_in_explorer_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        L"ShowCopilotButton",
        V(0));
    MakeTweakObject<CGpoTweak>(ai, pList, L"Disable Recall AI Feature", L"disable_recall_ai", WinVer_Win11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsAI",
        L"AllowRecallEnablement",
        V(0));
    MakeTweakObject<CGpoTweak>(ai, pList, L"Disable Generative Fill in Paint", L"disable_paint_gen_fill", WinVer_Win11,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Paint",
        L"DisableGenerativeFill",
        V(1));
    MakeTweakObject<CGpoTweak>(ai, pList, L"Disable Image Creator in Paint", L"disable_paint_image_creator", WinVer_Win11,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Paint",
        L"DisableImageCreator",
        V(1));
    MakeTweakObject<CGpoTweak>(ai, pList, L"Disable Cocreator in Paint", L"disable_paint_cocreator", WinVer_Win11,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Paint",
        L"DisableCocreator",
        V(1));


    // *** Disable Cortana ***
    CTweakPtr cortana = MakeTweakObject<CTweakSet>(searchCat, pList, L"Disable Cortana", L"disable_cortana");
    MakeTweakObject<CGpoTweak>(cortana, pList, L"Disable Cortana App", L"disable_cortana_app", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"AllowCortana",
        V(0));
    MakeTweakObject<CGpoTweak>(cortana, pList, L"Forbid the Use of Location", L"forbid_location_use", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"AllowSearchToUseLocation",
        V(0));
    MakeTweakObject<CGpoTweak>(cortana, pList, L"Disable Cortana Above Lock Screen", L"disable_cortana_above_lock", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"AllowCortanaAboveLock",
        V(0));
    MakeTweakObject<CRegTweak>(cortana, pList, L"Disable Cortana Consent (User)", L"disable_cortana_consent_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Windows Search",
        L"CortanaConsent",
        V(0));

    // *** Disable Web Search ***
    CTweakPtr webSearch = MakeTweakObject<CTweakSet>(searchCat, pList, L"Disable Online Search", L"disable_online_search", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(webSearch, pList, L"Disable Web Search", L"disable_web_search", WinVer_WinXP,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"DisableWebSearch",
        V(1));
    MakeTweakObject<CGpoTweak>(webSearch, pList, L"Disable Connected Web Search", L"disable_connected_web_search", WinVer_Win81,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"ConnectedSearchUseWeb",
        V(0));
    MakeTweakObject<CGpoTweak>(webSearch, pList, L"Enforce Search Privacy", L"enforce_search_privacy", WinVer_Win81only,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"ConnectedSearchPrivacy",
        V(3));
    MakeTweakObject<CGpoTweak>(webSearch, pList, L"Disable Search Box Suggestions", L"disable_search_box_suggestions", WinVer_Win20H1,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"DisableSearchBoxSuggestions",
        V(1));
    MakeTweakObject<CGpoTweak>(webSearch, pList, L"Disable Explorer Search Box Suggestions", L"disable_explorer_search_suggestions", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Explorer", 
        L"DisableSearchBoxSuggestions", 
        V(1));
    //MakeTweakObject<CGpoTweak>(webSearch, pList, L"Disable Explorer Search Box Suggestions (User)", L"disable_explorer_search_suggestions_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Windows\\Explorer",
    //    L"DisableSearchBoxSuggestions",
    //    V(1));
    MakeTweakObject<CGpoTweak>(webSearch, pList, L"Disable Cloud Search", L"disable_cloud_search", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"AllowCloudSearch",
        V(0));
    MakeTweakObject<CGpoTweak>(webSearch, pList, L"Disable Dynamic Content in Windows Search Box", L"disable_dynamic_content_wsb", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"EnableDynamicContentInWSB",
        V(0));


    // *** Disable Search ***
    CTweakPtr search = MakeTweakObject<CTweakSet>(searchCat, pList, L"Disable Search", L"disable_search");
    MakeTweakObject<CSvcTweak>(search, pList, L"Disable Windows Search Service", L"disable_search_service", WinVer_Win6,
        L"WSearch");
    MakeTweakObject<CExecTweak>(search, pList, L"Disable searchUI.exe", L"disable_searchui_exe", WinVer_Win10,
        L"c:\\windows\\SystemApps\\Microsoft.Windows.Cortana_cw5n1h2txyewy\\SearchUI.exe",
        ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(search, pList, L"Remove Search Box (User)", L"remove_search_box_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Search",
        L"SearchboxTaskbarMode",
        V(0));
    MakeTweakObject<CGpoTweak>(webSearch, pList, L"Don't Update Search Companion", L"disable_search_companion_update", WinVer_WinXPto7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\SearchCompanion",
        L"DisableContentFileUpdates",
        V(1));



    /*  
    *  #########################################
    *              Windows Defender
    *  #########################################
    */

    CTweakPtr defenderCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Windows Defender", L"microsoft_defender");


    // *** Disable Defender ***
    CTweakPtr defender = MakeTweakObject<CTweakSet>(defenderCat, pList, L"Disable Defender", L"disable_defender");
    MakeTweakObject<CGpoTweak>(defender, pList, L"Disable Real-Time Protection", L"disable_realtime_protection", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection",
        L"DisableRealtimeMonitoring",
        V(1));
    MakeTweakObject<CGpoTweak>(defender, pList, L"Disable Behavior Monitoring", L"disable_defender_realtime_protection", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection",
        L"DisableBehaviorMonitoring",
        V(1));
    MakeTweakObject<CGpoTweak>(defender, pList, L"Disable On-Access Protection", L"disable_defender_on_access_protection", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection",
        L"DisableOnAccessProtection",
        V(1));
    MakeTweakObject<CGpoTweak>(defender, pList, L"Disable Scan on Realtime Enable", L"disable_scan_on_realtime_enable", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection",
        L"DisableScanOnRealtimeEnable",
        V(1));
    MakeTweakObject<CGpoTweak>(defender, pList, L"Turn off Defender Antivirus", L"turn_off_deffender", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender",
        L"DisableAntiSpyware",
        V(1),
        ETweakHint::eNotRecommended); // Warming MsMpEng resets this
    // MakeTweakObject<CGpoTweak>(defender, pList, L"Turn off Anti Virus", WinVer_Win6, // legacy not needed
    //     L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender",
    //     L"DisableAntiVirus",
    //     V(1));
    MakeTweakObject<CGpoTweak>(defender, pList, L"Turn off Application Guard", L"turn_off_application_guard", WinVer_Win10EE,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\AppHVSI",
        L"AllowAppHVSI_ProviderSet",
        V(0));
    MakeTweakObject<CRegTweak>(defender, pList, L"Disable SecurityHealthService Service", L"disable_securityhealthservice", WinVer_Win1703,
        L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\SecurityHealthService",
        L"Start",
        V(4),
        ETweakHint::eNotRecommended);
    MakeTweakObject<CTaskTweak>(defender, pList, L"Disanle Defender Tasks", L"disable_defender_task", WinVer_Win7,
        L"\\Microsoft\\Windows\\Windows Defender",
        L"*");
        // L"Windows Defender Cache"
        // L"Windows Defender Cleanup"
        // L"Windows Defender Scheduled Scan"
        // L"Windows Defender Verfication"

    // *** Restrict Defender ***
    CTweakPtr defender3 = MakeTweakObject<CTweakSet>(defenderCat, pList, L"Restrict Defender", L"restrict_defender", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(defender3, pList, L"Prevent Defender from deleting files", L"disable_routinely_taking_action", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender",
        L"DisableRoutinelyTakingAction",
        V(1));

    // *** Silence Defender ***
    CTweakPtr defender2 = MakeTweakObject<CTweakSet>(defenderCat, pList, L"Silence Defender", L"silence_defender", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(defender2, pList, L"Disable Enhanced Notifications", L"disable_enhanced_notifications", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Reporting",
        L"DisableEnhancedNotifications",
        V(1));
    MakeTweakObject<CGpoTweak>(defender2, pList, L"Disable Spynet Reporting", L"disable_spynet_reporting", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Spynet",
        L"SpynetReporting",
        V(0));
    MakeTweakObject<CGpoTweak>(defender2, pList, L"Don't Submit Samples", L"dont_submit_samples", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Spynet",
        L"SubmitSamplesConsent",
        V(2));
    // Set GPO @"SOFTWARE\Policies\Microsoft\Windows Defender\Signature Updates" "DefinitionUpdateFileSharesSources" DELETE (nope)
    // Set GPO @"SOFTWARE\Policies\Microsoft\Windows Defender\Signature Updates" "FallbackOrder" "SZ:FileShares" (nope)
    MakeTweakObject<CRegTweak>(defender2, pList, L"Disable Defender Summary Notifications", L"disable_defender_summary_notify", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows Defender Security Center\\Virus and threat protection",
        L"SummaryNotificationDisabled",
        V(1));
    MakeTweakObject<CFwTweak>(defender2, pList, L"Firewall Malware Protection Command Line Utility", L"fw_block_mpcmdrun", WinVer_Win10,
        L"%ProgramFiles%\\windows defender\\mpcmdrun.exe");

    // *** Disable SmartScreen ***
    CTweakPtr screen = MakeTweakObject<CTweakSet>(defenderCat, pList, L"Disable SmartScreen", L"disable_smartscreen");
    MakeTweakObject<CGpoTweak>(screen, pList, L"Turn off SmartScreen", L"turn_off_smartscreen", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"EnableSmartScreen",
        V(0));
    // Set GPO @"Software\Policies\Microsoft\Windows\System" "ShellSmartScreenLevel" DELETE
    MakeTweakObject<CGpoTweak>(screen, pList, L"Disable App Install Control", L"disable_app_install_control", WinVer_Win1703,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\SmartScreen",
        L"ConfigureAppInstallControlEnabled",
        V(0));
    MakeTweakObject<CRegTweak>(screen, pList, L"Disable Explorer SmartScreen (Legacy)", L"disable_explorer_smartscreen_legacy", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer",
        L"SmartScreenEnabled",
        V(L"Off"));
    // Set GPO @"Software\Policies\Microsoft\Windows Defender\SmartScreen" "ConfigureAppInstallControl" DELETE
    // App Store
    MakeTweakObject<CRegTweak>(screen, pList, L"No SmartScreen for Store Apps", L"disable_store_smartscreen", WinVer_Win81,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppHost",
        L"EnableWebContentEvaluation",
        V(0));
    MakeTweakObject<CRegTweak>(screen, pList, L"No SmartScreen for Store Apps (User)", L"disable_store_smartscreen_user", WinVer_Win81,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppHost",
        L"EnableWebContentEvaluation",
        V(0));
    // Edge
    MakeTweakObject<CGpoTweak>(screen, pList, L"Disable SmartScreen for Edge", L"disable_edge_smartscreen", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\PhishingFilter",
        L"EnabledV9",
        V(0),
        ETweakHint::eNotRecommended);
    // Set GPO @"Software\Policies\Microsoft\MicrosoftEdge\PhishingFilter" "PreventOverride" "0"
    MakeTweakObject<CGpoTweak>(screen, pList, L"Disable Edge SmartScreen", L"edge_disable_smartscreen", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", 
        L"SmartScreenEnabled", 
        V(0),
        ETweakHint::eNotRecommended);
    //MakeTweakObject<CGpoTweak>(screen, pList, L"Disable Edge SmartScreen (User)", L"edge_disable_smartscreen_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", 
    //    L"SmartScreenEnabled", 
    //    V(0),
    //    ETweakHint::eNotRecommended);
    // IE
    MakeTweakObject<CGpoTweak>(screen, pList, L"Disable SmartScreen for IE", L"disable_ie_smartscreen", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\PhishingFilter",
        L"EnabledV9",
        V(0),
        ETweakHint::eNotRecommended);
    //
    MakeTweakObject<CExecTweak>(screen, pList, L"Disable smartscreen.exe", L"disable_smartscreen_exe", WinVer_Win6,
        L"%SystemRoot%\\System32\\smartscreen.exe",
        ETweakHint::eBreaking);


    // *** MRT-Tool ***
    CTweakPtr mrt = MakeTweakObject<CTweakSet>(defenderCat, pList, L"Silence MRT-Tool", L"silence_mrt_tool", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(mrt, pList, L"Don't Report Infections", L"dont_report_infections", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MRT",
        L"DontReportInfectionInformation",
        V(1));
    MakeTweakObject<CFwTweak>(mrt, pList, L"Firewall MRT-Tool", L"fw_block_mrt", WinVer_Win7,
        L"%SystemRoot%\\system32\\mrt.exe");
    MakeTweakObject<CGpoTweak>(mrt, pList, L"Disable MRT-Tool", L"disable_mrt_tool", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MRT",
        L"DontOfferThroughWUAU",
        V(1),
        ETweakHint::eNotRecommended);



    /*  
    *  #########################################
    *              Windows Update
    *  #########################################
    */
            
    CTweakPtr updateCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Windows Update", L"windows_update");


    // *** Disable Automatic Windows Update ***
    CTweakPtr noAutoUpdates = MakeTweakObject<CTweakSet>(updateCat, pList, L"Disable Automatic Windows Update", L"disable_auto_update");
    MakeTweakObject<CGpoTweak>(noAutoUpdates, pList, L"Disable Automatic Update", L"disable_automatic_update", WinVer_WinXPto8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU",
        L"NoAutoUpdate",
        V(1));
    MakeTweakObject<CGpoTweak>(noAutoUpdates, pList, L"Disable Automatic Update (Enterprise Only)", L"disable_automatic_update_ent", WinVer_Win10EE, // since 10 this tweak only works on enterprise and alike
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU",
        L"NoAutoUpdate",
        V(1));
    MakeTweakObject<CRegTweak>(noAutoUpdates, pList, L"Set Update UX Option", L"set_update_ux_option", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings",
        L"UxOption",
        V(1));
    MakeTweakObject<CRegTweak>(noAutoUpdates, pList, L"Defer Feature Upgrades", L"defer_feature_upgrades", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings",
        L"DeferUpgrade",
        V(1));
    MakeTweakObject<CSvcTweak>(noAutoUpdates, pList, L"Disable Edge Update Service", L"disable_edgeupdate_service", WinVer_Win10, 
        L"edgeupdate");
    MakeTweakObject<CSvcTweak>(noAutoUpdates, pList, L"Disable Edge Update Medium Service", L"disable_edgeupdatem_service", WinVer_Win10, 
        L"edgeupdatem");

    // *** Disable Driver Update ***
    CTweakPtr drv = MakeTweakObject<CTweakSet>(updateCat, pList, L"Disable Driver Update", L"disable_driver_update");
    MakeTweakObject<CGpoTweak>(drv, pList, L"Don't Update Drivers With", L"dont_update_drivers", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
        L"ExcludeWUDriversInQualityUpdate",
        V(1));
    MakeTweakObject<CGpoTweak>(drv, pList, L"Don't get Device Info from Web", L"dont_get_device_info_web", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Device Metadata",
        L"PreventDeviceMetadataFromNetwork",
        V(1),
        ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(drv, pList, L"Prevent Device Metadata Downloads", L"prevent_device_metadata_user", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Device Metadata",
        L"PreventDeviceMetadataFromNetwork",
        V(1),
        ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(drv, pList, L"Disable Online Driver Search", L"disable_online_driver_search", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\DriverSearching",
        L"SearchOrderConfig",
        V(0),
        ETweakHint::eNotRecommended);

    // *** Disable Windows Update Services ***
    CTweakPtr noUpdates = MakeTweakObject<CTweakSet>(updateCat, pList, L"Disable Windows Update Services", L"disable_wu_services");
    MakeTweakObject<CSvcTweak>(noUpdates, pList, L"Disable Windows Update Service", L"disable_wu_service", WinVer_WinXP,
        L"wuauserv");
    MakeTweakObject<CSvcTweak>(noUpdates, pList, L"Windows Update Medic Service", L"disable_wu_medic_service", WinVer_Win1803, // this one gives access denided!!!!
        L"WaaSMedicSvc", 
        ETweakHint::eNotRecommended);
    MakeTweakObject<CSvcTweak>(noUpdates, pList, L"Update Orchestrator Service", L"disable_update_orchestrator_service", WinVer_Win10,
        L"UsoSvc");
    MakeTweakObject<CExecTweak>(noUpdates, pList, L"Block upfc.exe (Updateability From SCM) Execution", L"block_upfc", WinVer_Win10,
        L"%SystemRoot%\\System32\\upfc.exe", 
        ETweakHint::eNotRecommended);
    MakeTweakObject<CExecTweak>(noUpdates, pList, L"Block SIH Client Execution", L"block_sih_client", WinVer_Win10,
        L"%SystemRoot%\\System32\\sihclient.exe", 
        ETweakHint::eNotRecommended);
    MakeTweakObject<CFwTweak>(noUpdates, pList, L"Firewall SIH Client", L"fw_block_sih_client", WinVer_Win10,
        L"%SystemRoot%\\system32\\sihclient.exe");

    // *** Disable Access to Update Servers ***
    CTweakPtr blockWU = MakeTweakObject<CTweakSet>(updateCat, pList, L"Disable Access to Update Servers", L"disable_wu_servers");
    MakeTweakObject<CGpoTweak>(blockWU, pList, L"DoNotConnectToWindowsUpdateInternetLocations", L"disable_wu_internet_locations", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
        L"DoNotConnectToWindowsUpdateInternetLocations",
        V(1));
    MakeTweakObject<CGpoTweak>(blockWU, pList, L"WUServer", L"set_wu_server_blank", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
        L"WUServer",
        V(L"\" \""));
    MakeTweakObject<CGpoTweak>(blockWU, pList, L"WUStatusServer", L"set_wu_statusserver_blank", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
        L"WUStatusServer",
        V(L"\" \""));
    MakeTweakObject<CGpoTweak>(blockWU, pList, L"UpdateServiceUrlAlternate", L"set_update_service_url_alternate_blank", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
        L"UpdateServiceUrlAlternate",
        V(L"\" \""));
    MakeTweakObject<CGpoTweak>(blockWU, pList, L"UseWUServer", L"use_wu_server", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU",
        L"UseWUServer",
        V(1));

    // *** Disable Windows Insider ***
    CTweakPtr noInsider = MakeTweakObject<CTweakSet>(updateCat, pList, L"Disable Windows Insider", L"disable_insider_services");
    // Disable Microsoft feature trials
    MakeTweakObject<CRegTweak>(noInsider, pList, L"Disable Windows Experimentation", L"disable_experimentation", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\PolicyManager\\current\\device\\System",
        L"AllowExperimentation",
        V(0));
    MakeTweakObject<CGpoTweak>(noInsider, pList, L"Disable Feature Experimentation", L"disable_feature_experimentation", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\PreviewBuilds",
        L"EnableExperimentation",
        V(0));
    MakeTweakObject<CGpoTweak>(noInsider, pList, L"Disable Config Flighting", L"disable_config_flighting", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\PreviewBuilds",
        L"EnableConfigFlighting",
        V(0));
    // Disable receipt of preview builds
    MakeTweakObject<CGpoTweak>(noInsider, pList, L"Disable Preview Build Reception", L"disable_preview_build_reception", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\PreviewBuilds",
        L"AllowBuildPreview",
        V(0));
    // Remove "Windows Insider Program" from Settings
    MakeTweakObject<CRegTweak>(noInsider, pList, L"Hide Insider Program Page", L"hide_insider_page", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\WindowsSelfHost\\UI\\Visibility",
        L"HideInsiderPage",
        V(1));
    // Disable "Windows Insider Service"
    MakeTweakObject<CSvcTweak>(noInsider, pList, L"Disable Windows Insider Service", L"disable_insider_service", WinVer_Win10,
        L"wisvc");



    /*  
    *  #########################################
    *          Privacy & Advertisement
    *  #########################################
    */

    CTweakPtr privacyCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Privacy & Advertisement", L"windows_privacy");


    // *** Disable Advertisement ***
    CTweakPtr privacy = MakeTweakObject<CTweakSet>(privacyCat, pList, L"Disable Advertisement", L"disable_advertisement", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(privacy, pList, L"Turn off Advertising ID", L"turn_off_advertising_id", WinVer_Win81,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AdvertisingInfo",
        L"DisabledByGroupPolicy",
        V(1));
    MakeTweakObject<CGpoTweak>(privacy, pList, L"Turn off Consumer Experiences", L"turn_off_consumer_experiences", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent",
        L"DisableWindowsConsumerFeatures",
        V(1));
    MakeTweakObject<CGpoTweak>(privacy, pList, L"Disable Tailored Experiences", L"disable_tailored_experiences", WinVer_Win1703,
        L"HKLM\\Software\\Policies\\Microsoft\\Windows\\CloudContent",
        L"DisableTailoredExperiencesWithDiagnosticData",
        V(1));
    //MakeTweakObject<CGpoTweak>(privacy, pList, L"Disable Tailored Experiences (User)", L"disable_tailored_experiences_user", WinVer_Win1703,
    //    L"HKCU\\Software\\Policies\\Microsoft\\Windows\\CloudContent",
    //    L"DisableTailoredExperiencesWithDiagnosticData",
    //    V(1));
    MakeTweakObject<CRegTweak>(privacy, pList, L"Limit Tailored Experiences", L"limit_tailored_experiences", WinVer_Win1703,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Privacy",
        L"TailoredExperiencesWithDiagnosticDataEnabled",
        V(0));
    MakeTweakObject<CRegTweak>(privacy, pList, L"Limit Tailored Experiences (User)", L"limit_tailored_experiences_user", WinVer_Win1703,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Privacy",
        L"TailoredExperiencesWithDiagnosticDataEnabled",
        V(0));

    MakeTweakObject<CGpoTweak>(privacy, pList, L"Turn off Windows Spotlight", L"turn_off_windows_spotlight", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent",
        L"DisableWindowsSpotlightFeatures",
        V(1));
    //MakeTweakObject<CGpoTweak>(privacy, pList, L"Turn off Windows Spotlight (User)", L"turn_off_windows_spotlight_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent",
    //    L"DisableWindowsSpotlightFeatures",
    //    V(1));
    MakeTweakObject<CGpoTweak>(privacy, pList, L"Disable OnlineTips in Settings", L"disable_online_tips", WinVer_Win1709,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
        L"AllowOnlineTips",
        V(0));
    MakeTweakObject<CGpoTweak>(privacy, pList, L"Don't show Windows Tips", L"dont_show_windows_tips", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent",
        L"DisableSoftLanding",
        V(1));

    MakeTweakObject<CGpoTweak>(privacy, pList, L"Disable Consumer Account State Content", L"disable_consumer_account_state", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent", 
        L"DisableConsumerAccountStateContent", 
        V(1));
    MakeTweakObject<CGpoTweak>(privacy, pList, L"Disable Cloud Optimized Content", L"disable_cloud_optimized_content", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent", 
        L"DisableCloudOptimizedContent", 
        V(1));

    MakeTweakObject<CGpoTweak>(privacy, pList, L"Disable Windows Feeds", L"disable_windows_feeds", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Feeds", 
        L"EnableFeeds", 
        V(0));
    MakeTweakObject<CFwTweak>(privacy, pList, L"Firewall Feeds Synchronization", L"fw_block_msfeedssync", WinVer_Win10,
        L"%SystemRoot%\\system32\\msfeedssync.exe");

    MakeTweakObject<CRegTweak>(privacy, pList, L"Disable StartMenu Ad's (User)", L"disable_suggestions_338388_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"SubscribedContent-338388Enabled",
        V(0));
    MakeTweakObject<CRegTweak>(privacy, pList, L"Disable System Pane Suggestions (User)", L"disable_system_pane_suggestions_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"SystemPaneSuggestionsEnabled",
        V(0));
    MakeTweakObject<CRegTweak>(privacy, pList, L"Disable Soft Landing (User)", L"disable_soft_landing_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"SoftLandingEnabled",
        V(0));
    MakeTweakObject<CRegTweak>(privacy, pList, L"Disable LockScreen Ad's (User)", L"disable_suggestions_338389_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"SubscribedContent-338389Enabled",
        V(0));
    MakeTweakObject<CRegTweak>(privacy, pList, L"Disable Settings App Ad's (User)", L"disable_suggestions_353696_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"SubscribedContent-353696Enabled",
        V(0));
    MakeTweakObject<CRegTweak>(privacy, pList, L"Disable Suggested Content 338393 (User)", L"disable_suggestions_338393_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"SubscribedContent-338393Enabled",
        V(0));
    MakeTweakObject<CRegTweak>(privacy, pList, L"Disable Suggested Content 338394 (User)", L"disable_suggestions_338394_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"SubscribedContent-338394Enabled",
        V(0));
    MakeTweakObject<CRegTweak>(privacy, pList, L"Disable Suggested Content 338396 (User)", L"disable_suggestions_338396_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"SubscribedContent-338396Enabled",
        V(0));
    MakeTweakObject<CRegTweak>(privacy, pList, L"Disable Sync Provider Notifications in File Explorer (User)", L"disable_sync_provider_notifications_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        L"ShowSyncProviderNotifications",
        V(0));

    // *** Disable Unsolicited App Installation ***
    CTweakPtr delivery = MakeTweakObject<CTweakSet>(privacyCat, pList, L"Disable Unsolicited App Installation", L"disable_unsolicited_apps", ETweakHint::eRecommended);
    MakeTweakObject<CRegTweak>(delivery, pList, L"ContentDeliveryAllowed", L"content_delivery_allowed", WinVer_Win1607,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"ContentDeliveryAllowed",
        V(0));
    MakeTweakObject<CRegTweak>(delivery, pList, L"OemPreInstalledAppsEnabled", L"oem_preinstalled_apps_enabled", WinVer_Win1607,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"OemPreInstalledAppsEnabled",
        V(0));
    MakeTweakObject<CRegTweak>(delivery, pList, L"PreInstalledAppsEnabled", L"preinstalled_apps_enabled", WinVer_Win1607,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"PreInstalledAppsEnabled",
        V(0));
    // MakeTweakObject<CRegTweak>(delivery, pList, L"PreInstalledAppsEverEnabled (User)", WinVer_Win1607, ...
    //    L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
    //    L"PreInstalledAppsEverEnabled",
    //    V(0));
    MakeTweakObject<CRegTweak>(delivery, pList, L"Disable Silent App Installations", L"silent_installed_apps_enabled", WinVer_Win1607,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"SilentInstalledAppsEnabled",
        V(0));
    MakeTweakObject<CRegTweak>(delivery, pList, L"Disable Silent App Installations (32-bit)", L"silent_installed_apps_enabled_32", WinVer_Win10,
        L"HKLM\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"SilentInstalledAppsEnabled",
        V(0));
    MakeTweakObject<CRegTweak>(delivery, pList, L"Disable Silent App Installations (User)", L"silent_installed_apps_enabled_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"SilentInstalledAppsEnabled",
        V(0));
    //MakeTweakObject<CRegTweak>(delivery, pList, L"Disable Soft Landing (User)", L"soft_landing_enabled_user", WinVer_Win1607,
    //    L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
    //    L"SoftLandingEnabled",
    //    V(0));
    // MakeTweakObject<CRegTweak>(delivery, pList, L"SubscribedContentEnabled (User)", WinVer_Win1607, ...
    //    L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
    //    L"SubscribedContentEnabled",
    //    V(0));

    // *** No Lock Screen ***
    CTweakPtr lockScr = MakeTweakObject<CTweakSet>(privacyCat, pList, L"Disable Lock Screen", L"disable_lock_screen", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(lockScr, pList, L"Don't use Lock Screen", L"dont_use_lock_screen", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Personalization",
        L"NoLockScreen",
        V(1));
    MakeTweakObject<CGpoTweak>(privacyCat, pList, L"Disable Camera on Lock Screen", L"disable_lock_screen_camera", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Personalization", 
        L"NoLockScreenCamera", 
        V(1));
    MakeTweakObject<CGpoTweak>(lockScr, pList, L"Enable LockScreen Image", L"enable_lockscreen_image", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Personalization",
        L"LockScreenOverlaysDisabled",
        V(1));
    MakeTweakObject<CGpoTweak>(lockScr, pList, L"Set LockScreen Image", L"set_lockscreen_image", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Personalization",
        L"LockScreenImage",
        V(L"C:\\windows\\web\\screen\\lockscreen.jpg"));
    MakeTweakObject<CFwTweak>(lockScr, pList, L"Firewall LockApp", L"fw_block_lockapp", WinVer_Win10,
        L"%SystemRoot%\\systemapps\\Microsoft.LockApp_cw5n1h2txyewy\\LockApp.exe");

    // *** No Parental Controlls ***
    CTweakPtr noPar = MakeTweakObject<CTweakSet>(privacyCat, pList, L"Disable Parental Controls", L"disable_parental_controls");
    MakeTweakObject<CGpoTweak>(noPar, pList, L"Disable Parental Controls Feature", L"disable_wpc", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\ParentalControls",
        L"ParentalControlsEnabled",
        V(0));
    MakeTweakObject<CSvcTweak>(noPar, pList, L"Disable Family Safety Monitor Service", L"disable_wpc_monitor", WinVer_Win10,
        L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\WpcMonSvc");
    MakeTweakObject<CRegTweak>(noPar, pList, L"Disable ParentalControls App", L"disable_wpc_app", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Parental Controls",
        L"Enabled",
        V(0));
    MakeTweakObject<CFwTweak>(noPar, pList, L"Firewall Parental Controls", L"fw_block_wpc_app", WinVer_Win10,
        L"%SystemRoot%\\systemapps\\ParentalControls_cw5n1h2txyewy\\WpcUapApp.exe");
    MakeTweakObject<CTaskTweak>(noPar, pList, L"Disanle Family Safety Monitor Tasks", L"disable_wpc_monitor_task", WinVer_Win10,
        L"\\Microsoft\\Windows\\Shell",
        L"FamilySafetyMonitor");
    MakeTweakObject<CTaskTweak>(noPar, pList, L"Disanle Family Safety Refresh Task Tasks", L"disable_wpc_refresh_task", WinVer_Win10,
        L"\\Microsoft\\Windows\\Shell",
        L"FamilySafetyRefreshTask");

    // *** No Personalization ***
    CTweakPtr spying = MakeTweakObject<CTweakSet>(privacyCat, pList, L"No Personalization", L"disable_personalization", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(spying, pList, L"Disable input personalization", L"disable_input_personalization", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\InputPersonalization",
        L"AllowInputPersonalization",
        V(0));
    MakeTweakObject<CGpoTweak>(spying, pList, L"Disable Test Collection", L"disable_test_collection", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\InputPersonalization",
        L"RestrictImplicitTextCollection",
        V(1));
    MakeTweakObject<CRegTweak>(spying, pList, L"Disable Test Collection (User)", L"disable_test_collection_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\InputPersonalization",
        L"RestrictImplicitTextCollection",
        V(1));
    MakeTweakObject<CRegTweak>(spying, pList, L"Disable Contact Harvesting (User)", L"disable_contact_harvesting_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\InputPersonalization\\TrainedDataStore",
        L"HarvestContacts",
        V(0));
    MakeTweakObject<CRegTweak>(spying, pList, L"Disable Touch Keyboard Text Prediction (User)", L"disable_touch_kb_prediction_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\TabletTip\\1.7",
        L"EnableTextPrediction",
        V(0), 
        ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(spying, pList, L"Disable Inc Collection", L"disable_inc_collection", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\InputPersonalization",
        L"RestrictImplicitInkCollection",
        V(1));
    //MakeTweakObject<CGpoTweak>(spying, pList, L"Disable Inc Collection (User)", L"disable_inc_collection_user", WinVer_Win6,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\InputPersonalization",
    //    L"RestrictImplicitInkCollection",
    //    V(1));
    MakeTweakObject<CGpoTweak>(spying, pList, L"Disable Linguistic Data Collection", L"disable_linguistic_data_collection", WinVer_Win1803,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\TextInput",
        L"AllowLinguisticDataCollection",
        V(0));
    MakeTweakObject<CGpoTweak>(spying, pList, L"Disable Handwriting Error Reports", L"disable_handwriting_error_reports", WinVer_Win6to7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\HandwritingErrorReport",
        L"PreventHandwritingErrorReports",
        V(1));
    MakeTweakObject<CGpoTweak>(spying, pList, L"Disable Handwriting Data Sharing", L"disable_handwriting_data_sharing", WinVer_Win6to7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\TabletPC",
        L"PreventHandwritingDataSharing",
        V(1));
    // Set USER GPO @"Software\Policies\Microsoft\Windows\HandwritingErrorReports" "PreventHandwritingErrorReports" "1" (deprecated)
    // Set USER GPO @"Software\Policies\Microsoft\Windows\TabletPC" "PreventHandwritingDataSharing" "1" (deprecated)	
    MakeTweakObject<CRegTweak>(spying, pList, L"Disable Privacy Policy Acceptance (User)", L"disable_privacy_policy_acceptance_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Personalization\\Settings",
        L"AcceptedPrivacyPolicy",
        V(0));
    MakeTweakObject<CRegTweak>(spying, pList, L"Restrict Implicit Ink Collection (User)", L"restrict_implicit_ink_collection_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Personalization\\Settings",
        L"RestrictImplicitInkCollection",
        V(1));

    // *** Disable Location ***
    CTweakPtr location = MakeTweakObject<CTweakSet>(privacyCat, pList, L"Protect Location", L"protect_location");
    MakeTweakObject<CGpoTweak>(location, pList, L"Disable Location Provider", L"disable_location", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\LocationAndSensors",
        L"DisableLocation",
        V(1));
    MakeTweakObject<CGpoTweak>(location, pList, L"Disable Location Sensors", L"disable_location_sensors", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\LocationAndSensors",
        L"DisableSensors",
        V(1));
    MakeTweakObject<CGpoTweak>(location, pList, L"Disable Location Scripting", L"disable_location_scripting", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\LocationAndSensors",
        L"DisableLocationScripting",
        V(1));
    MakeTweakObject<CGpoTweak>(location, pList, L"Disable Windows Location Provider", L"disable_location_provider", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\LocationAndSensors",
        L"DisableWindowsLocationProvider",
        V(1));
    //MakeTweakObject<CGpoTweak>(location, pList, L"Don't Share Lang List", L"dont_share_language_list_user", WinVer_Win10,
    //    L"HKCU\\Control Panel\\International\\User Profile",
    //    L"HttpAcceptLanguageOptOut",
    //    V(1), 
    //    ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(location, pList, L"Disable Location Service (User)", L"disable_location_service_user",
        WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\DeviceAccess\\Global\\{BFA794E4-F964-4FDB-90F6-51056BFE4B44}", L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(location, pList, L"Disable Sensor Permission for Location (User)", L"disable_location_sensor_permission_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Sensor\\Permissions\\{BFA794E4-F964-4FDB-90F6-51056BFE4B44}",
        L"SensorPermissionState",
        V(0));
    MakeTweakObject<CSvcTweak>(location, pList, L"Disable Location Service", L"disable_lfsvc_service", WinVer_Win10, 
        L"lfsvc");

    // *** No Registration ***
    CTweakPtr privOther = MakeTweakObject<CTweakSet>(privacyCat, pList, L"No Registration", L"disable_registration", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(privOther, pList, L"Disable KMS GenTicket", L"disable_kms_genticket", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows NT\\CurrentVersion\\Software Protection Platform",
        L"NoGenTicket",
        V(1));
    MakeTweakObject<CGpoTweak>(privOther, pList, L"Disable Registration", L"disable_registration_wizard", WinVer_WinXP,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Registration Wizard Control",
        L"NoRegistration",
        V(1));
    MakeTweakObject<CGpoTweak>(privOther, pList, L"Disable Windows Media DRM Online Access", L"disable_wmdrm_online", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\WMDRM",
        L"DisableOnline",
        V(1), 
        ETweakHint::eNotRecommended);
    MakeTweakObject<CSvcTweak>(privOther, pList, L"Disable LicenseManager Service", L"disable_licensemanager_service", WinVer_Win10,
        L"LicenseManager",
        ETweakHint::eNotRecommended);
    MakeTweakObject<CFwTweak>(privOther, pList, L"Firewall Windows Activation Client", L"fw_sliu_host", WinVer_Win10,
        L"%SystemRoot%\\system32\\slui.exe",
        L"", ETweakHint::eNotRecommended);

    // *** No Push Notifications ***
    //CTweakPtr push = MakeTweakObject<CTweakSet>(privacyCat, pList, L"No Push Notifications", L"disable_push_notifications", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(privacyCat, pList, L"Disable Cloud Notification", L"disable_cloud_notification", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\PushNotifications",
        L"NoCloudApplicationNotification",
        V(1));
    //MakeTweakObject<CGpoTweak>(push, pList, L"Disable Cloud Notification (User)", L"disable_cloud_notification_user", WinVer_Win8,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\PushNotifications",
    //    L"NoCloudApplicationNotification",
    //    V(1));

    // *** OOBE ***
    CTweakPtr oobe = MakeTweakObject<CTweakSet>(privacyCat, pList, L"OOBE Tweaks", L"oobe_tweaks", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(oobe, pList, L"Disable Privacy Experience on OOBE", L"disable_oobe_privacy_experience", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\OOBE", 
        L"DisablePrivacyExperience", 
        V(1));

	MakeTweakObject<CRegTweak>(oobe, pList, L"Disable Privacy Experience on OOBE (User)", L"disable_oobe_privacy_experience_user", WinVer_Win10,
		L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\OOBE",
		L"DisablePrivacyExperience",
		V(1),
        ETweakHint::eNotRecommended);
	MakeTweakObject<CRegTweak>(oobe, pList, L"Disable OOBE (User)", L"disable_oobe_user", WinVer_Win10,
		L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\OOBE",
		L"DisableOOBE",
		V(1),
        ETweakHint::eNotRecommended);
	MakeTweakObject<CRegTweak>(oobe, pList, L"Skip User OOBE (User)", L"skipuser_oobe_user", WinVer_Win10,
		L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\OOBE",
		L"SkipUserOOBE",
		V(1),
        ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(oobe, pList, L"Disable First Logon Animation", L"disable_first_logon_animation", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
        L"EnableFirstLogonAnimation",
        V(0));

    // second-chance out of box experience (SCOOBE)
    MakeTweakObject<CRegTweak>(oobe, pList, L"Disable Scoobe System Setting (User)", L"disable_scoobe_system_setting_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\UserProfileEngagement",
        L"ScoobeSystemSettingEnabled", V(0));  // Disable Scoobe System Setting

    MakeTweakObject<CSvcTweak>(oobe, pList, L"Disable Retail Demo Service", L"disable_retaildemo_service", WinVer_Win10, 
        L"RetailDemo");
    MakeTweakObject<CFwTweak>(oobe, pList, L"Firewall Assigned Access LockApp", L"fw_block_assignedaccesslockapp", WinVer_Win10,
        L"%SystemRoot%\\systemapps\\Microsoft.Windows.AssignedAccessLockApp_cw5n1h2txyewy\\AssignedAccessLockApp.exe");


    // *** Biometrics ***
    CTweakPtr biometrics = MakeTweakObject<CTweakSet>(privacyCat, pList, L"Disable Biometrics", L"no_biometrics");
    MakeTweakObject<CGpoTweak>(biometrics, pList, L"Disable Biometrics", L"disable_biometrics", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Biometrics",
        L"Enabled",
        V(0));
    MakeTweakObject<CSvcTweak>(biometrics, pList, L"Disable Biometric Service", L"disable_wbiosrvc_service", WinVer_Win10, 
        L"WbioSrvc");
    MakeTweakObject<CFwTweak>(biometrics, pList, L"Firewall Biometry Enrollment", L"fw_block_bioenrollmenthost", WinVer_Win10,
        L"%SystemRoot%\\systemapps\\Microsoft.BioEnrollment_cw5n1h2txyewy\\BioEnrollmentHost.exe");


    /*  
    *  #########################################
    *          Microsoft Account
    *  #########################################
    */

    CTweakPtr accountCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Microsoft Account", L"microsoft_account");


    // *** Disable OneDrive ***
    CTweakPtr onedrive = MakeTweakObject<CTweakSet>(accountCat, pList, L"Disable OneDrive", L"disable_onedrive");
    MakeTweakObject<CGpoTweak>(onedrive, pList, L"Do not default to OneDrive for library saves", L"disable_onedrive_default_save", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\OneDrive", 
        L"DisableLibrariesDefaultSaveToOneDrive", 
        V(1));
    MakeTweakObject<CGpoTweak>(onedrive, pList, L"Disable OneDrive file sync", L"disable_onedrive_filesync", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\OneDrive", 
        L"DisableFileSync", 
        V(1));
    MakeTweakObject<CGpoTweak>(onedrive, pList, L"Disable OneDrive Usage", L"disable_onedrive_usage", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\OneDrive",
        L"DisableFileSyncNGSC",
        V(1));
    MakeTweakObject<CGpoTweak>(onedrive, pList, L"Silence OneDrive", L"silence_onedrive", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\OneDrive",
        L"PreventNetworkTrafficPreUserSignIn",
        V(1));
    MakeTweakObject<CGpoTweak>(onedrive, pList, L"Silence OneDrive", L"disable_onedrive_metered_sync", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\OneDrive",
        L"DisableMeteredNetworkFileSync",
        V(0));
    // Run uninstaller
    
    // *** No Microsoft Accounts ***
    CTweakPtr account = MakeTweakObject<CTweakSet>(accountCat, pList, L"No Microsoft Accounts", L"disable_microsoft_accounts");
    MakeTweakObject<CGpoTweak>(account, pList, L"Disable Microsoft Accounts", L"disable_microsoft_accounts_policy", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
        L"NoConnectedUser",
        V(3));
    MakeTweakObject<CSvcTweak>(account, pList, L"Disable MS Account Login Service", L"disable_ms_account_service", WinVer_Win8,
        L"wlidsvc",
        ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(account, pList, L"Disable OneDrive Personal Sync (User)", L"disable_onedrive_personal_sync_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\OneDrive",
        L"DisablePersonalSync",
        V(1));
    MakeTweakObject<CFwTweak>(account, pList, L"Firewall AAD Broker Plugin", L"fw_block_aad_brokerplugin", WinVer_Win10,
        L"%SystemRoot%\\systemapps\\Microsoft.AAD.BrokerPlugin_cw5n1h2txyewy\\Microsoft.AAD.BrokerPlugin.exe", 
        L"", ETweakHint::eNotRecommended);
    MakeTweakObject<CFwTweak>(account, pList, L"Firewall Accounts Control Host", L"fw_block_accountscontrolhost", WinVer_Win10,
        L"%SystemRoot%\\systemapps\\Microsoft.AccountsControl_cw5n1h2txyewy\\AccountsControlHost.exe", 
        L"", ETweakHint::eNotRecommended);

    // *** No Settings Sync ***
    CTweakPtr sync = MakeTweakObject<CTweakSet>(accountCat, pList, L"No Settings Sync", L"disable_settings_sync", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(sync, pList, L"Disable Settings Sync", L"disable_settings_sync_policy", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\SettingSync",
        L"DisableSettingSync",
        V(2));
    MakeTweakObject<CGpoTweak>(sync, pList, L"Force Disable Settings Sync", L"force_disable_settings_sync", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\SettingSync",
        L"DisableSettingSyncUserOverride",
        V(1));
    MakeTweakObject<CGpoTweak>(sync, pList, L"Disable WiFi-Sense", L"disable_wifi_sense", WinVer_Win10to1709,
        L"HKLM\\SOFTWARE\\Microsoft\\wcmsvc\\wifinetworkmanager\\config",
        L"AutoConnectAllowedOEM",
        V(0));
    MakeTweakObject<CRegTweak>(sync, pList, L"Disable Settings Sync Policy (User)", L"disable_settings_sync_policy_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\SettingSync",
        L"SyncPolicy",
        V(5));
    MakeTweakObject<CRegTweak>(sync, pList, L"Disable Sync: Accessibility Settings (User)", L"disable_sync_accessibility_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\SettingSync\\Groups\\Accessibility",
        L"Enabled",
        V(0));
    MakeTweakObject<CRegTweak>(sync, pList, L"Disable Sync: Browser Settings (User)", L"disable_sync_browser_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\SettingSync\\Groups\\BrowserSettings",
        L"Enabled",
        V(0));
    MakeTweakObject<CRegTweak>(sync, pList, L"Disable Sync: Credentials (User)", L"disable_sync_credentials_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\SettingSync\\Groups\\Credentials",
        L"Enabled",
        V(0));
    MakeTweakObject<CRegTweak>(sync, pList, L"Disable Sync: Language Settings (User)", L"disable_sync_language_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\SettingSync\\Groups\\Language",
        L"Enabled",
        V(0));
    MakeTweakObject<CRegTweak>(sync, pList, L"Disable Sync: Personalization Settings (User)", L"disable_sync_personalization_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\SettingSync\\Groups\\Personalization",
        L"Enabled",
        V(0));
    MakeTweakObject<CRegTweak>(sync, pList, L"Disable Sync: Windows Settings (User)", L"disable_sync_windows_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\SettingSync\\Groups\\Windows",
        L"Enabled",
        V(0));

    // *** No Find My Device ***
    MakeTweakObject<CGpoTweak>(accountCat, pList, L"Don't Allow FindMyDevice", L"dont_allow_findmydevice", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\FindMyDevice",
        L"AllowFindMyDevice",
        V(0));

    // *** No Cloud Clipboard ***
    MakeTweakObject<CGpoTweak>(accountCat, pList, L"Disable Cloud Clipboard", L"disable_cloud_clipboard_policy", WinVer_Win1809,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"AllowCrossDeviceClipboard",
        V(0));

    // *** No Cloud Messages ***
    //CTweakPtr msgbak = MakeTweakObject<CTweakSet>(accountCat, pList, L"No Cloud Messages", L"disable_cloud_messages", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(accountCat, pList, L"Don't Sync Messages", L"dont_sync_messages", WinVer_Win1709,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Messaging",
        L"AllowMessageSync",
        V(0));
    //MakeTweakObject<CGpoTweak>(msgbak, pList, L"Don't Sync Messages (User)", L"dont_sync_messages_user", WinVer_Win1709,
    //    L"HKCU\\SOFTWARE\\Microsoft\\Messaging",
    //    L"CloudServiceSyncEnabled",
    //    V(0));

    // *** Disable Activity Feed ***
    CTweakPtr feed = MakeTweakObject<CTweakSet>(accountCat, pList, L"Disable User Tracking", L"disable_activity_feed", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(feed, pList, L"Disable Activity Feed", L"disable_activity_feed_policy", WinVer_Win1709,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"EnableActivityFeed",
        V(0));
    MakeTweakObject<CGpoTweak>(feed, pList, L"Don't Upload User Activities", L"dont_upload_user_activities", WinVer_Win1709,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"UploadUserActivities",
        V(0));
    MakeTweakObject<CGpoTweak>(feed, pList, L"Don't Publish User Activities", L"dont_publish_user_activities", WinVer_Win1709,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"PublishUserActivities",
        V(0));


    // *** No Cross Device Experience ***
    CTweakPtr cdp = MakeTweakObject<CTweakSet>(accountCat, pList, L"No Cross Device Experience", L"disable_cross_device", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(cdp, pList, L"Disable Cross Device Experience", L"disable_cross_device_experience", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"EnableCdp",
        V(0));
    MakeTweakObject<CRegTweak>(cdp, pList, L"Disable Phone Link Feature (User)", L"disable_phone_link_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Mobility",
        L"PhoneLinkEnabled",
        V(0));
    MakeTweakObject<CRegTweak>(cdp, pList, L"Disable Cross-Device Features (User)", L"disable_cross_device_features_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Mobility",
        L"CrossDeviceEnabled",
        V(0));
    MakeTweakObject<CRegTweak>(cdp, pList, L"Opt-Out of Cross-Device Features (User)", L"optout_cross_device_features_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Mobility",
        L"OptedIn",
        V(0));


    /*  
    *  #########################################
    *               Explorer Tweaks
    *  #########################################
    */

    CTweakPtr explorer = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Explorer Tweaks", L"explorer");

    MakeTweakObject<CGpoTweak>(explorer, pList, L"Disable Shell Instrumentation", L"disable_shell_instrumentation", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
        L"NoInstrumentation",
        V(1), 
        ETweakHint::eNotRecommended);

    //CTweakPtr hideMeet = MakeTweakObject<CTweakSet>(explorer, pList, L"Hide Meet Now Button", L"hide_meet_now_button");
    MakeTweakObject<CGpoTweak>(explorer, pList, L"Hide Meet Now Button (Machine)", L"hide_meet_now_button_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
        L"HideSCAMeetNow",
        V(1));
    //MakeTweakObject<CGpoTweak>(hideMeet, pList, L"Hide Meet Now Button (User)", L"hide_meet_now_button_user", WinVer_Win10,
    //    L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
    //    L"HideSCAMeetNow", 
    //    V(1));

    CTweakPtr noRecent = MakeTweakObject<CTweakSet>(explorer, pList, L"Disable collection of recently used items", L"explorer_no_recent", ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(noRecent, pList, L"Disable Frequent Folders in File Explorer (User)", L"disable_frequent_folders_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer",
        L"ShowFrequent",
        V(0));
    MakeTweakObject<CRegTweak>(noRecent, pList, L"Disable Recent Folders in File Explorer (User)", L"disable_recent_folders_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer",
        L"ShowRecent",
        V(0));
    MakeTweakObject<CRegTweak>(noRecent, pList, L"Disable Start Menu Recent Documents Tracking (User)", L"disable_start_recent_docs_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        L"Start_TrackDocs",
        V(0));
    MakeTweakObject<CRegTweak>(noRecent, pList, L"Disable Tracking of Programs in Start Menu", L"disable_tracking_start_progs_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        L"Start_TrackProgs",
        V(0));
    //MakeTweakObject<CRegTweak>(noRecent, pList, L"Clear Quick Access History (User)", L"clear_quick_access_history_link_user", WinVer_Win10,
    //    L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer",
    //    L"link",
    //    V(std::vector<byte>(4)));
    MakeTweakObject<CRegTweak>(noRecent, pList, L"Disable Taskbar Jump List in File Explorer (User)", L"disable_taskbar_jump_list_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        L"TaskbarMn",
        V(0));

    MakeTweakObject<CRegTweak>(explorer, pList, L"Set File Explorer to Open This PC by Default (User)", L"set_explorer_launch_to_this_pc_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        L"LaunchTo",
        V(1), 
        ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(explorer, pList, L"Enable Browsing in New Process (User)", L"enable_browsing_new_process_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\BrowseNewProcess",
        L"BrowseNewProcess",
        V(L"yes"), 
        ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(explorer, pList, L"Show Hidden Files (User)", L"show_hidden_files_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        L"Hidden",
        V(1), // 1 = Show hidden files
        ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(explorer, pList, L"Show Protected System Files (User)", L"show_protected_system_files_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        L"ShowSuperHidden",
        V(1), // 1 = Show protected system files
        ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(explorer, pList, L"Show File Extensions (User)", L"show_file_extensions_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        L"HideFileExt",
        V(0), // 0 = Show file extensions
        ETweakHint::eRecommended);


    // *** No Explorer Thumbnails ***
    CTweakPtr noThumbnails = MakeTweakObject<CTweakSet>(explorer, pList, L"Disable Thumbs.db", L"explorer_thumbs_db", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(noThumbnails, pList, L"Disable Thumbs.db on Network Folders", L"disable_thumbsdb_network", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Explorer", 
        L"DisableThumbsDBOnNetworkFolders", 
        V(1));
    MakeTweakObject<CGpoTweak>(noThumbnails, pList, L"Disable Thumbnail Cache Creation", L"disable_thumbnail_cache_creation", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
        L"NoThumbnailCache",
        V(1));
    MakeTweakObject<CGpoTweak>(noThumbnails, pList, L"Disable Thumbnail Cache Usage", L"disable_thumbnail_cache_usage", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
        L"DisableThumbnailCache",
        V(1));
    MakeTweakObject<CRegTweak>(noThumbnails, pList, L"Disable Thumbnail Cache in File Explorer (User)", L"no_thumbnail_cache_in_explorer_user", WinVer_Win7,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        L"NoThumbnailCache",
        V(1));
    MakeTweakObject<CRegTweak>(noThumbnails, pList, L"Disable Thumbnail Cache (Alternative) in File Explorer (User)", L"disable_thumbnail_cache_in_explorer_user", WinVer_Win7,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        L"DisableThumbnailCache",
        V(1));
    MakeTweakObject<CRegTweak>(noThumbnails, pList, L"Enable Icons Only Mode in File Explorer", L"enable_icons_only_mode_in_explorer_user", WinVer_Win7,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
        L"IconsOnly",
        V(1),
        ETweakHint::eNotRecommended);


    /*  
    *  #########################################
    *               Various Others
    *  #########################################
    */

    CTweakPtr miscCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Various Others", L"windows_misc");

    // *** No Explorer AutoComplete ***
    CTweakPtr ac = MakeTweakObject<CTweakSet>(miscCat, pList, L"No Explorer AutoComplete", L"disable_autocomplete", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(ac, pList, L"Disable Auto Suggest", L"disable_auto_suggest", WinVer_Win2k,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoComplete",
        L"AutoSuggest",
        V(L"no"));
    MakeTweakObject<CRegTweak>(ac, pList, L"Disable AutoSuggest in File Explorer", L"disable_auto_suggest_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoComplete",
        L"AutoSuggest",
        V(L"no"));

    CTweakPtr toast = MakeTweakObject<CTweakSet>(miscCat, pList, L"Notifications", L"notifications", ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(toast, pList, L"Disable Toast Notifications (User)", L"disable_toast_notifications_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\PushNotifications",
        L"ToastEnabled",
        V(0));
    MakeTweakObject<CRegTweak>(toast, pList, L"Disable Toast Notifications on Lock Screen (User)", L"disable_toasts_lock_screen_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Notifications\\Settings",
        L"NOC_GLOBAL_SETTING_ALLOW_TOASTS_ABOVE_LOCK",
        V(0));

    // *** No Maintenance ***
    CTweakPtr maintenance = MakeTweakObject<CTweakSet>(miscCat, pList, L"Disable Maintenance", L"disable_maintenance", ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(maintenance, pList, L"Disable Scheduled Maintenance", L"disable_scheduled_maintenance", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Schedule\\Maintenance",
        L"MaintenanceDisabled",
        V(1));
    MakeTweakObject<CRegTweak>(maintenance, pList, L"Disable Scheduled Maintenance (WOW6432)", L"disable_scheduled_maintenance_wow6432", WinVer_Win8,
        L"HKLM\\SOFTWARE\\WOW6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Schedule\\Maintenance",
        L"MaintenanceDisabled",
        V(1));
    MakeTweakObject<CTaskTweak>(maintenance, pList, L"Device Maintenance Task", L"disable_maintenancetasks_task", WinVer_Win8,
        L"\\Microsoft\\Windows\\capabilityaccessmanager",
        L"maintenancetasks");
    MakeTweakObject<CTaskTweak>(maintenance, pList, L"Disable Windows System Assessment Tests Task", L"disable_winsat_task", WinVer_Win7,
        L"\\Microsoft\\Windows\\Maintenance",
        L"WinSAT",
        ETweakHint::eNotRecommended);

    // *** Remove Quick Note from Action Center ***
    MakeTweakObject<CRegTweak>(miscCat, pList, L"Remove Quick Note from Action Center", L"remove_quicknote_actioncenter", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ActionCenter\\Quick Actions\\All\\SystemSettings_Launcher_QuickNote",
        L"Type",
        V(0));


    MakeTweakObject<CRegTweak>(miscCat, pList, L"Disable Windows Media Player Usage Tracking (User)", L"wmp_disable_usage_tracking_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\MediaPlayer\\Preferences",
        L"UsageTracking",
        V(0));

    // *** No Speech Updates ***
    MakeTweakObject<CGpoTweak>(miscCat, pList, L"Don't Update SpeechModel", L"disable_speech_model_update", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Speech",
        L"AllowSpeechModelUpdate",
        V(0));

    // *** No Font Updates ***
    MakeTweakObject<CGpoTweak>(miscCat, pList, L"Don't Update Fonts", L"disable_font_updates_policy", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"EnableFontProviders",
        V(0));

    // *** No Certificate Updates ***
    MakeTweakObject<CGpoTweak>(miscCat, pList, L"Disable Certificate Auto Update", L"disable_cert_auto_update_policy", WinVer_WinXP,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\SystemCertificates\\AuthRoot",
        L"DisableRootAutoUpdate",
        V(1),
        ETweakHint::eNotRecommended);

    // *** Disable Map Updates ***
    CTweakPtr map = MakeTweakObject<CTweakSet>(miscCat, pList, L"Disable Map Updates", L"disable_map_updates");
    MakeTweakObject<CGpoTweak>(map, pList, L"Turn off unsolicited Maps Downloads", L"disable_unsolicited_maps_download", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Maps",
        L"AllowUntriggeredNetworkTrafficOnSettingsPage",
        V(0));
    MakeTweakObject<CGpoTweak>(map, pList, L"Turn off Auto Maps Update", L"disable_auto_maps_update", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Maps",
        L"AutoDownloadAndUpdateMapData",
        V(0));

    // *** No Internet OpenWith ***
    CTweakPtr iopen = MakeTweakObject<CTweakSet>(miscCat, pList, L"No Internet OpenWith", L"disable_internet_openwith", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(iopen, pList, L"Disable Internet OpenWith", L"disable_internet_openwith_policy", WinVer_WinXPto7,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
        L"NoInternetOpenWith",
        V(1));

    MakeTweakObject<CGpoTweak>(iopen, pList, L"Disable Internet OpenWith (User)", L"disable_internet_openwith_user", WinVer_WinXPto7,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
        L"NoInternetOpenWith",
        V(1));

    MakeTweakObject<CGpoTweak>(miscCat, pList, L"Disable News and Interests", L"disable_news_and_interests", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Dsh",
        L"AllowNewsAndInterests",
        V(0));


    /*  
    *  #########################################
    *              System
    *  #########################################
    */

    CTweakPtr system = MakeTweakObject<CTweakGroup>(pRoot, pList, L"System Tweaks", L"system");

    MakeTweakObject<CGpoTweak>(system, pList, L"Disable Fast Startup", L"disable_fast_startup", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System", 
        L"HiberbootEnabled", 
        V(0),
        ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(system, pList, L"Allow Blocking Apps at Shutdown", L"allow_block_shutdown_apps", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System", 
        L"AllowBlockingAppsAtShutdown", 
        V(0),
        ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(system, pList, L"Enable Auto End Tasks", L"enable_auto_end_tasks_user", WinVer_Win10,
        L"HKCU\\Control Panel\\Desktop",
        L"AutoEndTasks",
        V(1),
        ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(system, pList, L"Enable Verbose Startup/Shutdown Status", L"enable_verbose_status", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
        L"VerboseStatus",
        V(1));
    MakeTweakObject<CRegTweak>(system, pList, L"Show Crash Parameters on BSOD", L"show_bsod_parameters", WinVer_Win10,
        L"HKLM\\SYSTEM\\CurrentControlSet\\Control\\CrashControl",
        L"DisplayParameters",
        V(1));
    MakeTweakObject<CRegTweak>(system, pList, L"Clear Page File at Shutdown", L"clear_pagefile_shutdown", WinVer_Win10,
        L"HKLM\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management",
        L"ClearPageFileAtShutdown",
        V(1),
        ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(system, pList, L"Encrypt Page File", L"encrypt_pagefile", WinVer_Win10,
        L"HKLM\\SYSTEM\\CurrentControlSet\\Control\\FileSystem",
        L"NtfsEncryptPagingFile",
        V(1),
        ETweakHint::eRecommended);
    MakeTweakObject<CSvcTweak>(system, pList, L"Disable SysMain (Superfetch)", L"disable_sysmain", WinVer_Win7,
        L"SysMain",
        ETweakHint::eNotRecommended);

    // *** Clipboard ***
    CTweakPtr clipboard = MakeTweakObject<CTweakSet>(system, pList, L"Clipboard Tweaks", L"clipboard_tweaks", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(clipboard, pList, L"Disable Clipboard History", L"disable_clipboard_history", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System", 
        L"AllowClipboardHistory", 
        V(0));
    MakeTweakObject<CRegTweak>(clipboard, pList, L"Disable Clipboard History (User)", L"disable_clipboard_history_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Clipboard",
        L"EnableClipboardHistory",
        V(0));


    /*  
    *  #########################################
    *               Network Options
    *  #########################################
    */

    CTweakPtr net = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Network Tweaks", L"network");
    MakeTweakObject<CGpoTweak>(net, pList, L"Disable Smart Name Resolution", L"disable_smart_name_resolution", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows NT\\DNSClient",
        L"DisableSmartNameResolution",
        V(1), 
        ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(net, pList, L"Do Not Restore Mapped Drives at Logon", L"no_restore_network_drives", WinVer_Win10,
        L"HKLM\\SYSTEM\\CurrentControlSet\\Control\\NetworkProvider",
        L"RestoreConnection",
        V(0), 
        ETweakHint::eNotRecommended);
    MakeTweakObject<CRegTweak>(net, pList, L"Disable Internet Connectivity Check", L"disable_active_probing", WinVer_Win10,
        L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\NlaSvc\\Parameters\\Internet",
        L"EnableActiveProbing",
        V(0),
        ETweakHint::eBreaking);
    MakeTweakObject<CRegTweak>(net, pList, L"Disable Bluetooth Advertising", L"disable_bt_advertising", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\PolicyManager\\current\\device\\Bluetooth",
        L"AllowAdvertising",
        V(0), 
        ETweakHint::eNotRecommended);
    MakeTweakObject<CFwTweak>(net, pList, L"Firewall Device Association Framework Provider Host", L"fw_block_das_host", WinVer_Win10,
        L"%SystemRoot%\\system32\\dashost.exe", 
        L"", ETweakHint::eNotRecommended);

    MakeTweakObject<CGpoTweak>(net, pList, L"Disable NTP Client", L"disable_ntp_client_policy", WinVer_WinXP,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\W32time\\TimeProviders\\NtpClient",
        L"Enabled",
        V(0));

    MakeTweakObject<CGpoTweak>(net, pList, L"Disable Active Probeing", L"disable_active_probe", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\NetworkConnectivityStatusIndicator",
        L"NoActiveProbe",
        V(1));

    MakeTweakObject<CGpoTweak>(net, pList, L"Disable Teredo Tunneling", L"disable_teredo_tunneling", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\TCPIP\\v6Transition",
        L"Teredo_State",
        V(L"Disabled"));

    // *** Disable Delivery Optimizations ***
    CTweakPtr dodm = MakeTweakObject<CTweakSet>(net, pList, L"No Delivery Optimizations", L"disable_delivery_opt", ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(dodm, pList, L"Disable Delivery Optimizations", L"disable_delivery_optimizations", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DeliveryOptimization",
        L"DODownloadMode",
        V(L"100"));
    MakeTweakObject<CRegTweak>(dodm, pList, L"Set Delivery Optimization to LAN Only", L"delivery_opt_lan_only", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\DeliveryOptimization\\Config",
        L"DODownloadMode",
        V(1));
    MakeTweakObject<CSvcTweak>(dodm, pList, L"Disable Delivery Optimization Service", L"disable_dosvc_service", WinVer_Win10, 
        L"DoSvc");


    /*  
    *  #########################################
    *              Apps & Store
    *  #########################################
    */

    CTweakPtr appCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Apps & Store", L"store_and_apps");


    // *** Disable Store ***
    CTweakPtr store = MakeTweakObject<CTweakSet>(appCat, pList, L"Disable Store", L"disable_store");
    MakeTweakObject<CGpoTweak>(store, pList, L"Disable Microsoft Store App", L"disable_microsoft_store_ui", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\WindowsStore",
        L"RemoveWindowsStore",
        V(1));
    MakeTweakObject<CGpoTweak>(store, pList, L"Don't Auto Update Apps", L"disable_auto_update_apps", WinVer_Win10EE,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\WindowsStore",
        L"AutoDownload",
        V(2));
    MakeTweakObject<CGpoTweak>(store, pList, L"Disable App Uri Handlers", L"disable_app_uri_handlers", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"EnableAppUriHandlers",
        V(0), ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(store, pList, L"Disable New App Installed Notification", L"disable_new_app_alert", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Explorer", 
        L"NoNewAppAlert", 
        V(1), 
        ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(delivery, pList, L"Disable Push to Install", L"disable_push_to_install", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\PushToInstall",
        L"DisablePushToInstall",
        V(1));

    // *** Disable Store Telemetrs ***
    //CTweakPtr storeTel = MakeTweakObject<CTweakSet>(appCat, pList, L"Disable Store Telemetry", L"disable_store_telemetry", ETweakHint::eRecommended);
    MakeTweakObject<CRegTweak>(appCat, pList, L"Disable Store Telemetry", L"minimize_store_telemetry", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CPSS\\Store\\AllowTelemetry",
        L"Value",
        V(0), 
        ETweakHint::eRecommended);

    // *** Disable Store Apps ***
    //CTweakPtr storeApps = MakeTweakObject<CTweakSet>(appCat, pList, L"Disable Store Apps", L"disable_store_apps", ETweakHint::eBreaking);
    MakeTweakObject<CGpoTweak>(appCat, pList, L"Disable Store Apps", L"disable_store_apps", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\WindowsStore",
        L"DisableStoreApps",
        V(1), 
        ETweakHint::eBreaking);

    // *** Lockdown Apps ***
    CTweakPtr apps = MakeTweakObject<CTweakSet>(appCat, pList, L"Lockdown Apps (Has NO effect on regular programs)", L"lockdown_apps");

    CTweakPtr appDiag = MakeTweakObject<CTweakSet>(apps, pList, L"Disable App Diagnostics Access", L"deny_app_diagnostics", ETweakHint::eRecommended);
    MakeTweakObject<CRegTweak>(appDiag, pList, L"Disable App Diagnostics Access (Machine)", L"deny_app_diagnostics_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\appDiagnostics",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appDiag, pList, L"Disable App Diagnostics Access (User)", L"deny_app_diagnostics_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\appDiagnostics",
        L"Value",
        V(L"Deny"));

    CTweakPtr appAcc = MakeTweakObject<CTweakSet>(apps, pList, L"Don't Let Apps Access AccountInfo", L"deny_user_account_information", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(appAcc, pList, L"Don't Let Apps Access AccountInfo", L"disable_apps_access_accountinfo", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessAccountInfo",
        V(2));
    MakeTweakObject<CRegTweak>(appAcc, pList, L"Disable User Account Information Access (Machine)", L"deny_user_account_information_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\userAccountInformation",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appAcc, pList, L"Disable User Account Information Access (User)", L"deny_user_account_information_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\userAccountInformation",
        L"Value",
        V(L"Deny"));

    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Calendar", L"disable_apps_access_calendar", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessCalendar",
        V(2));

    CTweakPtr appCalls = MakeTweakObject<CTweakSet>(apps, pList, L"Don't Let Apps Access CallHistory", L"deny_phone_call_history");
    MakeTweakObject<CGpoTweak>(appCalls, pList, L"Don't Let Apps Access CallHistory", L"disable_apps_access_callhistory", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessCallHistory",
        V(2));
    MakeTweakObject<CRegTweak>(appCalls, pList, L"Disable Phone Call History Access (Machine)", L"deny_phone_call_history_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\phoneCallHistory",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appCalls, pList, L"Disable Phone Call History Access (User)", L"deny_phone_call_history_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\phoneCallHistory",
        L"Value",
        V(L"Deny"));

    CTweakPtr appCall = MakeTweakObject<CTweakSet>(apps, pList, L"Disable Phone Access", L"deny_phone_call_access");
    MakeTweakObject<CGpoTweak>(appCall, pList, L"Don't Let Apps Access Phone", L"deny_phone_call", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessPhone",
        V(2));
    MakeTweakObject<CRegTweak>(appCall, pList, L"Disable Phone Access (Machine)", L"deny_phone_call_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\phoneCall",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appCall, pList, L"Disable Phone Access (User)", L"deny_phone_call_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\phoneCall",
        L"Value",
        V(L"Deny"));

    CTweakPtr appRadio = MakeTweakObject<CTweakSet>(apps, pList, L"Disable Radios Access", L"deny_radios");
    MakeTweakObject<CRegTweak>(appRadio, pList, L"Disable Radios Access (Machine)", L"deny_radios_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\radios",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appRadio, pList, L"Disable Radios Access (User)", L"deny_radios_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\radios",
        L"Value",
        V(L"Deny"));

    CTweakPtr appAct = MakeTweakObject<CTweakSet>(apps, pList, L"Deny Activity History Access", L"deny_activity_access");
    MakeTweakObject<CRegTweak>(appAct, pList, L"Deny Activity History Access (Machine)", L"deny_activity_access_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\activity",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appAct, pList, L"Deny Activity History Access (User)", L"deny_activity_access_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\activity",
        L"Value",
        V(L"Deny"));

    CTweakPtr appCamera = MakeTweakObject<CTweakSet>(apps, pList, L"Don't Let Apps Access Camera", L"deny_webcam");
    MakeTweakObject<CGpoTweak>(appCamera, pList, L"Don't Let Apps Access Camera", L"disable_apps_access_camera", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessCamera",
        V(2));
    MakeTweakObject<CRegTweak>(appCamera, pList, L"Disable Webcam Access (Machine)", L"deny_webcam_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\webcam",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appCamera, pList, L"Disable Webcam Access (User)", L"deny_webcam_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\webcam",
        L"Value",
        V(L"Deny"));

    CTweakPtr appContacts = MakeTweakObject<CTweakSet>(apps, pList, L"Don't Let Apps Access Contacts", L"deny_contacts");
    MakeTweakObject<CGpoTweak>(appContacts, pList, L"Don't Let Apps Access Contacts", L"disable_apps_access_contacts", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessContacts",
        V(2));
    MakeTweakObject<CRegTweak>(appContacts, pList, L"Disable Contacts Access (Machine)", L"deny_contacts_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\contacts",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appContacts, pList, L"Disable Contacts Access (User)", L"deny_contacts_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\contacts",
        L"Value",
        V(L"Deny"));

    CTweakPtr appEmail = MakeTweakObject<CTweakSet>(apps, pList, L"Disable Email Access", L"deny_email");
    MakeTweakObject<CGpoTweak>(appEmail, pList, L"Don't Let Apps Access Email", L"disable_apps_access_email", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessEmail",
        V(2));
    MakeTweakObject<CRegTweak>(appEmail, pList, L"Disable Email Access (Machine)", L"deny_email_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\email",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appEmail, pList, L"Disable Email Access (User)", L"deny_email_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\email",
        L"Value",
        V(L"Deny"));

    CTweakPtr appChat = MakeTweakObject<CTweakSet>(apps, pList, L"Disable Chat Access", L"deny_chat");
    MakeTweakObject<CRegTweak>(appChat, pList, L"Disable Chat Access (Machine)", L"deny_chat_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\chat",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appChat, pList, L"Disable Chat Access (User)", L"deny_chat_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\chat",
        L"Value",
        V(L"Deny"));

    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Location", L"disable_apps_access_location", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessLocation",
        V(2));

    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Messaging", L"disable_apps_access_messaging", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessMessaging",
        V(2));

    CTweakPtr appMicrophone = MakeTweakObject<CTweakSet>(apps, pList, L"Don't Let Apps Access Microphone", L"deny_microphone");
    MakeTweakObject<CGpoTweak>(appMicrophone, pList, L"Don't Let Apps Access Microphone", L"disable_apps_access_microphone", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessMicrophone",
        V(2));
    MakeTweakObject<CRegTweak>(appMicrophone, pList, L"Disable Microphone Access (Machine)", L"deny_microphone_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\microphone",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appMicrophone, pList, L"Disable Microphone Access (User)", L"deny_microphone_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\microphone",
        L"Value",
        V(L"Deny"));

    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Motion", L"disable_apps_access_motion", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessMotion",
        V(2));

    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Notifications", L"disable_apps_access_notifications", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessNotifications",
        V(2));

    CTweakPtr appCellular = MakeTweakObject<CTweakSet>(apps, pList, L"Disable Cellular Data Access", L"deny_cellular_data");
    MakeTweakObject<CRegTweak>(appCellular, pList, L"Disable Cellular Data Access (Machine)", L"deny_cellular_data_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\cellularData",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appCellular, pList, L"Disable Cellular Data Access (User)", L"deny_cellular_data_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\cellularData",
        L"Value",
        V(L"Deny"));

    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Radios", L"disable_apps_access_radios", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessRadios",
        V(2));

    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Tasks", L"disable_apps_access_tasks", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessTasks",
        V(2));

    CTweakPtr appAppointments = MakeTweakObject<CTweakSet>(apps, pList, L"Disable Appointments Access", L"deny_appointments");
    MakeTweakObject<CRegTweak>(appAppointments, pList, L"Disable Appointments Access (Machine)", L"deny_appointments_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\appointments",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appAppointments, pList, L"Disable Appointments Access (User)", L"deny_appointments_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\appointments",
        L"Value",
        V(L"Deny"));

    CTweakPtr appTasks = MakeTweakObject<CTweakSet>(apps, pList, L"Disable User Data Tasks Access", L"deny_user_data_tasks");
    MakeTweakObject<CRegTweak>(appTasks, pList, L"Disable User Data Tasks Access (Machine)", L"deny_user_data_tasks_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\userDataTasks",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appTasks, pList, L"Disable User Data Tasks Access (User)", L"deny_user_data_tasks_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\userDataTasks",
        L"Value",
        V(L"Deny"));

    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access TrustedDevices", L"disable_apps_access_trusteddevices", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessTrustedDevices",
        V(2));

    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps get Diagnostic Info", L"disable_apps_get_diaginfo", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsGetDiagnosticInfo",
        V(2));

    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Sync With Devices", L"disable_apps_sync_devices", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsSyncWithDevices",
        V(2));

    CTweakPtr appBackground = MakeTweakObject<CTweakSet>(apps, pList, L"Don't Let Apps Run In Background", L"disable_background_access_applications", ETweakHint::eBreaking);
    MakeTweakObject<CGpoTweak>(appBackground, pList, L"Don't Let Apps Run In Background", L"disable_apps_run_background", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsRunInBackground",
        V(2));
    MakeTweakObject<CRegTweak>(appBackground, pList, L"Disable Background Access for Applications (Machine)", L"disable_background_access_applications_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\BackgroundAccessApplications",
        L"GlobalUserDisabled",
        V(1));
    MakeTweakObject<CRegTweak>(appBackground, pList, L"Disable Background Access for Applications (User)", L"disable_background_access_applications_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\BackgroundAccessApplications",
        L"GlobalUserDisabled",
        V(1));
    MakeTweakObject<CFwTweak>(appBackground, pList, L"Firewall Background Task Host", L"fw_block_backgroundtaskhost", WinVer_Win10,
        L"%SystemRoot%\\system32\\backgroundtaskhost.exe", 
        L"", ETweakHint::eNotRecommended);
    MakeTweakObject<CFwTweak>(appBackground, pList, L"Firewall Download Upload Host", L"fw_block_backgroundtransferhost", WinVer_Win10,
        L"%SystemRoot%\\system32\\backgroundtransferhost.exe", 
        L"", ETweakHint::eNotRecommended);

    CTweakPtr appFiles = MakeTweakObject<CTweakSet>(apps, pList, L"Disable File System Access", L"deny_file_system_access");
    MakeTweakObject<CRegTweak>(appFiles, pList, L"Disable Broad File System Access (Machine)", L"deny_broad_file_system_access_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\broadFileSystemAccess",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appFiles, pList, L"Disable Broad File System Access (User)", L"deny_broad_file_system_access_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\broadFileSystemAccess",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appFiles, pList, L"Disable Documents Library Access (Machine)", L"deny_documents_library_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\documentsLibrary",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appFiles, pList, L"Disable Documents Library Access (User)", L"deny_documents_library_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\documentsLibrary",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appFiles, pList, L"Disable Pictures Library Access (Machine)", L"deny_pictures_library_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\picturesLibrary",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appFiles, pList, L"Disable Pictures Library Access (User)", L"deny_pictures_library_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\picturesLibrary",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appFiles, pList, L"Disable Videos Library Access (Machine)", L"deny_videos_library_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\videosLibrary",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appFiles, pList, L"Disable Videos Library Access (User)", L"deny_videos_library_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\videosLibrary",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appFiles, pList, L"Deny Downloads Folder Access (Machine)", L"deny_downloads_access_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\downloadsFolder",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appFiles, pList, L"Deny Downloads Folder Access (User)", L"deny_downloads_access_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\downloadsFolder",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appFiles, pList, L"Deny Music Library Access (Machine)", L"deny_music_library_access_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\musicLibrary",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appFiles, pList, L"Deny Music Library Access (User)", L"deny_music_library_access_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\musicLibrary",
        L"Value",
        V(L"Deny"));

    CTweakPtr appSpeech = MakeTweakObject<CTweakSet>(apps, pList, L"No Speach Activation", L"deny_speech_activation");
    MakeTweakObject<CGpoTweak>(appSpeech, pList, L"Disable Voice Activation", L"allow_apps_activate_with_voice", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsActivateWithVoice",
        V(2));
    MakeTweakObject<CRegTweak>(appSpeech, pList, L"Disable Voice Activation (User)", L"disable_voice_activation_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Speech_OneCore\\Settings\\VoiceActivation",
        L"AgentActivationEnabled",
        V(0));
    MakeTweakObject<CRegTweak>(appSpeech, pList, L"Disable Voice Activation for All Apps (User)", L"disable_voice_activation_all_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Speech_OneCore\\Settings\\VoiceActivation\\UserPreferenceForAllApps",
        L"AgentActivationEnabled",
        V(0));
    MakeTweakObject<CGpoTweak>(appSpeech, pList, L"Disable Voice Activation on Lock Screen", L"disable_voice_activation_lockscreen", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsActivateWithVoiceAboveLock",
        V(2));
    MakeTweakObject<CRegTweak>(appSpeech, pList, L"Disable Voice Activation on Lock Screen (User)", L"disable_voice_activation_lockscreen_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Speech_OneCore\\Settings\\VoiceActivation",
        L"AgentActivationOnLockScreenEnabled",
        V(0));
    MakeTweakObject<CRegTweak>(appSpeech, pList, L"Disable Voice Activation on Lock Screen for All Apps (User)", L"disable_voice_activation_lock_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Speech_OneCore\\Settings\\VoiceActivation\\UserPreferenceForAllApps",
        L"AgentActivationOnLockScreenEnabled",
        V(0));

    CTweakPtr appBT = MakeTweakObject<CTweakSet>(apps, pList, L"Deny Bluetooth Sync Access", L"deny_bluetooth_sync");
    MakeTweakObject<CRegTweak>(appBT, pList, L"Deny Bluetooth Sync Access (Machine)", L"deny_bluetooth_sync_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\bluetoothSync",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appBT, pList, L"Deny Bluetooth Sync Access (User)", L"deny_bluetooth_sync_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\bluetoothSync",
        L"Value",
        V(L"Deny"));

    CTweakPtr appGaze = MakeTweakObject<CTweakSet>(apps, pList, L"Deny Eye Tracking", L"deny_gaze_input");
    MakeTweakObject<CGpoTweak>(appGaze, pList, L"Don't Let Apps use Eye Tracking", L"disable_apps_gaze_input", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessGazeInput",
        V(2));
    MakeTweakObject<CRegTweak>(appGaze, pList, L"Deny Eye Tracking (Machine)", L"deny_gaze_input_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\gazeInput",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appGaze, pList, L"Deny Eye Tracking (User)", L"deny_gaze_input_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\gazeInput",
        L"Value",
        V(L"Deny"));

    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps use Spatial Perception", L"disable_apps_spatial_perception", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessBackgroundSpatialPerception",
        V(2));

    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps detect Human Presence", L"disable_apps_human_presence", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessHumanPresence",
        V(2));

    CTweakPtr appCapture = MakeTweakObject<CTweakSet>(apps, pList, L"Deny Screen Capture", L"deny_screen_capture");
    MakeTweakObject<CRegTweak>(appCapture, pList, L"Deny Screen Capture - NonPackaged (User)", L"deny_programmatic_capture_nonpackaged_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\graphicsCaptureProgrammatic\\NonPackaged",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CGpoTweak>(appCapture, pList, L"Deny Screen Capture", L"deny_programmatic_capture", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessGraphicsCaptureProgrammatic",
        V(2));
    MakeTweakObject<CRegTweak>(appCapture, pList, L"Deny Screen Capture (Machine)", L"deny_programmatic_capture_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\graphicsCaptureProgrammatic",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appCapture, pList, L"Deny Screen Capture (User)", L"deny_programmatic_capture_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\graphicsCaptureProgrammatic",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appCapture, pList, L"Deny Screen Capture Without Border - NonPackaged (User)", L"deny_borderless_capture_nonpackaged_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\graphicsCaptureWithoutBorder\\NonPackaged",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CGpoTweak>(appCapture, pList, L"Deny Screen Capture Without Border", L"deny_borderless_capture", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessGraphicsCaptureWithoutBorder",
        V(2));
    MakeTweakObject<CRegTweak>(appCapture, pList, L"Deny Screen Capture Without Border (Machine)", L"deny_borderless_capture_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\graphicsCaptureWithoutBorder",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appCapture, pList, L"Deny Screen Capture Without Border (User)", L"deny_borderless_capture_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\graphicsCaptureWithoutBorder",
        L"Value",
        V(L"Deny"));

    CTweakPtr appNotifications = MakeTweakObject<CTweakSet>(apps, pList, L"Disable User Notification Listener Access", L"deny_user_notification_listener");
    MakeTweakObject<CRegTweak>(appNotifications, pList, L"Disable User Notification Listener Access (Machine)", L"deny_user_notification_listener_machine", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\userNotificationListener",
        L"Value",
        V(L"Deny"));
    MakeTweakObject<CRegTweak>(appNotifications, pList, L"Disable User Notification Listener Access (User)", L"deny_user_notification_listener_user", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\CapabilityAccessManager\\ConsentStore\\userNotificationListener",
        L"Value",
        V(L"Deny"));

    MakeTweakObject<CFwTweak>(apps, pList, L"Firewall Runtime Broker", L"fw_block_runtimebroker", WinVer_Win10,
        L"%SystemRoot%\\system32\\runtimebroker.exe");


    // *** Disable Mail and People ***
    CTweakPtr mail = MakeTweakObject<CTweakSet>(appCat, pList, L"Block Mail and People", L"disable_mail_people", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(mail, pList, L"Disable Mail App", L"disable_mail_app", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Mail",
        L"ManualLaunchAllowed",
        V(0));
    MakeTweakObject<CGpoTweak>(mail, pList, L"Hide People from Taskbar (user)", L"hide_people_taskbar_user", WinVer_Win10,
        L"HKCU\\Software\\Policies\\Microsoft\\Windows\\Explorer",
        L"HidePeopleBar",
        V(1));
    MakeTweakObject<CRegTweak>(mail, pList, L"Disable People Band in File Explorer (User)", L"disable_people_band_user", WinVer_Win10,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced\\People",
        L"PeopleBand",
        V(0));



    /*  
    *  #########################################
    *              Web Browsers
    *  #########################################
    */

    CTweakPtr browserCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Web Browsers", L"wer_browsers");

    //CTweakPtr edgePolicy = MakeTweakObject<CTweakSet>(browserCat, pList, L"Harden Microsoft Edge (Machine Policies)", L"harden_edge_policies", ETweakHint::eRecommended);
    CTweakPtr edgePolicy = MakeTweakObject<CTweakSet>(browserCat, pList, L"Harden Microsoft Edge", L"harden_edge_policies", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Startup Boost", L"edge_disable_startup_boost", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"StartupBoostEnabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Enable Do Not Track", L"edge_enable_dnt", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"ConfigureDoNotTrack", V(1));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Network Prediction", L"edge_disable_net_predict", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"NetworkPredictionOptions", V(2), ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Search Suggestions", L"edge_disable_search_suggest", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"SearchSuggestEnabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Autofill (Addresses)", L"edge_disable_autofill_addr", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"AutofillAddressEnabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Navigation Error Web Service", L"edge_disable_nav_error_ws", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"ResolveNavigationErrorsUseWebService", V(0), ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Alternate Error Pages", L"edge_disable_alt_error_pages", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"AlternateErrorPagesEnabled", V(0), ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Site Info Sharing", L"edge_disable_siteinfo", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"SendSiteInfoToImproveServices", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Personalization Reporting", L"edge_disable_personalization", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"PersonalizationReportingEnabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Metrics Reporting", L"edge_disable_metrics", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"MetricsReportingEnabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Payment Method Query", L"edge_disable_payment_query", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"PaymentMethodQueryEnabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Autofill (Credit Cards)", L"edge_disable_autofill_cc", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"AutofillCreditCardEnabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Bing Suggestions", L"edge_disable_bing_suggest", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"AddressBarMicrosoftSearchInBingProviderEnabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable User Feedback", L"edge_disable_feedback", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"UserFeedbackAllowed", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Block 3rd-Party Cookies", L"edge_block_3p_cookies", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"BlockThirdPartyCookies", V(1));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Enable Strict Tracking Prevention", L"edge_strict_tracking", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"TrackingPrevention", V(3), ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Edge Shopping Assistant", L"edge_disable_shopping", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"EdgeShoppingAssistantEnabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Local Providers", L"edge_disable_local_providers", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"LocalProvidersEnabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Hubs Sidebar", L"edge_disable_hubs_sidebar", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"HubsSidebarEnabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Edge Follow", L"edge_disable_follow", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"EdgeFollowEnabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Hide First Run Experience", L"edge_hide_firstrun", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"HideFirstRunExperience", V(1));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable 3rd-Party Search Telemetry", L"edge_disable_3p_telemetry", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"Edge3PSerpTelemetryEnabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Enable Edge Sync", L"enable_edge_sync", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"SyncDisabled", V(0));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Set Edge Default Geolocation", L"set_edge_geolocation", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"DefaultGeolocationSetting", V(3));
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Site Safety Services", L"disable_site_safety_services", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"SiteSafetyServicesEnabled", V(0), ETweakHint::eNotRecommended);
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Edge Password Manager", L"disable_edge_password_manager", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"PasswordManagerEnabled", V(0), ETweakHint::eNotRecommended); 
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Browser Sign-in", L"disable_browser_signin", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"BrowserSignin", V(0)); 
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Web Widget", L"disable_web_widget", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"WebWidgetAllowed", V(0)); 
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Typosquatting Checker", L"disable_typosquatting_checker", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"TyposquattingCheckerEnabled", V(0), ETweakHint::eNotRecommended); 
    MakeTweakObject<CGpoTweak>(edgePolicy, pList, L"Disable Microsoft Editor Proofing", L"disable_microsoft_editor_proofing", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Edge", L"MicrosoftEditorProofingEnabled", V(0));

    //CTweakPtr edgePolicyUser = MakeTweakObject<CTweakSet>(browserCat, pList, L"Harden Microsoft Edge (User Policies)", L"harden_edge_user_policies", ETweakHint::eRecommended);
    ////MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Startup Boost", L"edge_disable_startup_boost_user", WinVer_Win10,
    ////    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"StartupBoostEnabled", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Enable Do Not Track", L"edge_enable_dnt_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"ConfigureDoNotTrack", V(1));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Network Prediction", L"edge_disable_net_predict_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"NetworkPredictionOptions", V(2), ETweakHint::eNotRecommended);
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Search Suggestions", L"edge_disable_search_suggest_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"SearchSuggestEnabled", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Autofill (Addresses)", L"edge_disable_autofill_addr_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"AutofillAddressEnabled", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Navigation Error Web Service", L"edge_disable_nav_error_ws_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"ResolveNavigationErrorsUseWebService", V(0), ETweakHint::eNotRecommended);
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Alternate Error Pages", L"edge_disable_alt_error_pages_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"AlternateErrorPagesEnabled", V(0), ETweakHint::eNotRecommended);
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Site Info Sharing", L"edge_disable_siteinfo_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"SendSiteInfoToImproveServices", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Personalization Reporting", L"edge_disable_personalization_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"PersonalizationReportingEnabled", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Metrics Reporting", L"edge_disable_metrics_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"MetricsReportingEnabled", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Payment Method Query", L"edge_disable_payment_query_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"PaymentMethodQueryEnabled", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Autofill (Credit Cards)", L"edge_disable_autofill_cc_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"AutofillCreditCardEnabled", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Bing Suggestions", L"edge_disable_bing_suggest_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"AddressBarMicrosoftSearchInBingProviderEnabled", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable User Feedback", L"edge_disable_feedback_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"UserFeedbackAllowed", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Block 3rd-Party Cookies", L"edge_block_3p_cookies_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"BlockThirdPartyCookies", V(1));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Enable Strict Tracking Prevention", L"edge_strict_tracking_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"TrackingPrevention", V(3), ETweakHint::eNotRecommended);
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Edge Shopping Assistant", L"edge_disable_shopping_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"EdgeShoppingAssistantEnabled", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Local Providers", L"edge_disable_local_providers_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"LocalProvidersEnabled", V(0));
    ////MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Hubs Sidebar", L"edge_disable_hubs_sidebar_user", WinVer_Win10,
    ////    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"HubsSidebarEnabled", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Edge Follow", L"edge_disable_follow_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"EdgeFollowEnabled", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Hide First Run Experience", L"edge_hide_firstrun_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"HideFirstRunExperience", V(1));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable 3rd-Party Search Telemetry", L"edge_disable_3p_telemetry_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"Edge3PSerpTelemetryEnabled", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Enable Edge Sync", L"enable_edge_sync_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"SyncDisabled", V(0));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Set Edge Default Geolocation", L"set_edge_geolocation_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"DefaultGeolocationSetting", V(3));
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Site Safety Services", L"disable_site_safety_services_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"SiteSafetyServicesEnabled", V(0), ETweakHint::eNotRecommended);
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Edge Password Manager", L"disable_edge_password_manager_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"PasswordManagerEnabled", V(0), ETweakHint::eNotRecommended); 
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Browser Sign-in", L"disable_browser_signin_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"BrowserSignin", V(0)); 
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Web Widget", L"disable_web_widget_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"WebWidgetAllowed", V(0)); 
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Typosquatting Checker", L"disable_typosquatting_checker_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"TyposquattingCheckerEnabled", V(0), ETweakHint::eNotRecommended); 
    //MakeTweakObject<CGpoTweak>(edgePolicyUser, pList, L"Disable Microsoft Editor Proofing", L"disable_microsoft_editor_proofing_user", WinVer_Win10,
    //    L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Edge", L"MicrosoftEditorProofingEnabled", V(0));

    // *** Lockdown MS Edge ***
    CTweakPtr edge = MakeTweakObject<CTweakSet>(browserCat, pList, L"Lockdown MS Edge (old, non Chromium)", L"lockdown_edge");
    MakeTweakObject<CGpoTweak>(edge, pList, L"Don't Update Compatibility Lists", L"disable_edge_compat_lists", WinVer_Win10to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\BrowserEmulation",
        L"MSCompatibilityMode",
        V(0));
    MakeTweakObject<CGpoTweak>(edge, pList, L"Set Blank Start Page", L"set_blank_startpage_edge", WinVer_Win10to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\Internet Settings",
        L"ProvisionedHomePages",
        V(L"<about:blank>"));
    MakeTweakObject<CGpoTweak>(edge, pList, L"Set 'DoNotTrack'", L"set_donottrack_edge", WinVer_Win10to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\Main",
        L"DoNotTrack",
        V(1));
    MakeTweakObject<CGpoTweak>(edge, pList, L"No Password Auto Complete", L"disable_password_autocomplete_edge", WinVer_Win10to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\Main",
        L"FormSuggest Passwords",
        V(L"no"));
    MakeTweakObject<CGpoTweak>(edge, pList, L"Disable First Start Page", L"disable_firststart_page_edge", WinVer_Win10to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\Main",
        L"PreventFirstRunPage",
        V(1));
    MakeTweakObject<CGpoTweak>(edge, pList, L"No Form Auto Complete", L"disable_form_autocomplete_edge", WinVer_Win10to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\Main",
        L"Use FormSuggest",
        V(L"no"));
    MakeTweakObject<CGpoTweak>(edge, pList, L"Disable AddressBar Suggestions", L"disable_addressbar_suggestions", WinVer_Win10to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\SearchScopes",
        L"ShowSearchSuggestionsGlobal",
        V(0));
    MakeTweakObject<CGpoTweak>(edge, pList, L"Disable AddressBar (drop down) Suggestions", L"disable_addressbar_dropdown_suggestions", WinVer_Win10to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\ServiceUI",
        L"ShowOneBox",
        V(0));
    MakeTweakObject<CGpoTweak>(edge, pList, L"Keep New Edge Tabs Empty", L"keep_new_edge_tabs_empty", WinVer_Win10to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\ServiceUI",
        L"AllowWebContentOnNewTabPage",
        V(0));
    MakeTweakObject<CGpoTweak>(edge, pList, L"Disable Books Library Updating", L"disable_books_library_update", WinVer_Win10to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\BooksLibrary",
        L"AllowConfigurationUpdateForBooksLibrary",
        V(0));
    MakeTweakObject<CGpoTweak>(edge, pList, L"Disable Microsoft Edge Prelaunch", L"disable_edge_prelaunch", WinVer_Win10to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\Main", 
        L"AllowPrelaunch", 
        V(0));
    MakeTweakObject<CGpoTweak>(edge, pList, L"Disable Microsoft Edge Tab Preloading", L"disable_edge_tab_preload", WinVer_Win10to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\TabPreloader", 
        L"AllowTabPreloading", 
        V(0));

    // *** Lockdown IE ***
    CTweakPtr ie = MakeTweakObject<CTweakSet>(browserCat, pList, L"Lockdown Internet Explorer", L"lockdown_ie");
    MakeTweakObject<CGpoTweak>(ie, pList, L"Disable Enhanced AddressBar Suggestions", L"disable_ie_enhanced_addressbar", WinVer_Win7to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer",
        L"AllowServicePoweredQSA",
        V(0));
    MakeTweakObject<CGpoTweak>(ie, pList, L"Turn off Browser Geolocation", L"disable_ie_geolocation", WinVer_Win7to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Geolocation",
        L"PolicyDisableGeolocation",
        V(1));
    MakeTweakObject<CGpoTweak>(ie, pList, L"Turn off Site Suggestions", L"disable_ie_site_suggestions", WinVer_WinXPto11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Suggested Sites",
        L"Enabled",
        V(0));
    MakeTweakObject<CGpoTweak>(ie, pList, L"Turn off FlipAhead Prediction", L"disable_ie_flipahead_prediction", WinVer_Win8to11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\FlipAhead",
        L"Enabled",
        V(0));
    MakeTweakObject<CGpoTweak>(ie, pList, L"Disable Sync of Feeds & Slices", L"disable_ie_feeds_sync", WinVer_WinXPto11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Feeds",
        L"BackgroundSyncStatus",
        V(0));
    MakeTweakObject<CGpoTweak>(ie, pList, L"Disable Compatibility View", L"disable_ie_compatibility_view", WinVer_WinXPto11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\BrowserEmulation",
        L"DisableSiteListEditing",
        V(1));
    MakeTweakObject<CGpoTweak>(ie, pList, L"Disable First Run Wizard", L"disable_ie_firstrun_wizard", WinVer_WinXPto11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Main",
        L"DisableFirstRunCustomize",
        V(1));
    //MakeTweakObject<CGpoTweak>(ie, pList, L"Set Blank Stat Page", WinVer_WinXPto11,
    //    L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Main",
    //    L"Start Page",
    //    "about:blank");
    MakeTweakObject<CGpoTweak>(ie, pList, L"Keep New Tabs Empty", L"keep_ie_newtabs_empty", WinVer_WinXPto11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\TabbedBrowsing",
        L"NewTabPageShow",
        V(0));
    MakeTweakObject<CGpoTweak>(ie, pList, L"Disable IE Infodelivery Update Check", L"disable_ie_update_check", WinVer_WinXPto11,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Infodelivery\\Restrictions", 
        L"NoUpdateCheck", 
        V(1));
    MakeTweakObject<CRegTweak>(ie, pList, L"Disable AutoComplete Append Completion (User)", L"disable_autocomplete_append_user", WinVer_Win10to11,
        L"HKCU\\Software\\Microsoft\\Internet Explorer\\AutoComplete",
        L"Append Completion",
        V(L"no"));
    MakeTweakObject<CRegTweak>(ie, pList, L"Disable IntelliForms Ask User Prompt (User)", L"disable_intelliforms_ask_user", WinVer_Win10to11,
        L"HKCU\\Software\\Microsoft\\Internet Explorer\\IntelliForms",
        L"AskUser",
        V(0));
    MakeTweakObject<CRegTweak>(ie, pList, L"Disable DoNotTrack and FormSuggest (User)", L"disable_donottrack_formsuggest_user", WinVer_Win10to11,
        L"HKCU\\Software\\Microsoft\\Internet Explorer\\Main",
        L"DoNotTrack",
        V(1));
    MakeTweakObject<CRegTweak>(ie, pList, L"Disable Form Suggest (User)", L"disable_form_suggest_user", WinVer_Win10to11,
        L"HKCU\\Software\\Microsoft\\Internet Explorer\\Main",
        L"Use FormSuggest",
        V(L"no"));
    MakeTweakObject<CRegTweak>(ie, pList, L"Disable FormSuggest PW Ask (User)", L"disable_form_suggest_pw_ask_user", WinVer_Win10to11,
        L"HKCU\\Software\\Microsoft\\Internet Explorer\\Main",
        L"FormSuggest PW Ask",
        V(L"no"));
    MakeTweakObject<CRegTweak>(ie, pList, L"Hide New Edge Button (User)", L"hide_new_edge_button_user", WinVer_Win10to11,
        L"HKCU\\Software\\Microsoft\\Internet Explorer\\Main",
        L"HideNewEdgeButton",
        V(1));
    MakeTweakObject<CRegTweak>(ie, pList, L"Disable Search Suggestions in Address Bar (User)", L"disable_search_suggestions_user", WinVer_Win10to11,
        L"HKCU\\Software\\Microsoft\\Internet Explorer\\SearchScopes",
        L"ShowSearchSuggestionsInAddressGlobal",
        V(0));


    /*  
    *  #########################################
    *       3rd-Party Telemetry
    *  #########################################
    */

    //CTweakPtr telemetry3Cat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"3rd-Party Telemetry", L"other_telemetry");

    // *** Disable Mozilla Spyware ***
    CTweakPtr mozilla = MakeTweakObject<CTweakSet>(browserCat, pList, L"Disable Mozilla (Firefox) Telemetry", L"no_mozilla_spyware");
    MakeTweakObject<CExecTweak>(mozilla, pList, L"Disable PingSender.exe", L"disable_ping_sender", WinVer_Win7,
        L"**\\PingSender.exe"); // "C:\Program Files\Mozilla Firefox\ or C:\Program Files\Mozilla Firefox\browser\"
    MakeTweakObject<CExecTweak>(mozilla, pList, L"Disable crashreporter.exe", L"disable_crashreporter", WinVer_Win7,
        L"**\\crashreporter.exe"); // "C:\Program Files\Mozilla Firefox\ or C:\Program Files\Mozilla Firefox\browser\"

    // *** Disable Google Spyware ***
    CTweakPtr google = MakeTweakObject<CTweakSet>(browserCat, pList, L"Disable Google Chrome Telemetry", L"no_google_spyware");
    MakeTweakObject<CExecTweak>(google, pList, L"Disable Software_Reporter_Tool.exe", L"disable_software_reporter_tool", WinVer_Win7,
        L"**\\Software_Reporter_Tool.exe"); // "C:\Users\<user>\AppData\Local\Google\Chrome\User Data\SwReporter\*\"
    MakeTweakObject<CExecTweak>(google, pList, L"Disable crashpad_handler.exe", L"disable_crashpad_handler", WinVer_Win7,
        L"**\\crashpad_handler.exe"); // "C:\Program Files\Google\Chrome\Application\*\"

    // *** NVidia Telemetry ***
    //CTweakPtr nvidia = MakeTweakObject<CTweakSet>(telemetry3Cat, pList, L"Disable NVidia Telemetry", L"no_nvidia_spyware");
    //MakeTweakObject<CExecTweak>(nvidia, pList, L"Disable nvtelemetrycontainer.exe", L"disable_nvtelemetrycontainer", WinVer_Win7,
    //    L"C:\\Program Files\\NVIDIA Corporation\\NvTelemetry\\nvtelemetrycontainer.exe");


    /*  
    *  #########################################
    *          Microsoft Office
    *  #########################################
    */

    CTweakPtr officeCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Microsoft Office", L"microsoft_office");


    // *** Disable Office Telemetry 0 ***
    CTweakPtr officeTelemetry = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Telemetry Components", L"disable_office_telemetry", ETweakHint::eRecommended);
    MakeTweakObject<CTaskTweak>(officeTelemetry, pList, L"Office Automatic Updates", L"office_auto_updates", WinVer_Win7,
        L"\\Microsoft\\Office",
        L"Office Automatic Updates");
    MakeTweakObject<CTaskTweak>(officeTelemetry, pList, L"Office Automatic Updates 2.0", L"office_auto_updates_2", WinVer_Win7,
        L"\\Microsoft\\Office",
        L"Office Automatic Updates 2.0");
    MakeTweakObject<CTaskTweak>(officeTelemetry, pList, L"OfficeTelemetryAgentFallBack2016&2019", L"office_telemetryagent_fallback", WinVer_Win7,
        L"\\Microsoft\\Office",
        L"OfficeTelemetryAgentFallBack2016");
    MakeTweakObject<CTaskTweak>(officeTelemetry, pList, L"OfficeTelemetryAgentLogOn2016&2019", L"office_telemetryagent_logon", WinVer_Win7,
        L"\\Microsoft\\Office",
        L"OfficeTelemetryAgentLogOn2016");
    MakeTweakObject<CTaskTweak>(officeTelemetry, pList, L"Office ClickToRun Service Monitor", L"office_clicktorun_monitor", WinVer_Win7,
        L"\\Microsoft\\Office",
        L"Office ClickToRun Service Monitor");
    MakeTweakObject<CExecTweak>(officeTelemetry, pList, L"Disable Office Telemetry Process", L"disable_office_telemetry_process", WinVer_Win7,
        L"C:\\program files\\microsoft office\\root\\office16\\msoia.exe");

    // *** Disable Office Telemetry 1 ***
    CTweakPtr officeTelemetryOSM = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Telemetry OSM (user)", L"disable_office_osm", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(officeTelemetryOSM, pList, L"Enablelogging (user)", L"disable_office_osm_enablelogging", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM",
        L"Enablelogging",
        V(0));
    MakeTweakObject<CGpoTweak>(officeTelemetryOSM, pList, L"EnableUpload (user)", L"disable_office_osm_enableupload", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM",
        L"EnableUpload",
        V(0));
    MakeTweakObject<CGpoTweak>(officeTelemetryOSM, pList, L"EnableFileObfuscation (user)", L"disable_office_osm_fileobfuscation", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM",
        L"EnableFileObfuscation",
        V(1));

    // *** Disable Office Telemetry 2 ***
    CTweakPtr officeTelemetryCommon = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Telemetry Common", L"disable_office_common", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(officeTelemetryCommon, pList, L"sendtelemetry (user)", L"disable_office_common_sendtelemetry", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\Common\\ClientTelemetry",
        L"sendtelemetry",
        V(3));
    MakeTweakObject<CGpoTweak>(officeTelemetryCommon, pList, L"DisableTelemetry (user)", L"disable_office_common_disabletelemetry", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\Common\\ClientTelemetry",
        L"DisableTelemetry",
        V(1));
    MakeTweakObject<CGpoTweak>(officeTelemetryCommon, pList, L"qmenable (user)", L"disable_office_common_qmenable", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common",
        L"qmenable",
        V(0));
    MakeTweakObject<CGpoTweak>(officeTelemetryCommon, pList, L"sendcustomerdata (user)", L"disable_office_common_sendcustomerdata", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common",
        L"sendcustomerdata",
        V(0));
    MakeTweakObject<CGpoTweak>(officeTelemetryCommon, pList, L"updatereliabilitydata (user)", L"disable_office_common_updatereliabilitydata", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common",
        L"updatereliabilitydata",
        V(0));

    // *** Disable Office Telemetry 3 ***
    CTweakPtr officeTelemetryFeedback = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Telemetry Feedback", L"disable_office_feedback", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(officeTelemetryFeedback, pList, L"feedback (user)", L"disable_office_feedback_feedback", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common\\feedback",
        L"enabled",
        V(0));
    MakeTweakObject<CGpoTweak>(officeTelemetryFeedback, pList, L"includescreenshot (user)", L"disable_office_feedback_includescreenshot", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common\\feedback",
        L"includescreenshot",
        V(0));
    MakeTweakObject<CGpoTweak>(officeTelemetryFeedback, pList, L"ptwoptin (user)", L"disable_office_feedback_ptwoptin", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common\\ptwatson",
        L"ptwoptin",
        V(0));

    // *** Disable Office Telemetry 4 ***
    CTweakPtr officeTelemetryByApp = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Telemetry by Application", L"disable_office_byapp", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"access (user)", L"disable_office_app_access", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"accesssolution",
        V(1));
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"olk (user)", L"disable_office_app_olk", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"olksolution",
        V(1));
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"onenote (user)", L"disable_office_app_onenote", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"onenotesolution",
        V(1));
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"ppt (user)", L"disable_office_app_ppt", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"pptsolution",
        V(1));
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"project (user)", L"disable_office_app_project", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"projectsolution",
        V(1));
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"publisher (user)", L"disable_office_app_publisher", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"publishersolution",
        V(1));
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"visio (user)", L"disable_office_app_visio", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"visiosolution",
        V(1));
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"wd (user)", L"disable_office_app_wd", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"wdsolution",
        V(1));
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"xl (user)", L"disable_office_app_xl", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"xlsolution",
        V(1));

    // *** Disable Office Telemetry 5 ***
    CTweakPtr officeTelemetryByType = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Telemetry by Type", L"disable_office_bytype", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(officeTelemetryByType, pList, L"agave (user)", L"disable_office_type_agave", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedsolutiontypes",
        L"agave",
        V(1));
    MakeTweakObject<CGpoTweak>(officeTelemetryByType, pList, L"appaddins (user)", L"disable_office_type_appaddins", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedsolutiontypes",
        L"appaddins",
        V(1));
    MakeTweakObject<CGpoTweak>(officeTelemetryByType, pList, L"comaddins (user)", L"disable_office_type_comaddins", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedsolutiontypes",
        L"comaddins",
        V(1));
    MakeTweakObject<CGpoTweak>(officeTelemetryByType, pList, L"documentfiles (user)", L"disable_office_type_documentfiles", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedsolutiontypes",
        L"documentfiles",
        V(1));
    MakeTweakObject<CGpoTweak>(officeTelemetryByType, pList, L"templatefiles (user)", L"disable_office_type_templatefiles", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedsolutiontypes",
        L"templatefiles",
        V(1));

    // *** Disable Office Online Features ***
    CTweakPtr officeOnline = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Online Features", L"disable_office_online", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(officeOnline, pList, L"skydrivesigninoption (user)", L"disable_office_online_skydrive", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common\\General",
        L"skydrivesigninoption",
        V(0));
    MakeTweakObject<CGpoTweak>(officeOnline, pList, L"shownfirstrunoptin (user)", L"disable_office_online_firstrunoptin", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common\\General",
        L"shownfirstrunoptin",
        V(1));
    MakeTweakObject<CGpoTweak>(officeOnline, pList, L"disablemovie (user)", L"disable_office_online_disablemovie", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Firstrun",
        L"disablemovie",
        V(1));



    /*  
    *  #########################################
    *          Visual Studio
    *  #########################################
    */

    CTweakPtr vsCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Visual Studio", L"visual_studio");


    CTweakPtr vsTelemetry = MakeTweakObject<CTweakSet>(vsCat, pList, L"Turn off VS Telemetry", L"disable_vs_telemetry", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(vsTelemetry, pList, L"Turn off Telemetry (User)", L"disable_vs_telemetry_switch_user", WinVer_Win7,
        L"HKCU\\Software\\Microsoft\\VisualStudio\\Telemetry",
        L"TurnOffSwitch",
        V(1));
    MakeTweakObject<CGpoTweak>(vsTelemetry, pList, L"Turn off PerfWatson2.exe", L"disable_vs_perfwatson2", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\VisualStudio\\SQM",
        L"OptIn",
        V(0));
	MakeTweakObject<CExecTweak>(vsTelemetry, pList, L"Block VCTIP.EXE", L"disable_vs_vctip", WinVer_Win7,
        L"C:\\Program Files**\\VC\\Tools\\MSVC\\**\\VCTIP.EXE");


    // *** Turn off the Feedback button ***
    CTweakPtr vsFeedback = MakeTweakObject<CTweakSet>(vsCat, pList, L"Turn off the Feedback button", L"disable_vs_feedback", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(vsFeedback, pList, L"DisableFeedbackDialog", L"disable_vs_feedback_dialog", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\VisualStudio\\Feedback",
        L"DisableFeedbackDialog",
        V(1));
    MakeTweakObject<CGpoTweak>(vsFeedback, pList, L"DisableEmailInput", L"disable_vs_email_input", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\VisualStudio\\Feedback",
        L"DisableEmailInput",
        V(1));
    MakeTweakObject<CGpoTweak>(vsFeedback, pList, L"DisableScreenshotCapture", L"disable_vs_screenshot_capture", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\VisualStudio\\Feedback",
        L"DisableScreenshotCapture",
        V(1));



#if 0
    /*  
    *  #########################################
    *              Testing
    *  #########################################
    */

    CTweakPtr testCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Test", L"test_tweaks");


    // *** Disable Store ***
    CTweakPtr tests = MakeTweakObject<CTweakSet>(testCat, pList, L"Tests", L"test_set");
    MakeTweakObject<CRegTweak>(tests, pList, L"Test", L"test_tweak_1", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Test",
        L"TestValue",
        V(1),
        ETweakHint::eNone);

#endif

    // HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{B2FE1952-0186-46C3-BAEC-A80AA35AC5B8}_NvTelemetry
    //"C:\Windows\SysWOW64\RunDll32.EXE" "C:\Program Files\NVIDIA Corporation\Installer2\InstallerCore\NVI2.DLL",UninstallPackage NvTelemetry

    // HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Uninstall\OneDriveSetup.exe
    //taskkill /f /im OneDrive.exe
    //%SystemRoot%\System32\OneDriveSetup.exe /uninstall
    //%SystemRoot%\SysWOW64\OneDriveSetup.exe /uninstall

    //[HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows Defender\Features]
    //"TamperProtection" = dword:00000000


#ifdef DUMP_TO_INI
    delete g_TweaksIni;
    g_TweaksIni = NULL;
#endif

    return pRoot;
}