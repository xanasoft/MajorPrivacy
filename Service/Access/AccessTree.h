#pragma once
#include "../Library/Common/Variant.h"

class CAccessTree
{
public:
	CAccessTree();
	virtual ~CAccessTree();

	struct SAccessStats
	{
		SAccessStats(uint32 AccessMask, uint64 AccessTime, bool bBlocked)
			: LastAccessTime(AccessTime), bBlocked(bBlocked), AccessMask(AccessMask) {}

		uint64	LastAccessTime = 0;
		bool	bBlocked = false;
		uint32	AccessMask = 0;
	};	

	typedef std::shared_ptr<SAccessStats> SAccessStatsPtr;

	void Add(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, bool bBlocked);
	void Clear();
	
	struct SPathNode
	{
		std::wstring Name;
		std::map<std::wstring, std::shared_ptr<SPathNode>> Branches;
		SAccessStatsPtr pStats;
	};

	typedef std::shared_ptr<SPathNode> SPathNodePtr;

	CVariant DumpTree() const;

protected:

	void Add(const SAccessStatsPtr& pStat, SPathNodePtr& pParent, const std::wstring& Path, size_t uOffset = 0);

	CVariant DumpTree(const SPathNodePtr& pParent) const;

	SPathNodePtr			m_Root;
};