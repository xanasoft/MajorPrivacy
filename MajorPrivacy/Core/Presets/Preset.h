#pragma once
#include "./Common/QtVariant.h"
#include "./Common/QtFlexGuid.h"

#include "../Library/API/PrivacyDefs.h"

///////////////////////////////////////////////////////////////////////////////////////
// CPreset

class CPreset : public QObject
{
	Q_OBJECT
public:
	CPreset();

	virtual CPreset* Clone() const;

	virtual QFlexGuid GetGuid() const				{ return m_Guid; }

	virtual QString GetName() const					{ return m_Name; }
	virtual void SetName(const QString& Name)		{ m_Name = Name; }

	virtual QString GetDescription() const			{ return m_Description; }

	virtual bool IsActive() const					{ return m_bIsActive; }

	virtual void SetIconFile(const QString& IconFile);
	virtual QString GetIconFile() const;
	virtual QIcon GetIcon() const					{ return m_Icon; }
	virtual void SetIcon(const QIcon& Icon)			{ m_Icon = Icon; }

	void SetData(const char* pKey, const QtVariant& Value) { if (Value.IsValid()) m_Data[pKey] = Value; else m_Data.Remove(pKey); }
	QtVariant GetData(const char* pKey) const { return m_Data.Has(pKey) ? QtVariant(m_Data[pKey]) : QtVariant(); }

	virtual QtVariant ToVariant(const SVarWriteOpt& Opts) const;
	virtual bool FromVariant(const class QtVariant& Preset);

protected:
	friend class CPresetWindow;

	virtual void WriteIVariant(QtVariantWriter& Data, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(QtVariantWriter& Data, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const QtVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const QtVariant& Data);


	void SetIconFile();
	void UpdateIconFile();

	QFlexGuid m_Guid;
	QString m_Name;
	QIcon m_Icon;
	QString m_Description;

	bool m_bIsActive = false;
	
	bool m_bUseScript = false;
	QString m_Script;

	QMap<QFlexGuid, SItemPreset> m_Items;

	QtVariant m_Data;
};

typedef QSharedPointer<CPreset> CPresetPtr;
