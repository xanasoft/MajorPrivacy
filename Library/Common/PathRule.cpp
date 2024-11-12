//#include "pch.h"
#include "PathRule.h"
#ifndef KERNEL_MODE
#include <Rpc.h>
#endif

/////////////////////////////////////////////////////////////////////////////
// CPathRule
//

CPathRule::CPathRule(FW::AbstractMemPool* pMem)
	: FW::Object(pMem), m_PathPattern(pMem), m_Guid(pMem)
{
	SetNewGuid();
}

//CPathRule::CPathRule(FW::AbstractMemPool* pMem, const FW::StringW & Path)
//	: FW::Object(pMem), m_PathPattern(pMem)
//{
//	m_PathPattern.Set(Path);
//}

CPathRule::~CPathRule()
{
}

void CPathRule::SetNewGuid()
{
#ifdef KERNEL_MODE
	NTSTATUS status;
	GUID myGuid;
	status = ExUuidCreate(&myGuid);
	if (NT_SUCCESS(status)) {
		UNICODE_STRING guidString;
		status = RtlStringFromGUID(myGuid, &guidString);
		if (NT_SUCCESS(status)) {
			m_Guid.Assign(guidString.Buffer, guidString.Length / sizeof(wchar_t));
			RtlFreeUnicodeString(&guidString);
		}
	}
#else
	GUID myGuid;
	RPC_STATUS status = UuidCreate(&myGuid);
	if (status == RPC_S_OK || status == RPC_S_UUID_LOCAL_ONLY) {
		RPC_WSTR guidString = NULL;
		status = UuidToStringW(&myGuid, &guidString);
		if (status == RPC_S_OK) {
			m_Guid.Assign((wchar_t*)guidString, wcslen((wchar_t*)guidString));
			RpcStringFreeW(&guidString);
		}
	}
#endif
}

ULONG CPathRule::GetMatch(const FW::StringW& Path, size_t uOffset) const
{
	//const FW::StringW& SubPath = Path.SubStr(uOffset);

	return m_PathPattern.Match(Path);
}

/////////////////////////////////////////////////////////////////////////////
// CPathRuleList
//

CPathRuleList::CPathRuleList(FW::AbstractMemPool* pMem)
	: FW::AbstractContainer(pMem), m_Map(pMem)
{
	m_Root = m_pMem->New<SPathNode>();
}

CPathRuleList::~CPathRuleList()
{
}

bool CPathRuleList::Add(const CPathRulePtr& pRule)
{ 
	if(m_Map.Insert(pRule->GetGuid(), pRule, FW::EMapInsertMode::eNoReplace) != FW::EMapResult::eOK)
		return false;
	FW::StringW Path = pRule->GetPath();
	if(wchar_t* pstr = Path.Data())
		_wcslwr(pstr); // make string lower // todo
	return Add(pRule, m_Root, Path);
}

bool CPathRuleList::Add(const CPathRulePtr& pRule, FW::SharedPtr<SPathNode>& pParent, const FW::StringW& Path, size_t uOffset)
{
	while (Path.At(uOffset) == L'\\')
		uOffset++;

	size_t uPos = Path.Find(L'\\', uOffset);
	if (uPos == -1 && uOffset < Path.Length())
		uPos = Path.Length();
	
	FW::StringW Name;
	if (uPos != -1)
		Name = Path.SubStr(uOffset, uPos - uOffset);

	if (uPos == -1 || Name.Find(L'*') != -1)
	{
		pParent->Rules.Append(pRule);
		return true;
	}

	FW::SharedPtr<SPathNode>* ppBranch = pParent->Branches.GetValuePtr(Name, true);
	if (!ppBranch)
		return false; // alloc failure
	if (!*ppBranch) {
		*ppBranch = m_pMem->New<SPathNode>();
		if(!*ppBranch)
			return false; // alloc failure
		(*ppBranch)->Name = Name;

		//if (Name.Find(L'*') != -1)
		//{
		//	if (!pParent->Patterns.Append(FW::MakePair(m_pMem->New<CDosPattern>(Name), *ppBranch)))
		//		return false;// alloc failure
		//}
	}

	return Add(pRule, *ppBranch, Path, uPos + 1);
}

