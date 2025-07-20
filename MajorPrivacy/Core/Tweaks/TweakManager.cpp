#include "pch.h"
#include "TweakManager.h"
#include "../PrivacyCore.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../MajorPrivacy.h"

CTweakManager::CTweakManager(QObject* parent)
	: QObject(parent)
{
}

CTweakPtr CTweakManager::GetRoot() 
{
	if (!m_pRoot)
		Update();

	return m_pRoot; 
}

void CTweakManager::Update()
{
	QString Lang = theGUI->GetLanguage();
	if (Lang != m_Language)
	{
		m_Language = Lang;
		m_Translations.clear();
		LoadTranslations(Lang);
	}


	auto Ret = theCore->GetTweaks();
	if (Ret.IsError())
		return;

	QtVariant& Tweaks = Ret.GetValue();

	m_pRoot = QSharedPointer<CTweakList>(new CTweakList());
	m_Map.clear();

	m_AppliedCount = 0;
	m_FailedCount = 0;

	QtVariantReader(Tweaks).ReadRawList([&](const FW::CVariant& vData) {
		const QtVariant& Tweak = *(QtVariant*)&vData;

		QtVariantReader Reader(Tweak);

		QString Id = Reader.Find(API_V_TWEAK_ID).AsQStr();

		CTweakPtr pTweak;
		ETweakType Type = (ETweakType)Reader.Find(API_V_TWEAK_TYPE).To<uint32>();
		if (Type == ETweakType::eGroup)		pTweak = CTweakPtr(new CTweakGroup());
		else if (Type == ETweakType::eSet)	pTweak = CTweakPtr(new CTweakSet());
		else								pTweak = CTweakPtr(new CTweak(Type));

		pTweak->FromVariant(Tweak);

		QString Name = m_Translations.value(Id + "/Name");
		if (!Name.isEmpty()) pTweak->SetName(Name);

		QString Info = m_Translations.value(Id + "/Description");
		if (!Info.isEmpty()) pTweak->SetDescription(Info);

		pTweak->SetStatus((ETweakStatus)Reader.Find(API_V_TWEAK_STATUS).To<uint32>()); 

		m_Map[pTweak->GetId()] = pTweak;

		if (!pTweak->GetParentId().isEmpty()) {
			QSharedPointer<CTweakList> pParent;
			auto F = m_Map.find(pTweak->GetParentId());
			if (F != m_Map.end())
				pParent = F.value().objectCast<CTweakList>();
			ASSERT(pParent);
			if (pParent) {
				pParent->AddTweak(pTweak);
				if(QSharedPointer<CTweak> TweakPtr = pTweak.objectCast<CTweak>())
					TweakPtr->SetParent(pParent);

				if(pTweak->GetType() != ETweakType::eGroup && pTweak->GetType() != ETweakType::eSet)
				{
					if (pTweak->GetStatus() == ETweakStatus::eApplied || pTweak->GetStatus() == ETweakStatus::eSet)
						m_AppliedCount++;
					else if (pTweak->GetStatus() == ETweakStatus::eMissing || pTweak->GetStatus() == ETweakStatus::eIneffective)
						m_FailedCount++;
				}

				return;
			}
		}

		m_pRoot->AddTweak(pTweak);
	});
}

STATUS CTweakManager::ApplyTweak(const CTweakPtr& pTweak)
{
	STATUS Status = theCore->ApplyTweak(pTweak->GetId());
	emit TweaksChanged();
	return Status;
}

STATUS CTweakManager::UndoTweak(const CTweakPtr& pTweak)
{
	STATUS Status = theCore->UndoTweak(pTweak->GetId());
	emit TweaksChanged();
	return Status;
}

void CTweakManager::LoadTranslations(QString Lang)
{
	QSettings TweaksIni(QApplication::applicationDirPath() + "/Tweaks.ini", QSettings::IniFormat);

	QStringList Sections = TweaksIni.childGroups();
	if (Sections.isEmpty()) 
		return;
	
	QString Suffix;
	if(Lang != "en")
		Suffix = "_" + Lang;

	QString FirstSection = Sections.first();
	QString Test = TweaksIni.value(FirstSection + "/Name" + Suffix).toString();
	if (Test.isEmpty() && !Suffix.isEmpty()) {
		Lang.truncate(Lang.lastIndexOf('_')); // Short version as fallback
		Suffix = "_" + Lang;
		Test = TweaksIni.value(FirstSection + "/Name" + Suffix).toString();
	}
	if (Test.isEmpty())
		return;

	foreach(QString Section, Sections)
	{
		QString Name = TweaksIni.value(Section + "/Name" + Suffix).toString();
		if (!Name.isEmpty()) m_Translations[Section + "/Name"] = Name;

		QString Description = TweaksIni.value(Section + "/Description" + Suffix).toString();
		if (!Description.isEmpty()) m_Translations[Section + "/Description"] = Description;
	}
}