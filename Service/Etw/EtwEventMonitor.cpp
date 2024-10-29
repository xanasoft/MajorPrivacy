#include ".\etw\krabs.hpp"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include "..\Library\Types.h"
#ifndef ASSERT
#define ASSERT(x)
#endif
#include "EtwEventMonitor.h"
#include "../Library/Common/Strings.h"
#include "../Library/Common/DbgHelp.h"


#ifdef USE_ETW_FILE_IO
namespace krabs { namespace kernel {
CREATE_CONVENIENCE_KERNEL_PROVIDER(
	disk_file_io_provider_rd ,
	0,
	krabs::guids::file_io);
} }
#endif

struct SEtwEventMonitor
{
	SEtwEventMonitor() :
		// A trace can have any number of providers, which are identified by GUID. These
		// GUIDs are defined by the components that emit events, and their GUIDs can
		// usually be found with various ETW tools (like wevutil).
		dns_res_provider(krabs::guid(L"{55404E71-4DB9-4DEB-A5F5-8F86E46DDE56}")) // Microsoft-Windows-Winsock-NameResolution
//#ifdef _DEBUG
//		, test_provider(krabs::guid(L"{A0C1853B-5C40-4B15-8766-3CF1C58F985A}"))
//#endif
	{
	}

	~SEtwEventMonitor()
	{
		if (kernel_trace) kernel_trace->stop();
		if (kernel_thread) kernel_thread->join();

#ifdef USE_ETW_FILE_IO
		if (kernel_rundown) kernel_rundown->stop();
		if (kernel_thread_rd) kernel_thread_rd->join();
#endif

		if (user_trace) user_trace->stop();
		if (user_thread) user_thread->join();
	}

	std::shared_ptr<krabs::kernel_trace> kernel_trace;
#ifdef USE_ETW_DISK_IO
	krabs::kernel::disk_io_provider disk_provider;
#endif
	krabs::kernel::process_provider proc_provider;
	krabs::kernel::network_tcpip_provider tcp_provider;
	krabs::kernel::network_udpip_provider udp_provider;
	std::shared_ptr<std::thread> kernel_thread;

#ifdef USE_ETW_FILE_IO
	krabs::kernel::disk_file_io_provider file_provider;
	std::shared_ptr<krabs::kernel_trace> kernel_rundown;
	krabs::kernel::disk_file_io_provider_rd file_provider_rd;
	std::shared_ptr<std::thread> kernel_thread_rd;
#endif

	std::shared_ptr<krabs::user_trace> user_trace;
	krabs::provider<> dns_res_provider;
//#ifdef _DEBUG
//	krabs::provider<> test_provider;
//#endif
	std::shared_ptr<std::thread> user_thread;

	bool bRunning = false;

	std::mutex Mutex;
	std::vector<std::function<void(const SEtwNetworkEvent* pEvent)>> NetworkHandlers;
	std::vector<std::function<void(const SEtwDnsEvent* pEvent)>> DnsHandlers;
#ifdef USE_ETW_FILE_IO
	std::vector<std::function<void(const SEtwFileEvent* pEvent)>> FileHandlers;
#endif
#ifdef USE_ETW_DISK_IO
	std::vector<std::function<void(struct SEtwDiskEvent* pEvent)>> DiskHandlers;
#endif
	std::vector<std::function<void(const SEtwProcessEvent* pEvent)>> ProcessHandlers;
};

CEtwEventMonitor::CEtwEventMonitor()
{
	m = new SEtwEventMonitor();
}

CEtwEventMonitor::~CEtwEventMonitor()
{
	delete m;
}

void stripPort(std::wstring& entry) 
{
	if (!entry.empty() && entry[0] == L'[') {
		// Handle IPv6 addresses enclosed in brackets
		std::size_t end_bracket = entry.find(L']');
		if (end_bracket != std::wstring::npos) {
			// Check if there is a colon after the closing bracket indicating a port
			if (end_bracket + 1 < entry.length() && entry[end_bracket + 1] == L':') {
				// Strip the port from the address
				entry = entry.substr(0, end_bracket + 1);
			}
			// No port present; do nothing
		}
		// Malformed address; do nothing
	}
	else {
		// Handle IPv4 addresses and IPv6 addresses without brackets
		std::size_t pos = entry.rfind(L':');
		if (pos != std::wstring::npos) {
			// Check if the substring after the last colon is a valid port number
			std::wstring port_candidate = entry.substr(pos + 1);
			if (!port_candidate.empty() && std::all_of(port_candidate.begin(), port_candidate.end(), iswdigit)) {
				// Port exists; strip it from the address
				entry = entry.substr(0, pos);
			}
			// No valid port found; do nothing
		}
		// No colon found; address does not contain a port
	}
}

