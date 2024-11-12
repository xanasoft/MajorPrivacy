#include "pch.h"
#include "MajorPrivacy.h"
#include "Core\PrivacyCore.h"

#include <phnt_windows.h>
#include <phnt.h>

#include <QtWidgets/QApplication>

//#include "../NtCRT/NtCRT.h"
#include "../Library/Helpers/AppUtil.h"
#include "../Library/Helpers/NtUtil.h"
#include "../Library/Helpers/Reparse.h"



int main(int argc, char *argv[])
{
	srand(QDateTime::currentDateTimeUtc().toSecsSinceEpoch());

	//NTCRT_DEFINE(MyCRT);
	//InitGeneralCRT(&MyCRT);

	//std::wstring path = CNtPathMgr::Instance()->TranslateDosToNtPath(L"F:\\Projects\\MajorPrivacy");

	//CNtPathMgr::Instance()->InitDrives();
	//File_InitDrives(0xFFFFFFFF);
	//std::wstring testOld = File_TranslateTempLinks(path);

	//std::wstring testNt = CNtPathMgr::Instance()->TranslateTempLinks(path, false);

	/*std::wstring testNt = L"\\Device\\HarddiskVolume9\\MajorPrivacy\\MajorPrivacy\\x64\\Debug\\MajorPrivacy.exe";

	std::wstring testNew = CNtPathMgr::Instance()->TranslateTempLinks(testNt, true);

	std::wstring testDos = CNtPathMgr::Instance()->TranslateNtToDosPath(testNew);*/

	QString AppDir = QString::fromStdWString(GetApplicationDirectory());
	theConf = new CSettings(AppDir, "MajorPrivacy", "Xanasoft");

	theCore = new CPrivacyCore();

	QApplication App(argc, argv);
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
