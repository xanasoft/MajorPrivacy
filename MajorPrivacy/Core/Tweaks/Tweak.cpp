#include "pch.h"
#include "Tweak.h"
#include "../Service/ServiceApi.h"
#include "../Programs/ProgramID.h"

///////////////////////////////////////////////////////////////////////////////////////
// CAbstractTweak

CAbstractTweak::CAbstractTweak()
{

}

void CAbstractTweak::FromVariant(const class XVariant& Tweak)
{
	Tweak.ReadRawMap([&](const SVarName& Name, const CVariant& vData) { ReadValue(Name, *(XVariant*)&vData); });
}

void CAbstractTweak::ReadValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, SVC_API_TWEAK_NAME))		m_Name = Data.AsQStr();

	else if (VAR_TEST_NAME(Name, SVC_API_TWEAK_HINT))
	{
		QString Hint = Data.AsQStr();
		if (Hint == SVC_API_TWEAK_HINT_RECOMMENDED)		m_Hint = ETweakHint::eRecommended;
		else											m_Hint = ETweakHint::eNone;
	}

	else if (VAR_TEST_NAME(Name, SVC_API_TWEAK_STATUS))
	{
		QString Status = Data.AsQStr();
		if (Status == SVC_API_TWEAK_NOT_SET)			m_Status = ETweakStatus::eNotSet;
		else if (Status == SVC_API_TWEAK_SET)			m_Status = ETweakStatus::eSet;
		else if (Status == SVC_API_TWEAK_APPLIED)		m_Status = ETweakStatus::eApplied;
		else if (Status == SVC_API_TWEAK_MISSING)		m_Status = ETweakStatus::eMissing;
		else if (Status == SVC_API_TWEAK_TYPE_GROUP)	m_Status = ETweakStatus::eGroup;
		else m_Status = ETweakStatus::eNotSet;
	}
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

void CTweakList::ReadValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, SVC_API_TWEAK_LIST))
	{
		QMap<QString, CTweakPtr> OldMap = m_List;

		Data.ReadRawList([&](const CVariant& vData) {
			const XVariant& Tweak = *(XVariant*)&vData;

			QString Name = Tweak.Find(SVC_API_TWEAK_NAME).AsQStr();

			CTweakPtr pTweak = OldMap.take(Name);
			if (pTweak.isNull())
			{
				QString Type = Tweak.Find(SVC_API_TWEAK_TYPE).AsQStr();
				if (Type == SVC_API_TWEAK_TYPE_GROUP)
					pTweak = CTweakPtr(new CTweakGroup());
				else if (Type == SVC_API_TWEAK_TYPE_SET)
					pTweak = CTweakPtr(new CTweakSet());
				else 
					pTweak = CTweakPtr(new CTweak());
				m_List.insert(Name, pTweak);
			}

			pTweak->FromVariant(Tweak);
		});

		foreach(const QString& Name, OldMap.keys())
			m_List.remove(Name);
	}
	else
		CAbstractTweak::ReadValue(Name, Data);
}

///////////////////////////////////////////////////////////////////////////////////////
// CTweak

void CTweak::FromVariant(const class XVariant& Tweak)
{
	CAbstractTweak::FromVariant(Tweak);

	QString Type = Tweak.Find(SVC_API_TWEAK_TYPE).AsQStr();
	if (Type == SVC_API_TWEAK_TYPE_REG)
	{
		m_Type = ETweakType::eReg;
		m_Info = GetTypeStr() + ":\n" +
			Tweak.Find(SVC_API_TWEAK_KEY).AsQStr() + "\n" + 
			Tweak.Find(SVC_API_TWEAK_VALUE).AsQStr() + " = " + Tweak.Find(SVC_API_TWEAK_DATA).AsQStr();
	}
	else if (Type == SVC_API_TWEAK_TYPE_GPO)
	{
		m_Type = ETweakType::eGpo;
		m_Info = GetTypeStr() + ":\n" +
			Tweak.Find(SVC_API_TWEAK_KEY).AsQStr() + "\n" + 
			Tweak.Find(SVC_API_TWEAK_VALUE).AsQStr() + " = " + Tweak.Find(SVC_API_TWEAK_DATA).AsQStr();
	}
	else if (Type == SVC_API_TWEAK_TYPE_SVC)
	{
		m_Type = ETweakType::eSvc;
		m_Info = GetTypeStr() + ":\n" +
			Tweak.Find(SVC_API_TWEAK_SVC_TAG).AsQStr();
	}
	else if (Type == SVC_API_TWEAK_TYPE_TASK)
	{
		m_Type = ETweakType::eTask;
		m_Info = GetTypeStr() + ":\n" +
			Tweak.Find(SVC_API_TWEAK_FOLDER).AsQStr() + "\\" + Tweak.Find(SVC_API_TWEAK_ENTRY).AsQStr();
	}
	else if (Type == SVC_API_TWEAK_TYPE_FS)
	{
		m_Type = ETweakType::eFS;
		m_Info = GetTypeStr() + ":\n" +
			Tweak.Find(SVC_API_TWEAK_PATH).AsQStr();
	}
	else if (Type == SVC_API_TWEAK_TYPE_EXEC)
	{
		m_Type = ETweakType::eExec;
		m_Info = GetTypeStr() + ":\n" +
			Tweak.Find(SVC_API_TWEAK_PATH).AsQStr();
	}
	else if (Type == SVC_API_TWEAK_TYPE_FW)
	{
		m_Type = ETweakType::eFw;
		CProgramID ID;
		ID.FromVariant(Tweak.Find(SVC_API_TWEAK_PROG_ID));
		m_Info = GetTypeStr() + ":\n" +
			ID.ToString();
	}
}

void CTweak::ReadValue(const SVarName& Name, const XVariant& Data)
{
	CAbstractTweak::ReadValue(Name, Data);
}