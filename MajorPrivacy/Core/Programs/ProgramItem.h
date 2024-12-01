#pragma once

//#include "../Process.h"
#include "ProgramID.h"
#include "../../Helpers/FilePath.h"

struct SProgramStats
{
	int				ProgramsCount = 0;
	int				ServicesCount = 0;
	int				AppsCount = 0;
	int				GroupCount = 0;

	int				ProcessCount = 0;
	uint64			LastExecution = 0;
	int				ProgRuleCount = 0;
	int				ProgRuleTotal = 0;

	int				ResRuleCount = 0;
	int				ResRuleTotal = 0;
	uint32			AccessCount = 0;

	int				FwRuleCount = 0;
	int				FwRuleTotal = 0;
	int				SocketCount = 0;

	uint64			LastNetActivity = 0;

	uint64			LastFwAllowed = 0;
	uint64			LastFwBlocked = 0;

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

	virtual EProgramType GetType() const = 0;
	virtual QString GetTypeStr() const;

	void SetUID(quint64 UID) 						{ m_UID = UID; }
	quint64 GetUID() const							{ return m_UID; }
	void SetID(const CProgramID& ID) 				{ m_ID = ID; }
	const CProgramID& GetID() const					{ return m_ID; }

	virtual void SetName(const QString& Name)		{ m_Name = Name; }
	virtual QString GetName() const					{ return m_Name; }
	virtual QString GetNameEx() const				{ return m_Name; }
	virtual QStringList GetCategories() const		{ return m_Categories; }
	virtual QIcon DefaultIcon() const;
	virtual void SetIconFile(const QString& IconFile) { m_IconFile = IconFile; UpdateIconFile(); }
	virtual QString GetIconFile() const				{ return m_IconFile; }
	virtual QIcon GetIcon() const					{ return m_Icon; }
	virtual void SetInfo(const QString& Info)		{ m_Info = Info; }
	virtual QString GetInfo() const					{ return m_Info; }
	virtual QString GetPath(EPathType Type) const	{ return ""; }

	virtual QList<QWeakPointer<QObject>> GetGroups() const	{ return m_Groups; }

	virtual int GetFwRuleCount() const				{ return m_FwRuleIDs.count(); }
	virtual QSet<QString> GetFwRules() const		{ return m_FwRuleIDs; }

	virtual int GetProgRuleCount() const			{ return m_ProgRuleIDs.count(); }
	virtual QSet<QString> GetProgRules() const		{ return m_ProgRuleIDs; }

	virtual int GetResRuleCount() const				{ return m_ResRuleIDs.count(); }
	virtual QSet<QString> GetResRules() const		{ return m_ResRuleIDs; }

	virtual bool IsMissing() const					{ return m_IsMissing; }

	virtual void CountStats() = 0;
	virtual const SProgramStats* GetStats()			{ return &m_Stats; }

	virtual XVariant ToVariant(const SVarWriteOpt& Opts) const;
	virtual NTSTATUS FromVariant(const XVariant& Data);

protected:
	friend class CProgramManager;

	void SetIconFile();
	void UpdateIconFile();

	virtual void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const;
	virtual void ReadIValue(uint32 Index, const XVariant& Data);
	virtual void ReadMValue(const SVarName& Name, const XVariant& Data);

	quint64								m_UID = 0;
	CProgramID							m_ID;
	QString								m_Name;
	QStringList							m_Categories;
	QString								m_IconFile;
	QIcon								m_Icon;
	QString								m_Info;

	QList<QWeakPointer<QObject>>		m_Groups;

	QSet<QString>						m_FwRuleIDs;
	QSet<QString>						m_ProgRuleIDs;
	QSet<QString>						m_ResRuleIDs;

	bool								m_IsMissing = false;

	SProgramStats						m_Stats;
};

typedef QSharedPointer<CProgramItem> CProgramItemPtr;
typedef QWeakPointer<CProgramItem> CProgramItemRef;

