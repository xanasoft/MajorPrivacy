#pragma once
#include "./Common/QtVariant.h"

#include "../Library/API/PrivacyDefs.h"

///////////////////////////////////////////////////////////////////////////////////////
// CAbstractTweak

class CAbstractTweak : public QObject
{
	Q_OBJECT
public:
	CAbstractTweak();

	virtual QString GetId() const { return m_Id; }
	virtual QString GetParentId() const { return m_ParentId; }
	void SetName(const QString& Name) { m_Name = Name; }
	virtual QString GetName() const { return m_Name; }
	virtual int GetIndex() const { return m_Index; }
	virtual ETweakType GetType() const { return m_Type; }
	virtual void SetStatus(ETweakStatus Status) { m_Status = Status; }
	virtual ETweakStatus GetStatus() const { return m_Status; }
	virtual QString GetTypeStr() const;
	virtual QString GetStatusStr() const;
	virtual QVariant GetStatusColor() const;
	virtual ETweakHint GetHint() const { return m_Hint; }
	virtual QString GetHintStr() const;
	virtual QString GetInfo() const { return ""; }

	virtual void FromVariant(const class QtVariant& Tweak);

protected:

	virtual void ReadIValue(uint32 Index, const QtVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const QtVariant& Data);

	QString m_Id;
	QString m_ParentId;
	QString m_Name;
	int m_Index = 0;
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

	virtual void AddTweak(const CTweakPtr& pTweak) { m_List.insert(pTweak->GetId(), pTweak); }
	virtual QMap<QString, CTweakPtr> GetList() const { return m_List; }

	virtual QString GetStatusStr() const;

protected:

	void ReadIValue(uint32 Index, const QtVariant& Data) override;
	void ReadMValue(const SVarName& Name, const QtVariant& Data) override;

	QMap<QString, CTweakPtr> m_List;
};

///////////////////////////////////////////////////////////////////////////////////////
// CTweakGroup

class CTweakGroup : public CTweakList
{
	Q_OBJECT
public:
	CTweakGroup() { m_Type = ETweakType::eGroup; }

	virtual QString GetStatusStr() const { return ""; }
	virtual ETweakStatus GetStatus() const { return ETweakStatus::eGroup; }

	virtual QIcon GetIcon() const;

protected:
	void ReadIValue(uint32 Index, const QtVariant& Data) override;
	void ReadMValue(const SVarName& Name, const QtVariant& Data) override;

	mutable QIcon m_Icon;
	mutable bool m_IconLoaded = false;
};

///////////////////////////////////////////////////////////////////////////////////////
// CTweakSet

class CTweakSet : public CTweakList
{
	Q_OBJECT
public:
	CTweakSet() { m_Type = ETweakType::eSet; }

protected:
	void ReadIValue(uint32 Index, const QtVariant& Data) override;
	void ReadMValue(const SVarName& Name, const QtVariant& Data) override;
};

///////////////////////////////////////////////////////////////////////////////////////
// CTweak

class CTweak : public CAbstractTweak
{
	Q_OBJECT
public:
	CTweak(ETweakType Type) { m_Type = Type; }

	virtual void SetParent(const QSharedPointer<CTweakList>& Parent) { m_Parent = Parent; }

	virtual ETweakHint GetHint() const;

	virtual QString GetInfo() const { return m_Info; }

	virtual void FromVariant(const class QtVariant& Tweak);

protected:

	void ReadIValue(uint32 Index, const QtVariant& Data) override;
	void ReadMValue(const SVarName& Name, const QtVariant& Data) override;

	QString m_WinVer;
	QString m_Info;
	QWeakPointer<CTweakList> m_Parent;	
};