
#include "MemoryPool.h"

#ifndef NO_CRT
#include <memory>
#endif

#ifdef KERNEL_MODE

#define POOL_PAGE_SIZE          4096    // kernel page size
#define POOL_CELL_SIZE          16
#ifdef _WIN64
#define POOL_MASK_LEFT          0xFFFFFFFFFFFFF000UL
#else
#define POOL_MASK_LEFT          0xFFFFF000UL
#endif
#define POOL_MASK_RIGHT         0xFFF

#else

#define POOL_PAGE_SIZE          65536   // VirtualAlloc granularity
#define POOL_CELL_SIZE          128
#ifdef _WIN64
#define POOL_MASK_LEFT          0xFFFFFFFFFFFF0000UL
#else
#define POOL_MASK_LEFT          0xFFFF0000UL
#endif
#define POOL_MASK_RIGHT         0xFFFF

#endif

// The smallest size (in bytes) that triggers a "large-chunk" allocation.
// Any request exceeding three-quarters of a page is handled via a dedicated OS
// allocation rather than from the per-page cell-based pools.
#define LARGE_CHUNK_MINIMUM     (POOL_PAGE_SIZE * 3 / 4)

// When the number of free cells on a page falls below this threshold,
// we stop trying to satisfy small-block requests from it and move it to
// the "full pages" list. This avoids scanning nearly-full pages repeatedly.
#define FULL_PAGE_THRESHOLD     4

// After MAX_FAIL_COUNT unsuccessful attempts to find a contiguous run
// of the requested number of free cells in this page's bitmap, we assume
// the page is too fragmented for further small-cell allocations.
// At that point, it's removed from the "small pages" list and placed
// on the "full pages" list to avoid wasting time rescanning it.
#define MAX_FAIL_COUNT			((1 << 3) - 1)

// PAD_8: Round an arbitrary byte-size 'p' up to the nearest multiple of 8.
// This ensures 8-byte alignment for structures and blocks.
#define PAD_8(p)                (((p) + 7) & ~7)
// PAD_CELL: Round a byte-size 'p' up to the nearest multiple of POOL_CELL_SIZE.
// All small allocations and headers are aligned to whole cells.
#define PAD_CELL(p)             (((p) + POOL_CELL_SIZE - 1) & ~(POOL_CELL_SIZE - 1))
// NUM_CELLS: Compute how many fixed-size cells are needed to hold 'n' bytes,
// after padding up to cell-size alignment.
#define NUM_CELLS(n)            (PAD_CELL((ULONG)(n)) / POOL_CELL_SIZE)

// PAGE_HEADER_SIZE: The size, in bytes, of the PAGE metadata struct,
// rounded up to whole-cell alignment so the bitmap and user data start on
// a cell boundary.
#define PAGE_HEADER_SIZE        PAD_CELL(sizeof(PAGE))

// RAW_BITMAP_SIZE: The size, in bytes, of the allocation bitmap for each page.
// Note: Since we dont a priori know the size of the bitmap and only use the known header size
// we designate the bitmap large enough to map the entire remining page size, 
// even though some of it will be used for the bitmap itself.
#define RAW_BITMAP_SIZE         (((POOL_PAGE_SIZE - sizeof(HEADER)) / POOL_CELL_SIZE + 7) / 8)   

// NUM_PAGE_CELLS: The total number of allocatable cells on each page,
// after reserving space for the page header and bitmap.
#define NUM_PAGE_CELLS          ((POOL_PAGE_SIZE - PAGE_HEADER_SIZE) / POOL_CELL_SIZE)

// LARGE_CHUNK_SIZE: The size of the LARGE_CHUNK metadata struct, aligned to 8 bytes.
// Large chunks live at the end of their OS-allocated region.
#define LARGE_CHUNK_SIZE        PAD_8(sizeof(LARGE_CHUNK))

// BLOCK_SIZE: The size of our internal BLOCK header (stores the allocation size),
// aligned to 8 bytes for proper alignment of user data.
#define BLOCK_SIZE				PAD_8(sizeof(BLOCK))

