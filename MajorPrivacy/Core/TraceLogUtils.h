#pragma once
#include "TraceLogEntry.h"
#include "Programs/WindowsService.h"

struct SMergedLog {

	struct SLogState {
		//QVariant LastID;
		int LastCount = 0;
	};

	QMap<CProgramItemPtr, SLogState> States;

	QVector<QPair<CProgramFilePtr, CLogEntryPtr>> List;

	quint64 MergeSeqNr = 0;
	int LastCount = 0;
};

void MergeTraceLogs(SMergedLog* pLog, ETraceLogs Log, const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& ServicesEx = QSet<CWindowsServicePtr>());