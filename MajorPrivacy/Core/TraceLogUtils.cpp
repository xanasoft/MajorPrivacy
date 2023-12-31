#include "pch.h"
#include "TraceLogUtils.h"
#include "../Library/Common/DbgHelp.h"

void MergeTraceLogs(SMergedLog* pLog, ETraceLogs Log, const QSet<CProgramFilePtr>& Programs, const QSet<CWindowsServicePtr>& ServicesEx)
{
	bool bReset = false;
	if (!pLog->States.isEmpty()) {
		if (Programs.count() + ServicesEx.count() != pLog->States.count())
			bReset = true;
		else
		{
			foreach(CProgramFilePtr pProgram, Programs) 
			{
				SMergedLog::SLogState& State = pLog->States[pProgram];

				const QVector<CLogEntryPtr>& Entries = pProgram->GetTraceLog(Log)->Entries;
				if(Entries.count() < State.LastCount) {
					bReset = true;
					break;
				}
			}

			foreach(CWindowsServicePtr pService, ServicesEx) 
			{
				SMergedLog::SLogState& State = pLog->States[pService];

				CProgramFilePtr pProgram = pService->GetProgramFile();
				if (!pProgram) continue;
		
				const QVector<CLogEntryPtr>& Entries = pProgram->GetTraceLog(Log)->Entries;
				if(Entries.count() < State.LastCount) {
					bReset = true;
					break;
				}
			}
		}
	}
	if (bReset) {
		pLog->MergeSeqNr++;
		pLog->LastCount = 0;
		pLog->States.clear();
		pLog->List.clear();
	}

#ifdef _DEBUG
    uint64 start = GetUSTickCount();
#endif
	
	auto AddToLog = [&](SMergedLog::SLogState& State, const CProgramFilePtr& pProgram, const QString& FilterTag = "") {

		const QVector<CLogEntryPtr>& Entries = pProgram->GetTraceLog(Log)->Entries;

		//int i = 0;
		//if (Entries.count() >= State.LastCount && State.LastCount > 0)
		//{
		//	i = State.LastCount - 1;
		//	if (State.LastID == Entries.at(i)->GetUID())
		//		i++;
		//	else
		//		i = 0;
		//}

		int i = State.LastCount;

		//
		// We insert a sorted list into an other sorted list,
		// we know that all new entries will be added after the last entry from the previouse merge
		//

		int j = pLog->LastCount;

		for (; i < Entries.count(); i++)
		{
			CLogEntryPtr pEntry = Entries.at(i);

			if (FilterTag.isEmpty() || pEntry->GetOwnerService().compare(FilterTag, Qt::CaseInsensitive) == 0)
			{
				// Find the correct position using binary search
				int low = j, high = pLog->List.count();
				while (low < high) {
					int mid = low + (high - low) / 2;
					if (pLog->List.at(mid).second->GetTimeStamp() > pEntry->GetTimeStamp())
						high = mid;
					else
						low = mid + 1;
				}
				j = low;

				pLog->List.insert(j++, qMakePair(pProgram, pEntry));
			}
		}

		State.LastCount = Entries.count();
		//if (State.LastCount)
		//	State.LastID = Entries.last()->GetUID();
	};

	foreach(CProgramFilePtr pProgram, Programs) 
	{
		SMergedLog::SLogState& State = pLog->States[pProgram];

		AddToLog(State, pProgram);
	}

	foreach(CWindowsServicePtr pService, ServicesEx) 
	{
		SMergedLog::SLogState& State = pLog->States[pService];

		CProgramFilePtr pProgram = pService->GetProgramFile();
		if (!pProgram) continue;
		
		AddToLog(State, pProgram, pService->GetSvcTag());
	}

	pLog->LastCount = pLog->List.count();

#ifdef _DEBUG
    DbgPrint("MergeTraceLogs %d%s took %d ms\r\n", Log, bReset ? " reset" : "", (GetUSTickCount() - start) / 1000);
#endif
}