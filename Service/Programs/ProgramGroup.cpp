#include "pch.h"
#include "ProgramGroup.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../Processes/ProcessList.h"
#include "../ServiceCore.h"
#include "../Library/Common/Strings.h"

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

///////////////////////////////////////////////////////////////////////////////////////
// CAllPrograms

CAllPrograms::CAllPrograms() 
{ 
	m_ID = CProgramID(EProgramType::eAllPrograms); 
}

///////////////////////////////////////////////////////////////////////////////////////
// CProgramGroup

CProgramGroup::CProgramGroup(const std::wstring& Guid) 
{
	m_ID = CProgramID(MkLower(Guid), EProgramType::eProgramGroup);
}

///////////////////////////////////////////////////////////////////////////////////////
// CProgramRoot

CProgramRoot::CProgramRoot() 
{ 
	m_ID = CProgramID(EProgramType::eProgramRoot); 
}