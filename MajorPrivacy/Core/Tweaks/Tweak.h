#pragma once
#include "../Library/Common/XVariant.h"

#include "../Library/API/PrivacyDefs.h"

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

	virtual void ReadIValue(uint32 Index, const XVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const XVariant& Data);

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

	void ReadIValue(uint32 Index, const XVariant& Data) override;
	void ReadMValue(const SVarName& Name, const XVariant& Data) override;

	void ReadList(const XVariant& List);

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
	CTweak(ETweakType Type) { m_Type = Type; }

	virtual QString GetInfo() const { return m_Info; }

	virtual void FromVariant(const class XVariant& Tweak);

protected:

	void ReadIValue(uint32 Index, const XVariant& Data) override;
	void ReadMValue(const SVarName& Name, const XVariant& Data) override;

	QString m_Info;
};