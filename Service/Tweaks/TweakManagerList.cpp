#include "pch.h"
#include "Tweak.h"
#include "../Library/Common/DbgHelp.h"

// Windows Versions
#define WinVer_Win2k        SWinVer(2195)
#define WinVer_WinXP        SWinVer(2600)
#define WinVer_WinXPonly    SWinVer(2600, 3790)
#define WinVer_WinXPto7     SWinVer(2600, 7601)
#define WinVer_WinXPto8     SWinVer(2600, 9600)
#define WinVer_Win6         SWinVer(6002)
#define WinVer_Win6to7      SWinVer(6002, 7601)
#define WinVer_Win7         SWinVer(7601)
#define WinVer_Win7to81     SWinVer(7601, 9600)
#define WinVer_Win8         SWinVer(9200)
#define WinVer_Win81        SWinVer(9600)
#define WinVer_Win81only    SWinVer(9600, 9600)
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
// Windows 11
#define WinVer_Win11        SWinVer(22000)
#define WinVer_Win11_22H2   SWinVer(22621)
#define WinVer_Win23H2      SWinVer(22631)


template <class _Ty, class... _Types>
CTweakPtr MakeTweakObject(const CTweakPtr& pParent, std::map<std::wstring, CTweakPtr>* pList, _Types&&... _Args)
{
    CTweakPtr pTweak = std::make_shared<_Ty>(std::forward<_Types>(_Args)...);
    std::dynamic_pointer_cast<CTweakList>(pParent)->AddTweak(pTweak);
    pTweak->SetParent(pParent);
    if (pList) {
        bool bInserted = pList->insert(std::make_pair(pTweak->GetName(), pTweak)).second;
        ASSERT(bInserted); // names must be unique
    }
    //DbgPrint(L"Names[\"%s\"] = tr(\"%s\");\n", pTweak->GetName().c_str(), pTweak->GetName().c_str());
    return pTweak;
}

