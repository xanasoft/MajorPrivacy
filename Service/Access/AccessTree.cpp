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

void CAccessTree::Add(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked)
{ 
	Add(std::make_shared<SAccessStats>(AccessMask, AccessTime, NtStatus, IsDirectory, bBlocked), m_Root, Path);
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

CVariant CAccessTree::StoreTree(const SVarWriteOpt& Opts) const
{
	return StoreTree(m_Root);
}

CVariant CAccessTree::StoreTree(const SPathNodePtr& pParent) const
{
	CVariant Children;
	Children.BeginList();
	for (auto &Branch : pParent->Branches)
		Children.WriteVariant(StoreTree(Branch.second));
	Children.Finish();

	return StoreNode(pParent, Children);
}

void CAccessTree::LoadTree(const CVariant& Data)
{
	LoadTree(Data, m_Root);
}

void CAccessTree::LoadTree(const CVariant& Data, SPathNodePtr& pParent)
{
	pParent->pStats = std::make_shared<SAccessStats>(Data[API_V_ACCESS_MASK], Data[API_V_ACCESS_TIME], Data[API_V_ACCESS_STATUS], Data[API_V_ACCESS_IS_DIR], Data[API_V_ACCESS_BLOCKED]);
	CVariant Nodes = Data[API_V_ACCESS_NODES];
	for (uint32 i = 0; i < Nodes.Count(); i++)
	{
		CVariant Node = Nodes[i];
		std::wstring Name = Node[API_V_ACCESS_NAME].AsStr();
		auto& pBranch = pParent->Branches[MkLower(Name)];
		if (!pBranch) {
			pBranch = std::make_shared<SPathNode>();
			pBranch->Name = Name;
		}
		LoadTree(Node, pBranch);
	}
}

CVariant CAccessTree::DumpTree(uint64 LastActivity) const
{
	return DumpTree(m_Root, LastActivity);
}

CVariant CAccessTree::DumpTree(const SPathNodePtr& pParent, uint64 LastActivity) const
{
	int Count = 0;
	CVariant Children;
	Children.BeginList();
	for (auto& Branch : pParent->Branches) {
		CVariant Child = DumpTree(Branch.second, LastActivity);
		if (Child.IsValid()) {
			Count++;
			Children.WriteVariant(Child);
		}
	}
	Children.Finish();

	if (Count == 0) {
		if(pParent->pStats && pParent->pStats->LastAccessTime <= LastActivity)
			return CVariant();
		return StoreNode(pParent, CVariant());
	}
	return StoreNode(pParent, Children);
}

CVariant CAccessTree::StoreNode(const SPathNodePtr& pParent, const CVariant& Children) const
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
		Node.Write(API_V_ACCESS_STATUS, pParent->pStats->NtStatus);
		Node.Write(API_V_ACCESS_IS_DIR, pParent->pStats->IsDirectory);
	}

	if(Children.IsValid())
		Node.WriteVariant(API_V_ACCESS_NODES, Children);

	Node.Finish();
	return Node;
}