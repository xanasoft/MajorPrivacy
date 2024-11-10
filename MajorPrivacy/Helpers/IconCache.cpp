#include "pch.h"
#include <QFileIconProvider>
#include "../MiscHelpers/Common/OtherFunctions.h"
#include "IconCache.h"

QString g_IconCachePath;

void MakeFileIcon(const QString& Ext)
{
	if(Ext.length() > 10)
		return;

	QString CachePath = g_IconCachePath;
	CreateDir(CachePath);

	QIcon Icon;
	if(Ext == ".")
		Icon = QFileIconProvider().icon(QFileIconProvider::Folder);
	else
	{
		QFile TmpFile(CachePath + "tmp." + Ext);
		TmpFile.open(QIODevice::WriteOnly);

		Icon = QFileIconProvider().icon(QFileInfo(TmpFile.fileName()));

		TmpFile.close();
		TmpFile.remove();
	}

	int Sizes[2] = {16,32};
	for(int i=0; i<ARRAYSIZE(Sizes); i++)
	{
		QFile IconFile(CachePath + Ext + QString::number(Sizes[i]) + ".png");
		IconFile.open(QIODevice::WriteOnly);
		Icon.pixmap(Sizes[i], Sizes[i]).save(&IconFile, "png");
		IconFile.close();
	}
}

QIcon GetFileIcon(const QString& Ext, int Size)
{
	static QMap<QString, QIcon> m_Icons;

	if(!m_Icons.contains(Ext))
	{
		QString IconPath;
		if(Ext == "....")
			IconPath = ":/Icons/Collection";
		else if(Ext == "...")
			IconPath = ":/Icons/Multi";
		else
		{
			IconPath = g_IconCachePath + Ext + QString::number(Size) + ".png";
			if(!QFile::exists(IconPath))
			{
				MakeFileIcon(Ext);
				//IconPath = ":/Icon" + QString::number(Size) + ".png";
			}
		}
		m_Icons[Ext] = QIcon(IconPath);
	}
	return m_Icons[Ext];
}