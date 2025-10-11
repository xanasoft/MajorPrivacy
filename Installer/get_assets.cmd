rem @echo off

mkdir %~dp0\Assets
copy %~dp0\Languages.iss %~dp0\Assets\
copy %~dp0\license.txt %~dp0\Assets\
copy %~dp0\Certificate.dat %~dp0\Assets\
copy %~dp0\PrivacyAgent.ini %~dp0\Assets\
copy %~dp0\MajorPrivacy.ini %~dp0\Assets\
copy %~dp0\MajorPrivacy.iss %~dp0\Assets\
copy %~dp0\MajorPrivacyInstall.ico %~dp0\Assets\
copy %~dp0\..\MajorWallpaper.png %~dp0\Assets\
mkdir %~dp0\Assets\isl
copy %~dp0\isl\* %~dp0\Assets\isl\

