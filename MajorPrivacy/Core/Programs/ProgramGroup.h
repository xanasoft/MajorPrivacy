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

	virtual QMap<quint64, CProgramItemPtr> GetNodes() const { return m_Nodes; }

protected:
	friend class CProgramManager;

	void WriteIVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(XVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const XVariant& Data) override;
	void ReadMValue(const SVarName& Name, const XVariant& Data) override;

	QMap<quint64, CProgramItemPtr>		m_Nodes;
};

typedef QSharedPointer<CProgramSet> CProgramSetPtr;
typedef QWeakPointer<CProgramSet> CProgramSetRef;

///////////////////////////////////////////////////////////////////////////////////////
// CAllPrograms

class CAllPrograms : public CProgramSet
{
	Q_OBJECT
public:
	CAllPrograms(QObject* parent = nullptr) : CProgramSet(parent) {}

	virtual EProgramType GetType() const {return EProgramType::eAllPrograms; }

	virtual void CountStats();

protected:

	void ReadIValue(uint32 Index, const XVariant& Data) override;
	void ReadMValue(const SVarName& Name, const XVariant& Data) override;
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