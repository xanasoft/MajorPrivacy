#pragma once
#include "ProgramItem.h"

///////////////////////////////////////////////////////////////////////////////////////
// CProgramSet

class CProgramSet : public CProgramItem
{
	Q_OBJECT
public:
	CProgramSet(QObject* parent = nullptr) : CProgramItem(parent) {}

	virtual QMap<quint64, CProgramItemPtr> GetNodes() const { return m_Nodes; }

protected:
	friend class CProgramManager;

	QMap<quint64, CProgramItemPtr>		m_Nodes;
};

typedef QSharedPointer<CProgramSet> CProgramSetPtr;
typedef QWeakPointer<CProgramSet> CProgramSetRef;

///////////////////////////////////////////////////////////////////////////////////////
// CAllProgram

class CAllProgram : public CProgramSet
{
	Q_OBJECT
public:
	CAllProgram(QObject* parent = nullptr) : CProgramSet(parent) {}

	virtual void CountStats();

protected:
	virtual void ReadValue(const SVarName& Name, const XVariant& Data);
};

///////////////////////////////////////////////////////////////////////////////////////
// CProgramList

class CProgramList : public CProgramSet
{
	Q_OBJECT
public:
	CProgramList(QObject* parent = nullptr) : CProgramSet(parent) {}

	virtual void CountStats();
};

///////////////////////////////////////////////////////////////////////////////////////
// CProgramGroup

class CProgramGroup: public CProgramList
{
	Q_OBJECT
public:
	CProgramGroup(QObject* parent = nullptr) : CProgramList(parent) {}

	virtual QIcon DefaultIcon() const;
};

typedef QSharedPointer<CProgramGroup> CProgramGroupPtr;
typedef QWeakPointer<CProgramGroup> CProgramGroupRef;