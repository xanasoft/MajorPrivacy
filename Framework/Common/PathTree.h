#pragma once

#include "SmartPattern.h"

#define PATH_TREE_USE_SMARTPATTERN

/////////////////////////////////////////////////////////////////////////////
// CPathEntry
//

class FRAMEWORK_EXPORT CPathEntry : public FW::Object
{
public:
	CPathEntry(FW::AbstractMemPool* pMem, const wchar_t* Path = nullptr) : FW::Object(pMem), m_Path(pMem) {
		if (Path) m_Path = Path;
	}
	CPathEntry(FW::AbstractMemPool* pMem, const FW::StringW& Path) : FW::Object(pMem), m_Path(pMem) {
		m_Path = Path;
	}

	FW::StringW GetPath() const { return m_Path; }

protected:
	virtual bool SetPath(const FW::StringW& Path) { m_Path = Path; return true; }

	FW::StringW	m_Path;
};

typedef FW::SharedPtr<CPathEntry> CPathEntryPtr;


/////////////////////////////////////////////////////////////////////////////
// CPathTree
//

class FRAMEWORK_EXPORT CPathTree : public FW::Object
{
public:
	CPathTree(FW::AbstractMemPool* pMem);

	void SetSeparator(wchar_t Separator) { m_Seperator = Separator; }
	void SetSimplePattern(bool bSimplePattern) { m_bSimplePattern = bSimplePattern; }

	virtual int AddEntry(const CPathEntryPtr& pEntry);
	virtual bool RemoveEntry(const CPathEntryPtr& pEntry);
	virtual void Clear();

	struct SPathEntry
	{
		CPathEntryPtr pEntry;

		SPatternInfo Info;
		bool EndsWithSeparator = false;
	};

	typedef SPathEntry SFoundEntry;

	enum EMatchFlags {
		eNone = 0,
		eMayHaveSeparator = 1,
	};

	FW::List<SFoundEntry> GetEntries(FW::StringW Path, EMatchFlags Flags = eNone) const;
	FW::List<SFoundEntry> GetEntries(const wchar_t* pPath, EMatchFlags Flags = eNone) const	{ return GetEntries(FW::StringW(m_pMem, pPath), Flags); }

	CPathEntryPtr GetBestEntry(FW::StringW Path, EMatchFlags Flags = eNone) const;
	CPathEntryPtr GetBestEntry(const wchar_t* pPath, EMatchFlags Flags = eNone) const	{ return GetBestEntry(FW::StringW(m_pMem, pPath), Flags); }

	void Enum(void(*pCallback)(const FW::StringW& Path, const CPathEntryPtr& pEntry, void* pContext), void* pContext) const { Enum(FW::StringW(m_pMem, L""), &m_Root, pCallback, pContext); }

protected:

	struct SPathNode
	{
		FW::StringW	Name;
		FW::Table<FW::StringW, SPathNode> Branches;
#ifdef PATH_TREE_USE_SMARTPATTERN
		FW::Table<FW::StringW, FW::Pair<SPathNode, CSmartPatternPtr>> WildBranches;
#else
		FW::Table<FW::StringW, SPathNode> WildBranches;
#endif
		FW::List<SPathEntry> Entries;

		void Init(FW::AbstractMemPool* pMem) {
			Name = FW::StringW(pMem);
			Branches = FW::Table<FW::StringW, SPathNode>(pMem);
#ifdef PATH_TREE_USE_SMARTPATTERN
			WildBranches = FW::Table<FW::StringW, FW::Pair<SPathNode, CSmartPatternPtr>>(pMem);
#else
			WildBranches = FW::Table<FW::StringW, SPathNode>(pMem);
#endif
			Entries = FW::List<SPathEntry>(pMem);
		}
	};

	SPathEntry* Add(const CPathEntryPtr& pEntry, SPathNode* pParent, const FW::StringW& Path, size_t uOffset = 0);
	bool Remove(const CPathEntryPtr& pEntry, SPathNode* pParent, const FW::StringW& Path, size_t uOffset = 0);

	void GetEntries(FW::List<SFoundEntry>& Entries, const SPathNode* pParent, EMatchFlags Flags, const FW::StringW& Path, size_t uOffset = 0) const;

	void Enum(const FW::StringW& Path, const SPathNode* pParent, void(*pCallback)(const FW::StringW& Path, const CPathEntryPtr& pEntry, void* pContext), void* pContext) const;

	wchar_t m_Seperator = L'\\';
	bool m_bSimplePattern = false;
	SPathNode m_Root;
	size_t m_uEntryCount = 0;
};