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
	void Clear();
	
	struct SPathNode
	{
		TRACK_OBJECT(SPathNode)
		SPathNode() {}
		~SPathNode() {}
		std::wstring Name;
		std::map<std::wstring, std::shared_ptr<SPathNode>> Branches;
		SAccessStatsPtr pStats;
	};

	typedef std::shared_ptr<SPathNode> SPathNodePtr;

	CVariant StoreTree(const SVarWriteOpt& Opts) const;
	void LoadTree(const CVariant& Data);
	CVariant DumpTree(uint64 LastActivity) const;

protected:

	void Add(const SAccessStatsPtr& pStat, SPathNodePtr& pParent, const std::wstring& Path, size_t uOffset = 0);

	CVariant StoreTree(const SPathNodePtr& pParent) const;
	CVariant StoreNode(const SPathNodePtr& pParent, const CVariant& Children) const;
	void LoadTree(const CVariant& Data, SPathNodePtr& pParent);
	CVariant DumpTree(const SPathNodePtr& pParent, uint64 LastActivity) const;

	SPathNodePtr			m_Root;
};