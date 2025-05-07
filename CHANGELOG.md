# Changelog
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).







## [0.98.1] - 2025-05-

### Fixed
- fixed issue with installer not removing the driver
- 


## [0.98.0] - 2025-05-06

### Added
- added installer
- add add token integrity level elevation to enclave processes allowing for better UIPI (User Interface Privilege Isolation)
- added driver unload protection
- added secure desktop prompting capability to require trusted user OK to shutdown Core components
- added copy path to accesstree
- added approve/restore tweaks buttons to tweak view
- addes resource access restriction tweaks
- addes execution controll tweaks
- addes firewall rule tweaks
 
### Changed
- improved tweaks view
- tweak status is now saved to disk

### Fixed
- fixed BSOD bug in driver when agent crashed during process start in an enclave




## [0.97.2] - 2025-04-24

### Added
- added DNS filter server
- added support for firewall rule templates, these allow to define windows firewall rule tempaltes with wildcard enabled paths
- log and trace data memory usage column per program item
- added total service memory usage indicator
- added trace presets per process
- added option to allow debugging of processes in an enclave (disabled security!!!)
- added operation filter to ingress view
- added expand/colapse all button to program tree

### Changed
- Reworked path rule handling the new mechanism
  - the emchanism can now ise more complex rules, for now its set to simple dos pattern later this will be changed and an import machnism added
- firewall pop up now ignores incomming connection atempts
- refactored project structure
- improved and cleaned up framwork
- improved and cleaned up variant implementation
- log and trace data are now stored in per program memory pools

### Fixed
- deadlock when cleaning up program list
- fixed crash when stoping service while UI is still active




## [0.97.1] - 2025-03-04

### Added
- added dll injection mechanism
- added optional user password cache
- added more tray menu options
- added additional filters to network views

### Changed
- reworked and improved signature verification
- moved ingress regord to separate file
- renamed AccessLog.dat to AccessRecord.dat and TrafficLog.dat to TrafficRecord.dat
  - note: as this is still in beta the old files wont be loaded or cleaned up autoamtically
  - you can delete them manually from C:\ProgramData\Xanasoft\MajorPrivacy
- by default no records (Traffic, Execution, Ingress, Access) are stored to disk anymore
- changed the services ini file name to PrivacyAgent.ini
- the pop up notification no longer shows redundant entries

### Fixed
- fixed ZwLockRegistryKey usage incompatybility with HVCI
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
  - this allows to create user signatures without modifying fodlers of 3rd party applications
  - Mote: a *.mpsig file in the application folder will still take precedence
- added ability to sign a certificate instead of a file
- added auto name generation to rule creation dialog
- addded process audit rule, keeps an eye on the process as if it would be protected but without any protection
- files can now be signed from the library view
- explorer file propertie dialog can now be opened from the library/module view
- added icons to main panel tabs
- added program picker to rule creation dialogs
- added UserKey Signature based config protection [#28](https://github.com/xanasoft/MajorPrivacy/issues/28)
- added ZwLockRegistryKey based UserKey protection
  - Note: when engaged the driver can not be uninstalled untill after reboot
- added dedicated enclave configuration
- added a process view based side panel to the enclave view
- added icons to the enclave view
- added context menu to the enclave view
- added color coding to trace, rule, and otehr views
- made access mask columns human readable
- added signature db viewer

### Changed
- reworked internal path handling, this changes the file versions hance *.dat files from previosue builds will be discarded
- renamed *.sig files to *.mpsig and enachanced the format to make it future proof, old files wont be loaded anymore
- Streamlined the APIs

### Fixed
- fixed issue saving rules by the driver
- fixed issues with soem tweaks




## [0.96.2] - 2024-12-01

### Added
- added access tree cleanup, removed all entries refering not longer or never existing reosurces
- added option to clear individual trace logs of program items
- added option to ignore invalid resource accessa tempts (bad path syntax, etc)
- last network activity values are now saved to the programs.dat
- program item column counting all accessed files for any program item
- added autoamted Access log and trace log cleanup, default retention time is 14 days
- added detection of missing program files thay are indicated in gray in the program tree
- added clean up option to remove missing program items
- added custom user defined program groups
- added option to clear teh ignore list
- added option to run privacy agent as service
- added mount manager viewer
- added browse button to program editor window
- added icon picker to program editor window
- added secure volume specifiv access panel

### Changed
- replaced the Access tree with a new dynamic implementaion more siutable for the user case
- the program tree now can display different column selections for each page
- reworked nt to dos path resolution
- improved maintenance menu
- improved program item deletion

### Fixed
- improved driver loading added workarounf for outdated driver already being installed
- fixed consistency issue in CTreeItemModel
- fixed crash in CTraceModel
- fixed issue when a service changes its BinaryPath
- fixed issue not loading patterns for installed win32 programs
- fixed performance issues with *.dat viewer



## [0.96.1] - 2024-11-12

### Added
- access log is now saved to disk
- added option to trace registry accesses
- added dat fiel viewer
- added auto scroll to trace logs
- added fitlers to program view
- added new window layouts

### Changed
- improved tree behavioure (double click to expand/colapse all sub branches)
- improved access tree behavioure a lot
- improved traffic view
- changed Programs.dat format
  - WARNING: the old file will be discarded and not impprted

### Fixed
- fixed minor issue in driver post op cleanup
- fixed driver incompatybility with windows 10
- fixed issues with slow startup causing error messages
- fixed issues with reparse point resolution



## [0.96.0] - 2024-10-29

### Added
- added option to clean up all agent logs
- addes view filters and toolbars to variouse lists

### Changed
- reworked volume rule handling, rules can now be stored in $mpsys$ file in the volume root itself
  - such rules can not be altered when the volume is not mounted adding protection against maliciouse modifications
- reworked password handoff to imbox to make it more secure
- improved mount error handling
- improved GUI
- improved access logging, allowed operations are now logged from the post op with status
  - this allows to ignore access atempts to non existing objects (see settings)
- improved finder bar
- when starting the agent now enums all loaded libraries
 
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
  - access to sellected files and folders can be blocked or restricted
  - restriction can be to only allow folder browsing but not file reading or making faths read only
- added encrypted file container support
  - fille access rules can be set up to auto apply to loaded containers restricting access to trusted processes only
- added open files list
- added file access trace log
- added file access monitor, using trace data it builds a access tree for easier visualizatoin
- added running process tree
- added loaded modules (dlls) list for each process
- added execution monitor listing what starts what
- added ingress minotir listing which processes accessed other processes and with which access mask
- added execution trace log
- added dns cache viever
- added user key generation and file signing mechnism



## [0.90.0] - 2023-12-31

### Added
- Created a new Qt based UI
- Implemented a new driver to facilitate advanced isolation mechanisms

### Changed
- Ported PrivateWin10 functionality to C++ 
