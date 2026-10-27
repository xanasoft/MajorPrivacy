#pragma once
/*
 * Copyright 2023-2025 David Xanatos, xanasoft.com
 *
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
#define IMDISK_TYPE_USER                0x00000400
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

//
// One run of ImBox.exe, or several against the same shared section.
//
// ExecImDisk creates a section, fills it, runs ImBox once and throws it away,
// which is all a mount or a password change needs. The header editor needs
// more: it reads a header out, works on it, and writes it back, and the header
// carries the volume's master key.
//
// Keeping the section open across all of that is what keeps that key out of
// ordinary process memory. The section is page-locked when it is created and
// wiped when it is closed, the editor works in place inside it, and nothing is
// ever copied to the heap. That also makes the section long enough to hold a
// 64 KiB header, which the fixed 4 KiB SSection is not - see SHeaderData.
//
// Run() may be called more than once. Each run overwrites SSection's first
// kilobyte, because the password going in and the SCryptInfo coming out share
// it, so SetPassword() belongs before every call rather than once at the start.
//
class LIBRARY_EXPORT CImBoxSession
{
public:
	CImBoxSession();
	~CImBoxSession();

	CImBoxSession(const CImBoxSession&) = delete;
	CImBoxSession& operator=(const CImBoxSession&) = delete;

	// uHeaderLen 0 gives a plain 4 KiB SSection, anything else adds room for a
	// header of that length behind it.
	NTSTATUS			Create(ULONG uHeaderLen = 0);

	bool				IsValid() const { return m_pSection != NULL; }
	struct SSection*	Section() const { return m_pSection; }
	struct SHeaderData*	Header() const;
	ULONG				Size() const { return m_uSize; }

	// All three go straight into the locked section and nowhere else; the
	// caller's own copy is its own to clear.
	void				SetPassword(const struct SPassword* pPassword);
	void				SetPassword(const wchar_t* pPassword, size_t uSize, int iKdf, int iSlot = SPASS_SLOT_ANY);

	//
	// Hand over an already derived key rather than a password, so that ImBox
	// does not run the KDF again. Only useful to a caller that has just run it
	// itself - the header editor, between reading a header and writing it back.
	//
	void				SetRawKey(const void* pKey, size_t uSize);

	NTSTATUS			Run(const std::wstring& ImageFile, const std::wstring& Command);

protected:
	HANDLE				m_hSection = NULL;
	PVOID				m_pMapping = NULL;
	struct SSection*	m_pSection = NULL;
	ULONG				m_uSize = 0;
};