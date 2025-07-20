#include "pch.h"
#include "WinHelper.h"
#include <QDebug>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtWin>
#else
#include <windows.h>
#endif

#include <shellapi.h>
#include <Shlwapi.h>
#include <Shlobj.h>


/*QVariantMap ResolveShortcut(const QString& LinkPath)
{
    QVariantMap Link;

    HRESULT hRes = E_FAIL;
    IShellLink* psl = NULL;

    // buffer that receives the null-terminated string
    // for the drive and path
    TCHAR szPath[0x1000];
    // structure that receives the information about the shortcut
    WIN32_FIND_DATA wfd;

    // Get a pointer to the IShellLink interface
    hRes = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl);

    if (SUCCEEDED(hRes))
    {
        // Get a pointer to the IPersistFile interface
        IPersistFile*  ppf     = NULL;
        psl->QueryInterface(IID_IPersistFile, (void **) &ppf);

        // Open the shortcut file and initialize it from its contents
        hRes = ppf->Load(LinkPath.toStdWString().c_str(), STGM_READ);
        if (SUCCEEDED(hRes))
        {
            hRes = psl->Resolve(NULL, SLR_NO_UI | SLR_NOSEARCH | SLR_NOUPDATE);
            if (SUCCEEDED(hRes))
            {
                // Get the path to the shortcut target
                hRes = psl->GetPath(szPath, ARRAYSIZE(szPath), &wfd, SLGP_RAWPATH);
                if (hRes == S_OK)
                    Link["Path"] = QString::fromWCharArray(szPath);
                else
                {
                    PIDLIST_ABSOLUTE pidl;
                    hRes = psl->GetIDList(&pidl);

                    if (SUCCEEDED(hRes)) 
                    {
                        LPWSTR url = nullptr;
                        SHGetNameFromIDList(pidl, SIGDN_URL, &url);
                    
                        if (url) 
                        {
                            QUrl Url = QString::fromWCharArray(url);

                            if (Url.isLocalFile())
                                Link["Path"] = Url.path().mid(1).replace("/", "\\");
                            else
                                Link["Path"] = Url.toString();
                        
                            CoTaskMemFree(url);
                        }

                        CoTaskMemFree(pidl);
                    }
                }

                hRes = psl->GetArguments(szPath, ARRAYSIZE(szPath));
                if (!FAILED(hRes))
                    Link["Arguments"] = QString::fromWCharArray(szPath);

                hRes = psl->GetWorkingDirectory(szPath, ARRAYSIZE(szPath));
                if (!FAILED(hRes))
				    Link["WorkingDir"] = QString::fromWCharArray(szPath);

				int IconIndex;
                hRes = psl->GetIconLocation(szPath, ARRAYSIZE(szPath), &IconIndex);
                if (FAILED(hRes))
                    return Link;
				Link["IconPath"] = QString::fromWCharArray(szPath);
				Link["IconIndex"] = IconIndex;

                // Get the description of the target
                hRes = psl->GetDescription(szPath, ARRAYSIZE(szPath));
                if (FAILED(hRes))
                    return Link;
                Link["Info"] = QString::fromWCharArray(szPath);
            }
        }
    }

    return Link;
}*/

QPixmap LoadWindowsIcon(const QString& Path, quint32 Index)
{
	std::wstring path = QString(Path).replace("/", "\\").toStdWString();
	HICON icon = ExtractIconW(NULL, path.c_str(), Index);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QPixmap Icon = QtWin::fromHICON(icon);
#else
	QPixmap Icon = QPixmap::fromImage(QImage::fromHICON(icon));
#endif
	DestroyIcon(icon);
	return Icon;
}

bool PickWindowsIcon(QWidget* pParent, QString& Path, quint32& Index)
{
	wchar_t iconPath[MAX_PATH] = { 0 };
	Path.toWCharArray(iconPath);
	BOOL Ret = PickIconDlg((HWND)pParent->window()->winId(), iconPath, MAX_PATH, (int*)&Index);
	Path = QString::fromWCharArray(iconPath);
	return !!Ret;
}

