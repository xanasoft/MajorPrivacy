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

	/*std::wstring path = DosPathToNtPath(L"C:\\test2\\bla");

	//File_InitDrives(0xFFFFFFFF);
	//std::wstring testOld = File_TranslateTempLinks(path);

	std::wstring testNew = CNtPathMgr::Instance()->TranslateTempLinks(path);*/

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
