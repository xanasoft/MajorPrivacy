set inno_path=%~dp0.\InnoSetup

mkdir %~dp0.\Output

"%inno_path%\ISCC.exe" /O%~dp0.\Output %~dp0.\MajorPrivacy.iss /DMyAppVersion=0.98.5 /DMyAppArch=x64

pause