#include "pch.h"
#include "DnsProcLog.h"
#include "../../Etw/EtwEventMonitor.h"
#include "ServiceCore.h"
#include "../NetworkManager.h"
#include "DnsInspector.h"

CDnsProcLog::CDnsProcLog()
{
}

void CDnsProcLog::Update()
{
	std::unique_lock Lock(m_Mutex);

	// todo cleanup old entries by time stamps
}

static const CAddress LocalHost("127.0.0.1");
static const CAddress LocalHostV6("::1");

bool CDnsProcLog::ResolveHost(const CAddress& Address, const CHostNamePtr& pHostName)
{
	if (Address.IsNull()) // || Address == CAddress::AnyIPv4 || Address == CAddress::AnyIPv6)
		return false;

	// if its the local host return imminetly
	if (Address == LocalHost || Address == LocalHostV6)
		return false;

	SHostNameInfoPtr pHostInfo = GetHostForAddress(Address);
	if (pHostInfo) 
	{
		const SAddrInfoPtr& pAddrInfo = pHostInfo->Addresses[Address];
		pHostInfo->HitCount++;
		pAddrInfo->HitCount++;

		auto ptr = std::dynamic_pointer_cast<CResHostName>(pHostName);
		if (ptr) ptr->Resolve(pHostInfo->HostName, EDnsSource::eCapturedQuery, pAddrInfo->TimeStamp);
		return true;
	}

	// todo: add also to resolve list when the found entry was vers old
	m_ResolveList.insert(std::make_pair(Address, pHostName));

	return false;
}

void CDnsProcLog::OnEtwDnsEvent(const struct SEtwDnsEvent* pEvent)
{
	std::unique_lock Lock(m_Mutex);

	auto F = m_DnsByHost.find(pEvent->HostName);
	SHostNameInfoPtr pHostInfo;
	if(F != m_DnsByHost.end()) pHostInfo = F->second;
	else pHostInfo = m_DnsByHost.insert(std::make_pair(pEvent->HostName, std::make_shared<SHostNameInfo>(pEvent->HostName))).first->second;
	
	pHostInfo->TimeStamp = pEvent->TimeStamp;
	for (auto Address : pEvent->Results)
	{
		SAddrInfoPtr& pAddrInfo = pHostInfo->Addresses[Address];
		if (!pAddrInfo) {
			pAddrInfo = std::make_shared<SAddrInfo>(Address);
			m_DnsByAddr.insert(std::make_pair(Address, pHostInfo));
		}
		pAddrInfo->TimeStamp = pEvent->TimeStamp;

		for (auto I = m_ResolveList.find(Address); I != m_ResolveList.end() && I->first == Address; I = m_ResolveList.erase(I))
		{
			CHostNamePtr pHostName = I->second.lock();
			if (!pHostName) continue;
			auto ptr = std::dynamic_pointer_cast<CResHostName>(pHostName);
			if (ptr) ptr->Resolve(pEvent->HostName, EDnsSource::eCapturedQuery, pEvent->TimeStamp);
		}
	}
}

CDnsProcLog::SHostNameInfoPtr CDnsProcLog::GetHostForAddress(const CAddress& Address)
{
	SHostNameInfoPtr pBestHostInfo;
	uint64 BestTimeStamp = 0;
	for(auto F = m_DnsByAddr.find(Address); F != m_DnsByAddr.end() && F->first == Address; ++F)
	{
		SHostNameInfoPtr pHostInfo = F->second.lock();
		const SAddrInfoPtr& pAddrInfo = pHostInfo->Addresses[Address];

		if (!pBestHostInfo || pAddrInfo->TimeStamp > BestTimeStamp) {
			pBestHostInfo = pHostInfo;
			BestTimeStamp = pAddrInfo->TimeStamp;
		}
	}
	return pBestHostInfo;
}