CTweakPtr InitKnownTweaks(std::map<std::wstring, CTweakPtr>* pList)
{
    CTweakPtr pRoot = std::make_shared<CTweakGroup>(L"");
    


    /*  
    *  #########################################
    *       Telemetry & Error reporting
    *  #########################################
    */

    CTweakPtr telemetryCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Telemetry & Error Reporting"); //, "windows_telemetry");


    // *** Telemetry ***
    CTweakPtr telemetry = MakeTweakObject<CTweakSet>(telemetryCat, pList, L"Disable Telemetry", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(telemetry, pList, L"Minimize Telemetry", WinVer_Win10EE,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
        L"AllowTelemetry",
        0);
    MakeTweakObject<CGpoTweak>(telemetry, pList, L"Disable App. Compat. Telemetry", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat",
        L"AITEnable",
        0);
    MakeTweakObject<CGpoTweak>(telemetry, pList, L"Do not Show Feedback Notifications", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
        L"DoNotShowFeedbackNotifications",
        1);
    MakeTweakObject<CSvcTweak>(telemetry, pList, L"Disable Telemetry Service (DiagTrack-Listener)", WinVer_Win7,
        L"DiagTrack");
    MakeTweakObject<CRegTweak>(telemetry, pList, L"Disable AutoLogger-Diagtrack-Listener", WinVer_Win10,
        L"SYSTEM\\CurrentControlSet\\Control\\WMI\\Autologger\\AutoLogger-Diagtrack-Listener",
        L"Start",
        0);
    MakeTweakObject<CSvcTweak>(telemetry, pList, L"Disable Push Service", WinVer_Win10,
        L"dmwappushservice");
    MakeTweakObject<CGpoTweak>(telemetry, pList, L"Don't AllowDeviceNameInTelemetry", WinVer_Win1803,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
        L"AllowDeviceNameInTelemetry",
        0,
        ETweakHint::eNone);
    MakeTweakObject<CTaskTweak>(telemetry, pList, L"Device Census", WinVer_Win7,
        L"\\Microsoft\\Windows\\Device Information",
        L"Device");
    //Disable file Diagtrack-Listener.etl
    //(Disable file diagtrack.dll)
    //(Disable file BthTelemetry.dll)


    // *** AppCompat ***
    CTweakPtr appExp = MakeTweakObject<CTweakSet>(telemetryCat, pList, L"Disable Application Experience", ETweakHint::eRecommended);
    MakeTweakObject<CTaskTweak>(appExp, pList, L"Disable Application Experience Tasks", WinVer_Win6,
        L"\\Microsoft\\Windows\\Application Experience",
        L"*");
    MakeTweakObject<CSvcTweak>(appExp, pList, L"Disable Application Experience Service", WinVer_Win7to81,
        L"AeLookupSvc");
    //MakeTweakObject<CSvcTweak>(appExp, pList, L"Disable Application Information Service", WinVer_Win7, // this breaks UAC
    //    L"Appinfo");
    MakeTweakObject<CGpoTweak>(appExp, pList, L"Turn off Steps Recorder", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\windows\\AppCompat",
        L"DisableUAR",
        1);
    MakeTweakObject<CGpoTweak>(appExp, pList, L"Turn off Inventory Collector", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\windows\\AppCompat",
        L"DisableInventory",
        1);
    MakeTweakObject<CFSTweak>(appExp, pList, L"Disable CompatTelRunner.exe", WinVer_Win6,
        L"%SystemRoot%\\System32\\CompatTelRunner.exe");
    MakeTweakObject<CFSTweak>(appExp, pList, L"Disable DiagTrackRunner.exe", WinVer_Win7to81,
        L"%SystemRoot%\\CompatTel\\diagtrackrunner.exe");


    // *** CEIP ***
    CTweakPtr ceip = MakeTweakObject<CTweakSet>(telemetryCat, pList, L"Disable CEIP", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(ceip, pList, L"Turn of CEIP", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\SQMClient\\Windows",
        L"CEIPEnable",
        0);
    MakeTweakObject<CTaskTweak>(ceip, pList, L"Disable all CEIP Tasks", WinVer_Win6,
        L"\\Microsoft\\Windows\\Customer Experience Improvement Program",
        L"*");
    MakeTweakObject<CGpoTweak>(ceip, pList, L"Disable IE CEIP", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\SQM",
        L"DisableCustomerImprovementProgram",
        0,  // must be 0 according to https://getadmx.com/?Category=Windows_10_2016&Policy=Microsoft.Policies.InternetExplorer::SQM_DisableCEIP
        ETweakHint::eNone);
    MakeTweakObject<CGpoTweak>(ceip, pList, L"Disable Live Messenger CEIP", WinVer_WinXPto7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Messenger\\Client",
        L"CEIP",
        2,
        ETweakHint::eNone);
    //  Set USER GPO "SOFTWARE\\Policies\\Microsoft\\Messenger\\Client" "CEIP" "2"(deprecated)


    // *** Error Reporting ***
    CTweakPtr werr = MakeTweakObject<CTweakSet>(telemetryCat, pList, L"Disable Error Reporting", ETweakHint::eRecommended);
    MakeTweakObject<CRegTweak>(werr, pList, L"Turn off Windows Error Reporting", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\Windows Error Reporting",
        L"Disabled",
        1);
    MakeTweakObject<CGpoTweak>(werr, pList, L"Turn off Windows Error Reporting (GPO)", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Error Reporting",
        L"Disabled",
        1);
    MakeTweakObject<CGpoTweak>(werr, pList, L"Do not Report Errors", WinVer_WinXPonly,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\PCHealth\\ErrorReporting",
        L"DoReport",
        0);
    MakeTweakObject<CSvcTweak>(werr, pList, L"Disable Error Reporting Service", WinVer_Win6,
        L"WerSvc");
    MakeTweakObject<CTaskTweak>(werr, pList, L"Disable Error Reporting Tasks", WinVer_Win6,
        L"\\Microsoft\\Windows\\Windows Error Reporting",
        L"*");
    //(Disable file WerSvc.dll)
    //(Disable file wer*.exe)


    // *** Kernel Crash Dumps ***
    CTweakPtr kdmp = MakeTweakObject<CTweakSet>(telemetryCat, pList, L"Disable Kernel Crash Dumps", ETweakHint::eRecommended);
    MakeTweakObject<CRegTweak>(kdmp, pList, L"Disable System Crash Dumps", WinVer_Win2k,
        L"SYSTEM\\CurrentControlSet\\Control\\CrashControl",
        L"CrashDumpEnabled",
        0,
        ETweakHint::eNone);


    // *** Other Diagnostics ***
    CTweakPtr diag = MakeTweakObject<CTweakSet>(telemetryCat, pList, L"Other Diagnostics");
    MakeTweakObject<CGpoTweak>(diag, pList, L"Turn of MSDT", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\ScriptedDiagnosticsProvider\\Policy",
        L"DisableQueryRemoteServer",
        0); // must be 0 according to https://getadmx.com/?Category=Windows_10_2016&Policy=Microsoft.Policies.MSDT::MsdtSupportProvider
    MakeTweakObject<CGpoTweak>(diag, pList, L"Turn off Online Assist", WinVer_Win6to7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Assistance\\Client\\1.0",
        L"NoOnlineAssist",
        1);
    // Set USER GPO @"Software\Policies\Microsoft\Assistance\Client\1.0" "NoOnlineAssist" "1" (deprecated)
    // Set USER GPO @"Software\Policies\Microsoft\Assistance\Client\1.0" "NoExplicitFeedback" "1" (deprecated)
    // Set USER GPO @"Software\Policies\Microsoft\Assistance\Client\1.0" "NoImplicitFeedback" "1" (deprecated)
    MakeTweakObject<CSvcTweak>(diag, pList, L"Disable diagnosticshub Service", WinVer_Win10,
        L"diagnosticshub.standardcollector.service");
    MakeTweakObject<CGpoTweak>(diag, pList, L"Do not Update Disk Health Model", WinVer_Win1709,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\StorageHealth",
        L"AllowDiskHealthModelUpdates",
        0);
    MakeTweakObject<CTaskTweak>(diag, pList, L"Disable Disk Diagnostics Task", WinVer_Win6,
        L"\\Microsoft\\Windows\\DiskDiagnostic",
        L"Microsoft-Windows-DiskDiagnosticDataCollector",
        ETweakHint::eNone);



    /*  
    *  #########################################
    *              Cortana & search
    *  #########################################
    */

    CTweakPtr searchCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Search & Cortana"); //, "windows_search");


    // *** Disable Cortana ***
    CTweakPtr cortana = MakeTweakObject<CTweakSet>(searchCat, pList, L"Disable Cortana");
    MakeTweakObject<CGpoTweak>(cortana, pList, L"Disable Cortana App", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"AllowCortana",
        0);
    MakeTweakObject<CGpoTweak>(cortana, pList, L"Forbid the Use of Location", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"AllowSearchToUseLocation",
        0);

    // *** Disable Web Search ***
    CTweakPtr webSearch = MakeTweakObject<CTweakSet>(searchCat, pList, L"Disable Online Search", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(webSearch, pList, L"Disable Web Search", WinVer_WinXP,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"DisableWebSearch",
        1);
    MakeTweakObject<CGpoTweak>(webSearch, pList, L"Disable Connected Web Search", WinVer_Win81,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"ConnectedSearchUseWeb",
        0);
    MakeTweakObject<CGpoTweak>(webSearch, pList, L"Enforce Search Privacy", WinVer_Win81only,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"ConnectedSearchPrivacy",
        3);
    MakeTweakObject<CGpoTweak>(webSearch, pList, L"Disable Search Box Suggestions", WinVer_Win20H1,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Windows Search",
        L"DisableSearchBoxSuggestions",
        1);

    // *** Disable Search ***
    CTweakPtr search = MakeTweakObject<CTweakSet>(searchCat, pList, L"Disable Search");
    MakeTweakObject<CSvcTweak>(search, pList, L"Disable Windows Search Service", WinVer_Win6,
        L"WSearch");
    MakeTweakObject<CFSTweak>(search, pList, L"Disable searchUI.exe", WinVer_Win10,
        L"c:\\windows\\SystemApps\\Microsoft.Windows.Cortana_cw5n1h2txyewy\\SearchUI.exe",
        ETweakHint::eNone);
    MakeTweakObject<CGpoTweak>(search, pList, L"Remove Search Box", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Search",
        L"SearchboxTaskbarMode",
        0);
    MakeTweakObject<CGpoTweak>(webSearch, pList, L"Don't Update Search Companion", WinVer_WinXPto7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\SearchCompanion",
        L"DisableContentFileUpdates",
        1);



    /*  
    *  #########################################
    *              Windows Defender
    *  #########################################
    */

    CTweakPtr defenderCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Windows Defender"); //, "microsoft_defender" );


    // *** Disable Defender ***
    CTweakPtr defender = MakeTweakObject<CTweakSet>(defenderCat, pList, L"Disable Defender");
    MakeTweakObject<CGpoTweak>(defender, pList, L"Disable Real-Time Protection", WinVer_Win10, // starting with windows 1903 this must be disabled first to turn off tamper protection
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection",
        L"DisableRealtimeMonitoring",
        1);
    MakeTweakObject<CGpoTweak>(defender, pList, L"Turn off Anti Spyware", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender",
        L"DisableAntiSpyware",
        1);
    //MakeTweakObject<CGpoTweak>(defender, pList, L"Turn off Anti Virus", WinVer_Win6, // legacy not neede
    //    L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender",
    //    L"DisableAntiVirus",
    //    1);
    MakeTweakObject<CGpoTweak>(defender, pList, L"Torn off Application Guard", WinVer_Win10EE,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\AppHVSI",
        L"AllowAppHVSI_ProviderSet",
        0);
    MakeTweakObject<CRegTweak>(defender, pList, L"Disable SecurityHealthService Service", WinVer_Win1703,
        L"SYSTEM\\CurrentControlSet\\Services\\SecurityHealthService",
        L"Start",
        4,
        ETweakHint::eNone);

    // *** Silence Defender ***
    CTweakPtr defender2 = MakeTweakObject<CTweakSet>(defenderCat, pList, L"Silence Defender", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(defender2, pList, L"Disable Enhanced Notifications", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Reporting",
        L"DisableEnhancedNotifications",
        1);
    MakeTweakObject<CGpoTweak>(defender2, pList, L"Disable Spynet Reporting", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Spynet",
        L"SpynetReporting",
        0);
    MakeTweakObject<CGpoTweak>(defender2, pList, L"Don't Submit Samples", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Spynet",
        L"SubmitSamplesConsent",
        2);
    //Set GPO @"SOFTWARE\Policies\Microsoft\Windows Defender\Signature Updates" "DefinitionUpdateFileSharesSources" DELET (nope)
    //Set GPO @"SOFTWARE\Policies\Microsoft\Windows Defender\Signature Updates" "FallbackOrder" "SZ:FileShares"(nope)

    // *** Disable SmartScreen ***
    CTweakPtr screen = MakeTweakObject<CTweakSet>(defenderCat, pList, L"Disable SmartScreen", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(screen, pList, L"Turn off SmartScreen", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"EnableSmartScreen",
        0);
    // Set GPO @"Software\Policies\Microsoft\Windows\System" "ShellSmartScreenLevel" DEL
    MakeTweakObject<CGpoTweak>(screen, pList, L"Disable App Install Control", WinVer_Win1703,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Defender\\SmartScreen",
        L"ConfigureAppInstallControlEnabled",
        0);
    MakeTweakObject<CFSTweak>(screen, pList, L"Disable smartscreen.exe", WinVer_Win6,
        L"%SystemRoot%\\System32\\smartscreen.exe",
        ETweakHint::eNone);
    // Set GPO @"Software\Policies\Microsoft\Windows Defender\SmartScreen" "ConfigureAppInstallControl" DEL
    MakeTweakObject<CGpoTweak>(screen, pList, L"No SmartScreen for Store Apps", WinVer_Win81,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\AppHost",
        L"EnableWebContentEvaluation",
        0);
    // Edge
    MakeTweakObject<CGpoTweak>(screen, pList, L"Disable SmartScreen for Edge", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\PhishingFilter",
        L"EnabledV9",
        0,
        ETweakHint::eNone);
    //Set GPO @"Software\Policies\Microsoft\MicrosoftEdge\PhishingFilter" "PreventOverride" "0"
    // Edge
    MakeTweakObject<CGpoTweak>(screen, pList, L"Disable SmartScreen for IE", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\PhishingFilter",
        L"EnabledV9",
        0,
        ETweakHint::eNone);

    // *** MRT-Tool ***
    CTweakPtr mrt = MakeTweakObject<CTweakSet>(defenderCat, pList, L"Silence MRT-Tool", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(mrt, pList, L"Don't Report Infections", WinVer_Win2k,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MRT",
        L"DontReportInfectionInformation",
        1);
    MakeTweakObject<CGpoTweak>(mrt, pList, L"Disable MRT-Tool", WinVer_Win2k,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MRT",
        L"DontOfferThroughWUAU",
        1,
        ETweakHint::eNone);



    /*  
    *  #########################################
    *              Windows Update
    *  #########################################
    */
            
    CTweakPtr updateCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Windows Update");


    // *** Disable Automatic Windows Update ***
    CTweakPtr noAutoUpdates = MakeTweakObject<CTweakSet>(updateCat, pList, L"Disable Automatic Windows Update");
    MakeTweakObject<CGpoTweak>(noAutoUpdates, pList, L"Disable Automatic Update", WinVer_WinXPto8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU",
        L"NoAutoUpdate",
        1);
    MakeTweakObject<CGpoTweak>(noAutoUpdates, pList, L"Disable Automatic Update (Enterprise Only)", WinVer_Win10EE, // since 10 this tweak only works on enterprise and alike
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU",
        L"NoAutoUpdate",
        1);

    // *** Disable Driver Update ***
    CTweakPtr drv = MakeTweakObject<CTweakSet>(updateCat, pList, L"Disable Driver Update");
    MakeTweakObject<CGpoTweak>(drv, pList, L"Don't Update Drivers With", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
        L"ExcludeWUDriversInQualityUpdate",
        1);
    MakeTweakObject<CGpoTweak>(drv, pList, L"Don't get Device Info from Web", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Device Metadata",
        L"PreventDeviceMetadataFromNetwork",
        1);

    // *** Disable Windows Update ***
    CTweakPtr noUpdates = MakeTweakObject<CTweakSet>(updateCat, pList, L"Disable Windows Update Services");
    MakeTweakObject<CSvcTweak>(noUpdates, pList, L"Disable Windows Update Service", WinVer_WinXP,
        L"wuauserv");
    MakeTweakObject<CSvcTweak>(noUpdates, pList, L"Windows Update Medic Service", WinVer_Win1803,
        L"WaaSMedicSvc");
    MakeTweakObject<CSvcTweak>(noUpdates, pList, L"Update Orchestrator Service", WinVer_Win10,
        L"UsoSvc");

    // *** Disable Driver Update ***
    CTweakPtr blockWU = MakeTweakObject<CTweakSet>(updateCat, pList, L"Disable Access to Update Servers");
    MakeTweakObject<CGpoTweak>(blockWU, pList, L"DoNotConnectToWindowsUpdateInternetLocations", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
        L"DoNotConnectToWindowsUpdateInternetLocations",
        1);
    MakeTweakObject<CGpoTweak>(blockWU, pList, L"WUServer", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
        L"WUServer",
        L"\" \"");
    MakeTweakObject<CGpoTweak>(blockWU, pList, L"WUStatusServer", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
        L"WUStatusServer",
        L"\" \"");
    MakeTweakObject<CGpoTweak>(blockWU, pList, L"UpdateServiceUrlAlternate", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
        L"UpdateServiceUrlAlternate",
        L"\" \"");
    MakeTweakObject<CGpoTweak>(blockWU, pList, L"UseWUServer", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU",
        L"UseWUServer",
        1);

    // *** Disable Windows Insider Service ***
    CTweakPtr noInsider = MakeTweakObject<CTweakSet>(updateCat, pList, L"Disable Windows Insider Services");
    MakeTweakObject<CSvcTweak>(noInsider, pList, L"Disable Windows Insider Service", WinVer_Win10,
        L"wisvc");



    /*  
    *  #########################################
    *          Privacy & Advertisement
    *  #########################################
    */

    CTweakPtr privacyCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Privacy & Advertisement"); //, "windows_privacy");


    // *** Disable Advertisement ***
    CTweakPtr privacy = MakeTweakObject<CTweakSet>(privacyCat, pList, L"Disable Advertisement", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(privacy, pList, L"Turn off Advertising ID", WinVer_Win81,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AdvertisingInfo",
        L"DisabledByGroupPolicy",
        1);
    MakeTweakObject<CGpoTweak>(privacy, pList, L"Turn off Consumer Experiences", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent",
        L"DisableWindowsConsumerFeatures",
        1);
    MakeTweakObject<CGpoTweak>(privacy, pList, L"Limit Tailored Experiences", WinVer_Win1703,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Privacy",
        L"TailoredExperiencesWithDiagnosticDataEnabled",
        0);
    // Limit Tailored Experiences (user)
    MakeTweakObject<CGpoTweak>(privacy, pList, L"Limit Tailored Experiences (user)", WinVer_Win1703,
        L"HKCU\\Software\\Policies\\Microsoft\\Windows\\CloudContent",
        L"DisableTailoredExperiencesWithDiagnosticData",
        1);
    // Turn off Windows Spotlight
    MakeTweakObject<CGpoTweak>(privacy, pList, L"Turn off Windows Spotlight", WinVer_Win10,
        L"HKCU\\SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent",
        L"DisableWindowsSpotlightFeatures",
        1);
    MakeTweakObject<CGpoTweak>(privacy, pList, L"Disable OnlineTips in Settings", WinVer_Win1709,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
        L"AllowOnlineTips",
        0);
    MakeTweakObject<CGpoTweak>(privacy, pList, L"Don't show Windows Tips", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\CloudContent",
        L"DisableSoftLanding",
        1);

    // *** Disable Unsolicited App Installation ***
    CTweakPtr delivery = MakeTweakObject<CTweakSet>(privacyCat, pList, L"Disable Unsolicited App Installation", ETweakHint::eRecommended);
    MakeTweakObject<CRegTweak>(delivery, pList, L"ContentDeliveryAllowed", WinVer_Win1607,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"ContentDeliveryAllowed",
        0);
    MakeTweakObject<CRegTweak>(delivery, pList, L"OemPreInstalledAppsEnabled", WinVer_Win1607,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"OemPreInstalledAppsEnabled",
        0);
    MakeTweakObject<CRegTweak>(delivery, pList, L"PreInstalledAppsEnabled", WinVer_Win1607,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"PreInstalledAppsEnabled",
        0);
    //MakeTweakObject<CRegTweak>(delivery, pList, L"PreInstalledAppsEverEnabled (user)", WinVer_Win1607,
    //    L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
    //    L"PreInstalledAppsEverEnabled",
    //    0);
    MakeTweakObject<CRegTweak>(delivery, pList, L"SilentInstalledAppsEnabled", WinVer_Win1607,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"SilentInstalledAppsEnabled",
        0);
    MakeTweakObject<CRegTweak>(delivery, pList, L"SoftLandingEnabled (user)", WinVer_Win1607,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
        L"SoftLandingEnabled",
        0);
    //MakeTweakObject<CRegTweak>(delivery, pList, L"SubscribedContentEnabled (user)", WinVer_Win1607,
    //    L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\ContentDeliveryManager",
    //    L"SubscribedContentEnabled",
    //    0);

    // *** No Lock Screen ***
    CTweakPtr lockScr = MakeTweakObject<CTweakSet>(privacyCat, pList, L"Disable Lock Screen", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(lockScr, pList, L"Don't use Lock Screen", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Personalization",
        L"NoLockScreen",
        1);
    MakeTweakObject<CGpoTweak>(lockScr, pList, L"Enable LockScreen Image", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Personalization",
        L"LockScreenOverlaysDisabled",
        1);
    MakeTweakObject<CGpoTweak>(lockScr, pList, L"Set LockScreen Image", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Personalization",
        L"LockScreenImage",
        L"C:\\windows\\web\\screen\\lockscreen.jpg");

    // *** No Personalization ***
    CTweakPtr spying = MakeTweakObject<CTweakSet>(privacyCat, pList, L"No Personalization", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(spying, pList, L"Disable input personalization", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\InputPersonalization",
        L"AllowInputPersonalization",
        0);
    MakeTweakObject<CGpoTweak>(spying, pList, L"Disable Test Collection", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\InputPersonalization",
        L"RestrictImplicitTextCollection",
        1);
    MakeTweakObject<CGpoTweak>(spying, pList, L"Disable Inc Collection", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\InputPersonalization",
        L"RestrictImplicitInkCollection",
        1);
    MakeTweakObject<CGpoTweak>(spying, pList, L"Disable Linguistic Data Collection", WinVer_Win1803,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\TextInput",
        L"AllowLinguisticDataCollection",
        0);
    MakeTweakObject<CGpoTweak>(spying, pList, L"Disable Handwriting Error Reports", WinVer_Win6to7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\HandwritingErrorReport",
        L"PreventHandwritingErrorReports",
        1);
    MakeTweakObject<CGpoTweak>(spying, pList, L"Disable Handwriting Data Sharing", WinVer_Win6to7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\TabletPC",
        L"PreventHandwritingDataSharing",
        1);
    // Set USER GPO @"Software\Policies\Microsoft\Windows\HandwritingErrorReports" "PreventHandwritingErrorReports" "1" (deprecated)
    // Set USER GPO @"Software\Policies\Microsoft\Windows\TabletPC" "PreventHandwritingDataSharing" "1" (deprecated)	

    // *** Disable Location ***
    CTweakPtr location = MakeTweakObject<CTweakSet>(privacyCat, pList, L"Protect Location");
    MakeTweakObject<CGpoTweak>(location, pList, L"Disable Location Provider", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\LocationAndSensors",
        L"DisableLocation",
        1);
    MakeTweakObject<CGpoTweak>(location, pList, L"Don't Share Lang List", WinVer_Win10,
        L"Control Panel\\International\\User Profile",
        L"HttpAcceptLanguageOptOut",
        1);

    // *** No Registration ***
    CTweakPtr privOther = MakeTweakObject<CTweakSet>(privacyCat, pList, L"No Registration", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(privOther, pList, L"Disable KMS GenTicket", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows NT\\CurrentVersion\\Software Protection Platform",
        L"NoGenTicket",
        1);
    MakeTweakObject<CGpoTweak>(privOther, pList, L"Disable Registration", WinVer_WinXP,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Registration Wizard Control",
        L"NoRegistration",
        1);
    MakeTweakObject<CSvcTweak>(privOther, pList, L"Disable LicenseManager Service", WinVer_Win10,
        L"LicenseManager",
        ETweakHint::eNone);

    // *** No Push Notifications ***
    CTweakPtr push = MakeTweakObject<CTweakSet>(privacyCat, pList, L"No Push Notifications", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(push, pList, L"Disable Cloud Notification", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\PushNotifications",
        L"NoCloudApplicationNotification",
        1);
    MakeTweakObject<CGpoTweak>(push, pList, L"Disable Cloud Notification (user)", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\PushNotifications",
        L"NoCloudApplicationNotification",
        1);
    /*CTweakPtr oobe = MakeTweakObject<CTweakSet>(privacyCat, pList, L"Disable OOBE Post Setup", ETweakHint::eRecommended);
    oobe.Add(new Tweak("Disable OOBE Post Setup 1", TweakType.SetRegistry, WinVer.Win19H1)
    {
        Path = @"SOFTWARE\Microsoft\Windows\CurrentVersion\CloudExperienceHost\Intent\Wireless",
        Key = "NetworkState",
        Value = 0,
        usrLevel = true
    });
    oobe.Add(new Tweak("Disable OOBE Post Setup 2", TweakType.SetRegistry, WinVer.Win19H1)
    {
        Path = @"SOFTWARE\Microsoft\Windows\CurrentVersion\CloudExperienceHost\Intent\Wireless",
        Key = "ScoobeOnFirstConnect",
        Value = 1,
        usrLevel = true
    });
    oobe.Add(new Tweak("Disable OOBE Post Setup 3", TweakType.SetRegistry, WinVer.Win19H1)
    {
        Path = @"SOFTWARE\Microsoft\Windows\CurrentVersion\CloudExperienceHost\Intent\Wireless",
        Key = "ScoobeCheckCompleted",
        Value = 1,
        usrLevel = true
    });*/



    /*  
    *  #########################################
    *          Microsoft Account
    *  #########################################
    */

    CTweakPtr accountCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Microsoft Account"); //, "microsoft_account");


    // *** Disable OneDrive ***
    CTweakPtr onedrive = MakeTweakObject<CTweakSet>(accountCat, pList, L"Disable OneDrive", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(onedrive, pList, L"Disable OneDrive Usage", WinVer_Win10, // WinVer.Win7
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\OneDrive",
        L"DisableFileSyncNGSC",
        1);
    MakeTweakObject<CGpoTweak>(onedrive, pList, L"Silence OneDrive", WinVer_Win10, // WinVer.Win7
        L"HKLM\\SOFTWARE\\Microsoft\\OneDrive",
        L"PreventNetworkTrafficPreUserSignIn",
        1);
    // Run uninstaller
    // *** No Microsoft Accounts ***
    CTweakPtr account = MakeTweakObject<CTweakSet>(accountCat, pList, L"No Microsoft Accounts", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(account, pList, L"Disable Microsoft Accounts", WinVer_Win8, // or 10?
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
        L"NoConnectedUser",
        3);
    MakeTweakObject<CSvcTweak>(account, pList, L"Disable MS Account Login Service", WinVer_Win8, // or 10?
        L"wlidsvc",
        ETweakHint::eNone);

    // *** No Settings Sync ***
    CTweakPtr sync = MakeTweakObject<CTweakSet>(accountCat, pList, L"No Settings Sync", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(sync, pList, L"Disable Settings Sync", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\SettingSync",
        L"DisableSettingSync",
        2);
    MakeTweakObject<CGpoTweak>(sync, pList, L"Force Disable Settings Sync", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\SettingSync",
        L"DisableSettingSyncUserOverride",
        1);
    MakeTweakObject<CGpoTweak>(sync, pList, L"Disable WiFi-Sense", WinVer_Win10to1709,
        L"HKLM\\SOFTWARE\\Microsoft\\wcmsvc\\wifinetworkmanager\\config",
        L"AutoConnectAllowedOEM",
        0);

    // *** No Find My Device ***
    CTweakPtr find = MakeTweakObject<CTweakSet>(accountCat, pList, L"No Find My Device", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(find, pList, L"Don't Allow FindMyDevice", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\FindMyDevice",
        L"AllowFindMyDevice",
        0);

    // *** No Cloud Clipboard ***
    CTweakPtr ccb = MakeTweakObject<CTweakSet>(accountCat, pList, L"No Cloud Clipboard", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(ccb, pList, L"Disable Cloud Clipboard", WinVer_Win1809,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"AllowCrossDeviceClipboard",
        0);

    // *** No Cloud Messages ***
    CTweakPtr msgbak = MakeTweakObject<CTweakSet>(accountCat, pList, L"No Cloud Messages", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(msgbak, pList, L"Don't Sync Messages", WinVer_Win1709,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Messaging",
        L"AllowMessageSync",
        0);
    MakeTweakObject<CGpoTweak>(msgbak, pList, L"Don't Sync Messages (user)", WinVer_Win1709,
        L"HKLM\\SOFTWARE\\Microsoft\\Messaging",
        L"CloudServiceSyncEnabled",
        0);

    // *** Disable Activity Feed ***
    CTweakPtr feed = MakeTweakObject<CTweakSet>(accountCat, pList, L"Disable User Tracking", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(feed, pList, L"Disable Activity Feed", WinVer_Win1709,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"EnableActivityFeed",
        0);
    MakeTweakObject<CGpoTweak>(feed, pList, L"Don't Upload User Activities", WinVer_Win1709,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"UploadUserActivities",
        0);
    MakeTweakObject<CGpoTweak>(feed, pList, L"Don't Publish User Activities", WinVer_Win1709,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"PublishUserActivities",
        0);

    // *** No Cross Device Experience ***
    CTweakPtr cdp = MakeTweakObject<CTweakSet>(accountCat, pList, L"No Cross Device Experience", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(cdp, pList, L"Disable Cross Device Experience", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"EnableCdp",
        0);



    /*  
    *  #########################################
    *          Microsoft Office
    *  #########################################
    */

    CTweakPtr officeCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Microsoft Office"); //, "microsoft_office");


    // *** Disable Office Telemetry 0 ***
    CTweakPtr officeTelemetry = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Telemetry Components", ETweakHint::eRecommended);
    MakeTweakObject<CTaskTweak>(officeTelemetry, pList, L"Office Automatic Updates", WinVer_Win7,
        L"\\Microsoft\\Office",
        L"Office Automatic Updates");
    MakeTweakObject<CTaskTweak>(officeTelemetry, pList, L"Office Automatic Updates 2.0", WinVer_Win7,
        L"\\Microsoft\\Office",
        L"Office Automatic Updates 2.0");
    MakeTweakObject<CTaskTweak>(officeTelemetry, pList, L"OfficeTelemetryAgentFallBack2016&2019", WinVer_Win7,
        L"\\Microsoft\\Office",
        L"OfficeTelemetryAgentFallBack2016");
    MakeTweakObject<CTaskTweak>(officeTelemetry, pList, L"OfficeTelemetryAgentLogOn2016&2019", WinVer_Win7,
        L"\\Microsoft\\Office",
        L"OfficeTelemetryAgentLogOn2016");
    MakeTweakObject<CTaskTweak>(officeTelemetry, pList, L"Office ClickToRun Service Monitor", WinVer_Win7,
        L"\\Microsoft\\Office",
        L"Office ClickToRun Service Monitor");
    MakeTweakObject<CFSTweak>(officeTelemetry, pList, L"Disable Office Telemetry Process", WinVer_Win7,
        L"C:\\program files\\microsoft office\\root\\office16\\msoia.exe");

    // *** Disable Office Telemetry 1 ***
    CTweakPtr officeTelemetryOSM = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Telemetry OSM", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(officeTelemetryOSM, pList, L"Enablelogging", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM",
        L"Enablelogging",
        0);
    MakeTweakObject<CGpoTweak>(officeTelemetryOSM, pList, L"EnableUpload", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM",
        L"EnableUpload",
        0);
    MakeTweakObject<CGpoTweak>(officeTelemetryOSM, pList, L"EnableFileObfuscation", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM",
        L"EnableFileObfuscation",
        1);

    // *** Disable Office Telemetry 2 ***
    CTweakPtr officeTelemetryCommon = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Telemetry Common", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(officeTelemetryCommon, pList, L"sendtelemetry", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\Common\\ClientTelemetry",
        L"sendtelemetry",
        3);
    MakeTweakObject<CGpoTweak>(officeTelemetryCommon, pList, L"DisableTelemetry", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\Common\\ClientTelemetry",
        L"DisableTelemetry",
        1);
    MakeTweakObject<CGpoTweak>(officeTelemetryCommon, pList, L"qmenable", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common",
        L"qmenable",
        0);
    MakeTweakObject<CGpoTweak>(officeTelemetryCommon, pList, L"sendcustomerdata", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common",
        L"sendcustomerdata",
        0);
    MakeTweakObject<CGpoTweak>(officeTelemetryCommon, pList, L"updatereliabilitydata", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common",
        L"updatereliabilitydata",
        0);

    // *** Disable Office Telemetry 3 ***
    CTweakPtr officeTelemetryFeedback = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Telemetry Feedback", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(officeTelemetryFeedback, pList, L"feedback", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common\\feedback",
        L"enabled",
        0);
    MakeTweakObject<CGpoTweak>(officeTelemetryFeedback, pList, L"includescreenshot", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common\\feedback",
        L"includescreenshot",
        0);
    MakeTweakObject<CGpoTweak>(officeTelemetryFeedback, pList, L"ptwoptin", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common\\ptwatson",
        L"ptwoptin",
        0);

    // *** Disable Office Telemetry 4 ***
    CTweakPtr officeTelemetryByApp = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Telemetry by Application", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"access", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"accesssolution",
        1);
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"olk", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"olksolution",
        1);
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"onenote", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"onenotesolution",
        1);
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"ppt", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"pptsolution",
        1);
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"project", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"projectsolution",
        1);
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"publisher", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"publishersolution",
        1);
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"visio", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"visiosolution",
        1);
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"wd", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"wdsolution",
        1);
    MakeTweakObject<CGpoTweak>(officeTelemetryByApp, pList, L"xl", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedapplications",
        L"xlsolution",
        1);

    // *** Disable Office Telemetry 5 ***
    CTweakPtr officeTelemetryByType = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Telemetry by Type", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(officeTelemetryByType, pList, L"agave", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedsolutiontypes",
        L"agave",
        1);
    MakeTweakObject<CGpoTweak>(officeTelemetryByType, pList, L"appaddins", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedsolutiontypes",
        L"appaddins",
        1);
    MakeTweakObject<CGpoTweak>(officeTelemetryByType, pList, L"comaddins", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedsolutiontypes",
        L"comaddins",
        1);
    MakeTweakObject<CGpoTweak>(officeTelemetryByType, pList, L"documentfiles", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedsolutiontypes",
        L"documentfiles",
        1);
    MakeTweakObject<CGpoTweak>(officeTelemetryByType, pList, L"templatefiles", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\OSM\\preventedsolutiontypes",
        L"templatefiles",
        1);

    // *** Disable Office Online Features ***
    CTweakPtr officeOnline = MakeTweakObject<CTweakSet>(officeCat, pList, L"Disable Online Features", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(officeOnline, pList, L"skydrivesigninoption", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common\\General",
        L"skydrivesigninoption",
        0);
    MakeTweakObject<CGpoTweak>(officeOnline, pList, L"shownfirstrunoptin", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Common\\General",
        L"shownfirstrunoptin",
        1);
    MakeTweakObject<CGpoTweak>(officeOnline, pList, L"disablemovie", WinVer_Win7,
        L"HKCU\\Software\\Policies\\Microsoft\\Office\\16.0\\Firstrun",
        L"disablemovie",
        1);



    /*  
    *  #########################################
    *          Visual Studio
    *  #########################################
    */

    CTweakPtr vsCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Visual Studio"); //, "visual_studio");


    CTweakPtr vsTelemetry = MakeTweakObject<CTweakSet>(vsCat, pList, L"Turn off VS telemetry", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(vsTelemetry, pList, L"Turn off telemetry", WinVer_Win7,
        L"HKCU\\Software\\Microsoft\\VisualStudio\\Telemetry",
        L"TurnOffSwitch",
        1);
    MakeTweakObject<CGpoTweak>(vsTelemetry, pList, L"Turn off PerfWatson2.exe", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\VisualStudio\\SQM",
        L"OptIn",
        0);
    MakeTweakObject<CFSTweak>(vsTelemetry, pList, L"Disable Microsoft.ServiceHub.Controller.exe (Enterprise)", WinVer_Win7,
        L"%ProgramFiles(x86)%\\Microsoft Visual Studio\\2019\\Enterprise\\Common7\\ServiceHub\\controller\\Microsoft.ServiceHub.Controller.exe",
        ETweakHint::eNone);
    MakeTweakObject<CFSTweak>(vsTelemetry, pList, L"Disable Microsoft.ServiceHub.Controller.exe (Professional)", WinVer_Win7,
        L"%ProgramFiles(x86)%\\Microsoft Visual Studio\\2019\\Professional\\Common7\\ServiceHub\\controller\\Microsoft.ServiceHub.Controller.exe",
        ETweakHint::eNone);
    MakeTweakObject<CFSTweak>(vsTelemetry, pList, L"Disable Microsoft.ServiceHub.Controller.exe (Community)", WinVer_Win7,
        L"%ProgramFiles(x86)%\\Microsoft Visual Studio\\2019\\Community\\Common7\\ServiceHub\\controller\\Microsoft.ServiceHub.Controller.exe",
        ETweakHint::eNone);


    CTweakPtr vsFeedback = MakeTweakObject<CTweakSet>(vsCat, pList, L"Turn off the Feedback button", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(vsFeedback, pList, L"DisableFeedbackDialog", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\VisualStudio\\Feedback",
        L"DisableFeedbackDialog",
        1);
    MakeTweakObject<CGpoTweak>(vsFeedback, pList, L"DisableEmailInput", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\VisualStudio\\Feedback",
        L"DisableEmailInput",
        1);
    MakeTweakObject<CGpoTweak>(vsFeedback, pList, L"DisableScreenshotCapture", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\VisualStudio\\Feedback",
        L"DisableScreenshotCapture",
        1);



    /*  
    *  #########################################
    *               Various Others
    *  #########################################
    */

    CTweakPtr miscCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Various Others"); //, "windows_misc");


    // *** No Explorer AutoComplete ***
    CTweakPtr ac = MakeTweakObject<CTweakSet>(miscCat, pList, L"No Explorer AutoComplete", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(ac, pList, L"Disable Auto Suggest", WinVer_Win2k,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoComplete",
        L"AutoSuggest",
        L"no");

    // *** No Speech Updates ***
    CTweakPtr speech = MakeTweakObject<CTweakSet>(miscCat, pList, L"No Speech Updates");
    MakeTweakObject<CGpoTweak>(speech, pList, L"Don't Update SpeechModel", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Speech",
        L"AllowSpeechModelUpdate",
        0);

    // *** No Font Updates ***
    CTweakPtr font = MakeTweakObject<CTweakSet>(miscCat, pList, L"No Font Updates");
    MakeTweakObject<CGpoTweak>(font, pList, L"Don't Update Fonts", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"EnableFontProviders",
        0);

    // *** No Certificate Updates ***
    CTweakPtr cert = MakeTweakObject<CTweakSet>(miscCat, pList, L"Automatic certificate updates");
    MakeTweakObject<CGpoTweak>(cert, pList, L"Disable Certificate Auto Update", WinVer_WinXP,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\SystemCertificates\\AuthRoot",
        L"DisableRootAutoUpdate",
        1);

    // *** Disable NtpClient ***
    CTweakPtr ntp = MakeTweakObject<CTweakSet>(miscCat, pList, L"Date and Time (NTP Client)");
    MakeTweakObject<CGpoTweak>(ntp, pList, L"Disable NTP Client", WinVer_WinXP,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\W32time\\TimeProviders\\NtpClient",
        L"Enabled",
        0);

    // *** Disable Net Status ***
    CTweakPtr ncsi = MakeTweakObject<CTweakSet>(miscCat, pList, L"Disable Net Status");
    MakeTweakObject<CGpoTweak>(ncsi, pList, L"Disable Active Probeing", WinVer_Win6,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\NetworkConnectivityStatusIndicator",
        L"NoActiveProbe",
        1);

    // *** Disable Teredo IPv6 ***
    CTweakPtr teredo = MakeTweakObject<CTweakSet>(miscCat, pList, L"Disable Teredo (IPv6)");
    MakeTweakObject<CGpoTweak>(teredo, pList, L"Disable Teredo Tunneling", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\TCPIP\\v6Transition",
        L"Teredo_State",
        L"Disabled");

    // *** Disable Delivery Optimizations ***
    CTweakPtr dodm = MakeTweakObject<CTweakSet>(miscCat, pList, L"No Delivery Optimizations", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(dodm, pList, L"Disable Delivery Optimizations", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DeliveryOptimization",
        L"DODownloadMode",
        L"100");

    // *** Disable Map Updates ***
    CTweakPtr map = MakeTweakObject<CTweakSet>(miscCat, pList, L"Disable Map Updates");
    MakeTweakObject<CGpoTweak>(map, pList, L"Turn off unsolicited Maps Downloads", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Maps",
        L"AllowUntriggeredNetworkTrafficOnSettingsPage",
        0);
    MakeTweakObject<CGpoTweak>(map, pList, L"Turn off Auto Maps Update", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\Maps",
        L"AutoDownloadAndUpdateMapData",
        0);

    // *** No Internet OpenWith ***
    CTweakPtr iopen = MakeTweakObject<CTweakSet>(miscCat, pList, L"No Internet OpenWith", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(iopen, pList, L"Disable Internet OpenWith", WinVer_WinXPto7,
        L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
        L"NoInternetOpenWith",
        1);
    // Disable Internet OpenWith (user)
    MakeTweakObject<CGpoTweak>(iopen, pList, L"Disable Internet OpenWith (user)", WinVer_WinXPto7,
        L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
        L"NoInternetOpenWith",
        1);

    // *** Lockdown MS Edge ***
    CTweakPtr edge = MakeTweakObject<CTweakSet>(miscCat, pList, L"Lockdown MS Edge (non Chromium)");
    MakeTweakObject<CGpoTweak>(edge, pList, L"Don't Update Compatibility Lists", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\BrowserEmulation",
        L"MSCompatibilityMode",
        0);
    MakeTweakObject<CGpoTweak>(edge, pList, L"Set Blank Start Page", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\Internet Settings",
        L"ProvisionedHomePages",
        L"<about:blank>");
    MakeTweakObject<CGpoTweak>(edge, pList, L"Set 'DoNotTrack'", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\Main",
        L"DoNotTrack",
        1);
    MakeTweakObject<CGpoTweak>(edge, pList, L"No Password Auto Complete", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\Main",
        L"FormSuggest Passwords",
        L"no");
    MakeTweakObject<CGpoTweak>(edge, pList, L"Disable First Start Page", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\Main",
        L"PreventFirstRunPage",
        1);
    MakeTweakObject<CGpoTweak>(edge, pList, L"No Form Auto Complete", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\Main",
        L"Use FormSuggest",
        L"no");
    MakeTweakObject<CGpoTweak>(edge, pList, L"Disable AddressBar Suggestions", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\SearchScopes",
        L"ShowSearchSuggestionsGlobal",
        0);
    MakeTweakObject<CGpoTweak>(edge, pList, L"Disable AddressBar (drop down) Suggestions", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\ServiceUI",
        L"ShowOneBox",
        0);
    MakeTweakObject<CGpoTweak>(edge, pList, L"Keep New Edge Tabs Empty", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\ServiceUI",
        L"AllowWebContentOnNewTabPage",
        0);
    MakeTweakObject<CGpoTweak>(edge, pList, L"Disable Books Library Updating", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\MicrosoftEdge\\BooksLibrary",
        L"AllowConfigurationUpdateForBooksLibrary",
        0);

    // *** Lockdown IE ***
    CTweakPtr ie = MakeTweakObject<CTweakSet>(miscCat, pList, L"Lockdown Internet Explorer");
    MakeTweakObject<CGpoTweak>(ie, pList, L"Disable Enhanced AddressBar Suggestions", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer",
        L"AllowServicePoweredQSA",
        0);
    MakeTweakObject<CGpoTweak>(ie, pList, L"Turn off Browser Geolocation", WinVer_Win7,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Geolocation",
        L"PolicyDisableGeolocation",
        1);
    MakeTweakObject<CGpoTweak>(ie, pList, L"Turn off Site Suggestions", WinVer_WinXP,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Suggested Sites",
        L"Enabled",
        0);
    MakeTweakObject<CGpoTweak>(ie, pList, L"Turn off FlipAhead Prediction", WinVer_Win8,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\FlipAhead",
        L"Enabled",
        0);
    MakeTweakObject<CGpoTweak>(ie, pList, L"Disable Sync of Feeds & Slices", WinVer_WinXP,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Feeds",
        L"BackgroundSyncStatus",
        0);
    MakeTweakObject<CGpoTweak>(ie, pList, L"Disable Compatibility View", WinVer_WinXP,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\BrowserEmulation",
        L"DisableSiteListEditing",
        1);
    MakeTweakObject<CGpoTweak>(ie, pList, L"Disable First Run Wizard", WinVer_WinXP,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Main",
        L"DisableFirstRunCustomize",
        1);
    //MakeTweakObject<CGpoTweak>(ie, pList, L"Set Blank Stat Page", WinVer_WinXP,
    //    L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\Main",
    //    L"Start Page",
    //    "about:blank");
    MakeTweakObject<CGpoTweak>(ie, pList, L"Keep New Tabs Empty", WinVer_WinXP,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Internet Explorer\\TabbedBrowsing",
        L"NewTabPageShow",
        0);
 


    /*  
    *  #########################################
    *              Apps & Store
    *  #########################################
    */

    CTweakPtr appCat = MakeTweakObject<CTweakGroup>(pRoot, pList, L"Apps & Store"); //, "store_and_apps");


    // *** Disable Store ***
    CTweakPtr store = MakeTweakObject<CTweakSet>(appCat, pList, L"Disable Store");
    MakeTweakObject<CGpoTweak>(store, pList, L"Disable Store Apps", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\WindowsStore",
        L"DisableStoreApps",
        1);
    MakeTweakObject<CGpoTweak>(store, pList, L"Don't Auto Update Apps", WinVer_Win10EE,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\WindowsStore",
        L"AutoDownload",
        2);
    MakeTweakObject<CGpoTweak>(store, pList, L"Disable App Uri Handlers", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\System",
        L"EnableAppUriHandlers",
        0, ETweakHint::eNone);

    // *** Lockdown Apps ***
    CTweakPtr apps = MakeTweakObject<CTweakSet>(appCat, pList, L"Lockdown Apps", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access AccountInfo", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessAccountInfo",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Calendar", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessCalendar",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access CallHistory", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessCallHistory",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Camera", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessCamera",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Contacts", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessContacts",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Email", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessEmail",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Location", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessLocation",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Messaging", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessMessaging",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Microphone", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessMicrophone",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Motion", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessMotion",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Notifications", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessNotifications",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Radios", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessRadios",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access Tasks", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessTasks",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Access TrustedDevices", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsAccessTrustedDevices",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps get Diagnostic Info", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsGetDiagnosticInfo",
        2);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Run In Background", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsRunInBackground",
        2, ETweakHint::eNone);
    MakeTweakObject<CGpoTweak>(apps, pList, L"Don't Let Apps Sync With Devices", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\AppPrivacy",
        L"LetAppsSyncWithDevices",
        2);

    // *** Disable Mail and People ***
    CTweakPtr mail = MakeTweakObject<CTweakSet>(appCat, pList, L"Block Mail and People", ETweakHint::eRecommended);
    MakeTweakObject<CGpoTweak>(mail, pList, L"Disable Mail App", WinVer_Win10,
        L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows Mail",
        L"ManualLaunchAllowed",
        0);
    MakeTweakObject<CGpoTweak>(mail, pList, L"Hide People from Taskbar", WinVer_Win10,
        L"HKCU\\Software\\Policies\\Microsoft\\Windows\\Explorer",
        L"HidePeopleBar",
        1);


    // HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{B2FE1952-0186-46C3-BAEC-A80AA35AC5B8}_NvTelemetry
    //"C:\Windows\SysWOW64\RunDll32.EXE" "C:\Program Files\NVIDIA Corporation\Installer2\InstallerCore\NVI2.DLL",UninstallPackage NvTelemetry

    // HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Uninstall\OneDriveSetup.exe
    //taskkill /f /im OneDrive.exe
    //%SystemRoot%\System32\OneDriveSetup.exe /uninstall
    //%SystemRoot%\SysWOW64\OneDriveSetup.exe /uninstall

    //[HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows Defender\Features]
    //"TamperProtection" = dword:00000000

    /*
    Windows Registry Editor Version 5.00

    [HKEY_CURRENT_USER]

    [HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\CloudExperienceHost\Intent]

    [HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\CloudExperienceHost\Intent\Wireless]
    "NetworkState"=dword:00000000
    "ScoobeOnFirstConnect"=dword:00000001
    "ScoobeCheckCompleted"=dword:00000001
    */


    return pRoot;
}