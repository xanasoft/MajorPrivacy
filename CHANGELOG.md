# Changelog
All notable changes to this project will be documented in this file.
This project adheres to [Semantic Versioning](http://semver.org/).

## [1.0.0] - 2024-??-??

// TODO 




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