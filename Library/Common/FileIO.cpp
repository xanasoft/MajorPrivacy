#include "pch.h"
#include "FileIO.h"
#include "../Framework/Common/Buffer.h"
#include "../Common/StVariant.h"
#include "Strings.h"

namespace fs = std::filesystem;

//std::string ToPlatformNotation(const std::wstring& Path)
//{
//	std::string UPath;
//	WStrToUtf8(UPath, Path);
//	for(;;)
//	{
//#ifdef WIN32
//		std::wstring::size_type Pos = UPath.find("/");
//		if(Pos == std::wstring::npos)
//			break;
//		UPath.replace(Pos,1,"\\");
//#else
//		std::wstring::size_type Pos = UPath.find("\\");
//		if(Pos == std::wstring::npos)
//			break;
//		UPath.replace(Pos,1,"/");
//#endif
//	}
//	return UPath;
//}
//
//std::wstring ToApplicationNotation(const std::string& UPath)
//{
//	std::wstring Path;
//	Utf8ToWStr(Path, UPath);
//	for(;;)
//	{
//		std::wstring::size_type Pos = Path.find(L"\\");
//		if(Pos == std::wstring::npos)
//			break;
//		Path.replace(Pos,1,L"/");
//	}
//	return Path;
//}

bool FileExists(const std::wstring& Path)
{
    if (GetFileAttributesW(Path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        DWORD err = GetLastError();
        if(err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
            return false;
    }
    return true;
}

// Returns the file's last modification time as a Unix timestamp.
// On error (e.g. file not found), returns -1.
/*time_t GetFileModificationUnixTime(const std::wstring& Path) 
{
    // Open the file for reading.
    HANDLE hFile = CreateFileW(Path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return -1;
    }

    // Retrieve the file times.
    FILETIME ftCreation, ftLastAccess, ftLastWrite;
    if (!GetFileTime(hFile, &ftCreation, &ftLastAccess, &ftLastWrite)) {
        CloseHandle(hFile);
        return -1;
    }
    CloseHandle(hFile);

    // Convert FILETIME to a 64-bit integer (number of 100-nanosecond intervals since January 1, 1601).
    ULARGE_INTEGER ull;
    ull.LowPart  = ftLastWrite.dwLowDateTime;
    ull.HighPart = ftLastWrite.dwHighDateTime;

    // FILETIME represents the number of 100-nanosecond intervals since January 1, 1601.
    // The Unix epoch starts on January 1, 1970.
    // The difference between these epochs is 11644473600 seconds.
    // Since FILETIME is in 100-nanosecond intervals, the offset in FILETIME units is:
    const ULONGLONG EPOCH_DIFF_FILETIME = 11644473600ULL * 10000000ULL;

    if (ull.QuadPart < EPOCH_DIFF_FILETIME) {
        return 0; // The file time is before the Unix epoch.
    }

    // Subtract the epoch difference and convert to seconds.
    ull.QuadPart -= EPOCH_DIFF_FILETIME;
    return static_cast<time_t>(ull.QuadPart / 10000000ULL);
}*/

time_t GetFileModificationUnixTime(const std::wstring& Path) 
{
    std::filesystem::path path(Path);
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return -1; // Error reading file time
    }

#if __cpp_lib_chrono >= 201907L  // C++20: use clock_cast if available
    auto sysTimePoint = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
#else  // C++17: convert using a time offset hack
    auto sysTimePoint = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
#endif

    return std::chrono::system_clock::to_time_t(sysTimePoint);
}

bool SetFileModificationUnixTime(const std::wstring& Path, time_t newTime) 
{
    std::filesystem::path path(Path);
    std::chrono::system_clock::time_point systemTime = std::chrono::system_clock::from_time_t(newTime);
#if __cpp_lib_chrono >= 201907L
    auto fileTime = std::chrono::clock_cast<std::filesystem::file_time_type>(systemTime);
#else
    auto fileTime = std::filesystem::file_time_type::clock::now() + (systemTime - std::chrono::system_clock::now());
#endif
    std::error_code ec;
    std::filesystem::last_write_time(path, fileTime, ec);
    return !ec;
}

bool RemoveFile(const std::wstring& Path) 
{
    try 
    {
        return fs::remove(Path);
    } 
    catch (const std::exception& /*e*/) 
    {
        // Optionally log the error message: e.what()
        return false;
    }
}

bool RenameFile(const std::wstring& OldPath, const std::wstring& NewPath) 
{
    try {
        fs::rename(OldPath, NewPath);
        return true;
    } catch (const fs::filesystem_error&) {
        return false;
    }
}

//bool WriteFile(const std::wstring& Path, const std::wstring& Data) 
//{
//    std::ofstream f(Path, std::ios::binary);
//    if (!f) return false;
//
//    std::string UData;
//    WStrToUtf8(UData, Data);
//    f.write(UData.data(), UData.size());
//
//    return true;
//}
//
//bool ReadFile(const std::wstring& Path, std::wstring& Data) 
//{
//    std::ifstream f(Path, std::ios::binary);
//    if (!f) return false;
//
//    std::string UData((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
//    Utf8ToWStr(Data, UData);
//
//    return true;
//}
//
//bool WriteFile(const std::wstring& Path, const std::string& Data) 
//{
//    std::ofstream f(Path, std::ios::binary);
//    if (!f) return false;
//
//    f.write(Data.data(), Data.size());
//
//    return true;
//}
//
//bool ReadFile(const std::wstring& Path, std::string& Data) 
//{
//    std::ifstream f(Path, std::ios::binary);
//    if (!f) return false;
//
//    Data.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
//
//    return true;
//}

bool WriteFile(const std::wstring& path, const StVariant& data) 
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
        return false;

    CBuffer buffer;
    data.ToPacket(&buffer);

    file.write(reinterpret_cast<const char*>(buffer.GetBuffer()), buffer.GetSize());
    return file.good();
}

