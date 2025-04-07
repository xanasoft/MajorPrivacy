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
	switch (GetType())
	{
	case EProgramType::eProgramFile:		return tr("Executable File");
	case EProgramType::eFilePattern:		return tr("File Path Pattern");
	case EProgramType::eAppInstallation:	return tr("Installation");
	case EProgramType::eWindowsService:		return tr("Windows Service");
	case EProgramType::eAppPackage:			return tr("Application Package");

	case EProgramType::eProgramSet:			return tr("Program Set");
	case EProgramType::eProgramList:		return tr("Program List");
	case EProgramType::eProgramGroup:		return tr("Program Group");
	case EProgramType::eAllPrograms:		return tr("All Programs");
	//case EProgramType::eProgramRoot:		return tr("Program Root");
	}
	return tr("Unknown");
}