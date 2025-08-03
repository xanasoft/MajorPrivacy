#pragma once

#include "Memory.h"
#include "Header.h"
#include "Lock.h"

FW_NAMESPACE_BEGIN

class FRAMEWORK_EXPORT MemoryPool : public AbstractMemPool
{
public:
    MemoryPool(ULONG Tag = 'pmem');
    ~MemoryPool();

    bool IsValid() const { return m_Pool != nullptr; }

    static MemoryPool* Create(ULONG Tag = 'pmem');
	static void Destroy(MemoryPool* pPool);

    void* Alloc(size_t Size, uint32 Flags = 0) override;
    void Free(void* Ptr, uint32 Flags = 0) override;

    void CleanUp();

	size_t GetSize() const;
	ULONG GetSmallPages() const;
    ULONG GetEmptyPages() const;
	ULONG GetFullPages() const;
	ULONG GetLargePages() const;

protected:
    MemoryPool(struct POOL *Pool);

    static struct POOL* CreatePool(ULONG Tag);
    static void DeletePool(struct POOL *Pool);

    static void FreeEmptyPages(struct POOL *Pool, ULONG Count);
    static void FreeLargeEmptyPages(struct POOL *Pool);

    static struct PAGE* AllocPage(struct POOL *Pool, ULONG Tag);

    static void* GetCells(struct POOL* Pool, ULONG Count);
    static void FreeCells(void* Block, ULONG Count);

    static void InitCells(struct PAGE *Page);
    static void MarkCells(struct PAGE *Page, ULONG Index, ULONG Count);
    static void ClearCells(struct PAGE *Page, ULONG Index, ULONG Count);
    static ULONG FindCells(struct PAGE *Page, ULONG Count);

    static void* GetLargeChunk(struct POOL* Pool, SIZE_T Size);
    static void FreeLargeChunk(void* Block, SIZE_T Size);

    static void* AllocMem(SIZE_T Size, ULONG Tag);
    static void FreeMem(void* Ptr, ULONG Tag);

    struct POOL* m_Pool = nullptr;

private:
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;
};

FW_NAMESPACE_END