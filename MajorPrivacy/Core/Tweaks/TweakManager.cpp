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
	if (!m_pRoot) {
		m_pRoot = QSharedPointer<CTweakList>(new CTweakList());
		Update();
	}

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
				return;
			}
		}

		m_pRoot->AddTweak(pTweak);
	});
}

STATUS CTweakManager::ApplyTweak(const CTweakPtr& pTweak)
{
	return theCore->ApplyTweak(pTweak->GetId());
}

STATUS CTweakManager::UndoTweak(const CTweakPtr& pTweak)
{
	return theCore->UndoTweak(pTweak->GetId());
}

void CTweakManager::LoadTranslations(QString Lang)
{
	QSettings TweaksIni(QApplication::applicationDirPath() + "/Tweaks.ini", QSettings::IniFormat);

	QStringList Sections = TweaksIni.childGroups();
	if (Sections.isEmpty()) 
		return;
	
	QString FirstSection = Sections.first();
	QString Test = TweaksIni.value(FirstSection + "/Name_" + Lang).toString();
	if (Test.isEmpty()) {
		Lang.truncate(Lang.lastIndexOf('_')); // Short version as fallback
		Test = TweaksIni.value(FirstSection + "/Name_" + Lang).toString();
	}
	if (Test.isEmpty())
		return;

	foreach(QString Section, Sections)
	{
		QString Name = TweaksIni.value(Section + "/Name_" + Lang).toString();
		if (!Name.isEmpty()) m_Translations[Section + "/Name"] = Name;
		//QString Description = TweaksIni.value(Section + "/Description_" + Lang).toString();
		//if (!Description.isEmpty()) m_Translations[Section + "/Description"] = Name;
	}
}