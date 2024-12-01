#pragma once
#include "ProgramItem.h"

typedef std::wstring		TGroupId;

///////////////////////////////////////////////////////////////////////////////////////
// CProgramSet

class CProgramSet : public CProgramItem
{
public:
	CProgramSet() {}

	virtual EProgramType GetType() const {return EProgramType::eProgramSet; }

	virtual std::vector<CProgramItemPtr>		GetNodes() const			{ std::unique_lock lock(m_Mutex); return m_Nodes; }
	virtual bool								ContainsNode(const CProgramItemPtr& Item) const;

	virtual int									GetSpecificity() const { return 0; }	

protected:
	friend class CProgramManager;

	void WriteIVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(CVariant& Rule, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const CVariant& Data) override;
	void ReadMValue(const SVarName& Name, const CVariant& Data) override;

	std::vector<CProgramItemPtr>				m_Nodes;
};

typedef std::shared_ptr<CProgramSet> CProgramSetPtr;
typedef std::weak_ptr<CProgramSet> CProgramSetRef;

///////////////////////////////////////////////////////////////////////////////////////
// CAllPrograms

class CAllPrograms : public CProgramSet
{
public:
	CAllPrograms() { m_ID.Set(EProgramType::eAllPrograms, L""); }

	virtual EProgramType GetType() const {return EProgramType::eAllPrograms; }
};

///////////////////////////////////////////////////////////////////////////////////////
// CProgramList

class CProgramList : public CProgramSet
{
public:
	CProgramList() {}

	virtual EProgramType GetType() const {return EProgramType::eProgramList; }
};

typedef std::shared_ptr<CProgramList> CProgramListPtr;
typedef std::weak_ptr<CProgramList> CProgramListRef;

///////////////////////////////////////////////////////////////////////////////////////
// CProgramGroup

class CProgramGroup: public CProgramList
{
	TRACK_OBJECT(CProgramGroup)
public:
	CProgramGroup(const TGroupId& Guid) {
		m_ID.Set(EProgramType::eProgramGroup, Guid);
	}

	virtual EProgramType GetType() const {return EProgramType::eProgramGroup; }
};

typedef std::shared_ptr<CProgramGroup> CProgramGroupPtr;
typedef std::weak_ptr<CProgramGroup> CProgramGroupRef;

///////////////////////////////////////////////////////////////////////////////////////
// CProgramRoot

class CProgramRoot : public CProgramList
{
public:
	CProgramRoot() { m_ID.Set(EProgramType::eProgramRoot, L""); }

	virtual EProgramType GetType() const {return EProgramType::eProgramRoot; }

};
