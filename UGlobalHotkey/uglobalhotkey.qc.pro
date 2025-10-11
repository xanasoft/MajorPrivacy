QT = core gui
unix {
    QT += gui-private
}
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = UGlobalHotkey
TEMPLATE = lib
CONFIG += c++11

# Switch ABI to export (vs import, which is default)
DEFINES += UGLOBALHOTKEY_LIBRARY

include(uglobalhotkey-headers.pri)
include(uglobalhotkey-sources.pri)
include(uglobalhotkey-libs.pri)

MY_ARCH=$$(build_arch)
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
