#pragma once
#include "../lib_global.h"
#include "../../Framework/Header.h"
#include "../../Framework/String.h"
#include "../../Framework/List.h"
#include "../../Framework/Array.h"
#include "../../Framework/Map.h"
#include "../../Framework/Object.h"

#include "../../Library/Common/DosPattern.h"
#include "../../Library/Common/Variant.h"

#ifndef FRAMEWORK_EXPORT
#define FRAMEWORK_EXPORT LIBRARY_EXPORT
#endif


/////////////////////////////////////////////////////////////////////////////
// CPathRule
//

class LIBRARY_EXPORT CPathRule: public FW::Object
{
public:
	CPathRule(FW::AbstractMemPool* pMem = NULL);
	//CPathRule(FW::AbstractMemPool* pMem, const FW::StringW& Path);
	//CPathRule(FW::AbstractMemPool* pMem, const wchar_t* Path) : CPathRule(pMem, FW::StringW(pMem, Path)) {}
	virtual ~CPathRule();

	void SetNewGuid();
	void SetGuid(const FW::StringW& Guid)			{ m_Guid = Guid; }
	FW::StringW GetGuid() const						{ return m_Guid; }

	bool IsEnabled() const							{ return m_bEnabled; }
	void SetEnabled(bool bEnabled)					{ m_bEnabled = bEnabled; }

	bool SetPath(const FW::StringW& Path)			{ return m_PathPattern.Set(Path); }
	FW::StringW GetPath() const						{ return m_PathPattern.Get(); }
	bool IsExact() const							{ return m_PathPattern.IsExact(); }
	int GetWildcardCount() const					{ return m_PathPattern.GetWildCount(); }

	bool IsMatch(const FW::StringW& Path, size_t uOffset = 0) const { return !!GetMatch(Path, uOffset); }
	ULONG GetMatch(const FW::StringW& Path, size_t uOffset = 0) const;

protected:

	FW::StringW m_Guid;
	bool m_bEnabled = true;
	CDosPattern m_PathPattern;
};

typedef FW::SharedPtr<CPathRule> CPathRulePtr;


/////////////////////////////////////////////////////////////////////////////
// CPathRuleList
//

class LIBRARY_EXPORT CPathRuleList: public FW::AbstractContainer
{
public:
	CPathRuleList(FW::AbstractMemPool* pMem);
	virtual ~CPathRuleList();

	bool Add(const CPathRulePtr& pRule);
	bool Remove(const FW::StringW& Guid);
	void Clear();

	FW::Map<FW::StringW, CPathRulePtr> GetRuleMap() const		{ return m_Map; }
	CPathRulePtr GetRule(const FW::StringW& Guid) const			{ return m_Map.GetValue(Guid); }

	struct SFoundRule
	{
		CPathRulePtr pRule;
		ULONG uMatchLen = 0;
		bool bExact = false;
		int nWildcards = 0;
	};

	FW::List<SFoundRule> GetRules(FW::StringW Path) const;
	FW::List<SFoundRule> GetRules(const wchar_t* pPath) const	{ return GetRules(FW::StringW(m_pMem, pPath)); }

	struct SPathNode: FW::Object
	{
		SPathNode(FW::AbstractMemPool* pMem) : FW::Object(pMem)
			, Name(pMem)
			, Branches(pMem)
			//, Patterns(pMem)
			, Rules(pMem) {}

		FW::StringW Name;
		FW::Map<FW::StringW, FW::SharedPtr<SPathNode>> Branches;
		//FW::List<FW::Pair<CDosPatternPtr, FW::SharedPtr<SPathNode>>> Patterns;
		FW::List<CPathRulePtr> Rules;
	};

	void Enum(void(*pCallback)(const FW::StringW& Path, const CPathRulePtr& pRule, void* pContext), void* pContext) const { Enum(FW::StringW(m_pMem, L""), m_Root, pCallback, pContext); }

protected:

	bool Add(const CPathRulePtr& pRule, FW::SharedPtr<SPathNode>& pParent, const FW::StringW& Path, size_t uOffset = 0);
	void Remove(const CPathRulePtr& pRule, FW::SharedPtr<SPathNode>& pParent, const FW::StringW& Path, size_t uOffset = 0);

	FW::List<SFoundRule> GetRules(const FW::SharedPtr<SPathNode>& pParent, const FW::StringW& Path, size_t uOffset = 0) const;

	void Enum(const FW::StringW& Path, const FW::SharedPtr<SPathNode>& pParent, void(*pCallback)(const FW::StringW& Path, const CPathRulePtr& pRule, void* pContext), void* pContext) const;

	FW::SharedPtr<SPathNode>			m_Root;
	FW::Map<FW::StringW, CPathRulePtr>	m_Map;

};