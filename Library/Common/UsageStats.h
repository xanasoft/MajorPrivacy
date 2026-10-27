#pragma once
#include "../Framework/Core/Types.h"

//
// Per program usage history: CPU time, I/O bytes and network bytes, summed into
// fixed length time buckets. The service keeps these per program file / service
// (see Service/Programs/UsageLog.h), the GUI mirrors them per item and sums the
// selected items for the usage graphs.
//
// All buckets of a tier have the same length (Interval seconds). Only buckets in
// which something happened are stored, so an idle program costs nothing.
//

enum class EUsageTier
{
	eSeconds = 0,	// 1 s buckets, live view only, never saved
	eMinutes,		// 60 s buckets, kept for the configured retention, saved to UsageRecord.dat
	eMax
};

#pragma pack(push, 1)

struct SUsageSample
{
	uint32 CpuKernel = 0;		// ms of kernel time
	uint32 CpuUser = 0;			// ms of user time
	uint64 IoRead = 0;			// bytes
	uint64 IoWrite = 0;			// bytes
	uint64 IoOther = 0;			// bytes
	uint64 NetUpload = 0;		// bytes
	uint64 NetDownload = 0;		// bytes
	// added later, records written before these existed are shorter and read back with zeros here
	uint32 GpuTime = 0;			// ms of GPU node running time, normalized to one node (100% = all nodes of all GPUs busy)
	uint32 NpuTime = 0;			// ms of NPU node running time, normalized the same way
	uint64 LanUpload = 0;		// bytes, the part of NetUpload that went to LAN addresses (IsLanAddress)
	uint64 LanDownload = 0;		// bytes, the part of NetDownload that came from LAN addresses
	// open connections are a gauge, booked as connection seconds: the count at each sample times the
	// seconds since the last one. A bucket's connection seconds over its length is the average number
	// of open connections. The direction is the firewall log's, see CSocket::BookUsageConnections.
	uint32 ConnInbound = 0;		// connection seconds of inbound connections
	uint32 ConnOutbound = 0;	// connection seconds of outbound (and unclassified) connections

	void Add(const SUsageSample& Other)
	{
		CpuKernel += Other.CpuKernel;
		CpuUser += Other.CpuUser;
		IoRead += Other.IoRead;
		IoWrite += Other.IoWrite;
		IoOther += Other.IoOther;
		NetUpload += Other.NetUpload;
		NetDownload += Other.NetDownload;
		GpuTime += Other.GpuTime;
		NpuTime += Other.NpuTime;
		LanUpload += Other.LanUpload;
		LanDownload += Other.LanDownload;
		ConnInbound += Other.ConnInbound;
		ConnOutbound += Other.ConnOutbound;
	}

	bool IsZero() const
	{
		return CpuKernel == 0 && CpuUser == 0 && IoRead == 0 && IoWrite == 0 && IoOther == 0 && NetUpload == 0 && NetDownload == 0 && GpuTime == 0 && NpuTime == 0 && LanUpload == 0 && LanDownload == 0 && ConnInbound == 0 && ConnOutbound == 0;
	}
};

struct SUsageBucket
{
	uint32 Time = 0;			// unix time in seconds, start of the bucket
	SUsageSample Data;
};

#pragma pack(pop)

//
// Wire / file format of a bucket array: API_V_USAGE_INTERVAL (seconds per bucket),
// API_V_USAGE_REC_SIZE (sizeof(SUsageBucket) of the writer, so a reader can skip
// fields it does not know) and API_V_USAGE_DATA, the raw little endian SUsageBucket
// records back to back, sorted by Time.
//

__inline uint32 UsageTierInterval(EUsageTier Tier)
{
	switch (Tier)
	{
	case EUsageTier::eSeconds:	return 1;
	case EUsageTier::eMinutes:	return 60;
	default:					return 0;
	}
}
