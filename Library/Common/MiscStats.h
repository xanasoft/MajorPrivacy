#pragma once 
#include "../Types.h"

template <class T>
struct SRingBuffer
{
	SRingBuffer(size_t grow = 10)
	{
		Head = 0;
		Tail = 0;
		Size = 0;
		Grow = grow;
		Length = 0;
	}

	size_t size() const	{ return Size; }
	size_t free_size() const { return (Length - Size); }
	bool is_empty() const { return Size == 0; }
	bool is_full() const { return (free_size() <= 0); }

	void clear()
	{
		Head = Tail = Size = 0;
#ifdef _DEBUG
		Buffer.clear();
		Buffer.resize(Length);
#endif
	}

	T& at(size_t pos)
	{
		size_t idx = (Head + pos) % Length;
		return Buffer[idx];
	}

	T& front()
	{
		return Buffer[Head];
	}

	T& back()
	{
		return Buffer[Tail];
	}

	void pop_front()
	{
		if (is_empty())
			return;
#ifdef _DEBUG
		Buffer[Head] = T();
#endif
		Head = (Head + 1) % Length;
		Size--;
		if (Size == 0)
			Tail = Head;
	}

	void push_back(const T& value)
	{
		if (is_full())
			expand();
		if(Size > 0)
			Tail = (Tail + 1) % Length;
		Buffer[Tail] = value;
		Size++;
	}

	void expand()
	{
		std::vector<T> NewBuffer;
		NewBuffer.resize(Length + Grow);
		size_t togo = size();
		if (togo)
		{
			for (int i = 0; i < togo; i++)
				NewBuffer[i] = at(i);
			Head = 0;
			Tail = togo - 1;
		}
		Length = NewBuffer.size();
		Buffer = NewBuffer;
	}

protected:
	std::vector<T>	Buffer;
	size_t		Head;
	size_t		Tail;
	size_t		Size;
	size_t		Grow;
	size_t		Length;
};

template<class T>
struct SDelta
{
	SDelta()
	{
		Initialized = false;
		Value = 0;
		Delta = 0;
	}
	
	void Init(T New) {
		Initialized = true;
		Delta = 0;
		Value = New;
	}

	virtual void Update(T New) {
		if (!Initialized)
			Initialized = true;
		else if (New < Value) // some counters may reset...
			Delta = 0;
		else
			Delta = New - Value;
		Value = New;
	}

	T Value;
	T Delta;

private:
	bool Initialized;
};

typedef SDelta<uint32>	SDelta32;
typedef SDelta<uint64>	SDelta64;

#define AVG_INTERVAL (3*1000)

struct SRateCounter
{
	SRateCounter()
	{
		ByteRate = 0;
		TotalBytes = 0;
		TotalTime = 0;
	}

	/*virtual void Update(uint64 Interval, uint64 AddDelta)
	{
		while(TotalTime > AVG_INTERVAL && !RateStat.empty())
		{
			TotalTime -= RateStat.front().Interval;
			TotalBytes -= RateStat.front().Bytes;
			RateStat.pop_front();
		}

		RateStat.push_back(SStat(Interval, AddDelta));
		TotalTime += RateStat.back().Interval;
		TotalBytes += RateStat.back().Bytes;

		uint64 totalTime = TotalTime ? TotalTime : Interval;
		if(totalTime < AVG_INTERVAL/2)
			totalTime = AVG_INTERVAL;
		ByteRate = TotalBytes*1000/totalTime;
		ASSERT(ByteRate >= 0);
	}*/

	virtual void Update(uint64 Interval, uint64 AddDelta)
	{
		while(TotalTime > AVG_INTERVAL && !RateStat.is_empty())
		{
			SStat &Front = RateStat.front();
			TotalTime -= Front.Interval;
			TotalBytes -= Front.Bytes;
			RateStat.pop_front();
		}

		SStat Back = SStat(Interval, AddDelta);
		TotalTime += Back.Interval;
		TotalBytes += Back.Bytes;
		RateStat.push_back(Back);

		uint64 totalTime = TotalTime ? TotalTime : Interval;
		if(totalTime < AVG_INTERVAL/2)
			totalTime = AVG_INTERVAL;
		ByteRate = TotalBytes*1000/totalTime;
		ASSERT(ByteRate >= 0);
	}

