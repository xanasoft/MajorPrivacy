//#include "pch.h"
#include "PathTree.h"
#ifndef KERNEL_MODE
#include <xmemory>
#endif

#define INVALID_SMARTPATTERN  ((CPathTree::SPathEntry*)-1)

/////////////////////////////////////////////////////////////////////////////
// CPathTree
//

CPathTree::CPathTree(FW::AbstractMemPool* pMem) 
	: FW::Object(pMem) 
{
	m_Root.Init(pMem);
}

int CPathTree::AddEntry(const CPathEntryPtr& pEntry)
{
	FW::StringW Path = pEntry->GetPath();

	if (Path.IsEmpty())
		return -3; // empty path

	//
	// VALIDATE PATTERN:
	// 
	// The pattern is to be separeded by backsplash (by default) and it may contain wildcards 
	// 
	//		'?'  - any single charakter
	//		"*"	 - any number of charakters withing one path segment
	//		"**" - any number of charakters including path separators
	// 
	// Note: even when PATH_TREE_USE_SMARTPATTERN is defined, we do some of our own validation
	//

	SPatternInfo Info;

	size_t uLen = Path.Length();
	wchar_t* pPath = Path.Data(); // modify raw data in string object!
	for (size_t i = 0; i < uLen; i++)
	{
		wchar_t cur = pPath[i];
		// Check for ASCII control characters (1–31 and 127 (delete))
		if (cur < 32 || cur == 127)
			return -4; // Reject invalid control characters

		if ((cur >= L'A') && (cur <= L'Z'))
			pPath[i] += 32; // to lower case

		if (cur == L'?' && Info.FirstQuestionMark == (ULONG)-1)
			Info.FirstQuestionMark = (ULONG)i;

		if (cur == L'*')
		{
			if (pPath[i+1] == L'*') // double asterisk
			{
				// todo: two double asterisks within the same segment are not supported as well
				if (pPath[i+2] == L'*')
					return -2; // tripple (or more) asterisks are not supported

				if (Info.FirstDblAsterisk == (ULONG)-1)
					Info.FirstDblAsterisk = (ULONG)i;
				Info.LastDblAsterisk = (ULONG)i;

				i++; // skip second asterisk
			}
			else
			{
				if (Info.FirstAsterisk == (ULONG)-1)
					Info.FirstAsterisk = (ULONG)i;
				Info.LastAsterisk = (ULONG)i;
			}
		}

		else if (cur == L'\"' || cur == L'<' || cur == L'>' || cur == L'|')
			return -4; // Reject not allowed characters
	}

	Info.PatternLength = (ULONG)uLen;

	SPathEntry* pPathEntry = Add(pEntry, &m_Root, Path);
#ifdef PATH_TREE_USE_SMARTPATTERN
	if (pPathEntry == INVALID_SMARTPATTERN)
		return -2; // invalid pattern
#endif
	if (!pPathEntry)
		return -1; // alloc failure

	pPathEntry->Info = Info;
	pPathEntry->EndsWithSeparator = Path.At(uLen - 1) == m_Seperator;

	return 0; // no error
}

bool CPathTree::RemoveEntry(const CPathEntryPtr& pEntry)
{
	FW::StringW Path = pEntry->GetPath();
	Path.MakeLower();
	return Remove(pEntry, &m_Root, Path);
}

CPathTree::SPathEntry* CPathTree::Add(const CPathEntryPtr& pEntry, SPathNode* pParent, const FW::StringW& Path, size_t uOffset)
{
	while (Path.At(uOffset) == m_Seperator) { uOffset++; }
	bool bAtEnd = uOffset == Path.Length();

	if (bAtEnd)
	{
		m_uEntryCount++;
		return pParent->Entries.Append(SPathEntry{ pEntry });
	}

	size_t uPos = Path.Find(m_Seperator, uOffset);
	if (uPos == -1 && uOffset < Path.Length())
		uPos = Path.Length();

	FW::StringW Name = Path.SubStr(uOffset, uPos - uOffset);

	SPathNode* pBranch = nullptr;
	if (Name.Find(L'*') != -1 || Name.Find(L'?') != -1) // Wildcard
	{
#ifdef PATH_TREE_USE_SMARTPATTERN
		auto* pWildBranch = pParent->WildBranches.GetValuePtr(Name, true);
		if (pWildBranch)
		{
			pBranch = &pWildBranch->first;
			pWildBranch->second = New<CSmartPattern>();
			if (!m_bSimplePattern)
				pWildBranch->second->SetSeparator(m_Seperator);
			if (pWildBranch->second->Set(Name) != 0) {
#ifdef _DEBUG
				DebugBreak();
#endif
				return INVALID_SMARTPATTERN; // invalid pattern
			}
		}
#else
		pBranch = pParent->WildBranches.GetValuePtr(Name, true);
#endif
	}
	else // Rregular
		pBranch = pParent->Branches.GetValuePtr(Name, true);
	if (!pBranch)
		return nullptr; // alloc failure
	if (pBranch->Branches.Allocator() == nullptr) {
		pBranch->Init(m_pMem);
		pBranch->Name = Name;
	}

	return Add(pEntry, pBranch, Path, uPos);
}


