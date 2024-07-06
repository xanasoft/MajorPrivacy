#include "pch.h"
#include "ProgramLibrary.h"
#include "../Library/API/PrivacyAPI.h"

CProgramLibrary::CProgramLibrary(QObject* parent)
	: QObject(parent)
{

}

QIcon CProgramLibrary::DefaultIcon()
{
    static QIcon DllIcon;
    if(DllIcon.isNull())
    {
		DllIcon.addFile(":/dll16.png");
		DllIcon.addFile(":/dll32.png");
		DllIcon.addFile(":/dll48.png");
		DllIcon.addFile(":/dll64.png");
	}
    //return QIcon(":/Icons/Process.png");
	return DllIcon;
}

XVariant CProgramLibrary::ToVariant(const SVarWriteOpt& Opts) const
{
    XVariant Data;
    if (Opts.Format == SVarWriteOpt::eIndex) {
        Data.BeginIMap();
        WriteIVariant(Data, Opts);
    } else {  
        Data.BeginMap();
        WriteMVariant(Data, Opts);
    }
    Data.Finish();
    return Data;
}

NTSTATUS CProgramLibrary::FromVariant(const class XVariant& Data)
{
    if (Data.GetType() == VAR_TYPE_MAP)         Data.ReadRawMap([&](const SVarName& Name, const CVariant& Data) { ReadMValue(Name, Data); });
    else if (Data.GetType() == VAR_TYPE_INDEX)  Data.ReadRawIMap([&](uint32 Index, const CVariant& Data)        { ReadIValue(Index, Data); });
    else
        return STATUS_UNKNOWN_REVISION;
    return STATUS_SUCCESS;
}

