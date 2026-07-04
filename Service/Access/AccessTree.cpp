#include "pch.h"
#include "AccessTree.h"
#include "../Library/Common/Strings.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtObj.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/NtPathMgr.h"
#include "../Library/Helpers/MiscHelpers.h"
#include "../ServiceCore.h"

#define MAX_BRANCHES_BEFORE_COLLAPSE 1000

CAccessTree::CAccessTree()
{
}

CAccessTree::~CAccessTree()
{
}

void CAccessTree::MergeStats(SAccessStats& Target, const SAccessStats& Source)
{
	// Keep the most recent access time and its associated status
	if (Source.LastAccessTime > Target.LastAccessTime) {
		Target.LastAccessTime = Source.LastAccessTime;
		Target.NtStatus = Source.NtStatus;
		Target.IsDirectory = Source.IsDirectory;
	}
	// OR the access masks and blocked flags together
	Target.AccessMask |= Source.AccessMask;
	Target.bBlocked |= Source.bBlocked;
	// Sum up access counts
	Target.AccessCount += Source.AccessCount;
}

void CAccessTree::Add(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked)
{ 
	std::unique_lock lock(m_Mutex); 

	if (!m_Root) {
#ifdef DEF_USE_POOL
		if(!m_pMem)
			return;
		m_Root = m_pMem->New<SPathNode>();
#else
		m_Root = std::make_shared<SPathNode>();
#endif
	}

	if (Path.substr(0, 2) == L"\\\\")
		Add(SAccessStats(AccessMask, AccessTime, NtStatus, IsDirectory, bBlocked), m_Root, L"\\device\\mup\\" + Path.substr(2));
	else
		Add(SAccessStats(AccessMask, AccessTime, NtStatus, IsDirectory, bBlocked), m_Root, Path);
}

