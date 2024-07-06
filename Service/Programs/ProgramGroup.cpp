#include "pch.h"
#include "ProgramGroup.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Processes/ProcessList.h"
#include "../ServiceCore.h"

///////////////////////////////////////////////////////////////////////////////////////
// CProgramSet

bool CProgramSet::ContainsNode(const CProgramItemPtr& Item) const 
{ 
	std::unique_lock lock(m_Mutex);
	for (auto I = m_Nodes.begin(); I != m_Nodes.end(); I++) {
		if (*I == Item)
			return true;
	}
	return false;
}