#pragma once
#include "../Library/Common/StVariant.h"

class CHash
{
	TRACK_OBJECT(CHash)
public:
	CHash();
	~CHash();


protected:
	mutable std::shared_mutex m_Mutex;


	StVariant m_Data;
};

typedef std::shared_ptr<CHash> CHashPtr;