	__inline uint64	Get() const			{return ByteRate;}
	__inline void		Clear()				{ByteRate = 0;}

protected:
	uint64				ByteRate;

	struct SStat
	{
		SStat(uint64 i = 0, uint64 b = 0) {Interval = i; Bytes = b;}
		uint64 Interval;
		uint64 Bytes;
	};
	//std::list<SStat>			RateStat;
	SRingBuffer<SStat>	RateStat;
	uint64				TotalBytes;
	uint64				TotalTime;
};

/*struct SRateCounter2 : SRateCounter
{
	SRateCounter2()
	{
		TotalRate = 0;
	}

	virtual void Update(uint64 Interval, uint64 AddDelta)
	{
		SRateCounter::Update(Interval, AddDelta);

		while(Smooth.size() > 4)
		{
			TotalRate -= Smooth.front();
			Smooth.pop_front();
		}
		Smooth.push_back(ByteRate);
		TotalRate += ByteRate;

		ByteRate = TotalRate / Smooth.size();
	}

	__inline void		Clear() { ByteRate = 0; Smooth.clear(); TotalRate = 0; }

protected:
	//std::list<uint64>		Smooth;
	SRingBuffer<uint64>Smooth;
	uint64				TotalRate;
};*/

struct SSmoother
{
	SSmoother()
	{
		TotalValue = 0;
	}

	uint64 Smooth(uint64 newValue)
	{
		while(SmoothList.size() > 5)
		{
			TotalValue -= SmoothList.front();
			SmoothList.pop_front();
		}
		SmoothList.push_back(newValue);
		TotalValue += newValue;

		return TotalValue / SmoothList.size();
	}

	std::list<uint64>	SmoothList;
	uint64			TotalValue;
};

struct SNetStats
{
	SNetStats()
	{
		ReceiveCount = 0;
		ReceiveRaw = 0;
		SendCount = 0;
		SendRaw = 0;			
	}

	void AddReceive(uint64 size) {
		ReceiveCount++;
		ReceiveRaw += size;
	}

	void AddSend(uint64 size) {
		SendCount++;
		SendRaw += size;
	}

	void SetReceive(uint64 size, uint64 count) {
		ReceiveCount = count;
		ReceiveRaw = size;
	}

	void SetSend(uint64 size, uint64 count) {
		SendCount = count;
		SendRaw = size;
	}

	void UpdateStats(uint64 Interval)
	{
		ReceiveDelta.Update(ReceiveCount);
		ReceiveRawDelta.Update(ReceiveRaw);
		ReceiveRate.Update(Interval, ReceiveRawDelta.Delta);
		SendDelta.Update(SendCount);
		SendRawDelta.Update(SendRaw);
		SendRate.Update(Interval, SendRawDelta.Delta);
	}

	uint64			ReceiveCount;
	uint64			ReceiveRaw;
	uint64			SendCount;
	uint64			SendRaw;

    SDelta64		ReceiveDelta;
    SDelta64		ReceiveRawDelta;
    SDelta64		SendDelta;
    SDelta64		SendRawDelta;

	SRateCounter	ReceiveRate;
	SRateCounter	SendRate;
};

struct SIOStats
{
	SIOStats()
	{
		ReadCount = 0;
		ReadRaw = 0;
		WriteCount = 0;	
		WriteRaw = 0;
	}

	void AddRead(uint64 size) {
		ReadCount++;
		ReadRaw += size;
	}

	void AddWrite(uint64 size) {
		WriteCount++;
		WriteRaw += size;
	}

	void SetRead(uint64 size, uint64 count) {
		ReadCount = count;
		ReadRaw = size;
	}

	void SetWrite(uint64 size, uint64 count) {
		WriteCount = count;
		WriteRaw = size;
	}

