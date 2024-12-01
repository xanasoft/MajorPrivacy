# Changelog
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).



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
