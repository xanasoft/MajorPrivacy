call "%~dp0..\Installer\buildVariables.cmd" %*

REM @ECHO OFF

REM echo %*
REM IF "%~4" == "" ( set "openssl_version=3.4.0" ) ELSE ( set "openssl_version=%~4" )
REM IF "%~3" == "" ( set "qt6_version=6.3.1" ) ELSE ( set "qt6_version=%~3" )
REM IF "%~2" == "" ( set "qt_version=5.15.16" ) ELSE ( set "qt_version=%~2" )

IF "%openssl_version:~0,3%" == "1.1" ( set "sslMajorVersion=1_1" ) ELSE ( set "sslMajorVersion=3" )

IF %1 == x86 (
  set archPath=Win32
  call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
  set qtPath=%~dp0..\..\Qt\%qt_version%\msvc2022
  set instPath=%~dp0..\Build\Release
)
IF %1 == x64 (
  set archPath=x64
  call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
REM  set qtPath=%~dp0..\..\Qt\%qt6_version%\msvc2022_64
  set qtPath=%~dp0..\..\Qt\%qt_version%\msvc2022_64
  set instPath=%~dp0..\Build\Release
)
IF %1 == ARM64 (
  set archPath=ARM64
  call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsamd64_arm64.bat"
  set qtPath=%~dp0..\..\Qt\%qt6_version%\msvc2022_arm64
  set instPath=%~dp0..\Build\Release
  set "sslMajorVersion=1_1"
)

set redistPath=%VCToolsRedistDir%\%1\Microsoft.VC143.CRT
REM set redistPath="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Redist\MSVC\%VCToolsVersion%\%1\Microsoft.VC143.CRT"

@echo on

echo inst: %instPath%
echo arch: %archPath%
echo redistr: %redistPath%

mkdir %instPath%

ECHO Copying VC Runtime files
copy "%redistPath%\*" %instPath%\


ECHO Copying Qt libraries
echo Copying Qt6 libraries...
echo Copying Qt6Core.dll
copy %qtPath%\bin\Qt6Core.dll %instPath%\
echo Copying Qt6Gui.dll
copy %qtPath%\bin\Qt6Gui.dll %instPath%\
echo Copying Qt6Network.dll
copy %qtPath%\bin\Qt6Network.dll %instPath%\
echo Copying Qt6Widgets.dll
copy %qtPath%\bin\Qt6Widgets.dll %instPath%\
echo Copying Qt6Qml.dll
copy %qtPath%\bin\Qt6Qml.dll %instPath%\

echo Done copying libraries.

mkdir %instPath%\platforms
copy %qtPath%\plugins\platforms\qdirect2d.dll %instPath%\platforms\
copy %qtPath%\plugins\platforms\qminimal.dll %instPath%\platforms\
copy %qtPath%\plugins\platforms\qoffscreen.dll %instPath%\platforms\
copy %qtPath%\plugins\platforms\qwindows.dll %instPath%\platforms\

mkdir %instPath%\styles
rem Qt 6.7+
copy %qtPath%\plugins\styles\qmodernwindowsstyle.dll %instPath%\styles\

mkdir %instPath%\tls
copy %qtPath%\plugins\tls\qcertonlybackend.dll %instPath%\tls\
copy %qtPath%\plugins\tls\qopensslbackend.dll %instPath%\tls\
copy %qtPath%\plugins\tls\qschannelbackend.dll %instPath%\tls\

ECHO Copying OpenSSL libraries
IF %archPath% == Win32 (
  copy /y %~dp0OpenSSL\Win_x86\bin\libssl-%sslMajorVersion%.dll %instPath%\
  copy /y %~dp0OpenSSL\Win_x86\bin\libcrypto-%sslMajorVersion%.dll %instPath%\
)
IF NOT %archPath% == Win32 (
  copy /y %~dp0OpenSSL\Win_%archPath%\bin\libssl-%sslMajorVersion%-%archPath%.dll %instPath%\
  copy /y %~dp0OpenSSL\Win_%archPath%\bin\libcrypto-%sslMajorVersion%-%archPath%.dll %instPath%\
)


ECHO Copying 7zip library
copy /y %~dp07-Zip\7-Zip-%archPath%\7z.dll %instPath%\


ECHO Copying translations

mkdir %instPath%\translations\
copy /y %~dp0..\Build_MajorPrivacy_x64\Release\MajorPrivacy_*.qm %instPath%\translations\
copy /y %~dp0\qttranslations\qm\qt_*.qm %instPath%\translations\
copy /y %~dp0\qttranslations\qm\qtbase_*.qm %instPath%\translations\
copy /y %~dp0\qttranslations\qm\qtmultimedia_*.qm %instPath%\translations\

IF NOT %archPath% == ARM64 (
REM IF %archPath% == Win32 (
copy /y %qtPath%\translations\qtscript_*.qm %instPath%\translations\
copy /y %qtPath%\translations\qtxmlpatterns_*.qm %instPath%\translations\
)

"C:\Program Files\7-Zip\7z.exe" a %instPath%\translations.7z %instPath%\translations\*
rmdir /S /Q %instPath%\translations\


REM cleanup

rmdir /S /Q %instPath%\x64\ksidll
rmdir /S /Q %instPath%\x64\KernelIsolator
rem del /Q %instPath%\x64\ksi.pdb
rem del /Q %instPath%\x64\KernelIsolator.pdb

rmdir /S /Q %instPath%\ARM64\ksidll
rmdir /S /Q %instPath%\ARM64\KernelIsolator
rem del /Q %instPath%\ARM64\ksi.pdb
rem del /Q %instPath%\ARM64\KernelIsolator.pdb