// Uncomment and adjust if you need to enforce a maximum large-chunk size
// based on 32-bit addressable limits.
//#define LARGE_CHUNK_MAXIMUM     (0xFFFFFFFFu - (BLOCK_SIZE + LARGE_CHUNK_SIZE + POOL_PAGE_SIZE))

#define USE_FULL_LIST

FW_NAMESPACE_BEGIN

struct BLOCK 
{
    SIZE_T Size;
#pragma warning( push )
#pragma warning( disable : 4200)
    BYTE Ptr[0];
#pragma warning( pop )
};

struct HEADER 
{
	LIST_ENTRY ListEntry;
	struct POOL* Pool;
	ULONG EyeCatcher;
	USHORT NumFree;
	union {
		USHORT Flags;
#ifdef USE_FULL_LIST
		struct {
			USHORT 
				FailCount	: 3,
				IsFull		: 1,  // Page is on full list
				Reserved	: 12; // Reserved
		};
#endif
	};
};

struct PAGE 
{
	HEADER Header;
	UCHAR Bitmap[RAW_BITMAP_SIZE];
};

struct POOL 
{
    FW::Lock Lock;

    LIST_ENTRY Pages;
#ifdef USE_FULL_LIST
    LIST_ENTRY FullPages;
#endif
    LIST_ENTRY LargeChunks;

    ULONG EyeCatcher;
    ULONG SmallCount;
	ULONG EmptyCount;
    ULONG FullCount;
    ULONG LargeCount;

    //UCHAR InitialBitmap[RAW_BITMAP_SIZE];
};

struct LARGE_CHUNK // FOOTER
{
    LIST_ENTRY ListEntry;
    POOL *Pool;
    ULONG EyeCatcher;
    ULONG Count;
    void *Ptr;
};

MemoryPool::MemoryPool(ULONG Tag)
{
	m_Pool = CreatePool(Tag);
}

MemoryPool::MemoryPool(struct POOL* Pool)
{
	m_Pool = Pool;
}

MemoryPool::~MemoryPool()
{
	if (m_Pool) {
		DeletePool(m_Pool);
		m_Pool = nullptr;
	}
}

MemoryPool* MemoryPool::Create(ULONG Tag)
{
	POOL* Pool = CreatePool(Tag);
	if (!Pool)
		return nullptr;

	void* Ptr = GetCells(Pool, NUM_CELLS(sizeof(MemoryPool)));
	ASSERT(Ptr); // this must not fail pool is created with enough space

	MemoryPool* pPool = new (Ptr) MemoryPool(Pool);
	return pPool;
}

void MemoryPool::Destroy(MemoryPool* pPool)
{
	if (!pPool)
		return;

	POOL* Pool = pPool->m_Pool;
	DeletePool(Pool);
}

void* MemoryPool::Alloc(size_t Size, uint32 Flags)
{
	BLOCK* Block = NULL;
	if (!Size || !m_Pool)
		return NULL;

#ifdef LARGE_CHUNK_MAXIMUM
	if (Size > LARGE_CHUNK_MAXIMUM)
		return NULL;
#endif

	SIZE_T BlockSize = (sizeof(BLOCK) + Size);

	if (Size > LARGE_CHUNK_MINIMUM)
		Block = (BLOCK*)GetLargeChunk(m_Pool, BlockSize);
	else
		Block = (BLOCK*)GetCells(m_Pool, NUM_CELLS(BlockSize));

	if (!Block)
		return NULL;

	Block->Size = Size;
	return Block->Ptr;
}

void MemoryPool::Free(void* Ptr, uint32 Flags)
{
	if (!Ptr)
		return;

	BLOCK* Block = (BLOCK*)((BYTE*)Ptr - sizeof(BLOCK));

	SIZE_T BlockSize = (sizeof(BLOCK) + Block->Size);

	if (((ULONG_PTR)Block & (POOL_PAGE_SIZE - 1)) == 0)
		FreeLargeChunk(Block, BlockSize);
	else
		FreeCells(Block, NUM_CELLS(BlockSize));
}

