
TEMPLATE = app
TARGET = MajorPrivacy
PRECOMPILED_HEADER = pch.h

QT += core gui network widgets widgets-private concurrent core-private qml qml-private

CONFIG += lrelease

MY_ARCH=$$(build_arch)
equals(MY_ARCH, ARM64) {
#  message("Building ARM64")
  CONFIG(debug, debug|release):LIBS = -L../Build/Debug
  CONFIG(release, debug|release):LIBS = -L../Build/Release
} else:equals(MY_ARCH, x64) {
#  message("Building x64")
  CONFIG(debug, debug|release):LIBS += -L../Build/Debug
  CONFIG(release, debug|release):LIBS += -L../Build/Release
#  QT += winextras
} else {
#  message("Building x86")
  CONFIG(debug, debug|release):LIBS = -L../Build/Debug
  CONFIG(release, debug|release):LIBS = -L../Build/Release
#  QT += winextras
}


CONFIG(debug, debug|release):!contains(QMAKE_HOST.arch, x86_64):LIBS += -L../Build/Debug
CONFIG(release, debug|release):!contains(QMAKE_HOST.arch, x86_64):LIBS += -L../Build/Release

LIBS += -lNtdll -lAdvapi32 -lOle32 -lUser32 -lShell32 -lGdi32 -lNetapi32 -lIphlpapi -lws2_32 -lbcrypt -lVersion -ltaskschd -lMiscHelpers -lqtsingleapp -lUGlobalHotkey -lGeneralLibrary

CONFIG(release, debug|release):{
QMAKE_CXXFLAGS_RELEASE = $$QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO
QMAKE_LFLAGS_RELEASE = $$QMAKE_LFLAGS_RELEASE_WITH_DEBUGINFO
}


equals(MY_ARCH, ARM64) {
#  message("Building ARM64")
  CONFIG(debug, debug|release):DESTDIR = ../Build/Debug
  CONFIG(release, debug|release):DESTDIR = ../Build/Release
} else:equals(MY_ARCH, x64) {
#  message("Building x64")
  CONFIG(debug, debug|release):DESTDIR = ../Build/Debug
  CONFIG(release, debug|release):DESTDIR = ../Build/Release
} else {
#  message("Building x86")
  CONFIG(debug, debug|release):DESTDIR = ../Build/Debug
  CONFIG(release, debug|release):DESTDIR = ../Build/Release
}


INCLUDEPATH += .
INCLUDEPATH += ../phnt/include
DEPENDPATH += .
MOC_DIR += .
OBJECTS_DIR += debug
UI_DIR += .
RCC_DIR += .



include(MajorPrivacy.pri)

win32:RC_FILE = Resources/MajorPrivacy.rc