bool CEtwEventMonitor::Init()
{
	if (m->bRunning)
		return true;

	//
	// Note: before initialising ensure all event handlers are in place, 
	// else some etw providers won't be initialized
	//

	m->kernel_trace = std::make_shared<krabs::kernel_trace>(L"Privacy_EtKernelLogger");
#ifdef USE_ETW_FILE_IO
	m->kernel_rundown = std::make_shared<krabs::kernel_trace>(L"Privacy_EtKernelRundown");
#endif
	m->user_trace = std::make_shared<krabs::user_trace>(L"Privacy_EtUserLogger");

#ifdef USE_ETW_DISK_IO
	if (m->DiskHandlers.size() > 0)
	{
		m->disk_provider.add_on_event_callback([&](const EVENT_RECORD& record, const krabs::trace_context& trace_context) {
			//qDebug() << "disk event";
			krabs::schema schema(record, trace_context.schema_locator);

			SEtwDiskEvent Event;

			Event.Type = SEtwDiskEvent::EType::Unknown;

			switch (schema.event_opcode())
			{
			case EVENT_TRACE_TYPE_IO_READ:
				Event.Type = SEtwDiskEvent::EType::Read;
				break;
			case EVENT_TRACE_TYPE_IO_WRITE:
				Event.Type = SEtwDiskEvent::EType::Write;
				break;
			default:
				return;
			}

			krabs::parser parser(schema);

			Event.ProcessId = -1;
			Event.ThreadId = -1;

			// Since Windows 8, we no longer get the correct process/thread IDs in the
			// event headers for disk events. 
			//if (WindowsVersion >= WINDOWS_8)
			if (schema.thread_id() == ULONG_MAX)
			{
				Event.ThreadId = parser.parse<uint32_t>(L"IssuingThreadId");
				Event.ProcessId = -1; // indicate that it must be looked up by thread id
			}
			else if (schema.process_id() != ULONG_MAX)
			{
				Event.ProcessId = schema.process_id();
				Event.ThreadId = schema.thread_id();
			}

			Event.FileId = 0;
			krabs::binary FileObjectBin = parser.parse<krabs::binary>(L"FileObject");
			if (FileObjectBin.bytes().size() == 8)
				Event.FileId = *(uint64_t*)FileObjectBin.bytes().data();
			else if (FileObjectBin.bytes().size() == 4)
				Event.FileId = *(uint32_t*)FileObjectBin.bytes().data();
			Event.IrpFlags = parser.parse<uint32_t>(L"IrpFlags");
			Event.TransferSize = parser.parse<uint32_t>(L"TransferSize");
			Event.HighResResponseTime = parser.parse<uint64_t>(L"HighResResponseTime");


			CSectionLock Lock(&m->Lock);
			for (auto Handler : m->DiskHandlers)
				Handler(&Event);
		});
		m->kernel_trace->enable(m->disk_provider);
	}
#endif

#ifdef USE_ETW_FILE_IO
	auto file_callback = [](SEtwEventMonitor* m, const EVENT_RECORD &record, const krabs::trace_context& trace_context) {
		//qDebug() << "file event";
		krabs::schema schema(record, trace_context.schema_locator);
		/*
		qDebug() << QString("EventId: %1").arg(schema.event_id());
		qDebug() << QString("EventOpcode: %1").arg(schema.event_opcode());
		qDebug() << QString("ProcessId: %1").arg(schema.process_id());
		qDebug() << QString("ThreadId: %1").arg(schema.thread_id());
		qDebug() << QString("EventName: %1").arg(schema.event_name());
		qDebug() << QString("ProviderName: %1").arg(schema.provider_name());
	

		krabs::parser parser(schema);
		krabs::property_iterator PI = parser.properties();
		for (std::vector<krabs::property>::iterator I = PI.begin(); I != PI.end(); ++I)
		{
			QString Info = QString("\t %1:").arg(I->name());

			bool bOk = false;
			
			try { Info += QString::fromStdWString(parser.parse<wstring>(I->name()));	bOk = true; }
			catch (...) {}
			
			if(!bOk) try { Info += QString::number(parser.parse<uint64_t>(I->name()));	bOk = true; }
			catch (...) {}

			if (!bOk) try { Info += QString::number(parser.parse<uint32_t>(I->name()));	bOk = true; }
			catch (...) {}

			if (!bOk) try { Info += QString::number(parser.parse<ULONG_PTR>(I->name()));	bOk = true; }
			catch (...) {}

			if (!bOk) try { Info += QString::number((ULONG_PTR)parser.parse<void*>(I->name()));	bOk = true; }
			catch (...) {}


			if (!bOk) Info += QString("Type: %1").arg(I->type());

			qDebug() << Info;
		}

		qDebug() << "";*/

		SEtwFileEvent Event;

		Event.Type = SEtwFileEvent::EType::Unknown;

		switch (record.EventHeader.EventDescriptor.Opcode)
		{
		case 0: // Name
			Event.Type = SEtwFileEvent::EType::Name;
			break;
		case 32: // FileCreate
			Event.Type = SEtwFileEvent::EType::Create;
			break;
		case 36: // FileRundown
			Event.Type = SEtwFileEvent::EType::Rundown;
			break;
		case 35: // FileDelete
			Event.Type = SEtwFileEvent::EType::Delete;
			break;
		}

		if (Event.Type != SEtwFileEvent::EType::Unknown)
		{
			krabs::parser parser(schema);

			Event.FileId = (uint64)parser.parse<void*>(L"FileObject");
			Event.FileName = parser.parse<std::wstring>(L"FileName");

			Event.ProcessId = -1;
			Event.ThreadId = -1;

			// Since Windows 8, we no longer get the correct process/thread IDs in the
			// event headers for file events. 
			//if (WindowsVersion >= WINDOWS_8)
			if (schema.process_id() != ULONG_MAX)
			{
				Event.ProcessId = schema.process_id();
				Event.ThreadId = schema.thread_id();
			}

			CSectionLock Lock(&m->Lock);
			for (auto Handler : m->FileHandlers)
				Handler(&Event);
		}
	};

	m->file_provider.add_on_event_callback([this, file_callback](const EVENT_RECORD &record, const krabs::trace_context &trace_context) { file_callback(m, record, trace_context); });
	m->kernel_trace->enable(m->file_provider);

	m->file_provider_rd.add_on_event_callback([this, file_callback](const EVENT_RECORD &record, const krabs::trace_context &trace_context) { file_callback(m, record, trace_context); });
	m->kernel_rundown->enable(m->file_provider_rd);
#endif

	if (m->ProcessHandlers.size() > 0)
	{
		m->proc_provider.add_on_event_callback([&](const EVENT_RECORD& record, const krabs::trace_context& trace_context) {
			//qDebug() << "process event";
			krabs::schema schema(record, trace_context.schema_locator);

			if (schema.event_id() != 0)
				return;

			SEtwProcessEvent Event;

			Event.Type = SEtwProcessEvent::EType::Unknown;
			switch (schema.event_opcode())
			{
			case 1: Event.Type = SEtwProcessEvent::EType::Started; break;
			case 2: Event.Type = SEtwProcessEvent::EType::Stopped; break;
			default: // we dont care for other event types
				return;
			}

			krabs::parser parser(schema);

			Event.ProcessId = parser.parse<uint32_t>(L"ProcessId");
			Event.CommandLine = parser.parse<std::wstring>(L"CommandLine");
			Event.FileName = charArrayToWString(parser.parse<std::string>(L"ImageFileName").c_str());
			Event.ParentId = parser.parse<uint32_t>(L"ParentId");
			Event.ExitCode = (Event.Type == SEtwProcessEvent::EType::Stopped) ? parser.parse<int32_t>(L"ExitStatus") : 0;
			Event.TimeStamp = schema.timestamp().QuadPart;

			//qDebug() << FILETIME2ms(schema.timestamp().QuadPart) << GetTime();

			std::unique_lock<std::mutex> Lock(m->Mutex);
			for (auto Handler : m->ProcessHandlers)
				Handler(&Event);
		});
		m->kernel_trace->enable(m->proc_provider);
	}

	if (m->NetworkHandlers.size() > 0)
	{
		auto net_callback = [](SEtwEventMonitor* m, const EVENT_RECORD& record, const krabs::trace_context& trace_context) {
			krabs::schema schema(record, trace_context.schema_locator);
			//qDebug() << "net event";

			SEtwNetworkEvent Event;

			Event.Type = SEtwNetworkEvent::EType::Unknown;

			// TcpIp/UdpIp
			bool isIpv6;
			switch (record.EventHeader.EventDescriptor.Opcode)
			{
			case EVENT_TRACE_TYPE_SEND: // send
				Event.Type = SEtwNetworkEvent::EType::Send;
				isIpv6 = false;
				break;
			case EVENT_TRACE_TYPE_RECEIVE: // receive
				Event.Type = SEtwNetworkEvent::EType::Receive;
				isIpv6 = false;
				break;
			case EVENT_TRACE_TYPE_SEND + 16: // send ipv6
				Event.Type = SEtwNetworkEvent::EType::Send;
				isIpv6 = true;
				break;
			case EVENT_TRACE_TYPE_RECEIVE + 16: // receive ipv6
				Event.Type = SEtwNetworkEvent::EType::Receive;
				isIpv6 = true;
				break;
			default:
				return;
			}

			static GUID TcpIpGuid_I = { 0x9a280ac0, 0xc8e0, 0x11d1, { 0x84, 0xe2, 0x00, 0xc0, 0x4f, 0xb9, 0x98, 0xa2 } };
			static GUID UdpIpGuid_I = { 0xbf3a50c5, 0xa9c9, 0x4988, { 0xa0, 0x05, 0x2d, 0xf0, 0xb7, 0xc8, 0x0f, 0x80 } };

			Event.ProtocolType = 0;
			if (IsEqualGUID(record.EventHeader.ProviderId, TcpIpGuid_I))
				Event.ProtocolType = IPPROTO_TCP;
			else if (IsEqualGUID(record.EventHeader.ProviderId, UdpIpGuid_I))
				Event.ProtocolType = IPPROTO_UDP;

			Event.ProcessId = -1;
			Event.ThreadId = -1;
			Event.TransferSize = 0;

			Event.LocalPort = 0;
			Event.RemotePort = 0;

			if (!isIpv6)
			{
				struct TcpIpOrUdpIp_IPV4_Header
				{
					ULONG PID;
					ULONG size;
					ULONG daddr;
					ULONG saddr;
					USHORT dport;
					USHORT sport;
					//UINT64 Aux;
				} *data = (TcpIpOrUdpIp_IPV4_Header*)record.UserData;

				Event.ProcessId = data->PID;
				Event.TransferSize = data->size;

				Event.LocalAddress = CAddress(ntohl(data->saddr));
				Event.LocalPort = ntohs(data->sport);

				Event.RemoteAddress = CAddress(ntohl(data->daddr));
				Event.RemotePort = ntohs(data->dport);
			}
			else //if (isIpv6)
			{
				struct TcpIpOrUdpIp_IPV6_Header
				{
					ULONG PID;
					ULONG size;
					IN6_ADDR daddr;
					IN6_ADDR saddr;
					USHORT dport;
					USHORT sport;
					UINT64 Aux;
				} *data = (TcpIpOrUdpIp_IPV6_Header*)record.UserData;

				Event.ProcessId = data->PID;
				Event.TransferSize = data->size;

				Event.LocalAddress = CAddress(data->saddr.u.Byte);
				Event.LocalPort = ntohs(data->sport);

				Event.RemoteAddress = CAddress(data->daddr.u.Byte);
				Event.RemotePort = ntohs(data->dport);
			}

			// Note: Incomming UDP packets have the endpoints swaped :/
			if (Event.ProtocolType == IPPROTO_UDP && Event.Type == SEtwNetworkEvent::EType::Receive)
			{
				CAddress TempAddresss = Event.LocalAddress;
				uint16 TempPort = Event.LocalPort;
				Event.LocalAddress = Event.RemoteAddress;
				Event.LocalPort = Event.RemotePort;
				Event.RemoteAddress = TempAddresss;
				Event.RemotePort = TempPort;
			}

			Event.TimeStamp = schema.timestamp().QuadPart;

			//if(ProcessId == )
			//	qDebug() << ProcessId  << Type << LocalAddress.toString() << LocalPort << RemoteAddress.toString() << RemotePort;

			std::unique_lock<std::mutex> Lock(m->Mutex);
			for (auto Handler : m->NetworkHandlers)
				Handler(&Event);
		};

		m->tcp_provider.add_on_event_callback([this, net_callback](const EVENT_RECORD& record, const krabs::trace_context& trace_context) { net_callback(this->m, record, trace_context); });
		m->udp_provider.add_on_event_callback([this, net_callback](const EVENT_RECORD& record, const krabs::trace_context& trace_context) { net_callback(this->m, record, trace_context); });

		m->kernel_trace->enable(m->tcp_provider);
		m->kernel_trace->enable(m->udp_provider);
	}

	if (m->DnsHandlers.size() > 0)
	{
		// user_trace providers typically have any and all flags, whose meanings are
		// unique to the specific providers that are being invoked. To understand these
		// flags, you'll need to look to the ETW event producer.
		m->dns_res_provider.any(0xf0010000000003ff);
		m->dns_res_provider.add_on_event_callback([&](const EVENT_RECORD& record, const krabs::trace_context& trace_context) {
			krabs::schema schema(record, trace_context.schema_locator);

			if (schema.event_id() != 1001)
				return;

			krabs::parser parser(schema);

			uint32_t Status;
			if (!parser.try_parse(L"Status", Status) || Status != 0)
				return;

			SEtwDnsEvent Event;
			Event.ProcessId = schema.process_id();
			Event.ThreadId = schema.thread_id();
			Event.TimeStamp = schema.timestamp().QuadPart;

			Event.HostName = parser.parse<std::wstring>(L"NodeName");
			std::wstring Result = parser.parse<std::wstring>(L"Result");

			/*
			"192.168.163.1" "192.168.163.1;"
			"localhost" "[::1]:8307;127.0.0.1:8307;" <- wtf is this why is there a port?!
			"DESKTOP" "fe80::189a:f1c3:3e87:be81%12;192.168.10.12;"
			"telemetry.malwarebytes.com" "54.149.69.204;54.200.191.52;54.70.191.27;54.149.66.105;54.244.17.248;54.148.98.86;"
			"web.whatsapp.com" "31.13.84.51;"
			*/

			for (std::wstring Entry : SplitStr(Result, L";", false)) 
			{
				stripPort(Entry);

				CAddress IP;
				if (IP.FromWString(Entry))
					Event.Results.push_back(IP);
#ifdef _DEBUG
				else
					DbgPrint(L"Failed to parse dns result: %s\r\n", Entry.c_str());
#endif
			}

			std::unique_lock<std::mutex> Lock(m->Mutex);
			for (auto Handler : m->DnsHandlers)
				Handler(&Event);
		});
		m->user_trace->enable(m->dns_res_provider);
	}

//#ifdef _DEBUG
//		m->test_provider.any(0xf0010000000003ff);
//		m->test_provider.add_on_event_callback([&](const EVENT_RECORD& record, const krabs::trace_context& trace_context) {
//			krabs::schema schema(record, trace_context.schema_locator);
//
//			DbgPrint(L"Bam");
//		});
//		m->user_trace->enable(m->test_provider);
//#endif

	// reg add "HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\WMI" /v EtwMaxLoggers /t REG_DWORD /d 128

	m->kernel_thread = std::make_shared<std::thread>([&]() { 
		try {
			m->kernel_trace->start();
		}
		catch (std::runtime_error& err)
		{
			const char* what = err.what();
			OutputDebugStringA(what);
			ASSERT(0);
		}
	});

#ifdef USE_ETW_FILE_IO
	m->kernel_thread_rd = std::make_shared<std::thread>([&]() { 
		try {
			m->kernel_rundown->start();
		}
		catch (std::runtime_error& err)
		{
			const char* what = err.what();
			OutputDebugStringA(what);
			ASSERT(0);
		}
	});
#endif

	m->user_thread = std::make_shared<std::thread>([&]() { 
		try {
			m->user_trace->start(); 
		}
		catch (std::runtime_error& err)
		{
			const char* what = err.what();
			OutputDebugStringA(what);
			ASSERT(0);
		}
	});

	m->bRunning = true;
	return true;
}

