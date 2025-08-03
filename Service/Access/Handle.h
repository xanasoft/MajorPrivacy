#pragma once
#include "../Library/Common/Address.h"
#include "../Common/AbstractInfo.h"
#include "../Library/Common/StVariant.h"
#include "../Library/Common/MiscStats.h"

class CHandle: public CAbstractInfoEx
{
	TRACK_OBJECT(CHandle)
public:
	CHandle(const std::wstring& FileName);
	~CHandle();

	std::wstring		GetFileName() const				{ std::shared_lock Lock(m_Mutex); return m_FileName; }

	uint64				GetProcessId() const			{ std::shared_lock Lock(m_Mutex); return m_ProcessId; }
	std::shared_ptr<class CProcess> GetProcess() const	{ std::shared_lock Lock(m_Mutex); return m_pProcess.lock(); }


	StVariant ToVariant(FW::AbstractMemPool* pMemPool = nullptr) const;

protected:
	friend class CHandleList;

	void LinkProcess(const std::shared_ptr<class CProcess>& pProcess);

	std::wstring					m_FileName;
	uint64							m_ProcessId = 0;
	std::weak_ptr<class CProcess>	m_pProcess;

};

typedef std::shared_ptr<CHandle> CHandlePtr;