void MemoryPool::CleanUp()
{
	Locker Lock(m_Pool->Lock);

	FreeEmptyPages(m_Pool, (ULONG)-1);
}

POOL* MemoryPool::CreatePool(ULONG Tag)
{
	PAGE* Page = AllocPage(NULL, Tag);
	if (!Page)
		return nullptr;

	InitCells(Page);

	POOL* Pool = new ((UCHAR*)Page + PAGE_HEADER_SIZE) POOL();
	Page->Header.Pool = Pool;
	Pool->EyeCatcher = Tag;
	Pool->SmallCount = 0;
	Pool->EmptyCount = 0;
	Pool->FullCount = 0;
	Pool->LargeCount = 0;

	//MemCopy(Pool->InitialBitmap, Bitmap, RAW_BITMAP_SIZE);

	InitializeListHead(&Pool->Pages);
#ifdef USE_FULL_LIST
	InitializeListHead(&Pool->FullPages);
#endif
	InitializeListHead(&Pool->LargeChunks);
	InsertHeadList(&Pool->Pages, &Page->Header.ListEntry);
	Pool->SmallCount++;

	ULONG Count = NUM_CELLS(sizeof(POOL));
	Page->Header.NumFree -= (USHORT)Count;
	MarkCells(Page, 0, Count);

	return Pool;
}

void MemoryPool::DeletePool(POOL* Pool)
{
	ULONG Tag = Pool->EyeCatcher;

	while (!IsListEmpty(&Pool->LargeChunks)) {
		PLIST_ENTRY Entry = RemoveHeadList(&Pool->LargeChunks);
		LARGE_CHUNK* LargeChunk = CONTAINING_RECORD(Entry, LARGE_CHUNK, ListEntry);
		FreeMem(LargeChunk->Ptr, Tag);
	}

	while (!IsListEmpty(&Pool->Pages)) {
		PLIST_ENTRY Entry = RemoveHeadList(&Pool->Pages);
		PAGE* Page = CONTAINING_RECORD(Entry, PAGE, Header.ListEntry);
		if (((ULONG_PTR)Pool & POOL_MASK_LEFT) != (ULONG_PTR)Page)
			FreeMem(Page, Tag);
	}

#ifdef USE_FULL_LIST
	while (!IsListEmpty(&Pool->FullPages)) {
		PLIST_ENTRY Entry = RemoveHeadList(&Pool->FullPages);
		PAGE* Page = CONTAINING_RECORD(Entry, PAGE, Header.ListEntry);
		if (((ULONG_PTR)Pool & POOL_MASK_LEFT) != (ULONG_PTR)Page)
			FreeMem(Page, Tag);
	}
#endif

	Pool->~POOL();

	PAGE* Page = (PAGE*)((ULONG_PTR)Pool & POOL_MASK_LEFT);
	FreeMem(Page, Tag);
}

void MemoryPool::FreeEmptyPages(struct POOL* Pool, ULONG Count)
{
	for (PLIST_ENTRY Entry = Pool->Pages.Flink; Count && Entry != &Pool->Pages; )
	{
		PAGE* Page = CONTAINING_RECORD(Entry, PAGE, Header.ListEntry);
		ASSERT(Page->Header.EyeCatcher == Pool->EyeCatcher);

		Entry = Entry->Flink;

		if (Page->Header.NumFree == NUM_PAGE_CELLS)
		{
			RemoveEntryList(&Page->Header.ListEntry);
			Pool->SmallCount--;
			Pool->EmptyCount--;
			FreeMem(Page, Pool->EyeCatcher);
			Count--;
		}
	}
}

