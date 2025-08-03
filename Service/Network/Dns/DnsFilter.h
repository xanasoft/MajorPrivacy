#pragma once
#include "DnsServer.h"
#include "../Library/Status.h"
#include "../Library/API/PrivacyDefs.h"
#include "../Framework/Common/PathTree.h"
#include "..\Library\Common\Address.h"
#include "..\Library\Common\StVariant.h"
#include "..\Library\Common\FlexGuid.h"

class CDnsRule : public CPathEntry
{
public:

	enum EAction { eNone, eBlock, eAllow };

	CDnsRule(FW::AbstractMemPool* pMem) : CPathEntry(pMem) {}
	CDnsRule(FW::AbstractMemPool* pMem, const FW::StringW& Path, EAction Action, bool bTemporary = false) : CPathEntry(pMem, Path) {
		m_Action = Action;
		m_bTemporary = bTemporary;
	}

	void SetGuid(const CFlexGuid& Guid)		{ m_Guid = Guid; }
	CFlexGuid GetGuid() const				{ return m_Guid; }

	bool IsEnabled() const					{ return m_bEnabled; }

	bool IsTemporary() const				{ return m_bTemporary; }

	EAction GetAction() const				{ return m_Action; }

	void IncHitCount()						{ m_HitCount++; }

	virtual StVariant ToVariant(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;
	virtual STATUS FromVariant(const StVariant& Rule);

protected:
	virtual void WriteIVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(StVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const StVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const StVariant& Data);

	CFlexGuid		m_Guid;
	bool			m_bEnabled = true;

	bool 			m_bTemporary = false;

	std::string		m_HostName;
	EAction			m_Action = eNone;

	int				m_HitCount = 0;
};

struct SDnsFilterEvent
{
	std::wstring	HostName;
	CDnsRule::EAction Action = CDnsRule::eNone;
	std::wstring	ResolvedString;
	CAddress		Address;
	uint16			Type = 0;
	sint32			TTL = 0;
};

class CDnsFilter: public CDnsServer
{
public:
	CDnsFilter();
	virtual ~CDnsFilter();

	STATUS Init();

	STATUS Load();
	STATUS Store();

	STATUS LoadEntries(const StVariant& Entries);
	StVariant SaveEntries(const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr);

	RESULT(StVariant) SetEntry(const StVariant& Entry);
	RESULT(StVariant) GetEntry(const std::wstring& EntryId, const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr);
	STATUS DelEntry(const std::wstring& EntryId);

	virtual void UpdateBlockLists();
	virtual bool AreBlockListsLoaded() const { std::unique_lock lock(m_Mutex); return m_BlockList.size() > 0; }

	void RegisterEventHandler(const std::function<void(const SDnsFilterEvent* pEvent)>& Handler);
	template<typename T, class C>
	void RegisterEventHandler(T Handler, C This) { RegisterEventHandler(std::bind(Handler, This, std::placeholders::_1)); }

	StVariant GetBlockListInfo(FW::AbstractMemPool* pMemPool = nullptr) const;

protected:
	bool HandlePacket(DNS::Packet& Packet, const SAddress& Address) override;
	void SendResponse(DNS::Packet& Response, const SAddress& Address) override;

	void EmitChangeEvent(const CFlexGuid& Guid, enum class EConfigEvent Event);

	std::wstring GetFileNameFromUrl(const std::wstring& sUrl);
	virtual bool DownloadBlockList(const std::wstring& sBlockListUrl);

	virtual void LoadBlockLists_unlocked();

	virtual int LoadFilters_unlocked(const std::wstring& sHostsFile, CDnsRule::EAction Action, std::multimap<SKey, DNS::Binary>* pEntryMap = NULL);

	virtual bool AddEntry_unlocked(const FW::SharedPtr<CDnsRule>& pEntry);

	std::multimap<SKey, DNS::Binary> m_BlockList;
	FW::DefaultMemPool m_MemPool;
	CPathTree m_FilterTree;
	FW::Map<CFlexGuid, FW::SharedPtr<CDnsRule>> m_EntryMap;

	struct SBLockList {
		std::wstring Url;
		int Count = 0;
	};

	std::map<std::wstring, SBLockList> m_BlockLists;

	std::mutex m_DlMutex;
	friend DWORD CALLBACK CDnsFilter__DlThreadProc(LPVOID lpThreadParameter);
	volatile HANDLE m_hDlThread = NULL;
	std::map<std::wstring, std::wstring> m_DlList;

	mutable std::mutex m_Mutex;
	std::vector<std::function<void(const SDnsFilterEvent* pEvent)>> m_EventHandlers;
	void EmitEvent(const SDnsFilterEvent* pEvent);
};