bool CPathRuleList::Remove(const FW::StringW& Guid)
{
	CPathRulePtr pRule = m_Map.Take(Guid);
	if (!pRule)
		return false;
	FW::StringW Path = pRule->GetPath();
	if(wchar_t* pstr = Path.Data())
		_wcslwr(pstr); // make string lower // todo
	Remove(pRule, m_Root, Path);
	return true;
}

void CPathRuleList::Remove(const CPathRulePtr& pRule, FW::SharedPtr<SPathNode>& pParent, const FW::StringW& Path, size_t uOffset)
{
	while (Path.At(uOffset) == L'\\')
		uOffset++;

	size_t uPos = Path.Find(L'\\', uOffset);
	if (uPos == -1 && uOffset < Path.Length())
		uPos = Path.Length();

	FW::StringW Name;
	if (uPos != -1)
		Name = Path.SubStr(uOffset, uPos - uOffset);

	if (uPos == -1 || Name.Find(L'*') != -1)
	{
		for(auto I = pParent->Rules.begin(); I != pParent->Rules.end(); ++I) {
			if (*I == pRule) {
				pParent->Rules.erase(I);
				break;
			}
		}
		return;
	}

	FW::SharedPtr<SPathNode>* ppBranch = pParent->Branches.GetValuePtr(Name);
	if (!ppBranch || !*ppBranch)
		return;

	Remove(pRule, *ppBranch, Path, uPos + 1);

	if ((*ppBranch)->Branches.IsEmpty() && (*ppBranch)->Rules.IsEmpty())
		pParent->Branches.Remove(Name);
}

void CPathRuleList::Clear()
{
	m_Root->Branches.Clear();
	//m_Root->Patterns.Clear();
	m_Root->Rules.Clear();

	m_Map.Clear();
}

FW::List<CPathRuleList::SFoundRule> CPathRuleList::GetRules(FW::StringW Path) const
{ 
	if(wchar_t* pstr = Path.Data())
		_wcslwr(pstr); // make string lower // todo
	return GetRules(m_Root, Path); 
}

FW::List<CPathRuleList::SFoundRule> CPathRuleList::GetRules(const FW::SharedPtr<SPathNode>& pParent, const FW::StringW& Path, size_t uOffset) const
{
	while (Path.At(uOffset) == L'\\')
		uOffset++;

	size_t uPos = Path.Find(L'\\', uOffset);
	if (uPos == -1 && uOffset < Path.Length())
		uPos = Path.Length();

	//if(uPos == -1)
	//	return pParent->Rules;

	FW::StringW Name = Path.SubStr(uOffset, uPos - uOffset);

	//
	// Note: we use the allocator we got with the path
	//

	FW::List<SFoundRule> Rules(Path.Allocator()); 

	for (auto pRule : pParent->Rules) 
	{
		if (!pRule->IsEnabled())
			continue;
		ULONG uMatch = pRule->GetMatch(Path, uOffset);
		if(uMatch)
			Rules.Append(SFoundRule { pRule, uMatch, pRule->IsExact(), pRule->GetWildcardCount() });
	}

	FW::SharedPtr<SPathNode> pNode = pParent->Branches.GetValue(Name);
	if (pNode)
		Rules.Append(GetRules(pNode, Path, uPos + 1));
	
	//for(auto Pattern: pParent->Patterns)
	//{
	//	ULONG uMatch = Pattern.first->Match(Name);
	//	if (uMatch)
	//		Rules.Append(GetRules(Path, Pattern.second, uOffset + uMatch + 1));
	//}

	return Rules;
}

void CPathRuleList::Enum(const FW::StringW& Path, const FW::SharedPtr<SPathNode>& pParent, void(*pCallback)(const FW::StringW& Path, const CPathRulePtr& pRule, void* pContext), void* pContext) const
{
	for (auto pRule : pParent->Rules)
		pCallback(Path, pRule, pContext);
	//for (auto Patterns:	pParent->Patterns)
	//	Enum(Path + L"\\~ " + Patterns.first->Get(), Patterns.second, pCallback, pContext);

	for (auto& pBranch : pParent->Branches)
		Enum(Path + L"\\" + pBranch->Name, pBranch, pCallback, pContext);
}

