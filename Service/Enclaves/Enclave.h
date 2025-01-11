#pragma once
#include "../Library/Common/Variant.h"

class CEnclave
{
	TRACK_OBJECT(CEnclave)
public:
	CEnclave();
	~CEnclave();


protected:
	mutable std::shared_mutex m_Mutex;


	CVariant m_Data;
};

typedef std::shared_ptr<CEnclave> CEnclavePtr;