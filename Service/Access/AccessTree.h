#pragma once
#include "../Library/Common/Variant.h"
#include "../Library/API/PrivacyDefs.h"

class CAccessTree
{
	TRACK_OBJECT(CAccessTree)
public:
	CAccessTree();
	virtual ~CAccessTree();

	struct SAccessStats
	{
		TRACK_OBJECT(SAccessStats)
		SAccessStats(uint32 AccessMask, uint64 AccessTime, uint32 NtStatus, bool IsDirectory, bool bBlocked)
			: LastAccessTime(AccessTime), bBlocked(bBlocked), NtStatus(NtStatus), IsDirectory(IsDirectory), AccessMask(AccessMask) {}
		~SAccessStats() {}

		uint64	LastAccessTime = 0;
		bool	bBlocked = false;
		uint32	AccessMask = 0;
		uint32	NtStatus = 0;
		bool	IsDirectory = false;
	};	

	typedef std::shared_ptr<SAccessStats> SAccessStatsPtr;

	void Add(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked);

	uint32 GetAccessCount() const { std::unique_lock lock(m_Mutex); return m_Root->TotalCount; }

	void Clear();

	void CleanUp(bool* pbCancel, uint32* puCounter);
	void Truncate();
	
	struct SPathNode
	{
		TRACK_OBJECT(SPathNode)
		SPathNode() {}
		~SPathNode() {}
		std::wstring Name;
		std::map<std::wstring, std::shared_ptr<SPathNode>> Branches;
		SAccessStatsPtr pStats;
		uint32 TotalCount = 0;
	};

	typedef std::shared_ptr<SPathNode> SPathNodePtr;

	CVariant StoreTree(const SVarWriteOpt& Opts) const;
	void LoadTree(const CVariant& Data);
	CVariant DumpTree(uint64 LastActivity) const;

protected:
	mutable std::recursive_mutex	m_Mutex;

	bool Add(const SAccessStatsPtr& pStat, SPathNodePtr& pParent, const std::wstring& Path, size_t uOffset = 0);

	CVariant StoreTree(const SPathNodePtr& pParent) const;
	CVariant StoreNode(const SPathNodePtr& pParent, const CVariant& Children) const;
	uint32 LoadTree(const CVariant& Data, SPathNodePtr& pParent);
	CVariant DumpTree(const SPathNodePtr& pParent, uint64 LastActivity) const;

	bool CleanUp(SPathNodePtr& pParent, const std::wstring& Path, bool* pbCancel, uint32* puCounter);
	uint64 Truncate(SPathNodePtr& pParent, uint64 CleanupDate);

	SPathNodePtr			m_Root;
};