PAGE* MemoryPool::AllocPage(POOL* Pool, ULONG Tag)
{
	PAGE* Page = (PAGE*)AllocMem(POOL_PAGE_SIZE, Tag);
	if (Page)
	{
		Page->Header.EyeCatcher = Tag;
		Page->Header.NumFree = NUM_PAGE_CELLS;
		Page->Header.Flags = 0;

		if (Pool)
		{
			InitCells(Page);
			//MemCopy(Page->Bitmap, Pool->InitialBitmap, RAW_BITMAP_SIZE);
			Page->Header.Pool = Pool;
			InsertHeadList(&Pool->Pages, &Page->Header.ListEntry);
			Pool->SmallCount++;
		}
	}
	return Page;
}

void* MemoryPool::GetCells(POOL* Pool, ULONG Count)
{
	Locker Lock(Pool->Lock);

	PAGE* Page = NULL;
	ULONG Index = (ULONG)-1;

#ifdef USE_FULL_LIST
	for (PLIST_ENTRY Entry = Pool->Pages.Flink; Entry != &Pool->Pages; )
	{
		Page = CONTAINING_RECORD(Entry, PAGE, Header.ListEntry);
		ASSERT(Page->Header.EyeCatcher == Pool->EyeCatcher);

		Entry = Entry->Flink;

		if (Page->Header.NumFree >= Count)
		{
			Index = FindCells(Page, Count);
			if (Index != -1)
				break;

			Page->Header.FailCount++;
			if (Page->Header.FailCount >= MAX_FAIL_COUNT)
			{
				Page->Header.IsFull = 1;
				RemoveEntryList(&Page->Header.ListEntry);
				InsertHeadList(&Pool->FullPages, &Page->Header.ListEntry);
				Pool->FullCount++;
			}
		}
		else if(Page->Header.FailCount > 0)
			Page->Header.FailCount--;
	}
#else
	if (Pool->Pages.Flink != &Pool->Pages)
	{
		PLIST_ENTRY Entry = Pool->Pages.Flink;
		PLIST_ENTRY End = Pool->Pages.Blink;

		for (;;)
		{
			Page = CONTAINING_RECORD(Entry, PAGE, Header.ListEntry);
			ASSERT(Page->Header.EyeCatcher == Pool->EyeCatcher);

			PLIST_ENTRY Next = Entry->Flink;

			if (Page->Header.NumFree >= Count)
			{
				Index = FindCells(Page, Count);
				if (Index != -1)
					break;

				RemoveEntryList(&Page->Header.ListEntry);
				InsertTailList(&Pool->Pages, &Page->Header.ListEntry);
			}

			if (Entry == End)
				break;
			Entry = Next;
		}
	}
#endif

	if (Index == -1)
	{
		Page = AllocPage(Pool, Pool->EyeCatcher);
		if (!Page)
			return NULL;
		Index = 0;
	}
	else if (Page->Header.NumFree == NUM_PAGE_CELLS)
		Pool->EmptyCount--;

	Page->Header.NumFree -= (USHORT)Count;
#ifdef USE_FULL_LIST
	if (Page->Header.NumFree < FULL_PAGE_THRESHOLD)
	{
		Page->Header.IsFull = 1;
		RemoveEntryList(&Page->Header.ListEntry);
		InsertHeadList(&Pool->FullPages, &Page->Header.ListEntry);
		Pool->FullCount++;
	}
#endif

	MarkCells(Page, Index, Count);

	UCHAR* Ptr = (UCHAR*)Page + PAGE_HEADER_SIZE + Index * POOL_CELL_SIZE;

	return Ptr;
}

