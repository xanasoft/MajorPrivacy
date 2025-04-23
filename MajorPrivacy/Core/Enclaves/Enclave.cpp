#include "pch.h"
#include "Enclave.h"
#include "../PrivacyCore.h"
#include "./Common/QtVariant.h"
#include "../Library/API/PrivacyAPI.h"
#include "../Library/Helpers/NtUtil.h"
#include "../../Helpers/WinHelper.h"
#include "../Driver/KSI/include/kphapi.h"
#include "../Library/API/PrivacyAPI.h"
#include "../MiscHelpers/Common/Common.h"

CEnclave::CEnclave(QObject* parent)
	: QObject(parent)
{

}

CEnclave* CEnclave::Clone() const
{
	CEnclave* pEnclave = new CEnclave();

	pEnclave->m_Guid = QUuid::createUuid().toString();
	pEnclave->m_bEnabled = m_bEnabled;

	pEnclave->m_Name = m_Name;
	//pEnclave->m_Grouping = m_Grouping;
	pEnclave->m_Description = m_Description;

	pEnclave->m_SignatureLevel = m_SignatureLevel;
	pEnclave->m_OnTrustedSpawn = m_OnTrustedSpawn;
	pEnclave->m_OnSpawn = m_OnSpawn;
	pEnclave->m_ImageLoadProtection = m_ImageLoadProtection;

	pEnclave->m_AllowDebugging = m_AllowDebugging;
	pEnclave->m_KeepAlive = m_KeepAlive;

	pEnclave->m_Data = m_Data;

	return pEnclave;
}

QtVariant CEnclave::ToVariant(const SVarWriteOpt& Opts) const
{
	QtVariantWriter Enclave;
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Enclave.BeginIndex();
		WriteIVariant(Enclave, Opts);
	} else {  
		Enclave.BeginMap();
		WriteMVariant(Enclave, Opts);
	}
	return Enclave.Finish();
}

NTSTATUS CEnclave::FromVariant(const class QtVariant& Enclave)
{
	if (Enclave.GetType() == VAR_TYPE_MAP)			QtVariantReader(Enclave).ReadRawMap([&](const SVarName& Name, const QtVariant& Data)	{ ReadMValue(Name, Data); });
	else if (Enclave.GetType() == VAR_TYPE_INDEX)	QtVariantReader(Enclave).ReadRawIndex([&](uint32 Index, const QtVariant& Data) { ReadIValue(Index, Data); });
	else
		return STATUS_UNKNOWN_REVISION;
	SetIconFile();
	return STATUS_SUCCESS;
}

void CEnclave::SetIconFile()
{
	if (!m_Icon.isNull())
		return;

	if (!GetIconFile().isEmpty())
		UpdateIconFile();

	if (m_Icon.availableSizes().isEmpty())
		m_Icon = QIcon(":/Icons/Enclave1.png");
}

void CEnclave::SetIconFile(const QString& IconFile) 
{ 
	SetData(API_S_ICON, IconFile);
	UpdateIconFile(); 
}

QString CEnclave::GetIconFile() const
{ 
	QString IconFile = GetData(API_S_ICON).AsQStr();
	return  IconFile;
}

void CEnclave::UpdateIconFile()
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

QString CEnclave::GetSignatureLevelStr(KPH_VERIFY_AUTHORITY SignAuthority)
{
	switch (SignAuthority) {
	case KphDevAuthority:	return tr("Developer Signed");			// Part of Majror Privacy
	case KphUserAuthority:	return tr("User Signed");				// Signed by the User Himself
	case KphMsAuthority:	return tr("Microsoft");					// Signed by Microsoft
	case KphAvAuthority:	return tr("Microsoft/AV");				// Signed by Microsoft or Antivirus
	case KphMsCodeAuthority:return tr("Microsoft Trusted");			// Signed by Microsoft or Code Root
	case KphUnkAuthority:	return tr("Unknown Root");
	case KphNoAuthority:	return tr("None");
	case KphUntestedAuthority: return tr("Undetermined");
	}
	return "Unknown";
}

QString CEnclave::GetOnSpawnStr(EProgramOnSpawn OnSpawn)
{
	switch (OnSpawn) {
	case EProgramOnSpawn::eAllow:	return tr("Allow");
	case EProgramOnSpawn::eBlock:	return tr("Block");
	case EProgramOnSpawn::eEject:	return tr("Eject");
	}
	return "Unknown";
}