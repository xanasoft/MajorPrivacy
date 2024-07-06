#include "pch.h"
#include "Tweak.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Programs/ProgramID.h"

///////////////////////////////////////////////////////////////////////////////////////
// CAbstractTweak

CAbstractTweak::CAbstractTweak()
{

}

void CAbstractTweak::FromVariant(const class XVariant& Tweak)
{
	if (Tweak.GetType() == VAR_TYPE_MAP)		Tweak.ReadRawMap([&](const SVarName& Name, const CVariant& Data)	{ ReadMValue(Name, Data); });
	else if (Tweak.GetType() == VAR_TYPE_INDEX)	Tweak.ReadRawIMap([&](uint32 Index, const CVariant& Data)			{ ReadIValue(Index, Data); });
	// todo err
}

QString CAbstractTweak::GetTypeStr() const
{
	switch (m_Type)
	{
	case ETweakType::eGroup:	return tr("Tweak Group");
	case ETweakType::eSet:		return tr("Tweak Set");
	case ETweakType::eReg:		return tr("Registry");
	case ETweakType::eGpo:		return tr("Group Policy");
	case ETweakType::eSvc:		return tr("Service");
	case ETweakType::eTask:		return tr("Task");
	case ETweakType::eFS:		return tr("File System");
	case ETweakType::eFw:		return tr("Firewall");
	default:					return tr("Unknown");
	}
	return "";
}

QString CAbstractTweak::GetStatusStr() const
{
	switch (GetStatus())
	{
	case ETweakStatus::eNotSet:		return tr("Not set");
	case ETweakStatus::eApplied:	return tr("Applied");
	case ETweakStatus::eSet:		return tr("Set");
	case ETweakStatus::eMissing:	return tr("Missing");
	//case ETweakStatus::eGroup:		return tr("Group");
	//default:						return tr("Unknown");
	}
	return "";
}

///////////////////////////////////////////////////////////////////////////////////////
// CTweakList

void CTweakList::ReadList(const XVariant& List)
{
	QMap<QString, CTweakPtr> OldMap = m_List;

	List.ReadRawList([&](const CVariant& vData) {
		const XVariant& Tweak = *(XVariant*)&vData;

		QString Name = Tweak.Find(API_V_NAME).AsQStr();

		CTweakPtr pTweak = OldMap.take(Name);
		if (pTweak.isNull())
		{
			ETweakType Type = (ETweakType)Tweak.Find(API_V_TWEAK_TYPE).To<uint32>();
			if (Type == ETweakType::eGroup)		pTweak = CTweakPtr(new CTweakGroup());
			else if (Type == ETweakType::eSet)	pTweak = CTweakPtr(new CTweakSet());
			else								pTweak = CTweakPtr(new CTweak());
			m_List.insert(Name, pTweak);
		}

		pTweak->FromVariant(Tweak);
	});

	foreach(const QString & Name, OldMap.keys())
		m_List.remove(Name);
}

