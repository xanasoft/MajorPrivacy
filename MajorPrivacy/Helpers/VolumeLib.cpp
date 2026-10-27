/*
 * Copyright 2026 David Xanatos, xanasoft.com
 *
 */

#include "pch.h"
#include "VolumeLib.h"

#include <QLibrary>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>

static const dcvol_api*	g_pApi = nullptr;
static bool				g_bTried = false;
static QString			g_Error;

const dcvol_api* CDcVolume::Api()
{
	if (g_bTried)
		return g_pApi;
	g_bTried = true;

	//
	// Next to the executable, and only there. QLibrary with a bare name would
	// walk the search path, and a DLL picked up from somewhere else is one that
	// gets handed a volume's master key - so the path is spelled out.
	//
	QString Path = QDir::toNativeSeparators(
		QCoreApplication::applicationDirPath() + "/" + QString::fromWCharArray(DCVOL_DLL_NAME));

	if (!QFileInfo::exists(Path)) {
		g_Error = QObject::tr("%1 was not found next to MajorPrivacy.exe.").arg(QString::fromWCharArray(DCVOL_DLL_NAME));
		return nullptr;
	}

	QLibrary* pLib = new QLibrary(Path);
	if (!pLib->load()) {
		g_Error = QObject::tr("%1 could not be loaded: %2").arg(QString::fromWCharArray(DCVOL_DLL_NAME)).arg(pLib->errorString());
		delete pLib;
		return nullptr;
	}

	dcvol_get_api_fn pGetApi = (dcvol_get_api_fn)pLib->resolve(DCVOL_GET_API_NAME);
	if (!pGetApi) {
		g_Error = QObject::tr("%1 does not export %2; it is not the library this build expects.")
			.arg(QString::fromWCharArray(DCVOL_DLL_NAME)).arg(DCVOL_GET_API_NAME);
		pLib->unload();
		delete pLib;
		return nullptr;
	}

	const dcvol_api* pApi = pGetApi(DCVOL_API_VERSION);
	if (!pApi || pApi->size != sizeof(dcvol_api)) {
		g_Error = QObject::tr("%1 implements a different interface version than this build of MajorPrivacy.")
			.arg(QString::fromWCharArray(DCVOL_DLL_NAME));
		pLib->unload();
		delete pLib;
		return nullptr;
	}

	//
	// The QLibrary is deliberately leaked. Everything the editor holds -
	// allocations, the function table itself - lives inside this module, so
	// unloading it while any of that is still reachable would be a use after
	// free. It stays for the life of the process, which is what a plugin that
	// owns memory has to do.
	//
	g_pApi = pApi;
	return g_pApi;
}

QString CDcVolume::Error()
{
	if (g_Error.isEmpty())
		return QObject::tr("The volume library is not available.");
	return g_Error;
}
