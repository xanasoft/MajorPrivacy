#pragma once

//#include "../Process.h"
#include "ProgramID.h"

struct SProgramStats
{
	int				ProcessCount = 0;
	
	int				FwRuleCount = 0;
	int				SocketCount = 0;

	uint64			LastActivity = 0;

	uint64			Upload = 0;
	uint64			Download = 0;
	uint64			Uploaded = 0;
	uint64			Downloaded = 0;
};

class CProgramItem: public QObject
{
	Q_OBJECT
public:
	CProgramItem(QObject* parent = nullptr);

	void SetUID(quint64 UID) 						{ m_UID = UID; }
	quint64 GetUID() const							{ return m_UID; }
	void SetID(const CProgramID& ID) 				{ m_ID = ID; }
	const CProgramID& GetID() const					{ return m_ID; }

	void SetName(const QString& Name)				{ m_Name = Name; }
	virtual QString GetName() const					{ return m_Name; }
	virtual QString GetNameEx() const				{ return m_Name; }
	virtual QStringList GetCategories() const		{ return m_Categories; }
	virtual QIcon DefaultIcon() const;
	void SetIconFile(const QString& IconFile);
	virtual QIcon GetIcon() const					{ return m_Icon; }
	virtual QString GetInfo() const					{ return m_Info; }
	virtual QString GetPath() const					{ return ""; }

	virtual int GetFwRuleCount() const				{ return m_FwRuleIDs.count(); }
	virtual QSet<QString> GetFwRules() const		{ return m_FwRuleIDs; }

	virtual void CountStats() = 0;
	virtual const SProgramStats* GetStats()			{ return &m_Stats; }


	virtual void FromVariant(const class XVariant& Item);

protected:
	friend class CProgramManager;

	virtual void ReadValue(const SVarName& Name, const XVariant& Data);

	quint64								m_UID = 0;
	CProgramID							m_ID;
	QString								m_Name;
	QStringList							m_Categories;
	QString								m_IconFile;
	QIcon								m_Icon;
	QString								m_Info;

	QList<QWeakPointer<QObject>>		m_Groups;

	QSet<QString>						m_FwRuleIDs;

	SProgramStats						m_Stats;
};

typedef QSharedPointer<CProgramItem> CProgramItemPtr;
typedef QWeakPointer<CProgramItem> CProgramItemRef;

