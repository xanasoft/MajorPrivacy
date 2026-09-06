#include "pch.h"
#include "EventLog.h"
#include "../../Library/Helpers/MiscHelpers.h"
#include "../../Library/API/PrivacyAPI.h"
#include "../../Library/API/PrivacyAPIs.h"
#include "./Network/NetworkManager.h"
#include "./Access/AccessManager.h"
#include "./Programs/ProgramManager.h"
#include "./Enclaves/EnclaveManager.h"
#include "./HashDB/HashDB.h"
#include "./Presets/PresetManager.h"
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
		
    case eLogDnsRuleAdded:
    case eLogDnsRuleModified:
    case eLogDnsRuleRemoved:
        return GetDnsRuleEventInfoStr(Type, Data);

    case eLogResRuleAdded:
    case eLogResRuleModified:
    case eLogResRuleRemoved:
		return GetResRuleEventInfoStr(Type, Data);

    case eLogSecureEnclaveAdded:
    case eLogSecureEnclaveModified:
    case eLogSecureEnclaveRemoved:
		return GetSecEnclaveEventInfoStr(Type, Data);

    case eLogExecRuleAdded:
    case eLogExecRuleModified:
    case eLogExecRuleRemoved:
		return GetExecRuleEventInfoStr(Type, Data);
    case eLogExecStartBlocked:{
        QString Name = Data[API_V_NAME].To<QString>();
        return tr("Startup was blocked, Program '%1'").arg(Name);
    }

    case eLogHashDbEntryAdded:
    case eLogHashDbEntryModified:
    case eLogHashDbEntryRemoved:
		return GetHashDbEventInfoStr(Type, Data);

    case eLogProgramAdded:
    case eLogProgramModified:
    case eLogProgramRemoved:
    case eLogProgramMissing: 
    case eLogProgramCleanedUp: 
    case eLogProgramBlocked:
		return GetProgramEventInfoStr(Type, Data);
    
    case eLogConfigPresetAdded:
    case eLogConfigPresetModified:
    case eLogConfigPresetRemoved:
		return GetPresetEventInfoStr(Type, Data);

    case eLogSvcConfigSaved:
        return tr("Privacy Agent Config Saved");
    case eLogSvcConfigDiscarded:
        return tr("Privacy Agent Config Discarded");
    case eLogDrvConfigSaved:
        return tr("Kernel Isolator Config Saved");
    case eLogDrvConfigDiscarded:
        return tr("Kernel Isolator Config Discarded");

    case eLogSvcStarted:
        return tr("Privacy Agent Started");
        

    case eLogScriptEvent: {
        QString Message = Data[API_V_DATA].To<QString>();
        return Message;
    }

    default: {
        QJsonDocument doc(QJsonValue::fromVariant(Data.ToQVariant()).toObject());			
        return tr("Unknown Event (%1): %2").arg(Type).arg(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    }
    }
}

QString CEventLog::GetFwRuleEventInfoStr(ELogEventType Type, const QtVariant& Data)
{
	//CFwRulePtr pRule = theCore->NetworkManager()->GetFwRule(Data[API_V_GUID].To<QString>());
    QString Name = Data[API_V_NAME].To<QString>();
    switch (Type)
    {
	case eLogFwRuleAdded:           return tr("Firewall Rule '%1' Added").arg(Name);
	case eLogFwRuleModified:        return tr("Firewall Rule '%1' Modified").arg(Name);
	case eLogFwRuleRemoved:         return tr("Firewall Rule '%1' Removed").arg(Name);
    case eLogFwRuleGenerated:       return tr("Firewall Rule '%1' Generated from Template").arg(Name);
    case eLogFwRuleApproved:        return tr("Firewall Rule '%1' Approved").arg(Name);
    case eLogFwRuleRestored:        return tr("Firewall Rule '%1' Restored").arg(Name);
    case eLogFwRuleRejected:        return tr("Firewall Rule '%1' Rejected").arg(Name);
    default:                        return tr("Firewall Rule '%1' Unknown Event").arg(Name);
    }
}

QString CEventLog::GetDnsRuleEventInfoStr(ELogEventType Type, const QtVariant& Data)
{
    //CDnsRulePtr pRule = theCore->NetworkManager()->GetDnsRuleByGuid(Data[API_V_GUID].To<QString>());
	QString Name = Data[API_V_NAME].To<QString>();
    switch (Type)
    {
    case eLogDnsRuleAdded:          return tr("DNS Rule '%1' Added").arg(Name);
    case eLogDnsRuleModified:       return tr("DNS Rule '%1' Modified").arg(Name);
    case eLogDnsRuleRemoved:        return tr("DNS Rule '%1' Removed").arg(Name);
    default:                        return tr("DNS Rule '%1' Unknown Event").arg(Name);
	}
}

