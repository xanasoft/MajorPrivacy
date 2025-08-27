# Changelog
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).










## [0.98.4] - 2025-08-27

### Added 
- added failsafe mechanisms to the Kernel Isolator driver
  - the driver can now detect if windows was booted in safe mode and then it will not enforce any rules, this allows the user to recover from a misconfiguration which breaks the system
  - the driver is counting unsuccessfull boot atempts and after 3 failed boots it will automatically load the last good rule set, after 5 failed boots it will not load any rule set at all
- added hold & refresh button to most view
- added option flatten program view
- added program item missing event
- added option to also sign the CA which issued a signign certificate
- added menu option to traffic view to directly block domains in the dns filter or programs in the firewall
- added option to hide default windows firewall rules
- added option to hide default windows libraries in the module load view
- added online updater
- added missing 1st start wizard pages
- added more ui config options
- added option to prevetn hibernation when protected secure volumes are mounted
 
### Changed
- GUI is now multi threaded, this allows the GUI to be more responsive
- improved image checking mechanism
- improved status page
- Redesigned the user controlled Code Integrity verification mechanism
  - Instead of using C:\ProgramData\Xanasoft\MajorPrivacy\sig_db files all user trusted hashes and certtificates are now stored in the registry alongside the protected configuration
  - Caution: the new code does not automatically import old signatures, you need to re-authorize your files
- improved log cleanup handling
- improved ui performance arounf exec records

