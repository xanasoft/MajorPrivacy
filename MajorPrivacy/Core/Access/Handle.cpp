#include "pch.h"
#include "Handle.h"
#include "../Library/API/PrivacyAPI.h"

CHandle::CHandle(QObject* parent)
	: QObject(parent)
{
}

void CHandle::FromVariant(const class XVariant& Handle)
{
	Handle.ReadRawIMap([&](uint32 Index, const CVariant& vData) {
		const XVariant& Data = *(XVariant*)&vData;

		switch (Index)
		{
		case API_V_ACCESS_REF:			m_HandleRef = Data; break;
		case API_V_ACCESS_PATH:			m_Path.Set(Data.AsQStr(), EPathType::eNative); break;
		case API_V_ACCESS_PID:			m_ProcessId = Data; break;
		}
	});
}