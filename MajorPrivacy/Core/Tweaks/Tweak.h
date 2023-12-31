#pragma once
#include "../Library/Common/XVariant.h"

enum class ETweakType
{
	eGroup = -1,
	eSet = 0,
	eReg,
	eGpo,
	eSvc,
	eTask,
	eFS,
	eExec,
	eFw,
	eUnknown
};

enum class ETweakStatus
{
	eGroup = -1,    // tweak is group
	eNotSet = 0,    // tweak is not set
	eApplied,       // tweak was not set by user but is applied
	eSet,           // tweak is set
	eMissing        // tweak was set but is not applied
};

enum class ETweakHint
{
	eNone = 0,
	eRecommended
};

///////////////////////////////////////////////////////////////////////////////////////
// CAbstractTweak

class CAbstractTweak : public QObject
{
	Q_OBJECT
public:
	CAbstractTweak();

	virtual QString GetName() const { return m_Name; }
	virtual ETweakType GetType() const { return m_Type; }
	virtual QString GetTypeStr() const;
	virtual ETweakStatus GetStatus() const { return m_Status; }
	virtual QString GetStatusStr() const;
	virtual QString GetInfo() const { return ""; }

	virtual void FromVariant(const class XVariant& Tweak);

protected:

	virtual void ReadValue(const SVarName& Name, const XVariant& Data);

	QString m_Name;
	ETweakType m_Type = ETweakType::eUnknown;
	ETweakHint m_Hint = ETweakHint::eNone;
	ETweakStatus m_Status = ETweakStatus::eNotSet;
};

typedef QSharedPointer<CAbstractTweak> CTweakPtr;

///////////////////////////////////////////////////////////////////////////////////////
// CTweakList

class CTweakList : public CAbstractTweak
{
	Q_OBJECT
public:

	virtual QMap<QString, CTweakPtr> GetList() const { return m_List; }

protected:

	virtual void ReadValue(const SVarName& Name, const XVariant& Data);

	QMap<QString, CTweakPtr> m_List;
};

///////////////////////////////////////////////////////////////////////////////////////
// CTweakGroup

class CTweakGroup : public CTweakList
{
	Q_OBJECT
public:
	CTweakGroup() { m_Type = ETweakType::eGroup; }

	virtual ETweakStatus GetStatus() const { return ETweakStatus::eGroup; }
};

///////////////////////////////////////////////////////////////////////////////////////
// CTweakSet

class CTweakSet : public CTweakList
{
	Q_OBJECT
public:
	CTweakSet() { m_Type = ETweakType::eSet; }
};

///////////////////////////////////////////////////////////////////////////////////////
// CTweak

class CTweak : public CAbstractTweak
{
	Q_OBJECT
public:

	virtual QString GetInfo() const { return m_Info; }

	virtual void FromVariant(const class XVariant& Tweak);

protected:

	virtual void ReadValue(const SVarName& Name, const XVariant& Data);

	QString m_Info;
};