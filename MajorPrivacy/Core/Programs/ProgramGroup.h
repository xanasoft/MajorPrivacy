#pragma once
#include "ProgramItem.h"

///////////////////////////////////////////////////////////////////////////////////////
// CProgramSet

class CProgramSet : public CProgramItem
{
	Q_OBJECT
public:
	CProgramSet(QObject* parent = nullptr) : CProgramItem(parent) {}

	virtual EProgramType GetType() const {return EProgramType::eProgramSet; }

	virtual QHash<quint64, CProgramItemPtr> GetNodes() const { QReadLocker Lock(&m_Mutex); return m_Nodes; }

	virtual size_t GetLogMemUsage() const;

protected:
	friend class CProgramManager;

	//virtual void MergeStats();

	void WriteIVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(QtVariantWriter& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const QtVariant& Data) override;
	void ReadMValue(const SVarName& Name, const QtVariant& Data) override;

	QHash<quint64, CProgramItemPtr>		m_Nodes;
};

typedef QSharedPointer<CProgramSet> CProgramSetPtr;
typedef QWeakPointer<CProgramSet> CProgramSetRef;

///////////////////////////////////////////////////////////////////////////////////////
// CAllPrograms

class CAllPrograms : public CProgramSet
{
	Q_OBJECT
public:
	CAllPrograms(QObject* parent = nullptr) : CProgramSet(parent) { m_ID = CProgramID(EProgramType::eAllPrograms); }

	virtual QString GetPath() const					{ return "*"; }
	virtual EProgramType GetType() const {return EProgramType::eAllPrograms; }

	virtual void CountStats();

protected:

	void ReadIValue(uint32 Index, const QtVariant& Data) override;
	void ReadMValue(const SVarName& Name, const QtVariant& Data) override;
};

///////////////////////////////////////////////////////////////////////////////////////
// CProgramList

class CProgramList : public CProgramSet
{
	Q_OBJECT
public:
	CProgramList(QObject* parent = nullptr) : CProgramSet(parent) {}

	virtual EProgramType GetType() const {return EProgramType::eProgramList; }
	
	virtual void CountStats();
};

///////////////////////////////////////////////////////////////////////////////////////
// CProgramGroup

class CProgramGroup: public CProgramList
{
	Q_OBJECT
public:
	CProgramGroup(QObject* parent = nullptr) : CProgramList(parent) {}

	virtual EProgramType GetType() const {return EProgramType::eProgramGroup; }

	virtual QIcon DefaultIcon() const;
};

typedef QSharedPointer<CProgramGroup> CProgramGroupPtr;
typedef QWeakPointer<CProgramGroup> CProgramGroupRef;

///////////////////////////////////////////////////////////////////////////////////////
// CProgramRoot

class CProgramRoot : public CProgramList
{
	Q_OBJECT
public:
	CProgramRoot(QObject* parent = nullptr) : CProgramList(parent) {}
	virtual EProgramType GetType() const {return EProgramType::eProgramRoot; }

};