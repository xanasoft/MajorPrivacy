#pragma once

#include "../Programs/ProgramId.h"
#include "../Access/Handle.h"
#include "../Processes/Process.h"
#include "../Programs/ProgramRule.h"
#include "./Common/QtFlexGuid.h"

class CHash : public QObject
{
	Q_OBJECT
public:
	CHash(QObject* parent = NULL);
	CHash(EHashType Type, const QByteArray& Hash, const QString& Name, QObject* parent = NULL);

	CHash* Clone() const;

	QString GetName() const							{ return m_Name; }
	void SetName(const QString& Name)				{ m_Name = Name; }

	bool IsEnabled() const							{ return m_bEnabled; }
	void SetEnabled(bool bEnabled)					{ m_bEnabled = bEnabled; }

	bool IsTemporary() const						{ return m_bTemporary; }
	void SetTemporary(bool bTemporary)				{ m_bTemporary = bTemporary; }

	EHashType GetType() const						{ return m_Type; }

	QByteArray GetHash() const						{ return m_Hash; }

	void AddEnclave(const QFlexGuid& EnclaveId);
	void RemoveEnclave(const QFlexGuid& EnclaveId);
	bool HasEnclave(const QFlexGuid& EnclaveId) const;
	void AddCollection(const QString& Collection);
	QList<QString> GetCollections() const			{ return m_Collections; }


	void SetData(const char* pKey, const QtVariant& Value) { if (Value.IsValid()) m_Data[pKey] = Value; else m_Data.Remove(pKey); }
	QtVariant GetData(const char* pKey) const { return m_Data.Has(pKey) ? QtVariant(m_Data[pKey]) : QtVariant(); }

	QtVariant ToVariant(const SVarWriteOpt& Opts) const;
	NTSTATUS FromVariant(const class QtVariant& Hash);

protected:
	friend class CHashEntryWnd;

	void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const;
	void ReadIValue(uint32 Index, const QtVariant& Data);
	void ReadMValue(const SVarName& Name, const QtVariant& Data);

	QString					m_Name; // Filename or Subject
	QString					m_Description;
	bool 					m_bEnabled = true;
	bool 					m_bTemporary = false;
	EHashType				m_Type = EHashType::eUnknown;
	QByteArray				m_Hash;

	QList<QFlexGuid>		m_Enclaves; // Enclave Guids
	QList<QString>			m_Collections;

	QtVariant m_Data;
};

typedef QSharedPointer<CHash> CHashPtr;