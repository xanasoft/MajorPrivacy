#include "pch.h"
#include "Tweak.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Programs/ProgramID.h"

///////////////////////////////////////////////////////////////////////////////////////
// CAbstractTweak

CAbstractTweak::CAbstractTweak()
{

}

void CAbstractTweak::FromVariant(const class QtVariant& Tweak)
{
	if (Tweak.GetType() == VAR_TYPE_MAP)		QtVariantReader(Tweak).ReadRawMap([&](const SVarName& Name, const QtVariant& Data) { ReadMValue(Name, Data); });
	else if (Tweak.GetType() == VAR_TYPE_INDEX)	QtVariantReader(Tweak).ReadRawIndex([&](uint32 Index, const QtVariant& Data) { ReadIValue(Index, Data); });
	// todo err
}

QString CAbstractTweak::GetTypeStr() const
{
	switch (m_Type)
	{
	case ETweakType::eGroup:	return tr("Tweak Group");
	case ETweakType::eSet:		return tr("Tweak Bundle");
	case ETweakType::eReg:		return tr("Set Registry Option");
	case ETweakType::eGpo:		return tr("Configure Group Policy");
	case ETweakType::eSvc:		return tr("Disable System Service");
	case ETweakType::eTask:		return tr("Disable Scheduled Task");
	case ETweakType::eRes:		return tr("Resource Access Rule");
	case ETweakType::eExec:		return tr("Execution Control Rule");
	case ETweakType::eFw:		return tr("Firewall Rule");
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
	case ETweakStatus::eIneffective: return tr("Ineffective");
	case ETweakStatus::eCustom:		return tr("Custom");
	case ETweakStatus::eGroup:		return tr("Group");
	case ETweakStatus::eNotAvailable: return tr("Not available");
	//default:						return tr("Unknown");
	}
	return "";
}

QVariant CAbstractTweak::GetStatusColor() const
{
	switch (GetStatus())
	{
	case ETweakStatus::eApplied:	return QColor(255, 255, 128);
	case ETweakStatus::eSet:		return QColor(128, 255, 128);
	case ETweakStatus::eMissing:	return QColor(255, 128, 128);
	case ETweakStatus::eIneffective: return QColor(255, 128, 255);
	case ETweakStatus::eCustom:		return QColor(128, 255, 255);
	case ETweakStatus::eNotAvailable: return QColor(128, 128, 128);
		//case ETweakStatus::eNotSet:
	}
	return QVariant();
}

QString CAbstractTweak::GetHintStr() const
{
	switch (GetHint())
	{
	//case ETweakHint::eNone:			return tr("No hint");
	case ETweakHint::eRecommended:		return tr("Recommended");
	case ETweakHint::eNotRecommended:	return tr("Not recommended");
	case ETweakHint::eBreaking:			return tr("Breaking");
	}
	return "";
}

///////////////////////////////////////////////////////////////////////////////////////
// CTweakList

QString CTweakList::GetStatusStr() const
{
	int ACount = 0;
	foreach(const CTweakPtr & pTweak, m_List)
	{
		if (pTweak->GetStatus() == ETweakStatus::eSet || pTweak->GetStatus() == ETweakStatus::eApplied)
			ACount++;
	}
	if(ACount && ACount != m_List.count())
		return tr("%1 (%2/%3)").arg(CAbstractTweak::GetStatusStr()).arg(ACount).arg(m_List.count());
	return tr("%1 (%2)").arg(CAbstractTweak::GetStatusStr()).arg(m_List.count());
}

///////////////////////////////////////////////////////////////////////////////////////
// CTweakGroup

QIcon CTweakGroup::GetIcon() const
{
	if (m_IconLoaded)
		return m_Icon;

	if (m_Id == "WindowsTelemetry")
		m_Icon = QIcon(":/Icons/Telemetry.png");
	else if (m_Id == "OtherTelemetry")
		m_Icon = QIcon(":/Icons/Telemetry3.png");
	else if (m_Id == "WindowsSearch")
		m_Icon = QIcon(":/Icons/Search2.png");
	else if (m_Id == "MicrosoftDefender")
		m_Icon = QIcon(":/Icons/Shield4.png");
	else if (m_Id == "WindowsUpdate")
		m_Icon = QIcon(":/Icons/Update.png");
	else if (m_Id == "WindowsPrivacy")
		m_Icon = QIcon(":/Icons/Anon.png");
	else if (m_Id == "MicrosoftAccount")
		m_Icon = QIcon(":/Icons/Users.png");
	else if (m_Id == "MicrosoftOffice")
		m_Icon = QIcon(":/Icons/MsOffice.png");
	else if (m_Id == "VisualStudio")
		m_Icon = QIcon(":/Icons/VisualStudio.png");
	else if (m_Id == "WindowsMisc")
		m_Icon = QIcon(":/Icons/Windows.png");
	else if (m_Id == "StoreAndApps")
		m_Icon = QIcon(":/Icons/MsStore.png");
	else if (m_Id == "System")
		m_Icon = QIcon(":/Icons/Advanced.png");
	else if (m_Id == "WerBrowsers")
		m_Icon = QIcon(":/Icons/Internet.png");
	else if (m_Id == "Explorer")
		m_Icon = QIcon(":/Icons/Directory.png");
	else if (m_Id == "Network")
		m_Icon = QIcon(":/Icons/Network.png");
	else if (m_Id == "TestTweaks")
		m_Icon = QIcon(":/Icons/Warning.png");

	m_IconLoaded = true;
	return m_Icon;
}

///////////////////////////////////////////////////////////////////////////////////////
// CTweak


ETweakHint CTweak::GetHint() const 
{ 
	/*if (auto pSet = m_Parent.lock().objectCast<CTweakSet>())
	{		
		if(m_Hint == ETweakHint::eNone)
			return ETweakHint::eNotRecommended;
		else if(m_Hint == ETweakHint::eRecommended)
			return ETweakHint::eNone;
	}*/
	return m_Hint; 
}