### Fixed
- fixed BSOD issue with remote file systems
- fixed issue with translation list not being filled
- fixed issue with accept firewall rule change from summary page
- fixed Hardlinked program runs outside the enclave [#49](https://github.com/xanasoft/MajorPrivacy/issues/49)
- fixed resource leak in open handle list
- fixed bug in thread pool
- fixed many minor UI glitsches
- fixed UPX compressed files causing repeated Exec Notifications [#76](https://github.com/xanasoft/MajorPrivacy/issues/76)
- fixed issues with image signature verification
- fixed issues with updating host names
- fixed sometimes image protection being aplpied to not protected prcesses
- fixed issue with notification window



## [0.98.3] - 2025-08-04

### Added
- added the user as aditional parameter to resource access rules [#44](https://github.com/xanasoft/MajorPrivacy/issues/44)

### Changed
- Reduced CPU usage of UI and Service
- ImBox no longer updates container file timestamps when accessing an secure encrypted volume
- Improved windows firewall rule handling on windows 23H2 and later for Store Apps

### Fixed
- fixed issue in ImBox.exe old volumes wont be mountable anymore! Use previouse build to save data then re create volumes with turent or later build.
- fixed Dat Editor: Tree view expansion fails in subsequent .dat files after opening the first [#61](https://github.com/xanasoft/MajorPrivacy/issues/61)
- fixed Execution Monitor shows no entries when Time filter is selected [#60](https://github.com/xanasoft/MajorPrivacy/issues/60)
- fixed Only the last added group is available in the "Add to Group" menu [#48](https://github.com/xanasoft/MajorPrivacy/issues/48)
- fixed Directory Listing and Read actions permits folder creation [#68](https://github.com/xanasoft/MajorPrivacy/issues/68)


 

## [0.98.2] - 2025-07-22

### Added
- added mechanism for blocking driver events
- added JsEngine allowing to create scripted rules
  - currently most of teh script PI is not yet implemented more api's will come in the next builds
- added a summary page
  - displays the current status of the system
  - identified privacy issues
  - relevant events
  - full trace log
- adde windows firewall rule change detection mechanism
  - allows to detect rule changes and restore them to the original state

### Changed
- Switched to VS2022 toolset
- Switched to Qt 6
- improved notification window

### Fixed
- bsod issue with driver
- fixed Files signed with Security > Sign File are added to the database with a forward slash [#73](https://github.com/xanasoft/MajorPrivacy/issues/73)
- fixed Error when deleting folder with signatures in Signature Database [#74](https://github.com/xanasoft/MajorPrivacy/issues/74)
- fixed [UI] Tweak descriptions are not showing up [#69](https://github.com/xanasoft/MajorPrivacy/issues/69)
- fixed issue with volume rules loading



## [0.98.1a] - 2025-05-23

### Fixed
- fixed [Driver/Service] KernelIsolator registry keys reset when launched by PrivacyAgent, losing custom settings [#71](github.com/xanasoft/MajorPrivacy/issues/71)



## [0.98.1] - 2025-05-23

### Added
- enabled translation support
  - let me know in the forum if you want to help with translations or add your own language, already added
	- added German AI translation
	- added Polish AI translation
	- added Turkish translation (Thanks offhub)
	- added Simplified Chinese translation
- added MS VC Redistributable to the installer (required for ImDisk UI)
- added translation support for tweaks

### Fixed
- fixed issue with installer not removing the driver
- cleaned up tweaks
- fixed issue with network shares [#47](https://github.com/xanasoft/MajorPrivacy/issues/47)
  - Note: network locations are not checked for process existence anymore
- fixed driver config not being saved on system shutdown
- fixed issues with protected folder display [#39](https://github.com/xanasoft/MajorPrivacy/issues/39)
- fixed issue with rules for network shared
  - Note: do not use mahhed drive letters for rules but ful UNC paths!!!
- fixed issue with rule order application [#40](https://github.com/xanasoft/MajorPrivacy/issues/40)
- fixed issue with enclave rule precedence

### Changed
- refactored driver code



## [0.98.0] - 2025-05-06

### Added
- added installer
- added token integrity level elevation to enclave processes allowing for better UIPI (User Interface Privilege Isolation)
- added driver unload protection
- added secure desktop prompting capability to require trusted user confirmation to shutdown Core components
- added copy path to accesstree
- added approve/restore tweaks buttons to tweak view
- added resource access restriction tweaks
- added execution control tweaks
- added firewall rule tweaks
 
### Changed
- improved tweaks view
- tweak status is now saved to disk

### Fixed
- fixed BSOD bug in driver when agent crashed during process start in an enclave




## [0.97.2] - 2025-04-24

### Added
- added DNS filter server
- added support for firewall rule templates; these allow defining windows firewall rule templates with wildcard enabled paths
- log and trace data memory usage column per program item
- added total service memory usage indicator
- added trace presets per process
- added option to allow debugging of processes in an enclave (disabled security!!!)
- added operation filter to ingress view
- added expand/collapse all button to program tree

### Changed
- Reworked path rule handling to the new mechanism
  - the mechanism can now use more complex rules; for now, it's set to simple DOS pattern. Later, this will be changed, and an import mechanism added
- firewall pop up now ignores incoming connection attempts
- refactored project structure
- improved and cleaned up framework
- improved and cleaned up variant implementation
- log and trace data are now stored in per program memory pools

### Fixed
- deadlock when cleaning up program list
- fixed crash when stopping service while UI is still active




## [0.97.1] - 2025-03-04

### Added
- added dll injection mechanism
- added optional user password cache
- added more tray menu options
- added additional filters to network views

### Changed
- reworked and improved signature verification
- moved ingress record to separate file
- renamed AccessLog.dat to AccessRecord.dat and TrafficLog.dat to TrafficRecord.dat
  - note: as this is still in beta, the old files won't be loaded or cleaned up automatically
  - you can delete them manually from C:\ProgramData\Xanasoft\MajorPrivacy
- by default, no records (Traffic, Execution, Ingress, Access) are stored to disk anymore
- changed the services ini file name to PrivacyAgent.ini
- the pop up notification no longer shows redundant entries

### Fixed
- fixed ZwLockRegistryKey usage incompatibility with HVCI
- fixed process protection related ui crashes around dll loading
- fixed crash when setting firewall filtering mode




## [0.97.0] - 2025-01-11

### Added
- added option to add folders to the Secure Volumes view for easier handling of folder security
- added option to rename Secure Volumes / Protected Folders
- added progress dialog when loading larger data sets from the agent
- added temporary rules
- added options to disable trace/access logging
- added options to configure trace/access log limits
- added tweak info panel
- added centralized signature DB folder (C:\ProgramData\Xanasoft\MajorPrivacy\sig_db\) [#21](https://github.com/xanasoft/MajorPrivacy/issues/21)
  - this allows creating user signatures without modifying folders of 3rd party applications
  - Note: a *.mpsig file in the application folder will still take precedence
- added ability to sign a certificate instead of a file
- added auto name generation to rule creation dialog
- added process audit rule; keeps an eye on the process as if it would be protected but without any protection
- files can now be signed from the library view
- explorer file properties dialog can now be opened from the library/module view
- added icons to main panel tabs
- added program picker to rule creation dialogs
- added UserKey Signature based config protection [#28](https://github.com/xanasoft/MajorPrivacy/issues/28)
- added ZwLockRegistryKey based UserKey protection
  - Note: when engaged the driver can not be uninstalled untill after reboot
- added dedicated enclave configuration
- added a process view based side panel to the enclave view
- added icons to the enclave view
- added context menu to the enclave view
- added color coding to trace, rule, and other views
- made access mask columns human readable
- added signature db viewer

### Changed
- reworked internal path handling, this changes the file versions hence *.dat files from previous builds will be discarded
- renamed *.sig files to *.mpsig and enhanced the format to make it future proof, old files wont be loaded anymore
- Streamlined the APIs

### Fixed
- fixed issue saving rules by the driver
- fixed issues with some tweaks




## [0.96.2] - 2024-12-01

### Added
- added access tree cleanup, removed all entries refering not longer or never existing reosurces
- added option to clear individual trace logs of program items
- added option to ignore invalid resource accessa tempts (bad path syntax, etc)
- last network activity values are now saved to the programs.dat
- program item column counting all accessed files for any program item
- added automated Access log and trace log cleanup, default retention time is 14 days
- added detection of missing program files thay are indicated in gray in the program tree
- added clean up option to remove missing program items
- added custom user defined program groups
- added option to clear the ignore list
- added option to run privacy agent as service
- added mount manager viewer
- added browse button to program editor window
- added icon picker to program editor window
- added secure volume specific access panel

### Changed
- replaced the Access tree with a new dynamic implementaion more suitable for the user case
- the program tree now can display different column selections for each page
- reworked nt to dos path resolution
- improved maintenance menu
- improved program item deletion

### Fixed
- improved driver loading added workaround for outdated driver already being installed
- fixed consistency issue in CTreeItemModel
- fixed crash in CTraceModel
- fixed issue when a service changes its BinaryPath
- fixed issue not loading patterns for installed win32 programs
- fixed performance issues with *.dat viewer



## [0.96.1] - 2024-11-12

### Added
- access log is now saved to disk
- added option to trace registry accesses
- added dat file viewer
- added auto scroll to trace logs
- added filters to program view
- added new window layouts

### Changed
- improved tree behavior (double click to expand/colapse all sub branches)
- improved access tree behavior significantly
- improved traffic view
- changed Programs.dat format
  - WARNING: the old file will be discarded and not impprted

### Fixed
- fixed minor issue in driver post op cleanup
- fixed driver incompatibility with windows 10
- fixed issues with slow startup causing error messages
- fixed issues with reparse point resolution



## [0.96.0] - 2024-10-29

### Added
- added option to clean up all agent logs
- addes view filters and toolbars to various lists

### Changed
- reworked volume rule handling, rules can now be stored in $mpsys$ file in the volume root itself
  - such rules can not be altered when the volume is not mounted adding protection against maliciouse modifications
- reworked password handoff to imbox to make it more secure
- improved mount error handling
- improved GUI
- improved access logging; allowed operations are now logged from the post op with status
  - this allows ignoring access attempts to non existing objects (see settings)
- improved finder bar
- when starting, the agent now enumerates all loaded libraries
 
### Fixed
- fixed issues with volume unmounting
- fixed issues with driver communication
- fixed dnscache related memory leak
- fixed a race condition in the process list



## [0.95.0] - 2024-06-30

### Added
- added driver based process protection
- added driver based process controll rules
- added driver based file and folder protection
  - access to selected files and folders can be blocked or restricted
  - restriction can allow only folder browsing but not file reading, or make paths read-only
- added encrypted file container support
  - file access rules can be set to auto-apply to loaded containers, restricting access to trusted processes only
- added open files list
- added file access trace log
- added file access monitor; using trace data it builds an access tree for easier visualizatoin
- added running process tree
- added loaded modules (dlls) list for each process
- added execution monitor listing what starts what
- added ingress monitor listing which processes accessed other processes and with which access mask
- added execution trace log
- added dns cache viewer
- added user key generation and file signing mechanism



## [0.90.0] - 2023-12-31

### Added
- Created a new Qt based UI
- Implemented a new driver to facilitate advanced isolation mechanisms

### Changed
- Ported PrivateWin10 functionality to C++ 
