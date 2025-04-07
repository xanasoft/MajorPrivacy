#include "pch.h"
#include "Handle.h"
#include "../Library/API/PrivacyAPI.h"

CHandle::CHandle(QObject* parent)
	: QObject(parent)
{
}

void CHandle::FromVariant(const class QtVariant& Handle)
{
	QtVariantReader(Handle).ReadRawIndex([&](uint32 Index, const FW::CVariant& vData) {
		const QtVariant& Data = *(QtVariant*)&vData;

		switch (Index)
		{
		case API_V_ACCESS_REF:			m_HandleRef = Data; break;
		case API_V_ACCESS_PATH:			m_NtPath = Data.AsQStr(); break;
		case API_V_PID:					m_ProcessId = Data; break;
		}
	});
}