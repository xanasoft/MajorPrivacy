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

	void CheckTweaks();

	StVariant GetTweaks(const SVarWriteOpt& Opts, uint32 CallerPID) const;

	STATUS ApplyTweak(const std::wstring& Id, uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);
	STATUS UndoTweak(const std::wstring& Id, uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);
	STATUS ApproveTweak(const std::wstring& Id, uint32 CallerPID);

	bool AreTweaksDirty() const { return m_bTweaksDirty; }

	class CGPO* GetLockedGPO();
	void ReleaseGPO(class CGPO* pGPO);

protected:
	mutable std::mutex m_Mutex;
	std::shared_ptr<CTweakList> m_pRoot;
	std::map<std::wstring, CTweakPtr> m_Map;
	class CGPO* m_pGPO = NULL;
	std::mutex m_GPOMutex;

	bool m_bTweaksDirty = false;
};