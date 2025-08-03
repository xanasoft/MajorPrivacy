#include "pch.h"
#include "VolumeManager.h"
#include "Core/PrivacyCore.h"
#include "Core/Programs/ProgramManager.h"
#include "../Library/API/PrivacyAPI.h"
#include "./Common/QtVariant.h"
#include "../../MiscHelpers/Common/Common.h"
#include "Core/Access/AccessManager.h"

CVolumeManager::CVolumeManager(QObject* parent)
 : QObject(parent)
{
	LoadVolumes();
}

CVolumeManager::~CVolumeManager()
{
	SaveVolumes();
}

void CVolumeManager::Update()
{
	auto Ret = theCore->GetVolumes();
	if (Ret.IsError())
		return;

	QtVariant& Volumes = Ret.GetValue();

	QMap<QString, CVolumePtr> OldMap = m_List;

	QtVariantReader(Volumes).ReadRawList([&](const FW::CVariant& vData) {
		const QtVariant& Volume = *(QtVariant*)&vData;

		QtVariant vImagePath(Volume.Allocator());
		FW::CVariantReader::Find(Volume, API_V_VOL_PATH, vImagePath);
		QString ImagePath = vImagePath.AsQStr().toLower();

		CVolumePtr pVolume = OldMap.take(ImagePath);
		if (pVolume.isNull())
		{
			pVolume = CVolumePtr(new CVolume());
			m_List.insert(ImagePath, pVolume);
		}
		if (!pVolume->IsMounted())
			pVolume->SetMounted(true);
		
		pVolume->FromVariant(Volume);
	});

	foreach(const CVolumePtr& pVolume, OldMap) {
		if (pVolume->IsMounted())
			pVolume->SetMounted(false);
	}
}

STATUS CVolumeManager::AddVolume(const QString& Path)
{
	if (Path.endsWith("\\")) 
	{
		CAccessRulePtr pRule = CAccessRulePtr(new CAccessRule(theCore->ProgramManager()->GetAll()->GetID()));
		pRule->SetType(EAccessRuleType::eProtect);
		pRule->SetEnabled(true);
		pRule->SetName(tr("%1 - Protection").arg(Path));
		pRule->SetPath(Path + "*");
		STATUS Status = theCore->AccessManager()->SetAccessRule(pRule);
		if(!Status)
			return Status;
	}

	CVolumePtr pVolume = CVolumePtr(new CVolume());
	pVolume->SetImagePath(Path);
	m_List.insert(Path.toLower(), pVolume);

	return OK;
}

STATUS CVolumeManager::RemoveVolume(const QString& Path)
{
	for (auto pRule : theCore->AccessManager()->GetAccessRules())
	{
		if (pRule->GetType() != EAccessRuleType::eProtect || pRule->IsVolumeRule())
			continue;

		QString CurPath = pRule->GetPath();
		if (CurPath.startsWith("\\") || !CurPath.endsWith("*") || CurPath.contains("/"))
			continue;
		CurPath.truncate(CurPath.length() - 1);

		if (Path.compare(CurPath, Qt::CaseInsensitive) == 0)
		{
			STATUS Status = theCore->AccessManager()->DelAccessRule(pRule);
			if(!Status)
				return Status;
		}
	}

	m_List.remove(Path.toLower());
	return OK;
}

bool CVolumeManager::LoadVolumes()
{
	foreach(const QString & Key, theConf->ListKeys("SecureVolumes"))
	{
		QString Data = theConf->GetValue("SecureVolumes/" + Key).toString();
		QStringList List = SplitStr(Data, "|");

		CVolumePtr pVolume = CVolumePtr(new CVolume());
		pVolume->SetImagePath(List[0].replace("/","\\"));
		if(List.size() >= 2) pVolume->SetName(List[1]);

		m_List.insert(pVolume->GetImagePath().toLower(), pVolume);
	}

	return true;
}

bool CVolumeManager::SaveVolumes()
{
	foreach(const QString & Key, theConf->ListKeys("SecureVolumes"))
		theConf->DelValue("SecureVolumes/" + Key);

	for(auto pVolume: m_List)
	{
		QString Data;
		Data = pVolume->GetImagePath() + "|" + pVolume->GetName();
		theConf->SetValue("SecureVolumes/Volume_" + QCryptographicHash::hash(pVolume->GetImagePath().toLatin1(), QCryptographicHash::Sha256).mid(0,16).toHex(), Data);
	}

	return true;
}

void CVolumeManager::UpdateProtectedFolders()
{
	QMap<CVolumePtr, bool> Folders;

	for (auto pRule : theCore->AccessManager()->GetAccessRules()) 
	{
		if (pRule->GetType() != EAccessRuleType::eProtect || pRule->IsVolumeRule())
			continue;

		QString Path = pRule->GetPath();
		if(Path.startsWith("\\") || !Path.endsWith("*") || Path.contains("/"))
			continue;
		Path.truncate(Path.length() - 1);

		CVolumePtr& pVolume = m_List[Path.toLower()];
		if (pVolume.isNull())
		{
			pVolume = CVolumePtr(new CVolume());
			pVolume->SetImagePath(Path);
		}
		Folders[pVolume] |= pRule->IsEnabled();
	}

	for(auto I = Folders.begin(); I != Folders.end(); ++I)
		I.key()->SetStatus(I.value() ? CVolume::eSecFolder : CVolume::eFolder);
}