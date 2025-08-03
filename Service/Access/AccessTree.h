#pragma once
#include "../Library/Common/StVariant.h"
#include "../Library/API/PrivacyDefs.h"

#include "../Framework/Core/MemoryPool.h"
#include "../Framework/Core/Object.h"
#include "../Framework/Core/Map.h"

class CAccessTree
{
	TRACK_OBJECT(CAccessTree)
public:
	CAccessTree();
	~CAccessTree();

#ifdef DEF_USE_POOL
	void SetPool(FW::AbstractMemPool* pMem) { m_pMem = pMem; }
#endif

	struct SAccessStats
	{
		//TRACK_OBJECT(SAccessStats)
		SAccessStats(uint32 AccessMask = 0, uint64 AccessTime = 0, uint32 NtStatus = 0, bool IsDirectory = false, bool bBlocked = false)
			: LastAccessTime(AccessTime), bBlocked(bBlocked), NtStatus(NtStatus), IsDirectory(IsDirectory), AccessMask(AccessMask) {}
		~SAccessStats() {}

		uint64	LastAccessTime = 0;
		bool	bBlocked = false;
		uint32	AccessMask = 0;
		uint32	NtStatus = 0;
		bool	IsDirectory = false;
	};	

	void Add(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked);

	uint32 GetAccessCount() const { std::unique_lock lock(m_Mutex); return m_Root ? m_Root->TotalCount : 0; }

	void Clear();

	void CleanUp(bool* pbCancel, uint32* puCounter);
	void Truncate();

#ifdef DEF_USE_POOL
	struct SPathNode : FW::Object
#else
	struct SPathNode
#endif
	{
		//TRACK_OBJECT(SPathNode)
#ifdef DEF_USE_POOL
		SPathNode(FW::AbstractMemPool* pMem) : FW::Object(pMem), Name(pMem), Branches(pMem) {}
#else
		SPathNode() {}
#endif
		~SPathNode() {}
#ifdef DEF_USE_POOL
		FW::StringW Name;
		FW::Map<FW::StringW, FW::SharedPtr<SPathNode>> Branches;
#else
		std::wstring Name;
		std::map<std::wstring, std::shared_ptr<SPathNode>> Branches;
#endif
		SAccessStats Stats;
		uint32 TotalCount = 0;
	};

#ifdef DEF_USE_POOL
	typedef FW::SharedPtr<SPathNode> SPathNodePtr;
#else
	typedef std::shared_ptr<SPathNode> SPathNodePtr;
#endif

	StVariant StoreTree(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	void LoadTree(const StVariant& Data);
	StVariant DumpTree(uint64 LastActivity, FW::AbstractMemPool* pMemPool = nullptr) const;

protected:
	mutable std::recursive_mutex	m_Mutex;

	bool Add(const SAccessStats& Stat, SPathNodePtr& pParent, const std::wstring& Path, size_t uOffset = 0);

	StVariant StoreTree(const SPathNodePtr& pParent, FW::AbstractMemPool* pMemPool = nullptr) const;
	StVariant StoreNode(const SPathNodePtr& pParent, const StVariant& Children, FW::AbstractMemPool* pMemPool = nullptr) const;
	uint32 LoadTree(const StVariant& Data, SPathNodePtr& pParent);
	StVariant DumpTree(const SPathNodePtr& pParent, uint64 LastActivity, FW::AbstractMemPool* pMemPool = nullptr) const;

	bool CleanUp(SPathNodePtr& pParent, const std::wstring& Path, bool* pbCancel, uint32* puCounter);
	uint64 Truncate(SPathNodePtr& pParent, uint64 CleanupDate);

	SPathNodePtr			m_Root;

#ifdef DEF_USE_POOL
	FW::AbstractMemPool*	m_pMem = nullptr;
#endif
};