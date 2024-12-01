#include "pch.h"
#include "AccessTree.h"
#include "../Library/Common/Strings.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtObj.h"
#include "../Library/Helpers/NtUtil.h"
#include "../ServiceCore.h"

CAccessTree::CAccessTree()
{
	m_Root = std::make_shared<SPathNode>();
}

CAccessTree::~CAccessTree()
{
}

void CAccessTree::Add(const std::wstring& Path, uint32 AccessMask, uint64 AccessTime, NTSTATUS NtStatus, bool IsDirectory, bool bBlocked)
{ 
	std::unique_lock lock(m_Mutex); 

	Add(std::make_shared<SAccessStats>(AccessMask, AccessTime, NtStatus, IsDirectory, bBlocked), m_Root, Path);
}

bool CAccessTree::Add(const SAccessStatsPtr& pStat, SPathNodePtr& pParent, const std::wstring& Path, size_t uOffset)
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
		return false;
	}

	bool bAdded = false;
	auto &pBranch = pParent->Branches[MkLower(Name)];
	if (!pBranch) {
		bAdded = true;
		pBranch = std::make_shared<SPathNode>();
		pBranch->Name = Name;
	}

	if(Add(pStat, pBranch, Path, uPos + 1))
		bAdded = true;

	if(bAdded)
		pParent->TotalCount++;
	return bAdded;
}

void CAccessTree::Clear()
{
	std::unique_lock lock(m_Mutex); 

	m_Root->Branches.clear();
}

CVariant CAccessTree::StoreTree(const SVarWriteOpt& Opts) const
{
	return StoreTree(m_Root);
}

CVariant CAccessTree::StoreTree(const SPathNodePtr& pParent) const
{
	std::unique_lock lock(m_Mutex);
	auto Branches = pParent->Branches;
	lock.unlock();

	CVariant Children;
	Children.BeginList();
	for (auto &Branch : Branches)
		Children.WriteVariant(StoreTree(Branch.second));
	Children.Finish();

	return StoreNode(pParent, Children);
}

void CAccessTree::LoadTree(const CVariant& Data)
{
	std::unique_lock lock(m_Mutex); 

	LoadTree(Data, m_Root);
}

uint32 CAccessTree::LoadTree(const CVariant& Data, SPathNodePtr& pParent)
{
	uint32 Count = 0;
	pParent->pStats = std::make_shared<SAccessStats>(Data[API_V_ACCESS_MASK], Data[API_V_ACCESS_TIME], Data[API_V_ACCESS_STATUS], Data[API_V_ACCESS_IS_DIR], Data[API_V_ACCESS_BLOCKED]);
	CVariant Nodes = Data[API_V_ACCESS_NODES];
	for (uint32 i = 0; i < Nodes.Count(); i++)
	{
		CVariant Node = Nodes[i];
		std::wstring Name = Node[API_V_ACCESS_NAME].AsStr();
		auto& pBranch = pParent->Branches[MkLower(Name)];
		if (!pBranch) {
			Count++;
			pBranch = std::make_shared<SPathNode>();
			pBranch->Name = Name;
		}
		Count += LoadTree(Node, pBranch);
	}
	pParent->TotalCount += Count;
	return Count;
}

CVariant CAccessTree::DumpTree(uint64 LastActivity) const
{
	std::unique_lock lock(m_Mutex); 

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

void CAccessTree::CleanUp(bool* pbCancel, uint32* puCounter)
{
	CleanUp(m_Root, L"", pbCancel, puCounter);
}

bool CAccessTree::CleanUp(SPathNodePtr& pParent, const std::wstring& Path, bool* pbCancel, uint32* puCounter)
{
	if (!Path.empty())
	{
		HANDLE handle = NULL;
		NTSTATUS status = STATUS_NOT_IMPLEMENTED;

		if (Path.length() > 8 && _wcsnicmp(Path.c_str(), L"\\Device\\", 8) == 0)
		{
			if(Path.length() == 17 && _wcsicmp(Path.c_str(), L"\\Device\\NamedPipe") == 0)
				return false; // always clean up named pipes we dont show them in the gui anyways

			if (Path.find_first_of(L"<>:\"/|?*") != std::wstring::npos)
				status = STATUS_OBJECT_PATH_SYNTAX_BAD; // this can provoke a BSOD!!!
			else

			//if (Path.length() > 16 && _wcsnicmp(Path.c_str(), L"\\Device\\Harddisk", 16) == 0)
			//if (Path.length() > 16 && _wcsnicmp(Path.c_str(), L"\\Device\\HarddiskVolume", 22) == 0)
			{
				IO_STATUS_BLOCK Iosb;
				status = NtCreateFile(&handle, /*FILE_READ_ATTRIBUTES |*/ SYNCHRONIZE, SNtObject(Path).Get(), &Iosb, NULL, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN, 0, NULL, 0);
			}
		}
		else if (Path.length() > 10 && _wcsnicmp(Path.c_str(), L"\\REGISTRY\\", 10) == 0)
		{
			if(Path.length() == 11 && _wcsicmp(Path.c_str(), L"\\REGISTRY\\A") == 0)
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

	for (auto& Branch : Branches)
	{
		if (pbCancel && *pbCancel)
			break;

		bool bOk = CleanUp(Branch.second, Path + L"\\" + Branch.first, pbCancel, puCounter);

		if (puCounter) {
			*puCounter += 1;
			if (!bOk)
				*puCounter += Branch.second->TotalCount;
		}

		lock.lock();
		if (!bOk)
			pParent->Branches.erase(Branch.first);
		else
			Count += 1 + Branch.second->TotalCount;
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

	uint64 CleanupDateMinutes = theCore->Config()->GetUInt64("Service", "TraceLogRetentionMinutes", 60 * 24 * 14); // default 14 days
	uint64 CleanupDate = GetCurrentTimeAsFileTime() - (CleanupDateMinutes * 60 * 10000000ULL);

	Truncate(m_Root, CleanupDate);
}

uint64 CAccessTree::Truncate(SPathNodePtr& pParent, uint64 CleanupDate)
{
	uint64 LatestAccess = pParent->pStats ? pParent->pStats->LastAccessTime : 0;

	uint32 Count = 0;

	for (auto I = pParent->Branches.begin(); I != pParent->Branches.end();)
	{
		uint64 LastAccess = Truncate(I->second, CleanupDate);
		if (LastAccess > LatestAccess)
			LatestAccess = LastAccess;

		if (LastAccess < CleanupDate)
		{
			I = pParent->Branches.erase(I);
		}
		else
		{
			Count += 1 + I->second->TotalCount;
			++I;
		}
	}

	pParent->TotalCount = Count;

	return  LatestAccess;
}