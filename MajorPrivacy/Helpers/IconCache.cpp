#include "pch.h"
#include <QFileIconProvider>
#include "../MiscHelpers/Common/Common.h"
#include "../MiscHelpers/Common/OtherFunctions.h"
#include "IconCache.h"
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <vector>

QString g_IconCachePath;
QMap<QString, QIcon> g_IconCache;

// Helper function to convert HICON to QPixmap
QPixmap qt_fromHICON(HICON icon)
{
    if (!icon)
        return QPixmap();

    ICONINFO iconInfo = { 0 };
    if (!GetIconInfo(icon, &iconInfo))
        return QPixmap();

    BITMAP bmp;
    if (!GetObject(iconInfo.hbmColor, sizeof(BITMAP), &bmp)) {
        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);
        return QPixmap();
    }

    int width = bmp.bmWidth;
    int height = bmp.bmHeight;

    HDC hDC = GetDC(nullptr);

    BITMAPINFOHEADER bi = { 0 };
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = height; // Bottom-up DIB
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    std::vector<BYTE> imageData(width * height * 4);

    if (!GetDIBits(hDC, iconInfo.hbmColor, 0, height, imageData.data(), reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS)) {
        ReleaseDC(nullptr, hDC);
        DeleteObject(iconInfo.hbmColor);
        DeleteObject(iconInfo.hbmMask);
        return QPixmap();
    }

    ReleaseDC(nullptr, hDC);

    QImage image(reinterpret_cast<uchar*>(imageData.data()), width, height, QImage::Format_ARGB32);
    image = image.mirrored(); // Flip the image vertically

    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);

    return QPixmap::fromImage(image);
}

// Function to get the icon associated with a file extension
QIcon getFileExtensionIcon(const QString& extension)
{
    // Create a temporary file name with the given extension
    QString tempFileName = "dummy." + extension;

    SHFILEINFOW sfi = { 0 };
    DWORD_PTR hr = SHGetFileInfoW(reinterpret_cast<const wchar_t*>(tempFileName.utf16()),
        FILE_ATTRIBUTE_NORMAL,
        &sfi,
        sizeof(sfi),
        SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_LARGEICON | SHGFI_SYSICONINDEX);

    if (sfi.hIcon == nullptr) {
        return QIcon(); // Failed to get icon
    }

    // Get the default unknown file icon's index
    /*SHFILEINFOW unknownSfi = { 0 };
    DWORD_PTR unknownHr = SHGetFileInfoW(L"Unknown", FILE_ATTRIBUTE_NORMAL, &unknownSfi, sizeof(unknownSfi),
        SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_LARGEICON | SHGFI_SYSICONINDEX);

    if (unknownSfi.hIcon == nullptr) {
        DestroyIcon(sfi.hIcon); // Clean up the icon we obtained earlier
        return QIcon(); // Failed to get default file icon
    }

    // Compare the icon indices
    if (sfi.iIcon == unknownSfi.iIcon) {
        // The icons are the same, so we can consider this as no specific icon
        DestroyIcon(sfi.hIcon);
        DestroyIcon(unknownSfi.hIcon);
        qDebug() << "No Icon for extension:" << extension;
        return QIcon();
    }*/

    // Create QIcon from HICON
    QIcon finalIcon;
    QList<QSize> sizes = { QSize(16, 16), QSize(32, 32)/*, QSize(48, 48), QSize(64, 64), QSize(256, 256)*/ };

    for (const QSize& size : sizes) {
        HICON hIconSized = (HICON)CopyImage(sfi.hIcon, IMAGE_ICON, size.width(), size.height(), LR_COPYFROMRESOURCE);
        if (hIconSized) {
            QPixmap pixmap = qt_fromHICON(hIconSized);
            if (!pixmap.isNull()) {
                finalIcon.addPixmap(pixmap);
            }
            DestroyIcon(hIconSized);
        }
    }

    // Clean up
    DestroyIcon(sfi.hIcon);
    //DestroyIcon(unknownSfi.hIcon);

    return finalIcon.isNull() ? QIcon() : finalIcon;
}

// Function to get the default file icon (unknown file icon)
QIcon getDefaultFileIcon()
{
    SHFILEINFOW sfi = { 0 };
    DWORD_PTR hr = SHGetFileInfoW(L"Unknown", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi),
        SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_LARGEICON | SHGFI_SYSICONINDEX);

    if (sfi.hIcon == nullptr) {
        return QIcon(); // Failed to get default file icon
    }

    // Create QIcon from HICON
    QIcon finalIcon;
    QList<QSize> sizes = { QSize(16, 16), QSize(32, 32), QSize(48, 48), QSize(256, 256) };

    for (const QSize& size : sizes) {
        HICON hIconSized = (HICON)CopyImage(sfi.hIcon, IMAGE_ICON, size.width(), size.height(), LR_COPYFROMRESOURCE);
        if (hIconSized) {
            QPixmap pixmap = qt_fromHICON(hIconSized);
            if (!pixmap.isNull()) {
                finalIcon.addPixmap(pixmap);
            }
            DestroyIcon(hIconSized);
        }
    }

    // Clean up
    DestroyIcon(sfi.hIcon);

    return finalIcon;
}

