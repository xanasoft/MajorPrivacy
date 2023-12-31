#pragma once
#include "../Library/Common/Address.h"
#include "../Library/Helpers/Scoped.h"

//#define USE_ETW_DISK_IO
//#define USE_ETW_FILE_IO

struct SEtwNetworkEvent
{
	enum class EType {
		Unknown = 0,
		Receive,
		Send,
	};

	EType Type;
	uint64 ProcessId;
	uint64 ThreadId;
	uint32 ProtocolType;
	uint32 TransferSize;
	CAddress LocalAddress;
	uint16 LocalPort;
	CAddress RemoteAddress;
	uint16 RemotePort;
	uint64 TimeStamp;
};

struct SEtwDnsEvent
{
	uint64 ProcessId;
	uint64 ThreadId;
	uint64 TimeStamp;
	std::wstring HostName;
	std::vector<CAddress> Results;
};

#ifdef USE_ETW_DISK_IO
struct SEtwDiskEvent
{
	enum class EType {
		Unknown = 0,
		Read,
		Write,
	};

	EType Type;
	uint64 FileId;
	uint64 ProcessId;
	uint64 ThreadId;
	uint32 IrpFlags;
	uint32 TransferSize;
	uint64 HighResResponseTime;
};
#endif

#ifdef USE_ETW_FILE_IO
struct SEtwFileEvent
{
	enum class EType {
		Unknown = 0,
		Name,
		Create,
		Delete,
		Rundown,
	};

	EType Type;
	uint64 FileId;
	uint64 ProcessId;
	uint64 ThreadId;
	std::wstring FileName;
};
#endif

struct SEtwProcessEvent
{
	enum class EType {
		Unknown = 0,
		Started,
		Stopped,
	};

	EType Type;
	uint64 ProcessId;
	uint64 ParentId;
	std::wstring FileName;
	std::wstring CommandLine;
	uint64 TimeStamp;
	uint32 ExitCode;
};

class CEtwEventMonitor
{
public:
	CEtwEventMonitor();
    ~CEtwEventMonitor();

	bool		Init();

	void RegisterNetworkHandler(const std::function<void(const SEtwNetworkEvent* pEvent)>& Handler);
    template<typename T, class C>
    void RegisterNetworkHandler(T Handler, C This) { RegisterNetworkHandler(std::bind(Handler, This, std::placeholders::_1)); }

	void RegisterDnsHandler(const std::function<void(const SEtwDnsEvent* pEvent)>& Handler);
    template<typename T, class C>
    void RegisterDnsHandler(T Handler, C This) { RegisterDnsHandler(std::bind(Handler, This, std::placeholders::_1)); }

#ifdef USE_ETW_FILE_IO
	void RegisterFileHandler(const std::function<void(const SEtwFileEvent* pEvent)>& Handler);
    template<typename T, class C>
    void RegisterFileHandler(T Handler, C This) { RegisterFileHandler(std::bind(Handler, This, std::placeholders::_1)); }
#endif

#ifdef USE_ETW_DISK_IO
	void RegisterDiskHandler(const std::function<void(struct SEtwDiskEvent* pEvent)>& Handler);
    template<typename T, class C>
    void RegisterDnskHandler(T Handler, C This) { RegisterDiskHandler(std::bind(Handler, This, std::placeholders::_1)); }
#endif

	void RegisterProcessHandler(const std::function<void(const SEtwProcessEvent* pEvent)>& Handler);
    template<typename T, class C>
    void RegisterProcessHandler(T Handler, C This) { RegisterProcessHandler(std::bind(Handler, This, std::placeholders::_1)); }

private:
	struct SEtwEventMonitor* m;
};
