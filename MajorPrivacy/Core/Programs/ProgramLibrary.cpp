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

QtVariant CProgramLibrary::ToVariant(const SVarWriteOpt& Opts) const
{
    QtVariantWriter Data;
    if (Opts.Format == SVarWriteOpt::eIndex) {
        Data.BeginIndex();
        WriteIVariant(Data, Opts);
    } else {  
        Data.BeginMap();
        WriteMVariant(Data, Opts);
    }
    return Data.Finish();
}

NTSTATUS CProgramLibrary::FromVariant(const class QtVariant& Data)
{
    if (Data.GetType() == VAR_TYPE_MAP)         QtVariantReader(Data).ReadRawMap([&](const SVarName& Name, const QtVariant& Data) { ReadMValue(Name, Data); });
    else if (Data.GetType() == VAR_TYPE_INDEX)  QtVariantReader(Data).ReadRawIndex([&](uint32 Index, const QtVariant& Data) { ReadIValue(Index, Data); });
    else
        return STATUS_UNKNOWN_REVISION;
    return STATUS_SUCCESS;
}