FW::List<CPathTree::SFoundEntry> CPathTree::GetEntries(FW::StringW Path, EMatchFlags Flags) const
{ 
	Path.MakeLower();

	//
	// Note: we use the allocator we got with the path
	//

	FW::List<SFoundEntry> Entries(Path.Allocator()); 

	GetEntries(Entries, &m_Root, Flags, Path); 

	return Entries;
}

#ifndef PATH_TREE_USE_SMARTPATTERN
static size_t CPathTree__MatchSuffix(const wchar_t* pSuffix, const wchar_t* pName, size_t uNameLen, size_t* pNamePos)
{
	size_t uSuffixLen = 0;
	for (; pSuffix[uSuffixLen] && pSuffix[uSuffixLen] != L'*'; uSuffixLen++);

	if(uNameLen < uSuffixLen) // when remaining name is shorter then the suffix it can't match
		return FW::StringW::NPos;

	for (; (*pNamePos) <= uNameLen - uSuffixLen; (*pNamePos)++)
	{
		for (size_t uCur = 0; ; uCur++)
		{
			if(uCur >= uSuffixLen)
				return uSuffixLen; // Suffix matched
			if (pSuffix[uCur] != pName[(*pNamePos) + uCur] && pSuffix[uCur] != L'?')
				break; // missmatch
		}
	}

	return FW::StringW::NPos; // no match
}
#endif

