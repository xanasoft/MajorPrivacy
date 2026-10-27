@echo on

set "qt_version=6.8.4"
set "qt6_version=6.8.4"
set "openssl_version=3.4.0"
set "ghSsl_user=xanasoft"
set "ghSsl_repo=openssl-builds"
set "ghQtBuilds_user=xanasoft"
set "ghQtBuilds_repo=qt-builds"
rem set "ghQtBuilds_hash_x86=6b25cb24755b158e580e73b7889a659e093012cc3d6428d2bfbff8ebad238cf2"
set "ghQtBuilds_hash_x64=94e56b85a640a785415fbd2cacf70be341d16ce09fd96d98bce7e369c933ebc9"



rem the archive names change between qt releases, keep them in one place
rem x64 is the release only build, it carries a -release suffix and ships no debug libs
set "ghQtBuilds_file_x86=qt-everywhere-%qt_version%-Windows_7-MSVC2022-x86.7z"
set "ghQtBuilds_file_x64=qt-everywhere-%qt_version%-Windows_7-MSVC2022-x86_64-release.7z"
rem since 6.8.4 the qt.io submodule archives are named ...-everywhere-opensource-src-...
set "qtTranslations_file=qttranslations-everywhere-opensource-src-%qt6_version%.zip"