	void UpdateStats(uint64 Interval)
	{
		ReadDelta.Update(ReadCount);
		ReadRawDelta.Update(ReadRaw);
		ReadRate.Update(Interval, ReadRawDelta.Delta);
		WriteDelta.Update(WriteCount);
		WriteRawDelta.Update(WriteRaw);
		WriteRate.Update(Interval, WriteRawDelta.Delta);
	}

	uint64			ReadCount;
	uint64			ReadRaw;
	uint64			WriteCount;
	uint64			WriteRaw;

	SDelta64		ReadDelta;
    SDelta64		ReadRawDelta;
    SDelta64		WriteDelta;
    SDelta64		WriteRawDelta;

	SRateCounter	ReadRate;
	SRateCounter	WriteRate;
};

struct SIOStatsEx: SIOStats
{
	SIOStatsEx()
	{
		OtherCount = 0;	
		OtherRaw = 0;
	}

	void SetOther(uint64 size, uint64 count) {
		OtherCount = count;
		OtherRaw = size;
	}


	void UpdateStats(uint64 Interval)
	{
		SIOStats::UpdateStats(Interval);
		OtherDelta.Update(OtherCount);
		OtherRawDelta.Update(OtherRaw);
		OtherRate.Update(Interval, OtherRawDelta.Delta);
	}

	uint64			OtherCount;
	uint64			OtherRaw;

    SDelta64		OtherDelta;
    SDelta64		OtherRawDelta;

	SRateCounter	OtherRate;
};

struct SSockStats
{
	SSockStats()
	{
		LastStatUpdate = GetCurTick();
	}

	uint64 UpdateStats()
	{
		uint64 curTick = GetCurTick();
		uint64 time_ms = curTick - LastStatUpdate;
		LastStatUpdate = curTick;

		Net.UpdateStats(time_ms);

		return time_ms;
	}

	uint64		LastStatUpdate;

	SNetStats	Net;
};

struct SProcStats
{
	SProcStats()
	{
		LastStatUpdate = GetCurTick();
	}

	uint64 UpdateStats()
	{
		uint64 curTick = GetCurTick();
		uint64 time_ms = curTick - LastStatUpdate;
		LastStatUpdate = curTick;

		Net.UpdateStats(time_ms);
		//Disk.UpdateStats(time_ms);
		Io.UpdateStats(time_ms);

		return time_ms;
	}

	uint64		LastStatUpdate;

	SNetStats	Net;
	//SIOStats	Disk;
	SIOStatsEx	Io;
};

struct SSysStats: SProcStats
{
	uint64 UpdateStats()
	{
		uint64 time_ms = SProcStats::UpdateStats();

		Lan.UpdateStats(time_ms);

		MMapIo.UpdateStats(time_ms);

		//NetIf.UpdateStats(time_ms);

#ifdef WIN32
		SambaServer.UpdateStats(time_ms);
		SambaClient.UpdateStats(time_ms);
#endif

		return time_ms;
	}
	
	SNetStats	Lan;

	SIOStatsEx	MMapIo;

	//SNetStats	NetIf;

#ifdef WIN32
	SNetStats	SambaServer;
	SNetStats	SambaClient;
#endif
};

struct SUnOverflow
{
	SUnOverflow()
	{
		OverflowCount = 0;
		Value32 = 0;
	}

	uint64	FixValue(uint32 curValue)
	{
		if (curValue < Value32)
			OverflowCount++;
		Value32 = curValue;
		return GetValue();
	}

	__inline uint64 GetValue()
	{
        return ((uint64)OverflowCount * 0xFFFFFFFFULL) + (uint64)Value32;
	}


	uint32 OverflowCount;
	uint32 Value32;
};

struct SDelta32_64 : SDelta<uint64>, SUnOverflow
{
	virtual void Update(uint32 New) {
		SDelta<uint64>::Update(FixValue(New));
	}
	virtual void Update64(uint64 New) {
		SDelta<uint64>::Update(New);
	}
};