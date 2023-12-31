#include "pch.h"
#include "ProgramItem.h"
#include "../ProcessList.h"
#include "../PrivacyCore.h"
#include "../Library/Common/XVariant.h"
#include "../Service/ServiceAPI.h"
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

void CProgramItem::SetIconFile(const QString& IconFile)
{
	if (m_IconFile == IconFile && !m_Icon.isNull())
		return;
	m_IconFile = IconFile;

	if (!IconFile.isEmpty())
	{
		QString Path = QString::fromStdWString(NtPathToDosPath(IconFile.toStdWString()));

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

void CProgramItem::FromVariant(const class XVariant& Item)
{
	Item.ReadRawMap([&](const SVarName& Name, const CVariant& vData) { ReadValue(Name, *(XVariant*)&vData); });
}

void CProgramItem::ReadValue(const SVarName& Name, const XVariant& Data)
{
		 if (VAR_TEST_NAME(Name, SVC_API_PROG_NAME))		m_Name = Data.AsQStr();

	else if (VAR_TEST_NAME(Name, SVC_API_PROG_ICON))		SetIconFile(Data.AsQStr());
	else if (VAR_TEST_NAME(Name, SVC_API_PROG_INFO))		m_Info = Data.AsQStr();

	else if (VAR_TEST_NAME(Name, SVC_API_PROG_FW_RULES))	m_FwRuleIDs = Data.AsQSet();
}