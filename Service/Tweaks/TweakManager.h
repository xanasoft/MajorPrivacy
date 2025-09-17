#pragma once
#include "Tweak.h"

class CTweakManager
{
	TRACK_OBJECT(CTweakManager)
public:
	CTweakManager();
	~CTweakManager();

	STATUS Init();

	STATUS Load();
	STATUS Store();

	uint32 GetRevision() const { std::unique_lock Lock(m_Mutex); return m_Revision; }

	void CheckTweaks();

	StVariant GetTweaks(uint32 CallerPID, const SVarWriteOpt& Opts, FW::AbstractMemPool* pMemPool = nullptr) const;

	STATUS ApplyTweak(const std::wstring& Id, uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);
	STATUS UndoTweak(const std::wstring& Id, uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);
	STATUS ApproveTweak(const std::wstring& Id, uint32 CallerPID);

	bool AreTweaksDirty() const { return m_bTweaksDirty; }

	class CGPO* GetLockedGPO();
	void ReleaseGPO(class CGPO* pGPO);

protected:

	static STATUS Load(std::shared_ptr<CTweakList>& pRoot, std::map<std::wstring, CTweakPtr>& Map);

	mutable std::mutex m_Mutex;
	std::shared_ptr<CTweakList> m_pRoot;
	std::map<std::wstring, CTweakPtr> m_Map;
	class CGPO* m_pGPO = NULL;
	std::mutex m_GPOMutex;
	uint32 m_Revision = 0;

	bool m_bTweaksDirty = false;
};