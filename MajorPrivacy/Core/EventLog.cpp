#include "pch.h"
#include "EventLog.h"
#include "../../Library/Helpers/MiscHelpers.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../../Library/API/PrivacyAPIs.h"
#include "./Network/NetworkManager.h"
#include "./PrivacyCore.h"
#include <QJsonDocument>
#include <QJsonObject>


QtVariant CEventLogEntry::ToVariant(const SVarWriteOpt& Opts) const
{
    QtVariantWriter Data;
    if (Opts.Format == SVarWriteOpt::eIndex) {
        Data.BeginIndex();
        WriteIVariant(Data, Opts);
    } 
	//else {  
    //    Data.BeginMap();
    //    WriteMVariant(Data, Opts);
    //}
    return Data.Finish();
}

NTSTATUS CEventLogEntry::FromVariant(const class StVariant& Data)
{
	if (Data.GetType() == VAR_TYPE_INDEX)  StVariantReader(Data).ReadRawIndex([&](uint32 Index, const StVariant& Data) { ReadIValue(Index, Data); });
    //else if (Data.GetType() == VAR_TYPE_MAP)         StVariantReader(Data).ReadRawMap([&](const SVarName& Name, const StVariant& Data) { ReadMValue(Name, Data); });
    else
        return STATUS_UNKNOWN_REVISION;
    return STATUS_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////
// CEventLog

CEventLog::CEventLog(QObject* parent)
    :QObject(parent)
{
}

STATUS CEventLog::AddEntry(const QtVariant& Entry, bool bLive)
{
    CEventLogEntryPtr pEntry = CEventLogEntryPtr(new CEventLogEntry());
    NTSTATUS status = pEntry->FromVariant(Entry);
    if (!NT_SUCCESS(status))
        return ERR(status);
    m_Entries.append(pEntry);
    if (bLive) emit NewEntry(pEntry);
    return OK;
}

STATUS CEventLog::LoadEntries(const QtVariant& Entries)
{
    m_Entries.clear();
	StVariantReader(Entries).ReadRawList([&](const StVariant& Entry) {
        AddEntry(Entry, false);
	});
	return OK;
}

QString CEventLog::GetEventLevelStr(ELogLevels Level)
{
    switch (Level)
    {
    case ELogLevels::eInfo:			return tr("Info");
	case ELogLevels::eSuccess:		return tr("Success");
    case ELogLevels::eWarning:		return tr("Warning");
    case ELogLevels::eError:		return tr("Error");
    case ELogLevels::eCritical:		return tr("Critical");
    default:                        return tr("Unknown");
    }
}

QIcon CEventLog::GetEventLevelIcon(ELogLevels Level)
{
    switch (Level)
    {
    case ELogLevels::eInfo:			return QIcon(":/Icons/Info.png");
	case ELogLevels::eSuccess:		return QIcon(":/Icons/Approve2.png");
	case ELogLevels::eWarning:		return QIcon(":/Icons/Warning.png");
    case ELogLevels::eError:		return QIcon(":/Icons/Error.png");
    case ELogLevels::eCritical:		return QIcon(":/Icons/Critical.png");
    default:                        return QIcon(":/Icons/Info.png");
    }
}

QString CEventLog::GetEventInfoStr(const CEventLogEntryPtr& pEntry)
{
	QtVariant Data = pEntry->GetEventData();

	ELogEventType Type = (ELogEventType)pEntry->GetEventType();

    switch (Type)
    {
    case eLogFwModeChanged: {
		FwFilteringModes Mode = (FwFilteringModes)Data[API_V_FW_RULE_FILTER_MODE].To<sint32>();
        switch (Mode)
        {
        case FwFilteringModes::NoFiltering: return tr("Firewall Mode Changed to NO Filtering.");
        case FwFilteringModes::BlockList: return tr("Firewall Mode Changed to Block List.");
        case FwFilteringModes::AllowList: return tr("Firewall Mode Changed to Allow List.");
        default: return tr("Firewall Mode Changed to Unknown.");
        }
    }
    case eLogFwRuleAdded:
    case eLogFwRuleModified:
    case eLogFwRuleRemoved:
    case eLogFwRuleGenerated:
    case eLogFwRuleApproved:
    case eLogFwRuleRestored:
    case eLogFwRuleRejected:
		return GetFwRuleEventInfoStr(Type, Data);
		
    //case eLogResRuleAdded:
    //case eLogResRuleModified:
    //case eLogResRuleRemoved:

    //case eLogExecRuleAdded:
    //case eLogExecRuleModified:
    //case eLogExecRuleRemoved:
    case eLogExecStartBlocked:{
        QString Name = Data[API_V_NAME].To<QString>();
        return tr("Startup was blocked, Program '%1'").arg(Name);
    }

    case eLogProgramMissing: {
        QString Name = Data[API_V_NAME].To<QString>();
        return tr("Program no longer present '%1'").arg(Name);
    }

    case eLogProgramCleanedUp: {
        QString Name = Data[API_V_NAME].To<QString>();
        return tr("Removed no longer existign Program '%1'").arg(Name);
    }
                             
    case eLogProgramBlocked: {
        QString Name = Data[API_V_NAME].To<QString>();
        if(Data[API_V_SIGN_FLAGS].To<uint32>() & MP_VERIFY_FLAG_COHERENCY_FAIL)
            return tr("Program failed Coherency Check and was blocked '%1'").arg(Name);
        else
			return tr("Program had insificient Signature leven and was blocked '%1'").arg(Name);
    }

    case eLogScriptEvent: {
        QString Message = Data[API_V_DATA].To<QString>();
        return Message;
    }

    default: {
        QJsonDocument doc(QJsonValue::fromVariant(Data.ToQVariant()).toObject());			
        return tr("Unknown Event: %1").arg(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    }
    }
}

QString CEventLog::GetFwRuleEventInfoStr(ELogEventType Type, const QtVariant& Data)
{
	CFwRulePtr pRule = theCore->NetworkManager()->GetFwRule(Data[API_V_GUID].To<QString>());
    QString Name = Data[API_V_NAME].To<QString>();

    switch (Type)
    {
	case eLogFwRuleAdded:       return tr("Firewall Rule '%1' Added").arg(Name);
	case eLogFwRuleModified:    return tr("Firewall Rule '%1' Modified").arg(Name);
	case eLogFwRuleRemoved:     return tr("Firewall Rule '%1' Removed").arg(Name);
    case eLogFwRuleGenerated:   return tr("Firewall Rule '%1' Generated from Template").arg(Name);
    case eLogFwRuleApproved:    return tr("Firewall Rule '%1' Approved").arg(Name);
    case eLogFwRuleRestored:    return tr("Firewall Rule '%1' Restored").arg(Name);
    case eLogFwRuleRejected:    return tr("Firewall Rule '%1' Rejected").arg(Name);
    default:                    return tr("Firewall Rule '%1' Unknown Event").arg(Name);
    }
}