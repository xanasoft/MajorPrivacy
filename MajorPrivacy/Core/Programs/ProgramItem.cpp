#include "pch.h"
#include "ProgramItem.h"
#include "../Processes/ProcessList.h"
#include "../PrivacyCore.h"
#include "../Library/Common/XVariant.h"
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
	if(!m_Icon.isNull())
		return;

	if (!m_IconFile.isEmpty())
	{
		QString Path = QString::fromStdWString(NtPathToDosPath(m_IconFile.toStdWString()));

		StrPair NameIndex = Split2(Path, ",", true);
		if (NameIndex.second.length() > 2) { // todo improve
			NameIndex.first = Path;
			NameIndex.second = "0";
		}
		QString Ext = Split2(NameIndex.first, ".", true).second;
		if (Ext.compare("exe", Qt::CaseInsensitive) == 0 || Ext.compare("dll", Qt::CaseInsensitive) == 0)
			m_Icon = LoadWindowsIconEx(NameIndex.first, NameIndex.second.toInt());
		else
			m_Icon = QIcon(QPixmap(Path));
	}

	if (m_Icon.availableSizes().isEmpty())
		m_Icon = DefaultIcon();
}

XVariant CProgramItem::ToVariant(const SVarWriteOpt& Opts) const
{
	XVariant Data;
	if (Opts.Format == SVarWriteOpt::eIndex) {
		Data.BeginIMap();
		WriteIVariant(Data, Opts);
	} else {  
		Data.BeginMap();
		WriteMVariant(Data, Opts);
	}
	Data.Finish();
	return Data;
}

NTSTATUS CProgramItem::FromVariant(const class XVariant& Data)
{
	if (Data.GetType() == VAR_TYPE_MAP)         Data.ReadRawMap([&](const SVarName& Name, const CVariant& Data) { ReadMValue(Name, Data); });
	else if (Data.GetType() == VAR_TYPE_INDEX)  Data.ReadRawIMap([&](uint32 Index, const CVariant& Data)        { ReadIValue(Index, Data); });
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