// Function to get the folder icon
QIcon getFolderIcon()
{
    SHFILEINFOW sfi = { 0 };
    DWORD_PTR hr = SHGetFileInfoW(L"Folder", FILE_ATTRIBUTE_DIRECTORY, &sfi, sizeof(sfi),
        SHGFI_USEFILEATTRIBUTES | SHGFI_ICON | SHGFI_LARGEICON | SHGFI_SYSICONINDEX);

    if (sfi.hIcon == nullptr) {
        return QIcon(); // Failed to get folder icon
    }

    // Create QIcon from HICON
    QIcon finalIcon;
    QList<QSize> sizes = { QSize(16, 16), QSize(32, 32), QSize(48, 48), QSize(256, 256) };

    for (const QSize& size : sizes) {
        HICON hIconSized = (HICON)CopyImage(sfi.hIcon, IMAGE_ICON, size.width(), size.height(), LR_COPYFROMRESOURCE);
        if (hIconSized) {
            QPixmap pixmap = qt_fromHICON(hIconSized);
            if (!pixmap.isNull()) {
                finalIcon.addPixmap(pixmap);
            }
            DestroyIcon(hIconSized);
        }
    }

    // Clean up
    DestroyIcon(sfi.hIcon);

    return finalIcon;
}

//void MakeFileIcon(const QString& Ext)
//{
//	QString CachePath = g_IconCachePath;
//	CreateDir(CachePath);
//
//	QIcon Icon;
//	if(Ext == ".")
//		Icon = QFileIconProvider().icon(QFileIconProvider::Folder);
//	else
//	{
//		QFile TmpFile(CachePath + "tmp." + Ext);
//		TmpFile.open(QIODevice::WriteOnly);
//
//		Icon = QFileIconProvider().icon(QFileInfo(TmpFile.fileName()));
//
//		TmpFile.close();
//		TmpFile.remove();
//	}
//
//	if (Icon.isNull())
//		return;
//
//	int Sizes[2] = {16,32};
//	for(int i=0; i<ARRAYSIZE(Sizes); i++)
//	{
//		QFile IconFile(CachePath + Ext + QString::number(Sizes[i]) + ".png");
//		IconFile.open(QIODevice::WriteOnly);
//		Icon.pixmap(Sizes[i], Sizes[i]).save(&IconFile, "png");
//		IconFile.close();
//	}
//}

QString sanitizeFileName(const QString& fileName) 
{
	QRegExp validPattern("[^<>:\"/\\|?*]+");
	if (validPattern.indexIn(fileName) != -1)
		return validPattern.cap(0);
	return fileName;
}

bool IsSameIcon(const QIcon& Icon1, const QIcon& Icon2, int size = 16)
{
    QImage Image1 = Icon1.pixmap(size, size).toImage();
    QImage Image2 = Icon2.pixmap(size, size).toImage();
	if (Image1.size() != Image2.size())
		return false;
	for (int y = 0; y < Image1.height(); y++)
	{
		for (int x = 0; x < Image1.width(); x++)
		{
			if (Image1.pixel(x, y) != Image2.pixel(x, y))
				return false;
		}
	}
	return true;
}

QIcon GetFileIcon(const QString& FileName, int Size)
{
	if (g_IconCache.isEmpty())
	{
		if (g_IconCachePath.isEmpty())
            g_IconCachePath = QDir::tempPath() + "/IconCache/";
        CreateDir(g_IconCachePath);

		g_IconCache[""] = getDefaultFileIcon();
        g_IconCache["."] = getFolderIcon();
	}

	StrPair PathName = Split2(FileName, "\\", true);
	QString Name = sanitizeFileName(PathName.second.isEmpty() ? FileName : PathName.second);

	QString Ext = Split2(Name, ".", true).second;
	if (Ext.isEmpty())
		Ext = "file";

	if(!g_IconCache.contains(Ext))
	{
		QString IconPath = g_IconCachePath + Ext + QString::number(Size) + ".png";
		if(QFile::exists(IconPath))
            g_IconCache[Ext] = QIcon(IconPath);
        else
		{
            QIcon Icon = getFileExtensionIcon(Ext);
            if (Icon.isNull() || IsSameIcon(Icon, g_IconCache[""]))
				g_IconCache[Ext] = g_IconCache[""]; // don't cache unknown file icon
            else
            {
                g_IconCache[Ext] = Icon;

                int Sizes[2] = { 16, 32 };
                for (int i = 0; i < ARRAYSIZE(Sizes); i++)
                {
                    QFile IconFile(g_IconCachePath + Ext + QString::number(Sizes[i]) + ".png");
                    IconFile.open(QIODevice::WriteOnly);
                    Icon.pixmap(Sizes[i], Sizes[i]).save(&IconFile, "png");
                    IconFile.close();
                }
            }
		}
	}

	return g_IconCache[Ext];
}