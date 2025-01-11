#pragma once
#include "../lib_global.h"
#include "../Status.h"

class LIBRARY_EXPORT CNtPathMgr
{
public:
	CNtPathMgr();
	~CNtPathMgr();

	static CNtPathMgr* Instance();

	static void Dispose();

	std::wstring TranslateDosToNtPath(std::wstring DosPath);
	std::wstring TranslateNtToDosPath(std::wstring NtPath);

	struct SDrive
	{
		WCHAR DriveLetter = 0;
		//WCHAR SN[10] = { 0 };
		std::wstring DevicePath;
		bool Subst = false;
	};

	std::vector<std::shared_ptr<SDrive>> GetDosDrives() { std::unique_lock lock(m_Mutex); return m_Drives; }
	std::shared_ptr<const SDrive> GetDriveForPath(const std::wstring& Path);
	std::shared_ptr<const SDrive> GetDriveForUncPath(const std::wstring& Path, ULONG* OutPrefixLen);
	std::shared_ptr<const SDrive> GetDriveForLetter(WCHAR DriveLetter);

	struct SMount
	{
		std::wstring MountPoint; // Dos Path
		std::wstring DevicePath;
	};

	std::list<std::shared_ptr<SMount>> GetMountPoints() { std::unique_lock lock(m_Mutex); return m_Mounts; }
	std::shared_ptr<const SMount> GetMountForPath(const std::wstring& DosPath);
	std::shared_ptr<const SMount> GetMountForDevice(const std::wstring& NtPath);

	struct SGuid
	{
		WCHAR Guid[38 + 1] = { 0 };
		std::wstring DevicePath;
	};

	std::vector<std::shared_ptr<SGuid>> GetVolumeGuids() { std::unique_lock lock(m_Mutex); return m_Guids; }
	std::shared_ptr<const SGuid> GetGuidOfDevice(const std::wstring& DevicePath);
	std::shared_ptr<const SGuid> GetDeviceForGuid(const std::wstring& Guid);

	void UpdateDevices();

	void RegisterDeviceChangeCallback(void (*func)(void*), void* param);
	void UnRegisterDeviceChangeCallback(void (*func)(void*), void* param);

	static ULONG GetVolumeSN(const std::wstring& DriveNtPath);

	static bool IsDosPath(const std::wstring& Path);

protected:
	static CNtPathMgr* m_pInstance;

	std::recursive_mutex m_Mutex;

	void InitDosDrives();
	void AddDrive(WCHAR Letter, const std::wstring& DevicePath, bool Subst);
	void AdjustDrives(ULONG Index, bool Subst, const std::wstring& Path);

	std::vector<std::shared_ptr<SDrive>> m_Drives;
	

	void InitNtMountMgr();
	void AddGuid(const wchar_t* Guid, const std::wstring& DevicePath);
	void AddMount(const std::wstring& MountPath, const std::wstring& DevicePath);

	std::vector<std::shared_ptr<SGuid>> m_Guids;
	std::list<std::shared_ptr<SMount>> m_Mounts;


	std::wstring GetName_TranslateSymlinks(const std::wstring& Path, bool* pTranslated);


	std::list<std::pair<void(*)(void*), void*>> m_DeviceChangeCallbacks;

	// Device Shange Notificaiton
	friend DWORD WINAPI NotificationThread(LPVOID lpParam);
	friend LRESULT CALLBACK HiddenWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void RegisterForDeviceNotifications();
	void UnRegisterForDeviceNotifications();

	HANDLE m_NotificationThread = NULL;
	HWND m_HiddenWindow = NULL;
	HDEVNOTIFY m_DeviceNotifyHandle = NULL;
};