QString CEventLog::GetResRuleEventInfoStr(ELogEventType Type, const QtVariant& Data)
{
    //CAccessRulePtr pRule = theCore->AccessManager()->GetAccessRuleByGuid(Data[API_V_GUID].To<QString>());
    QString Name = Data[API_V_NAME].To<QString>();
    switch (Type)
    {
    case eLogResRuleAdded:          return tr("Resource Access Rule '%1' Added").arg(Name);
	case eLogResRuleModified:       return tr("Resource Access Rule '%1' Modified").arg(Name);
	case eLogResRuleRemoved:        return tr("Resource Access Rule '%1' Removed").arg(Name);
	default:                        return tr("Resource Access Rule '%1' Unknown Event").arg(Name);
	}
}

QString CEventLog::GetSecEnclaveEventInfoStr(ELogEventType Type, const QtVariant& Data)
{
    //CEnclavePtr pEnclave = theCore->EnclaveManager()->GetEnclave(Data[API_V_GUID].To<QString>());
    QString Name = Data[API_V_NAME].To<QString>();
    switch (Type)
    {
    case eLogSecureEnclaveAdded:    return tr("Secure Enclave '%1' Added").arg(Name);
    case eLogSecureEnclaveModified: return tr("Secure Enclave '%1' Modified").arg(Name);
    case eLogSecureEnclaveRemoved:  return tr("Secure Enclave '%1' Removed").arg(Name);
    default:                        return tr("Secure Enclave '%1' Unknown Event").arg(Name);
	}
}

QString CEventLog::GetExecRuleEventInfoStr(ELogEventType Type, const QtVariant& Data)
{
	//CProgramRulePtr pRule = theCore->ProgramManager()->GetProgramRuleByGuid(Data[API_V_GUID].To<QString>());
    QString Name = Data[API_V_NAME].To<QString>();
    switch (Type)
    {
    case eLogExecRuleAdded:         return tr("Execution Rule '%1' Added").arg(Name);
    case eLogExecRuleModified:      return tr("Execution Rule '%1' Modified").arg(Name);
    case eLogExecRuleRemoved:       return tr("Execution Rule '%1' Removed").arg(Name);
    default:                        return tr("Execution Rule '%1' Unknown Event").arg(Name);
	}
}

QString CEventLog::GetHashDbEventInfoStr(ELogEventType Type, const QtVariant& Data)
{
	//CHashPtr pEntry = theCore->HashDB()->GetHash(
    QString Name = Data[API_V_NAME].To<QString>();
    switch (Type)
    {
    case eLogHashDbEntryAdded:      return tr("HashDB Entry '%1' Added").arg(Name);
    case eLogHashDbEntryModified:   return tr("HashDB Entry '%1' Modified").arg(Name);
    case eLogHashDbEntryRemoved:    return tr("HashDB Entry '%1' Removed").arg(Name);
    default:                        return tr("HashDB Entry '%1' Unknown Event").arg(Name);
    }
}

QString CEventLog::GetPresetEventInfoStr(ELogEventType Type, const QtVariant& Data)
{
	//CPresetPtr pPreset = theCore->PresetManager()->GetPreset(Data[API_V_GUID].To<QString>());
    QString Name = Data[API_V_NAME].To<QString>();
    switch (Type)
    {
    case eLogConfigPresetAdded:     return tr("Config Preset '%1' Added").arg(Name);
    case eLogConfigPresetModified:  return tr("Config Preset '%1' Modified").arg(Name);
    case eLogConfigPresetRemoved:   return tr("Config Preset '%1' Removed").arg(Name);
    default:                        return tr("Config Preset '%1' Unknown Event").arg(Name);
	}
}

QString CEventLog::GetProgramEventInfoStr(ELogEventType Type, const QtVariant& Data)
{
	//CProgramItemPtr pProgram = theCore->ProgramManager()->GetProgramByUID
    QString Name = Data[API_V_NAME].To<QString>();
    switch (Type)
    {
    case eLogProgramAdded:          return tr("Program '%1' Added").arg(Name);
    case eLogProgramModified:       return tr("Program '%1' Modified").arg(Name);
    case eLogProgramRemoved:        return tr("Program '%1' Removed").arg(Name);
    case eLogProgramMissing:        return tr("Program no longer present '%1'").arg(Name);
    case eLogProgramCleanedUp:      return tr("Removed no longer existign Program '%1'").arg(Name);
    case eLogProgramBlocked: {
        if(Data[API_V_SIGN_FLAGS].To<uint32>() & MP_VERIFY_FLAG_COHERENCY_FAIL)
            return tr("Program failed Coherency Check and was blocked '%1'").arg(Name);
        else
            return tr("Program had insificient Signature leven and was blocked '%1'").arg(Name);
    }
    default:                        return tr("Program '%1' Unknown Event").arg(Name);
	}
}