bool ReadFile(const std::wstring& path, StVariant& data) 
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;

    file.seekg(0, std::ios::end);
    std::size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    CBuffer buffer(size, true);
    file.read(reinterpret_cast<char*>(buffer.GetBuffer()), size);

    if (!file)
        return false;

    data.FromPacket(&buffer);
    return true;
}

bool WriteFile(const std::wstring& path, const CBuffer& data, std::uint64_t offset) 
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
        return false;

    if(offset != -1)
        file.seekp(offset);

    file.write(reinterpret_cast<const char*>(data.GetBuffer()), data.GetSize());
    return file.good();
}

bool ReadFile(const std::wstring& path, CBuffer& data, std::uint64_t offset) 
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;

    // Determine the size of the file
    file.seekg(0, std::ios::end);
    std::size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::size_t toRead;
    if (offset != -1)
    {
        if (offset > size)
            return false;

        // If the size is specified in data, use that. Otherwise, use the file size.
        toRead = data.GetSize() == 0 ? (size - offset) : data.GetSize();

        file.seekg(offset);
        if (!file)
            return false;
    }
    else
        toRead = data.GetSize() == 0 ? size : data.GetSize();

    data.SetSize(toRead, true);
    file.read(reinterpret_cast<char*>(data.GetBuffer()), toRead);
    return file.good();
}

bool WriteFile(const std::wstring& Path, const std::vector<BYTE>& Data)
{
    fs::remove(Path);

	std::ofstream f(Path, std::ios::binary);
	if (!f) return false;

	f.write(reinterpret_cast<const char*>(Data.data()), Data.size());

	return true;
}

bool ReadFile(const std::wstring& Path, std::vector<BYTE>& Data)
{
	std::ifstream f(Path, std::ios::binary);
	if (!f) return false;

	f.seekg(0, std::ios::end);
	std::size_t Size = f.tellg();
	f.seekg(0, std::ios::beg);

	Data.resize(Size);
	f.read(reinterpret_cast<char*>(Data.data()), Size);

	return true;
}

bool ListDir(const std::wstring& Path, std::vector<std::wstring>& Entries, const wchar_t* Filter)
{
    ASSERT(Path.back() == L'\\');

    for (const auto& entry : fs::directory_iterator(Path))
    {
        std::wstring Name = entry.path().filename().wstring();
        if (Filter && !wildcmpex(Filter, Name.c_str()))
            continue;

        if (entry.is_directory())
        {
            if (Name.compare(L"..") == 0 || Name.compare(L".") == 0)
                continue;
            Entries.push_back(Path + Name + L"\\");
        }
        else
        {
            Entries.push_back(Path + Name);
        }
    }
    return true;
}

unsigned long long GetFileSize(const std::wstring& Path)
{
    return fs::file_size(Path);
}

bool CreateFullPath(std::wstring Path) 
{
    wchar_t* tempPath = (wchar_t*)Path.c_str();
    
    // Iterate through the path and create each folder
    for (wchar_t* p = tempPath + 1; *p; p++) {
        if (*p == L'\\' || *p == L'/') {
            *p = L'\0'; // Temporarily terminate the string
            if (!CreateDirectoryW(tempPath, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
                return false;
            *p = L'\\'; // Restore the original character
        }
    }

    // Create the final directory in the path
    if (!CreateDirectoryW(tempPath, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return false;

    return true;
}