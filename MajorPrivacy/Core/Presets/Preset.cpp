#include "pch.h"
#include "Preset.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Programs/ProgramID.h"
#include "../../Helpers/WinHelper.h"
#include "../MiscHelpers/Common/Common.h"

CPreset::CPreset()
{

}

CPreset* CPreset::Clone() const
{
	CPreset* pPreset = new CPreset();

	pPreset->m_Guid = QUuid::createUuid().toString();
	
	pPreset->m_Name = m_Name;
	pPreset->m_Description = m_Description;

	pPreset->m_Items = m_Items;

	pPreset->m_Data = m_Data;

	return pPreset;
}

QtVariant CPreset::ToVariant(const SVarWriteOpt& Opts) const
{
	QtVariantWriter Preset;
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Preset.BeginIndex();
		WriteIVariant(Preset, Opts);
	} else {  
		Preset.BeginMap();
		WriteMVariant(Preset, Opts);
	}
	return Preset.Finish();
}

bool CPreset::FromVariant(const class QtVariant& Data)
{
	if (Data.GetType() == VAR_TYPE_MAP)		QtVariantReader(Data).ReadRawMap([&](const SVarName& Name, const QtVariant& Data) { ReadMValue(Name, Data); });
	else if (Data.GetType() == VAR_TYPE_INDEX)	QtVariantReader(Data).ReadRawIndex([&](uint32 Index, const QtVariant& Data) { ReadIValue(Index, Data); });
	else
		return false;
	SetIconFile();
	return true;
}

void CPreset::SetIconFile()
{
	if (!m_Icon.isNull())
		return;

	if (!GetIconFile().isEmpty())
		UpdateIconFile();

	if (m_Icon.availableSizes().isEmpty())
		m_Icon = QIcon(":/Icons/Editor.png");
}

void CPreset::SetIconFile(const QString& IconFile) 
{ 
	SetData(API_S_ICON, IconFile);
	UpdateIconFile(); 
}

QString CPreset::GetIconFile() const
{ 
	QString IconFile = GetData(API_S_ICON).AsQStr();
	return  IconFile;
}

void CPreset::UpdateIconFile()
{
	QString Path = GetIconFile();
	int Index = 0;

	StrPair PathIndex = Split2(Path, ",", true);
	if (!PathIndex.second.isEmpty() && !PathIndex.second.contains(".")) {
		Path = PathIndex.first;
		Index = PathIndex.second.toInt();
	}
	QString Ext = Split2(Path, ".", true).second;
	if (Ext.compare("exe", Qt::CaseInsensitive) == 0 || Ext.compare("dll", Qt::CaseInsensitive) == 0)
		m_Icon = LoadWindowsIconEx(Path, Index);
	else
		m_Icon = QIcon(QPixmap(Path));
}
