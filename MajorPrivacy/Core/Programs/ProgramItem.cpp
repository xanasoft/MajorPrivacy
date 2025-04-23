#include "pch.h"
#include "ProgramItem.h"
#include "../Processes/ProcessList.h"
#include "../PrivacyCore.h"
#include "./Common/QtVariant.h"
#include "../Library/API/PrivacyAPI.h"
#include "../MiscHelpers/Common/Common.h"
#include "../Network/NetworkManager.h"
#include "../Library/Helpers/NtUtil.h"
#include "../../Helpers/WinHelper.h"


CProgramItem::CProgramItem(QObject* parent)
	: QObject(parent)
{
	m_UID = 0;
}

QIcon CProgramItem::DefaultIcon() const
{
	QIcon ExeIcon(":/exe16.png");
	ExeIcon.addFile(":/exe32.png");
	ExeIcon.addFile(":/exe48.png");
	ExeIcon.addFile(":/exe64.png");
	//return QIcon(":/Icons/Process.png");
	return ExeIcon;
}

void CProgramItem::SetIconFile()
{
	if (!m_Icon.isNull())
		return;

	if (!GetIconFile().isEmpty())
		UpdateIconFile();

	if (m_Icon.availableSizes().isEmpty())
		m_Icon = DefaultIcon();
}

void CProgramItem::UpdateIconFile()
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

QtVariant CProgramItem::ToVariant(const SVarWriteOpt& Opts) const
{
	QtVariantWriter Data;
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Data.BeginIndex();
		WriteIVariant(Data, Opts);
	} else {  
		Data.BeginMap();
		WriteMVariant(Data, Opts);
	}
	return Data.Finish();
}

NTSTATUS CProgramItem::FromVariant(const class QtVariant& Data)
{
	if (Data.GetType() == VAR_TYPE_MAP)         QtVariantReader(Data).ReadRawMap([&](const SVarName& Name, const QtVariant& Data) { ReadMValue(Name, Data); });
	else if (Data.GetType() == VAR_TYPE_INDEX)  QtVariantReader(Data).ReadRawIndex([&](uint32 Index, const QtVariant& Data) { ReadIValue(Index, Data); });
	else
		return STATUS_UNKNOWN_REVISION;
	SetIconFile();
	return STATUS_SUCCESS;
}

QString CProgramItem::GetTypeStr() const
{
	QString Type;

	switch (GetType())
	{
	case EProgramType::eProgramFile:		Type = tr("Executable File"); break;
	case EProgramType::eFilePattern:		Type = tr("File Path Pattern"); break;
	case EProgramType::eAppInstallation:	Type = tr("Installation"); break;
	case EProgramType::eWindowsService:		Type = tr("Windows Service"); break;
	case EProgramType::eAppPackage:			Type = tr("Application Package"); break;

	case EProgramType::eProgramSet:			Type = tr("Program Set"); break;
	case EProgramType::eProgramList:		Type = tr("Program List"); break;
	case EProgramType::eProgramGroup:		Type = tr("Program Group"); break;
	case EProgramType::eAllPrograms:		Type = tr("All Programs"); break;
	//case EProgramType::eProgramRoot:		Type = tr("Program Root"); break;

	default:								Type = tr("Unknown"); break;		
	}

	return Type;
}