void MemoryPool::FreeCells(void* Ptr, ULONG Count)
{
	PAGE* Page = (PAGE*)((ULONG_PTR)Ptr & POOL_MASK_LEFT);
	ULONG Index = (((ULONG)(ULONG_PTR)Ptr & POOL_MASK_RIGHT) - PAGE_HEADER_SIZE) / POOL_CELL_SIZE;

	POOL* Pool = Page->Header.Pool;
	ASSERT(Page->Header.EyeCatcher == Pool->EyeCatcher);

	Locker Lock(Pool->Lock);

	Page->Header.NumFree += (USHORT)Count;
#ifdef USE_FULL_LIST
	if (Page->Header.IsFull && Page->Header.NumFree > FULL_PAGE_THRESHOLD)
	{
		Page->Header.IsFull = 0;
		Page->Header.FailCount >>= 1; // /= 2;
		RemoveEntryList(&Page->Header.ListEntry);
		InsertHeadList(&Pool->Pages, &Page->Header.ListEntry);
		Pool->FullCount--;
	}
#endif
	
	ClearCells(Page, Index, Count);

	if (Page->Header.NumFree == NUM_PAGE_CELLS) 
	{
		Pool->EmptyCount++;

		if((Pool->EmptyCount << 1) > Pool->SmallCount) // Pool->EmptyCount * 2 > Pool->SmallCount
			FreeEmptyPages(Pool, Pool->EmptyCount >> 1); // Pool->EmptyCount / 2
	}
}

void MemoryPool::InitCells(PAGE *Page)
{
	UCHAR* Bitmap = Page->Bitmap;
	MemZero(Bitmap, RAW_BITMAP_SIZE);
	for (ULONG i = NUM_PAGE_CELLS; ;) {
		ULONG byte = i / 8;
		ULONG bit = 1 << (i & 7);
		if (byte >= RAW_BITMAP_SIZE)
			break;
		if (bit == 1) {
			Bitmap[byte] = 0xFF;
			i += 8;
		}
		else {
			Bitmap[byte] |= bit;
			++i;
		}
	}
}

void MemoryPool::MarkCells(PAGE *Page, ULONG Index, ULONG Count)
{
	UCHAR* Bitmap = Page->Bitmap + (Index / 8);
	ULONG Mask = 1 << (Index & 7);
	while (Count) {
		if (Mask == 1 && Count > 8) {
			*Bitmap = 0xFF;
			Bitmap++;
			Count -= 8;
		}
		else {
			*Bitmap |= Mask;
			Mask <<= 1;
			Mask = (Mask & 0xFF) | (Mask >> 8);
			Bitmap += Mask & 1;
			--Count;
		}
	}
}

void MemoryPool::ClearCells(PAGE *Page, ULONG Index, ULONG Count)
{
	UCHAR* Bitmap = Page->Bitmap + (Index / 8);
	ULONG Mask = 1 << (Index & 7);
	while (Count) {
		if (Mask == 1 && Count > 8) {
			*Bitmap = 0;
			Bitmap++;
			Count -= 8;
		}
		else {
			*Bitmap &= ~Mask;
			Mask <<= 1;
			Mask = (Mask & 0xFF) | (Mask >> 8);
			Bitmap += Mask & 1;
			--Count;
		}
	}
}

ULONG MemoryPool::FindCells(PAGE* Page, ULONG Count)
{
	UCHAR* Bitmap = Page->Bitmap;
	ULONG Mask = 1;

	for (ULONG i = 0; i < NUM_PAGE_CELLS; ) {
		ULONG bit = *Bitmap & Mask;
		ULONG j = i + 1;
		Mask <<= 1;
		Mask = (Mask & 0xFF) | (Mask >> 8);
		Bitmap += Mask & 1;
		if (bit) {
			while (j < NUM_PAGE_CELLS) {
				if (Mask == 1 && *Bitmap == 0xFF) {
					j += 8;
					++Bitmap;
				}
				else {
					if ((*Bitmap & Mask) == 0)
						break;
					++j;
					Mask <<= 1;
					Mask = (Mask & 0xFF) | (Mask >> 8);
					Bitmap += Mask & 1;
				}
			}
		}
		else {
			while (j < NUM_PAGE_CELLS && j - i < Count) {
				if (Mask == 1 && *Bitmap == 0) {
					j += 8;
					++Bitmap;
				}
				else {
					if ((*Bitmap & Mask) != 0)
						break;
					++j;
					Mask <<= 1;
					Mask = (Mask & 0xFF) | (Mask >> 8);
					Bitmap += Mask & 1;
				}
			}
			if (j - i >= Count)
				return i;
		}
		i = j;
	}

	return (ULONG)-1;
}

