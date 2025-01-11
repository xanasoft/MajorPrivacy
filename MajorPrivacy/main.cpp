#include "pch.h"
#include "MajorPrivacy.h"
#include "Core\PrivacyCore.h"

#include <phnt_windows.h>
#include <phnt.h>

#include <QtWidgets/QApplication>

//#include "../NtCRT/NtCRT.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#ifdef _DEBUG
//#include "../Library/Helpers/Reparse.h"
#include "../Library/Helpers/NtpathMgr.h"
//#include "../../Library/Common/FlexGuid.h"
#endif



int main(int argc, char *argv[])
{
	srand(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

	//NTCRT_DEFINE(MyCRT);
	//InitGeneralCRT(&MyCRT);

#ifdef _DEBUG
	
/*	QFlexGuid guid;
	guid.FromQS("{12345678-1234-1234-1234-AbCdEFABcdEf}");
	QString Test = guid.ToQS();
	CVariant var = guid.ToVariant();
	QFlexGuid guid2;
	guid2.FromVariant(var);
	QString Test2 = guid2.ToQS();
	return 0;*/

	//std::wstring path = CPathReparse::Instance()->TranslateDosToNtPath(L"F:\\Projects\\MajorPrivacy");

	//CPathReparse::Instance()->InitDrives();
	//File_InitDrives(0xFFFFFFFF);
	//std::wstring testOld = File_TranslateTempLinks(path);

	//std::wstring testNt = CPathReparse::Instance()->TranslateTempLinks(path, false);

	//std::wstring testNt = L"\\Device\\HarddiskVolume9\\MajorPrivacy\\MajorPrivacy\\x64\\Debug\\MajorPrivacy.exe";

	//std::wstring testNew = CPathReparse::Instance()->TranslateTempLinks(testNt, true);

	//std::wstring testDos = CPathReparse::Instance()->TranslateNtToDosPath(testNew);

	//std::wstring testDos = CPathReparse::Instance()->TranslateNtToDosPath(testNt);

	std::wstring path = CNtPathMgr::Instance()->TranslateDosToNtPath(L"F:\\Projects\\MajorPrivacy\\MajorPrivacy\\x64\\Debug\\MajorPrivacy.exe");
	std::wstring path2 = CNtPathMgr::Instance()->TranslateDosToNtPath(L"F:\\Projects\\!Test\\Sandboxie\\Sandboxie\\SandboxiePlus\\x64\\Debug\\SandMan.exe");

	std::wstring test = CNtPathMgr::Instance()->TranslateNtToDosPath(L"\\Device\\HarddiskVolume9\\MajorPrivacy\\MajorPrivacy\\x64\\Debug\\MajorPrivacy.exe");
	std::wstring test2 = CNtPathMgr::Instance()->TranslateNtToDosPath(L"\\Device\\HarddiskVolume17\\Sandboxie\\Sandboxie\\SandboxiePlus\\x64\\Debug\\SandMan.exe"); 

#endif


	QString AppDir = QString::fromStdWString(GetApplicationDirectory());
	theConf = new CSettings(AppDir, "MajorPrivacy", "Xanasoft");

	QApplication App(argc, argv);

	theCore = new CPrivacyCore();

	CMajorPrivacy* pWnd = new CMajorPrivacy;
	pWnd->show();
	int ret = App.exec();
	delete pWnd;

	delete theCore;
	theCore = NULL;

	delete theConf;
	theConf = NULL;

	return ret;
}
