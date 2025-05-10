#include "pch.h"
#include "AccessTree.h"
#include "../Library/Common/Strings.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtObj.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/NtPathMgr.h"
#include "../Library/Helpers/MiscHelpers.h"
#include "../ServiceCore.h"

CAccessTree::CAccessTree()
{
}

CAccessTree::~CAccessTree()
{
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
		pParent->Stats = Stat;
		return false;
	}

	bool bAdded = false;
#ifdef DEF_USE_POOL
	FW::StringW name = Name;
	name.MakeLower();
	auto pBranch = pParent->Branches[name];
#else
	auto &pBranch = pParent->Branches[MkLower(Name)];
#endif
	if (!pBranch) {
		bAdded = true;
#ifdef DEF_USE_POOL
		pBranch = m_pMem->New<SPathNode>();
		(*&pBranch)->Name = Name;
#else
		pBranch = std::make_shared<SPathNode>();
		pBranch->Name = Name;
#endif
	}

#ifdef DEF_USE_POOL
	if(Add(Stat, *&pBranch, Path, uPos + 1))
#else
	if(Add(Stat, pBranch, Path, uPos + 1))
#endif
		bAdded = true;

	if(bAdded)
		pParent->TotalCount++;
	return bAdded;
}

void CAccessTree::Clear()
{
	std::unique_lock lock(m_Mutex); 

	m_Root.reset();
}

StVariant CAccessTree::StoreTree(const SVarWriteOpt& Opts) const
{
	if(!m_Root)
		return StVariant();

	return StoreTree(m_Root);
}

StVariant CAccessTree::StoreTree(const SPathNodePtr& pParent) const
{
	std::unique_lock lock(m_Mutex);
	auto Branches = pParent->Branches;
	lock.unlock();

	StVariantWriter Children;
	Children.BeginList();
	for (auto &Branch : Branches)
#ifdef DEF_USE_POOL
		Children.WriteVariant(StoreTree(Branch));
#else
		Children.WriteVariant(StoreTree(Branch.second));
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
	pParent->Stats = SAccessStats(Data[API_V_ACCESS_MASK], Data[API_V_LAST_ACTIVITY], Data[API_V_NT_STATUS], Data[API_V_IS_DIRECTORY], Data[API_V_WAS_BLOCKED]);
	StVariant Nodes = Data[API_V_ACCESS_NODES];
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

StVariant CAccessTree::DumpTree(uint64 LastActivity) const
{
	std::unique_lock lock(m_Mutex); 

	if (!m_Root)
		return StVariant();

	return DumpTree(m_Root, LastActivity);
}

StVariant CAccessTree::DumpTree(const SPathNodePtr& pParent, uint64 LastActivity) const
{
	int Count = 0;
	StVariantWriter Children;
	Children.BeginList();
	for (auto& Branch : pParent->Branches) {
#ifdef DEF_USE_POOL
		StVariant Child = DumpTree(Branch, LastActivity);
#else
		StVariant Child = DumpTree(Branch.second, LastActivity);
#endif
		if (Child.IsValid()) {
			Count++;
			Children.WriteVariant(Child);
		}
	}

	if (Count == 0) {
		if(pParent->Stats.LastAccessTime && pParent->Stats.LastAccessTime <= LastActivity)
			return StVariant();
		return StoreNode(pParent, StVariant());
	}
	return StoreNode(pParent, Children.Finish());
}

StVariant CAccessTree::StoreNode(const SPathNodePtr& pParent, const StVariant& Children) const
{
	StVariantWriter Node;
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