#pragma once
#include "Tweak.h"

class CTweakManager
{
public:
	CTweakManager();
	~CTweakManager();

	STATUS Init();

	CTweakPtr GetRoot() const { std::unique_lock Lock(m_Mutex); return m_Root; }

	STATUS ApplyTweak(const std::wstring& Name, ETweakMode Mode = ETweakMode::eDefault);
	STATUS UndoTweak(const std::wstring& Name, ETweakMode Mode = ETweakMode::eDefault);

protected:
	friend class CGpoTweak;
	class CGPO* GetGPOLocked();

	mutable std::mutex m_Mutex;
	std::map<std::wstring, CTweakPtr> m_List;
	CTweakPtr m_Root;
	class CGPO* m_pGPO = NULL;
};