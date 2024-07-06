#include "pch.h"
#include "AccessTree.h"
#include "../Library/Common/Strings.h"
#include "../../Library/API/PrivacyAPI.h"

CAccessTree::CAccessTree()
{
	m_Root = std::make_shared<SPathNode>();
}

CAccessTree::~CAccessTree()
{
}

void CAccessTree::Add(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, bool bBlocked)
{ 
	Add(std::make_shared<SAccessStats>(AccessMask, AccessTime, bBlocked), m_Root, Path);
}

void CAccessTree::Add(const SAccessStatsPtr& pStat, SPathNodePtr& pParent, const std::wstring& Path, size_t uOffset)
{
	while (uOffset < Path.length() && Path.at(uOffset) == L'\\')
		uOffset++;

	size_t uPos = Path.find(L'\\', uOffset);
	if (uPos == -1 && uOffset < Path.length())
		uPos = Path.length();
	
	std::wstring Name;
	if (uPos != -1)
		Name = Path.substr(uOffset, uPos - uOffset);

	if (uPos == -1)
	{
		pParent->pStats = pStat;
		return;
	}

	auto &pBranch = pParent->Branches[MkLower(Name)];
	if (!pBranch) {
		pBranch = std::make_shared<SPathNode>();
		pBranch->Name = Name;
	}

	Add(pStat, pBranch, Path, uPos + 1);
}

void CAccessTree::Clear()
{
	m_Root->Branches.clear();
}

CVariant CAccessTree::DumpTree() const
{
	return DumpTree(m_Root);
}

CVariant CAccessTree::DumpTree(const SPathNodePtr& pParent) const
{
	CVariant Node;
	Node.BeginIMap();

	Node.Write(API_V_ACCESS_REF, (uint64)pParent.get());
	Node.Write(API_V_ACCESS_NAME, pParent->Name);

	if (pParent->pStats)
	{
		Node.Write(API_V_ACCESS_TIME, pParent->pStats->LastAccessTime);
		Node.Write(API_V_ACCESS_BLOCKED, pParent->pStats->bBlocked);
		Node.Write(API_V_ACCESS_MASK, pParent->pStats->AccessMask);
	}

	CVariant Children;
	Children.BeginList();
	for (auto &Branch : pParent->Branches)
		Children.WriteVariant(DumpTree(Branch.second));
	Children.Finish();
	if(Children.Count())
		Node.WriteVariant(API_V_ACCESS_NODES, Children);

	Node.Finish();
	return Node;
}