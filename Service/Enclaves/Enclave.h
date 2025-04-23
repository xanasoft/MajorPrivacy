#pragma once
#include "../Library/Common/StVariant.h"

class CEnclave
{
	TRACK_OBJECT(CEnclave)
public:
	CEnclave();
	~CEnclave();


protected:
	mutable std::shared_mutex m_Mutex;


	StVariant m_Data;
};

typedef std::shared_ptr<CEnclave> CEnclavePtr;