QIcon LoadWindowsIconEx(const QString &Path, quint32 Index) 
{
    std::wstring path = QString(Path).replace("/", "\\").toStdWString();
    QIcon icon;

    HICON hIconL = NULL;
    HICON hIconS = NULL;
    if (ExtractIconExW(path.c_str(), Index, &hIconL, &hIconS, 1) == 2)
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        icon.addPixmap(QtWin::fromHICON(hIconL));
        icon.addPixmap(QtWin::fromHICON(hIconS));
#else
        QImage imgL = QImage::fromHICON(hIconL);
        icon.addPixmap(QPixmap::fromImage(imgL));
        QImage imgS = QImage::fromHICON(hIconS);
        icon.addPixmap(QPixmap::fromImage(imgS));
#endif
        DestroyIcon(hIconL);
        DestroyIcon(hIconS);
    }

    return icon;
}

bool WindowsMoveFile(const QString& From, const QString& To)
{
    std::wstring from = QString(From).replace("/", "\\").toStdWString();
    from.append(L"\0", 1);
    std::wstring to = QString(To).replace("/", "\\").toStdWString();
    to.append(L"\0", 1);

    SHFILEOPSTRUCTW SHFileOp;
    memset(&SHFileOp, 0, sizeof(SHFILEOPSTRUCT));
    SHFileOp.hwnd = NULL;
    SHFileOp.wFunc = To.isEmpty() ? FO_DELETE : FO_MOVE;
    SHFileOp.pFrom = from.c_str();
    SHFileOp.pTo = to.c_str();
    SHFileOp.fFlags = NULL;    

    //The Copying Function
    return SHFileOperationW(&SHFileOp) == 0;
}

bool OpenFileProperties(const QString& Path) 
{
    std::wstring path = Path.toStdWString();

    // Initialize the SHELLEXECUTEINFO structure
    SHELLEXECUTEINFOW sei = { 0 };
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.fMask = SEE_MASK_INVOKEIDLIST;
    sei.hwnd = NULL; // Parent window handle, can be NULL
    sei.lpVerb = L"properties"; // Action to perform
    sei.lpFile = path.c_str(); // File path
    sei.nShow = SW_SHOW; // Show the dialog

    // Execute the command
    return ShellExecuteExW(&sei) == 0;
}

bool OpenFileFolder(const QString& Path)
{
    std::wstring command = L"/select,\"" + Path.toStdWString() + L"\"";

    // Execute the command
    return ShellExecuteW(NULL, L"open", L"explorer.exe", command.c_str(), NULL, SW_SHOWNORMAL) == 0;
}

HANDLE ShellExecuteQ(const QString& Command)
{
    QString File;
    QString Params;
    if (Command.left(1) == "\"") {
        int Pos = Command.indexOf("\"", 1);
        if (Pos == -1)
            return INVALID_HANDLE_VALUE;
        File = Command.mid(1, Pos - 1);
        Params = Command.mid(Pos + 2);
    }
    else {
        int Pos = Command.indexOf(" ");
        if (Pos == -1)
            File = Command;
        else {
            File = Command.left(Pos);
            Params = Command.mid(Pos + 1);
        }
    }

    SHELLEXECUTEINFOW shex;
    memset(&shex, 0, sizeof(SHELLEXECUTEINFOW));
    shex.cbSize = sizeof(SHELLEXECUTEINFO);
    shex.fMask = SEE_MASK_NOCLOSEPROCESS;
    shex.hwnd = NULL;
    shex.lpFile = (wchar_t*)File.utf16();
    shex.lpParameters = (wchar_t*)File.utf16();
    shex.nShow = SW_SHOWNORMAL;

    if(ShellExecuteExW(&shex))
		return shex.hProcess;
	return INVALID_HANDLE_VALUE;
}