void CPathTree::GetEntries(FW::List<SFoundEntry>& Entries, const SPathNode* pParent, EMatchFlags Flags, const FW::StringW& Path, size_t uOffset) const
{
	bool bHasSeparator = Path.At(uOffset) == m_Seperator;
	if (bHasSeparator) do { uOffset++; } while(Path.At(uOffset) == m_Seperator);		
	bool bAtEnd = uOffset == Path.Length();

	if (bAtEnd)
	{
		for (auto Entry : pParent->Entries)
		{
			if (Entry.EndsWithSeparator != bHasSeparator && (Flags & eMayHaveSeparator) == 0)
				continue;

			//
			// When matching a wildcard-terminated path like L"C:\\Program (x86)\\**",
			// we scan the remaining path in descending segments. For example, given:
			// 
			//     L"C:\\Program Files (x86)\\Common Files\\Application\\Database\\Table1"
			//
			// We check the following segments:
			//  1: L"\\Common Files\\Application\\Database\\Table1"
			//  2: L"\\Application\\Database\\Table1"
			//  3: L"\\Database\\Table1"
			//  4: L"\\Table1"
			//
			// This is necessary because the node may contain other matching entries.
			// For instance:
			// 
			//     L"C:\\Program (x86)\\**\\Application\\**"			matches 2
			//     L"C:\\Program (x86)\\**\\Application\\Database\\**"	matches 2
			//     L"C:\\Program (x86)\\**\\Database\\**"				matches 3
			//
			// So we must filter out redundant matches.
			// Since we dont expect many matching entries, we use a simple linear search.
			//

			for (auto& OldEntry : Entries) {
				if (OldEntry.pEntry == Entry.pEntry)
					goto skip;
			}

			Entries.Append(Entry);

		skip:
			;
		}
	}

	size_t uPos = Path.Find(m_Seperator, uOffset);
	if (uPos == -1 && uOffset < Path.Length())
		uPos = Path.Length();

	FW::StringW Name = Path.SubStr(uOffset, uPos - uOffset);

	if (!bAtEnd)
	{
		SPathNode* pBranch = pParent->Branches.GetContrPtr(Name);
		if (pBranch)
			GetEntries(Entries, pBranch, Flags, Path, uPos);
	}

	for (auto I = pParent->WildBranches.begin(); I != pParent->WildBranches.end(); ++I)
	{
#ifdef PATH_TREE_USE_SMARTPATTERN

		SPathNode* pBranch = &I.Value().first;

		if (bAtEnd) // special case when the pattern ends with a double asterisk
		{
			if(pBranch->Name == L"**" || (m_bSimplePattern && pBranch->Name == L"*"))
				GetEntries(Entries, pBranch, Flags, Path, Path.Length());
			continue;
		}

		auto& pSmartPattern = I.Value().second;

		size_t uLenLeft = Path.Length() - uOffset;
		const wchar_t* pPathLeft = Path.ConstData() + uOffset;

		CSmartPattern::SFragment::EType LastFragmentType = CSmartPattern::SFragment::eText;
		size_t uMatch = pSmartPattern->RawMatch(pPathLeft, uLenLeft, LastFragmentType);
		if (uPos == FW::StringW::NPos)
			continue;
		size_t uPathPos = uOffset + uMatch;

		if (LastFragmentType == CSmartPattern::SFragment::eDblAsterisk || (m_bSimplePattern && LastFragmentType == CSmartPattern::SFragment::eAsterisk))
		{
			GetEntries(Entries, pBranch, Flags, Path, Path.Length());

			for (; ; )
			{
				uPathPos = Path.Find(m_Seperator, uPathPos + 1);
				if (uPathPos == FW::StringW::NPos)
					break;
				GetEntries(Entries, pBranch, Flags, Path, uPathPos);
			}
		}
		else if (pPathLeft[uMatch] == 0 || pPathLeft[uMatch] == m_Seperator)
		{
			GetEntries(Entries, pBranch, Flags, Path, uPathPos);
		}
#else
		SPathNode* pBranch = &I.Value();

		if (bAtEnd) // special case when the pattern ends with a double asterisk
		{
			if(pBranch->Name == L"**" || (m_bSimplePattern && pBranch->Name == L"*"))
				GetEntries(Entries, pBranch, Flags, Path, Path.Length());
			continue;
		}

		auto& Pattern = I.Key();
		size_t uPatternLen = Pattern.Length();
		const wchar_t* pPattern = Pattern.ConstData();
		size_t uPatternPos = 0;

		size_t uNameLen = Name.Length();
		const wchar_t* pName = Name.ConstData();
		size_t uNamePos = 0;

		size_t uDblAsteriskPos = FW::StringW::NPos;

		//const wchar_t* pTest1 = pName + uNamePos;
		//const wchar_t* pTest2 = pPattern + uPatternPos;

		for (; uPatternPos < uPatternLen; uPatternPos++, uNamePos++)
		{
			if (pPattern[uPatternPos] == L'*')
				break;
			if (pPattern[uPatternPos] != pName[uNamePos] && pPattern[uPatternPos] != L'?') {
				uPatternPos = FW::StringW::NPos; // no match
				break;
			}
		}
		if (uPatternPos == FW::StringW::NPos)
			goto done; // prefix does not match

		if (uPatternPos == uPatternLen) // almost exact match just L'?' used
		{
			GetEntries(Entries, pBranch, Flags, Path, uPos);
			goto done;
		}

		// past this point the next char in the pattern is always L'*'

		if (!m_bSimplePattern && uPatternPos + 1 == uPatternLen) // single asterisk at end
		{
			GetEntries(Entries, pBranch, Flags, Path, uOffset + uNameLen);
			goto done;
		}

	next:
		if (m_bSimplePattern || pPattern[uPatternPos + 1] == L'*') // double asterisk
		{
			if(pPattern[uPatternPos + 1] == L'*')
				uPatternPos ++;
			uDblAsteriskPos = uPatternPos + 1;
			if (pPattern[uDblAsteriskPos] == 0) // double asterisk at end
			{
				GetEntries(Entries, pBranch, Flags, Path, Path.Length());
				goto done;
			}
			//else // **suffix
			//	fallthrough to single asterisk case to text the current node
		}

		// single asterisk with suffix
		{
			const wchar_t* pSuffix = pPattern + uPatternPos + 1;
			size_t uSuffixLen = CPathTree__MatchSuffix(pSuffix, pName, uNameLen, &uNamePos);
			if (uSuffixLen != FW::StringW::NPos) // found Suffix
			{
				if (pSuffix[uSuffixLen] == L'*')
				{
					uNamePos += uSuffixLen;
					uPatternPos += 1 + uSuffixLen;
					goto next;
				}
				else
				{
					GetEntries(Entries, pBranch, Flags, Path, uOffset + uNameLen);
				}
			}
		}

	done:
		if (uDblAsteriskPos != FW::StringW::NPos)
		{
			size_t uPathLen = Path.Length();
			//const wchar_t* pPath = Path.ConstData();
			size_t uPathPos = uOffset + uNameLen;

			if (pPattern[uDblAsteriskPos] != 0) // **suffix
			{
				size_t uPatternPos2 = uPatternPos;

				for (; uPathPos <= uPathLen; uPathPos = Path.Find(m_Seperator, uPathPos + 1))
				{
					size_t uOffset2 = uPathPos + 1;

					size_t uPos2 = Path.Find(m_Seperator, uOffset2);
					if (uPos2 == -1 && uOffset < Path.Length())
						uPos2 = Path.Length();

					FW::StringW Name2 = Path.SubStr(uOffset2, uPos2 - uOffset2);

					size_t uNameLen2 = Name2.Length();
					const wchar_t* pName2 = Name2.ConstData();
					size_t uNamePos2 = 0;

				next2:
					const wchar_t* pSuffix = pPattern + uPatternPos2 + 1;

					size_t uSuffixLen = CPathTree__MatchSuffix(pSuffix, pName2, uNameLen2, &uNamePos2);
					if (uSuffixLen != FW::StringW::NPos) // found Suffix
					{
						if (pSuffix[uSuffixLen] == L'*')
						{
							uNamePos2 += uSuffixLen;
							uPatternPos2 += 1 + uSuffixLen;
							goto next2;
						}
						else
						{
							GetEntries(Entries, pBranch, Flags, Path, uOffset + uNameLen);
						}
					}
				}
			}
			else // ** at end
			{
				for (; uPathPos <= uPathLen; uPathPos = Path.Find(m_Seperator, uPathPos + 1))
				{
					GetEntries(Entries, pBranch, Flags, Path, uPathPos);
				}
			}
		}
#endif
	}
}