void* MemoryPool::GetLargeChunk(POOL* Pool, SIZE_T Size)
{
	SIZE_T LargeChunkSize = (Size + LARGE_CHUNK_SIZE + POOL_PAGE_SIZE - 1) & ~(POOL_PAGE_SIZE - 1);

	void* Ptr = AllocMem(LargeChunkSize, Pool->EyeCatcher);
	if (!Ptr)
		return NULL;

	LARGE_CHUNK* LargeChunk = (LARGE_CHUNK*)((UCHAR*)Ptr + LargeChunkSize - LARGE_CHUNK_SIZE);

	LargeChunk->Pool = Pool;
	LargeChunk->EyeCatcher = Pool->EyeCatcher;
	LargeChunk->Count = (ULONG)(LargeChunkSize / POOL_PAGE_SIZE);
	LargeChunk->Ptr = Ptr;

	Locker Lock(Pool->Lock);
	InsertHeadList(&Pool->LargeChunks, &LargeChunk->ListEntry);
	Pool->LargeCount += LargeChunk->Count;

	return Ptr;
}

void MemoryPool::FreeLargeChunk(void* Ptr, SIZE_T Size)
{
	SIZE_T LargeChunkSize = (Size + LARGE_CHUNK_SIZE + POOL_PAGE_SIZE - 1) & ~(POOL_PAGE_SIZE - 1);

	LARGE_CHUNK* LargeChunk = (LARGE_CHUNK*)((UCHAR*)Ptr + LargeChunkSize - LARGE_CHUNK_SIZE);

	POOL* Pool = LargeChunk->Pool;
	ASSERT(LargeChunk->EyeCatcher == Pool->EyeCatcher);

	Locker Lock(Pool->Lock);
	RemoveEntryList(&LargeChunk->ListEntry);
	Pool->LargeCount -= LargeChunk->Count;
	Lock.Unlock();

	FreeMem(Ptr, Pool->EyeCatcher);
}

size_t MemoryPool::GetSize() const 
{ 
	return m_Pool ? (m_Pool->SmallCount + m_Pool->LargeCount) * POOL_PAGE_SIZE : 0; 
}

ULONG MemoryPool::GetSmallPages() const 
{ 
	return m_Pool ? m_Pool->SmallCount : 0; 
}

ULONG MemoryPool::GetEmptyPages() const 
{ 
	return m_Pool ? m_Pool->EmptyCount : 0; 
}

ULONG MemoryPool::GetFullPages() const 
{ 
	return m_Pool ? m_Pool->FullCount : 0; 
}

ULONG MemoryPool::GetLargePages() const 
{ 
	return m_Pool ? m_Pool->LargeCount : 0; 
}

void* MemoryPool::AllocMem(SIZE_T Size, ULONG Tag)
{
//#ifdef _DEBUG
//	printf("AllocMem: %llu\n", Size);
//#endif
#ifdef KERNEL_MODE

#if (NTDDI_VERSION >= NTDDI_WIN10_VB)
	return ExAllocatePool2(POOL_FLAG_NON_PAGED, Size, Tag);
#else
#pragma warning(suppress: 4996) // suppress deprecation warning
	return ExAllocatePoolWithTag(NonPagedPool, Size, Tag);
#endif
#else
	return VirtualAlloc(NULL, Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#endif
}

void MemoryPool::FreeMem(void* Ptr, ULONG Tag)
{
//#ifdef _DEBUG
//	printf("FreeMem: %p\n", Ptr);
//#endif
#ifdef KERNEL_MODE
	ExFreePoolWithTag(Ptr, Tag);
#else
	if (!VirtualFree(Ptr, 0, MEM_RELEASE)) {
		RaiseException(STATUS_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE_EXCEPTION, 0, NULL);
		ExitProcess(-1);
	}
#endif
}

FW_NAMESPACE_END