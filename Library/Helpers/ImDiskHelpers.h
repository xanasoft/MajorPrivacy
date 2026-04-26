#pragma once
/*
 * Copyright 2023-2025 David Xanatos, xanasoft.com
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "../lib_global.h"

#include <string>
#include <vector>
#include "..\..\Framework\Common\Buffer.h"
#include "../../ImBox/ImBox.h"

// Base names for device objects created in \Device
#define IMDISK_DEVICE_DIR_NAME              L"\\Device"
#define IMDISK_DEVICE_BASE_NAME             IMDISK_DEVICE_DIR_NAME  L"\\ImDisk"
#define IMDISK_CTL_DEVICE_NAME              IMDISK_DEVICE_BASE_NAME L"Ctl"

#define IMDISK_DRIVER_VERSION               0x0103


// Base value for the IOCTL's.
#define FILE_DEVICE_IMDISK                  0x8372

#define IOCTL_IMDISK_QUERY_VERSION          ((ULONG) CTL_CODE(FILE_DEVICE_IMDISK, 0x800, METHOD_BUFFERED, 0))
#define IOCTL_IMDISK_CREATE_DEVICE          ((ULONG) CTL_CODE(FILE_DEVICE_IMDISK, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS))
#define IOCTL_IMDISK_QUERY_DEVICE           ((ULONG) CTL_CODE(FILE_DEVICE_IMDISK, 0x802, METHOD_BUFFERED, 0))
#define IOCTL_IMDISK_QUERY_DRIVER           ((ULONG) CTL_CODE(FILE_DEVICE_IMDISK, 0x803, METHOD_BUFFERED, 0))
#define IOCTL_IMDISK_REFERENCE_HANDLE       ((ULONG) CTL_CODE(FILE_DEVICE_IMDISK, 0x804, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS))
#define IOCTL_IMDISK_SET_DEVICE_FLAGS       ((ULONG) CTL_CODE(FILE_DEVICE_IMDISK, 0x805, METHOD_BUFFERED, 0))
#define IOCTL_IMDISK_REMOVE_DEVICE          ((ULONG) CTL_CODE(FILE_DEVICE_IMDISK, 0x806, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS))
#define IOCTL_IMDISK_IOCTL_PASS_THROUGH     ((ULONG) CTL_CODE(FILE_DEVICE_IMDISK, 0x807, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS))
#define IOCTL_IMDISK_FSCTL_PASS_THROUGH     ((ULONG) CTL_CODE(FILE_DEVICE_IMDISK, 0x808, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS))
#define IOCTL_IMDISK_GET_REFERENCED_HANDLE  ((ULONG) CTL_CODE(FILE_DEVICE_IMDISK, 0x809, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS))

// Device type flags
#define IMDISK_TYPE_FILE                0x00000100
#define IMDISK_TYPE_VM                  0x00000200
#define IMDISK_TYPE_PROXY               0x00000300
#define IMDISK_TYPE(x)                  ((ULONG)(x) & 0x00000F00)

typedef struct _IMDISK_CREATE_DATA
{
    ULONG           DeviceNumber;
    DISK_GEOMETRY   DiskGeometry;
    LARGE_INTEGER   ImageOffset;
    ULONG           Flags;
    WCHAR           DriveLetter;
    USHORT          FileNameLength;
    WCHAR           FileName[1];
} IMDISK_CREATE_DATA, *PIMDISK_CREATE_DATA;


// Driver readiness
LIBRARY_EXPORT bool IsImDiskDriverReady();

// Device enumeration
LIBRARY_EXPORT bool ImDiskGetDeviceList(std::vector<ULONG>& DeviceList);
LIBRARY_EXPORT WCHAR ImDiskFindFreeDriveLetter();

// Device operations
LIBRARY_EXPORT HANDLE ImDiskOpenDeviceByName(const std::wstring& DevicePath, DWORD AccessMode);
LIBRARY_EXPORT HANDLE ImDiskOpenDeviceByNumber(ULONG DeviceNumber, DWORD AccessMode);
LIBRARY_EXPORT HANDLE ImDiskOpenDeviceByMountPoint(const std::wstring& MountPoint, DWORD AccessMode);

// Device queries
LIBRARY_EXPORT bool ImDiskQueryDevice(HANDLE Device, IMDISK_CREATE_DATA* CreateData, ULONG CreateDataSize);
LIBRARY_EXPORT std::wstring ImDiskQueryDeviceProxy(const std::wstring& DevicePath);
LIBRARY_EXPORT ULONGLONG ImDiskQueryDeviceSize(const std::wstring& DevicePath);
LIBRARY_EXPORT WCHAR ImDiskQueryDriveLetter(const std::wstring& DevicePath);

// Device removal - replaces CLI calls
LIBRARY_EXPORT bool ImDiskRemoveDevice(ULONG DeviceNumber, bool ForceDismount);
LIBRARY_EXPORT bool ImDiskForceRemoveDevice(ULONG DeviceNumber);
LIBRARY_EXPORT bool ImDiskForceRemoveDevice(HANDLE Device);

// Device extension
LIBRARY_EXPORT bool ImDiskExtendDevice(ULONG DeviceNumber, ULONGLONG ExtendSize);

LIBRARY_EXPORT PVOID AllocSecureMemory(HANDLE hProcess, size_t uSize);

LIBRARY_EXPORT NTSTATUS UpdateCommandLine(HANDLE hProcess, NTSTATUS(*Update)(std::wstring& s, PVOID p), PVOID param);

LIBRARY_EXPORT NTSTATUS CreateSecureSection(HANDLE* phSection, PVOID* ppMapping, SIZE_T uSize);

LIBRARY_EXPORT NTSTATUS ExecImDisk(const std::wstring& ImageFile, const wchar_t* pPassword, int iKdf, const std::wstring& Command, bool bWrite = false, CBuffer* pBuffer = NULL, USHORT uId = 0);