bool CAccessTree::Add(const SAccessStats& Stat, SPathNodePtr& pParent, const std::wstring& Path, size_t uOffset)
{
	while (uOffset < Path.length() && Path.at(uOffset) == L'\\')
		uOffset++;

	size_t uPos = Path.find(L'\\', uOffset);
	if (uPos == -1 && uOffset < Path.length())
		uPos = Path.length();

#ifdef DEF_USE_POOL
	FW::StringW Name(m_pMem);
	if (uPos != -1)
		Name.Assign(Path.c_str() + uOffset, uPos - uOffset);
#else
	std::wstring Name;
	if (uPos != -1)
		Name = Path.substr(uOffset, uPos - uOffset);
#endif

	if (uPos == -1)
	{
		MergeStats(pParent->Stats, Stat);
		return false;
	}

	bool bAdded = false;

	// Check if a catch-all "*" branch already exists - if so, use it
	// Use find() to avoid creating null entries via operator[]
#ifdef DEF_USE_POOL
	FW::StringW catchAllKey(m_pMem);
	catchAllKey.Assign(L"*", 1);
	auto itCatchAll = pParent->Branches.find(catchAllKey);
	if (itCatchAll != pParent->Branches.end()) {
		SPathNodePtr pCatchAll = itCatchAll.Value();
		if(Add(Stat, *&pCatchAll, Path, uPos + 1))
			bAdded = true;
		if(bAdded)
			pParent->TotalCount++;
		return bAdded;
	}
#else
	auto itCatchAll = pParent->Branches.find(L"*");
	if (itCatchAll != pParent->Branches.end()) {
		if(Add(Stat, itCatchAll->second, Path, uPos + 1))
			bAdded = true;
		if(bAdded)
			pParent->TotalCount++;
		return bAdded;
	}
#endif

	// Check if we need to collapse before looking up/creating the branch
	// Use find() to avoid creating null entries via operator[]
#ifdef DEF_USE_POOL
	FW::StringW name = Name;
	name.MakeLower();
	auto itBranch = pParent->Branches.find(name);
	bool bBranchExists = (itBranch != pParent->Branches.end());
#else
	std::wstring nameLower = MkLower(Name);
	auto itBranch = pParent->Branches.find(nameLower);
	bool bBranchExists = (itBranch != pParent->Branches.end());
#endif

	if (!bBranchExists) {
		// About to create a new branch - check if we've exceeded the threshold
		if (pParent->Branches.size() >= MAX_BRANCHES_BEFORE_COLLAPSE) {
			// Collapse all branches into a catch-all "*" branch
#ifdef DEF_USE_POOL
			auto pNewCatchAll = m_pMem->New<SPathNode>();
			(*&pNewCatchAll)->Name.Assign(L"*", 1);
#else
			auto pNewCatchAll = std::make_shared<SPathNode>();
			pNewCatchAll->Name = L"*";
#endif
			// Aggregate stats and total count from all existing branches
			uint32 totalCount = 0;
#ifdef DEF_USE_POOL
			for (auto I = pParent->Branches.begin(); I != pParent->Branches.end(); ++I) {
				SPathNodePtr pExisting = I.Value();
#else
			for (auto& branch : pParent->Branches) {
				SPathNodePtr pExisting = branch.second;
#endif
				if (!pExisting)
					continue; // Skip null entries
				MergeStats(pNewCatchAll->Stats, pExisting->Stats);
				totalCount += 1 + pExisting->TotalCount;
			}
			pNewCatchAll->TotalCount = totalCount;

			// Clear existing branches and add the catch-all
			pParent->Branches.clear();
#ifdef DEF_USE_POOL
			pParent->Branches[catchAllKey] = pNewCatchAll;
#else
			pParent->Branches[L"*"] = pNewCatchAll;
#endif
			// The total count remains the same (catch-all represents all collapsed entries)
			// Now add the new path to the catch-all branch
#ifdef DEF_USE_POOL
			if(Add(Stat, *&pNewCatchAll, Path, uPos + 1))
#else
			if(Add(Stat, pNewCatchAll, Path, uPos + 1))
#endif
				bAdded = true;
			if(bAdded)
				pParent->TotalCount++;
			return bAdded;
		}

		// Create the new branch - now safe to use operator[] since we're actually inserting
		bAdded = true;
#ifdef DEF_USE_POOL
		auto pBranch = pParent->Branches[name]; // SafeRef - not a reference
		pBranch = m_pMem->New<SPathNode>();
		(*&pBranch)->Name = Name;
		if(Add(Stat, *&pBranch, Path, uPos + 1))
			bAdded = true;
#else
		auto& pBranch = pParent->Branches[nameLower];
		pBranch = std::make_shared<SPathNode>();
		pBranch->Name = Name;
		if(Add(Stat, pBranch, Path, uPos + 1))
			bAdded = true;
#endif
	}
	else {
		// Branch already exists, use it
#ifdef DEF_USE_POOL
		SPathNodePtr pBranch = itBranch.Value();
		if(Add(Stat, *&pBranch, Path, uPos + 1))
			bAdded = true;
#else
		if(Add(Stat, itBranch->second, Path, uPos + 1))
			bAdded = true;
#endif
	}

	if(bAdded)
		pParent->TotalCount++;
	return bAdded;
}

void CAccessTree::Clear()
{
	std::unique_lock lock(m_Mutex); 

	m_Root.Clear();
}

StVariant CAccessTree::StoreTree(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool) const
{
	if(!m_Root)
		return StVariant(pMemPool);

	return StoreTree(m_Root, pMemPool);
}

StVariant CAccessTree::StoreTree(const SPathNodePtr& pParent, FW::AbstractMemPool* pMemPool) const
{
	std::unique_lock lock(m_Mutex);
	auto Branches = pParent->Branches;
	lock.unlock();

	StVariantWriter Children(pMemPool);
	Children.BeginList();
	for (auto &Branch : Branches)
#ifdef DEF_USE_POOL
		Children.WriteVariant(StoreTree(Branch, pMemPool));
#else
		Children.WriteVariant(StoreTree(Branch.second, pMemPool));
#endif

	return StoreNode(pParent, Children.Finish());
}

void CAccessTree::LoadTree(const StVariant& Data)
{
	std::unique_lock lock(m_Mutex); 

	if(!Data.IsValid())
		return;

	if (!m_Root) {
#ifdef DEF_USE_POOL
		if(!m_pMem)
			return;
		m_Root = m_pMem->New<SPathNode>();
#else
		m_Root = std::make_shared<SPathNode>();
#endif
	}

	LoadTree(Data, m_Root);
}

uint32 CAccessTree::LoadTree(const StVariant& Data, SPathNodePtr& pParent)
{
	uint32 Count = 0;
	pParent->Stats = SAccessStats(Data[API_V_ACCESS_MASK], Data[API_V_LAST_ACTIVITY], Data[API_V_NT_STATUS], Data[API_V_IS_DIRECTORY], Data[API_V_WAS_BLOCKED], Data.Get(API_V_ACCESS_COUNT).To<uint64>(0));
	StVariant Nodes = Data[API_V_ACCESS_NODES];

	// Check if the number of nodes to load exceeds threshold - if so, create a collapsed catch-all
	if (Nodes.Count() > MAX_BRANCHES_BEFORE_COLLAPSE) {
		// Create a catch-all branch to represent all the collapsed data
#ifdef DEF_USE_POOL
		FW::StringW catchAllKey(m_pMem);
		catchAllKey.Assign(L"*", 1);
		auto pCatchAll = m_pMem->New<SPathNode>();
		(*&pCatchAll)->Name.Assign(L"*", 1);
#else
		auto pCatchAll = std::make_shared<SPathNode>();
		pCatchAll->Name = L"*";
#endif
		// Aggregate stats from all nodes being collapsed
		uint32 totalCollapsedCount = 0;
		for (uint32 i = 0; i < Nodes.Count(); i++) {
			StVariant Node = Nodes[i];
			SAccessStats nodeStats(Node[API_V_ACCESS_MASK], Node[API_V_LAST_ACTIVITY], Node[API_V_NT_STATUS], Node[API_V_IS_DIRECTORY], Node[API_V_WAS_BLOCKED], Data.Get(API_V_ACCESS_COUNT).To<uint64>(0));
			MergeStats(pCatchAll->Stats, nodeStats);
			// Count this node plus estimate children (we skip loading children to save memory)
			StVariant ChildNodes = Node[API_V_ACCESS_NODES];
			totalCollapsedCount += 1 + ChildNodes.Count(); // Approximate count
			// If this node has no children and no access count, increment the catch-all's access count (fallback for old entries)
			if (!ChildNodes.Count() && nodeStats.AccessCount == 0) pCatchAll->Stats.AccessCount++;
		}
		pCatchAll->TotalCount = totalCollapsedCount;
#ifdef DEF_USE_POOL
		pParent->Branches[catchAllKey] = pCatchAll;
#else
		pParent->Branches[L"*"] = pCatchAll;
#endif
		Count = 1 + totalCollapsedCount;
		pParent->TotalCount += Count;
		return Count;
	}

	for (uint32 i = 0; i < Nodes.Count(); i++)
	{
		StVariant Node = Nodes[i];
#ifdef DEF_USE_POOL
		FW::StringW Name(m_pMem);
		Name = Node[API_V_ACCESS_NAME].ToStringW();
		FW::StringW name = Name;
		name.MakeLower();
		auto pBranch = pParent->Branches[name];
#else
		std::wstring Name = Node[API_V_ACCESS_NAME].AsStr();
		auto& pBranch = pParent->Branches[MkLower(Name)];
#endif
		if (!pBranch) {
			Count++;
#ifdef DEF_USE_POOL
			pBranch = m_pMem->New<SPathNode>();
			(*&pBranch)->Name = Name;
#else
			pBranch = std::make_shared<SPathNode>();
			pBranch->Name = Name;
#endif
		}
#ifdef DEF_USE_POOL
		Count += LoadTree(Node, *&pBranch);
#else
		Count += LoadTree(Node, pBranch);
#endif
	}
	pParent->TotalCount += Count;
	return Count;
}

StVariant CAccessTree::DumpTree(uint64 LastActivity, FW::AbstractMemPool* pMemPool) const
{
	std::unique_lock lock(m_Mutex); 

	if (!m_Root)
		return StVariant(pMemPool);

	return DumpTree(m_Root, LastActivity, pMemPool);
}

StVariant CAccessTree::DumpTree(const SPathNodePtr& pParent, uint64 LastActivity, FW::AbstractMemPool* pMemPool) const
{
	int Count = 0;
	StVariantWriter Children(pMemPool);
	Children.BeginList();
	for (auto& Branch : pParent->Branches) {
#ifdef DEF_USE_POOL
		StVariant Child = DumpTree(Branch, LastActivity, pMemPool);
#else
		StVariant Child = DumpTree(Branch.second, LastActivity, pMemPool);
#endif
		if (Child.IsValid()) {
			Count++;
			Children.WriteVariant(Child);
		}
	}

	if (Count == 0) {
		if(pParent->Stats.LastAccessTime && pParent->Stats.LastAccessTime <= LastActivity)
			return StVariant();
		return StoreNode(pParent, StVariant(pMemPool), pMemPool);
	}
	return StoreNode(pParent, Children.Finish(), pMemPool);
}

StVariant CAccessTree::StoreNode(const SPathNodePtr& pParent, const StVariant& Children, FW::AbstractMemPool* pMemPool) const
{
	StVariantWriter Node(pMemPool);
	Node.BeginIndex();

#ifdef DEF_USE_POOL
	Node.Write(API_V_ACCESS_REF, (uint64)pParent.Get());
#else
	Node.Write(API_V_ACCESS_REF, (uint64)pParent.get());
#endif
	Node.WriteEx(API_V_ACCESS_NAME, pParent->Name);

	if (pParent->Stats.LastAccessTime)
	{
		Node.Write(API_V_LAST_ACTIVITY, pParent->Stats.LastAccessTime);
		Node.Write(API_V_WAS_BLOCKED, pParent->Stats.bBlocked);
		Node.Write(API_V_ACCESS_MASK, pParent->Stats.AccessMask);
		Node.Write(API_V_NT_STATUS, pParent->Stats.NtStatus);
		Node.Write(API_V_IS_DIRECTORY, pParent->Stats.IsDirectory);
		if (pParent->Stats.AccessCount)
			Node.Write(API_V_ACCESS_COUNT, pParent->Stats.AccessCount);
	}

	if(Children.IsValid())
		Node.WriteVariant(API_V_ACCESS_NODES, Children);

	return Node.Finish();
}

void CAccessTree::CleanUp(bool* pbCancel, uint32* puCounter)
{
	if (!m_Root)
		return;

	CleanUp(m_Root, L"", pbCancel, puCounter);
}

bool CAccessTree::CleanUp(SPathNodePtr& pParent, const std::wstring& Path, bool* pbCancel, uint32* puCounter)
{
	if (!Path.empty())
	{
		HANDLE handle = NULL;
		NTSTATUS status = STATUS_NOT_IMPLEMENTED;
		IO_STATUS_BLOCK Iosb;

		if (CNtPathMgr::IsDosPath(Path))
		{
			std::wstring NtPath = CNtPathMgr::Instance()->TranslateDosToNtPath(Path);
			if (NtPath.empty())
				status = STATUS_UNRECOGNIZED_VOLUME;
			else if (NtPath.find_first_of(L"<>:\"/|?*") != std::wstring::npos)
				status = STATUS_OBJECT_PATH_SYNTAX_BAD; // this can provoke a BSOD!!!
			else
				status = NtCreateFile(&handle, /*FILE_READ_ATTRIBUTES |*/ SYNCHRONIZE, SNtObject(NtPath).Get(), &Iosb, NULL, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN, 0, NULL, 0);
		}
		else if (MatchPathPrefix(Path, L"\\Device"))
		{
			if(MatchPathPrefix(Path, L"\\Device\\NamedPipe"))
				return false; // always clean up named pipes we dont show them in the gui anyways

			if (Path.find_first_of(L"<>:\"/|?*") != std::wstring::npos)
				status = STATUS_OBJECT_PATH_SYNTAX_BAD; // this can provoke a BSOD!!!
			else

			//if (MatchPathPrefix(Path, L"\\Device\\Harddisk", 16))
			//if (MatchPathPrefix(Path, L"\\Device\\HarddiskVolume", 22))
				status = NtCreateFile(&handle, /*FILE_READ_ATTRIBUTES |*/ SYNCHRONIZE, SNtObject(Path).Get(), &Iosb, NULL, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN, 0, NULL, 0);
		}
		else if (MatchPathPrefix(Path, L"\\REGISTRY"))
		{
			if(MatchPathPrefix(Path.c_str(), L"\\REGISTRY\\A"))
				return false; // always clean up app exclusive hives

			status = NtOpenKey(&handle, SYNCHRONIZE, SNtObject(Path).Get());
		}

		if (NT_SUCCESS(status))
			NtClose(handle);
		else if (status == STATUS_UNRECOGNIZED_VOLUME || status == STATUS_OBJECT_TYPE_MISMATCH
			|| status == STATUS_OBJECT_NAME_NOT_FOUND || status == STATUS_OBJECT_PATH_NOT_FOUND
			|| status == STATUS_OBJECT_NAME_INVALID || status == STATUS_OBJECT_PATH_INVALID
			|| status == STATUS_BAD_NETWORK_PATH || status == STATUS_BAD_NETWORK_NAME || status == STATUS_OBJECT_PATH_SYNTAX_BAD
			|| status == STATUS_INVALID_PARAMETER
			) {
			//if (status == STATUS_OBJECT_PATH_SYNTAX_BAD)
			//	DbgPrint("BadSyntax: %S\r\n", Path.c_str());
			// else
			//	DbgPrint("NotFound: %S\r\n", Path.c_str());
			return false;
		}
		else if(status != STATUS_NOT_IMPLEMENTED && status != STATUS_ACCESS_DENIED 
			&& status != STATUS_DELETE_PENDING && status != STATUS_IO_REPARSE_TAG_NOT_HANDLED) {
			DbgPrint("Unexpected Status 0x%08X: %S\r\n", status, Path.c_str());
		}
	}

	std::unique_lock lock(m_Mutex);
	auto Branches = pParent->Branches;
	lock.unlock();

	uint32 Count = 0;

#ifdef DEF_USE_POOL
	for (auto I = Branches.begin(); I != Branches.end();++I)
#else
	for (auto& Branch : Branches)
#endif
	{
		if (pbCancel && *pbCancel)
			break;

		std::wstring ChildPath;
		if (!Path.empty())
#ifdef DEF_USE_POOL
			ChildPath = Path + L"\\" + I.Key().ConstData();
#else
			ChildPath = Path + L"\\" + Branch.first;
#endif
#ifdef DEF_USE_POOL
		else if (I.Key().Length() == 2 && I.Key().At(1) == L':')
#else
		else if (Branch.first.size() == 2 && Branch.first[1] == L':')
#endif
#ifdef DEF_USE_POOL
			ChildPath = I.Key().ConstData();
#else
			ChildPath = Branch.first;
#endif
		else
#ifdef DEF_USE_POOL
			ChildPath = L"\\" + std::wstring(I.Key().ConstData());
#else
			ChildPath = L"\\" + Branch.first;
#endif
#ifdef DEF_USE_POOL
		bool bOk = CleanUp(I.Value(), ChildPath, pbCancel, puCounter);
#else
		bool bOk = CleanUp(Branch.second, ChildPath, pbCancel, puCounter);
#endif

		if (puCounter) {
			*puCounter += 1;
			if (!bOk)
#ifdef DEF_USE_POOL
				*puCounter += I.Value()->TotalCount;
#else
				*puCounter += Branch.second->TotalCount;
#endif
		}

		lock.lock();
		if (!bOk)
#ifdef DEF_USE_POOL
			pParent->Branches.Remove(I.Key());
#else
			pParent->Branches.erase(Branch.first);
#endif
		else
#ifdef DEF_USE_POOL
			Count += 1 + I.Value()->TotalCount;
#else
			Count += 1 + Branch.second->TotalCount;
#endif
		lock.unlock();
	}

	lock.lock();
	pParent->TotalCount = Count;
	lock.unlock();

	return true;
}

void CAccessTree::Truncate()
{
	std::unique_lock lock(m_Mutex);

	if (!m_Root)
		return;

	uint64 CleanupDateMinutes = theCore->Config()->GetUInt64("Service", "TraceLogRetentionMinutes", 60 * 24 * 14); // default 14 days
	uint64 CleanupDate = GetCurrentTimeAsFileTime() - (CleanupDateMinutes * 60 * 10000000ULL);

	Truncate(m_Root, CleanupDate);
}

uint64 CAccessTree::Truncate(SPathNodePtr& pParent, uint64 CleanupDate)
{
	uint64 LatestAccess = pParent->Stats.LastAccessTime;

	uint32 Count = 0;

	for (auto I = pParent->Branches.begin(); I != pParent->Branches.end();)
	{
#ifdef DEF_USE_POOL
		uint64 LastAccess = Truncate(I.Value(), CleanupDate);
#else
		uint64 LastAccess = Truncate(I->second, CleanupDate);
#endif
		if (LastAccess > LatestAccess)
			LatestAccess = LastAccess;

		if (LastAccess < CleanupDate)
		{
			I = pParent->Branches.erase(I);
		}
		else
		{
#ifdef DEF_USE_POOL
			Count += 1 + I.Value()->TotalCount;
#else
			Count += 1 + I->second->TotalCount;
#endif
			++I;
		}
	}

	pParent->TotalCount = Count;

	return  LatestAccess;
}