void CEtwEventMonitor::RegisterNetworkHandler(const std::function<void(const SEtwNetworkEvent* pEvent)>& Handler) 
{
	std::unique_lock<std::mutex> Lock(m->Mutex);
	m->NetworkHandlers.push_back(Handler); 
}

void CEtwEventMonitor::RegisterDnsHandler(const std::function<void(const SEtwDnsEvent* pEvent)>& Handler) 
{
	std::unique_lock<std::mutex> Lock(m->Mutex);
	m->DnsHandlers.push_back(Handler); 
}

#ifdef USE_ETW_FILE_IO
void CEtwEventMonitor::RegisterFileHandler(const std::function<void(const SEtwFileEvent* pEvent)>& Handler) 
{
	std::unique_lock<std::mutex> Lock(m->Mutex);
	m->FileHandlers.push_back(Handler); 
}
#endif

#ifdef USE_ETW_DISK_IO
void CEtwEventMonitor::RegisterDiskHandler(const std::function<void(struct SEtwDiskEvent* pEvent)>& Handler) 
{
	std::unique_lock<std::mutex> Lock(m->Mutex);
	m->DiskHandlers.push_back(Handler); 
}
#endif

void CEtwEventMonitor::RegisterProcessHandler(const std::function<void(const SEtwProcessEvent* pEvent)>& Handler) 
{
	std::unique_lock<std::mutex> Lock(m->Mutex);
	m->ProcessHandlers.push_back(Handler); 
}