CPathEntryPtr CPathTree::GetBestEntry(FW::StringW Path, EMatchFlags Flags) const
{	
	auto Entries = GetEntries(Path, Flags);

	CPathEntryPtr pBestEntry;
	ULONG uBestMatchLen = 0;
	bool bBestExact = false;

	for (auto Entry : Entries)
	{
		ULONG uCurMatchLen = Entry.Info.GetMatchLen(); // length to first wildcard
		bool bCurExact = Entry.Info.IsExact(); // its exact when it has a non wildcard suffix

		if (uCurMatchLen > uBestMatchLen || (uCurMatchLen == uBestMatchLen && !bBestExact && bCurExact))
		{
			pBestEntry = Entry.pEntry;
			uBestMatchLen = uCurMatchLen;
			bBestExact = bCurExact;
		}
	}

	return pBestEntry;
}

bool CPathTree::Remove(const CPathEntryPtr& pEntry, SPathNode* pParent, const FW::StringW& Path, size_t uOffset)
{
	while (Path.At(uOffset) == m_Seperator)
		uOffset++;

	size_t uPos = Path.Find(m_Seperator, uOffset);
	if (uPos == -1 && uOffset < Path.Length())
		uPos = Path.Length();

	if (uPos == -1)
	{
		for(auto I = pParent->Entries.begin(); I != pParent->Entries.end(); ++I) {
			if (I->pEntry == pEntry) {
				pParent->Entries.erase(I);
				m_uEntryCount--;
				break;
			}
		}
		return true;
	}

	FW::StringW Name = Path.SubStr(uOffset, uPos - uOffset);

	bool bOk = false;

	SPathNode* pBranch = pParent->Branches.GetValuePtr(Name);
	if (pBranch) 
	{
		bOk |= Remove(pEntry, pBranch, Path, uPos + 1);

		if (pBranch->Branches.IsEmpty() && pBranch->WildBranches.IsEmpty() && pBranch->Entries.IsEmpty())
			pParent->Branches.Remove(Name);
	}

#ifdef PATH_TREE_USE_SMARTPATTERN
	SPathNode* pWildBranch = nullptr;
	if (auto* pPair = pParent->WildBranches.GetValuePtr(Name))
		pWildBranch = &pPair->first;
#else
	SPathNode* pWildBranch = pParent->WildBranches.GetValuePtr(Name);
#endif
	if (pWildBranch)
	{
		bOk |= Remove(pEntry, pWildBranch, Path, uPos + 1);

		if (pWildBranch->Branches.IsEmpty() && pWildBranch->WildBranches.IsEmpty() && pWildBranch->Entries.IsEmpty())
			pParent->WildBranches.Remove(Name);
	}

	return bOk;
}

void CPathTree::Clear()
{
	m_Root.Branches.Clear();
	m_Root.WildBranches.Clear();
	m_Root.Entries.Clear();
}

void CPathTree::Enum(const FW::StringW& Path, const SPathNode* pParent, void(*pCallback)(const FW::StringW& Path, const CPathEntryPtr& pEntry, void* pContext), void* pContext) const
{
	for (auto Entry : pParent->Entries)
		pCallback(Path, Entry.pEntry, pContext);
	//for (auto Patterns:	pParent->Patterns)
	//	Enum(Path + L"\\~ " + Patterns.first->Get(), Patterns.second, pCallback, pContext);

	for (auto& pBranch : pParent->Branches)
		Enum(Path + L"\\" + pBranch.Name, &pBranch, pCallback, pContext);

#ifdef PATH_TREE_USE_SMARTPATTERN
	for (auto& pBranch : pParent->WildBranches)
		Enum(Path + L"\\" + pBranch.first.Name, &pBranch.first, pCallback, pContext);
#else
	for (auto& pBranch : pParent->WildBranches)
		Enum(Path + L"\\" + pBranch.Name, &pBranch, pCallback, pContext);
#endif
}