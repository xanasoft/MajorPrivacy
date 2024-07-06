#pragma once
#include "../Library/Common/XVariant.h"
#include "../../Helpers/FilePath.h"
#include "../Library/API/PrivacyDefs.h"

class CProgramLibrary: public QObject
{
	Q_OBJECT

public:
	CProgramLibrary(QObject* parent = nullptr);

	static QIcon DefaultIcon();

	uint64 GetUID() const					{ return m_UID; }
	QString GetPath(EPathType Type) const	{ return m_Path.Get(Type); }
	QString GetName() const					{ return QFileInfo(m_Path.Get(EPathType::eWin32)).fileName(); }

	virtual XVariant ToVariant(const SVarWriteOpt& Opts) const;
	virtual NTSTATUS FromVariant(const XVariant& Data);

protected:

	virtual void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const XVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const XVariant& Data);

	uint64											m_UID;
	CFilePath										m_Path;
};

typedef QSharedPointer<CProgramLibrary> CProgramLibraryPtr;
