#pragma once
#include "ProgramItem.h"


///////////////////////////////////////////////////////////////////////////////////////
// CProgramSet

class CProgramSet : public CProgramItem
{
public:
	CProgramSet() {}

	virtual std::vector<CProgramItemPtr>		GetNodes() const			{ std::shared_lock lock(m_Mutex); return m_Nodes; }
	virtual bool								ContainsNode(const CProgramItemPtr& Item) const;

	virtual int									GetSpecificity() const { return 0; }	

protected:
	friend class CProgramManager;

	virtual void WriteVariant(CVariant& Data) const;

	std::vector<CProgramItemPtr>				m_Nodes;
};

typedef std::shared_ptr<CProgramSet> CProgramSetPtr;
typedef std::weak_ptr<CProgramSet> CProgramSetRef;

///////////////////////////////////////////////////////////////////////////////////////
// CAllProgram

class CAllProgram : public CProgramSet
{
public:
	CAllProgram() {
		m_ID.Set(CProgramID::eAll, L"");
	}

protected:
	virtual void WriteVariant(CVariant& Data) const;
};

///////////////////////////////////////////////////////////////////////////////////////
// CProgramList

class CProgramList : public CProgramSet
{
public:
	CProgramList() {}

};

///////////////////////////////////////////////////////////////////////////////////////
// CProgramGroup

class CProgramGroup: public CProgramList
{
public:
	CProgramGroup() {}

protected:
	virtual void WriteVariant(CVariant& Data) const;
};

typedef std::shared_ptr<CProgramGroup> CProgramGroupPtr;
typedef std::weak_ptr<CProgramGroup> CProgramGroupRef;
