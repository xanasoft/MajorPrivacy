#pragma once
#include "ProgramItem.h"

///////////////////////////////////////////////////////////////////////////////////////
// CProgramSet

class CProgramSet : public CProgramItem
{
public:
	CProgramSet() {}

	virtual EProgramType GetType() const {return EProgramType::eProgramSet; }

	virtual std::vector<CProgramItemPtr>		GetNodes() const			{ std::unique_lock lock(m_Mutex); return m_Nodes; }
	virtual bool								ContainsNode(const CProgramItemPtr& Item) const;

protected:
	friend class CProgramManager;

	void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
	void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
	void ReadIValue(uint32 Index, const StVariant& Data) override;
	void ReadMValue(const SVarName& Name, const StVariant& Data) override;

	std::vector<CProgramItemPtr>				m_Nodes;
};

typedef std::shared_ptr<CProgramSet> CProgramSetPtr;
typedef std::weak_ptr<CProgramSet> CProgramSetRef;

///////////////////////////////////////////////////////////////////////////////////////
// CAllPrograms

class CAllPrograms : public CProgramSet
{
public:
	CAllPrograms();

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
// CProgramListEx

class CProgramListEx : public CProgramList
{
public:
	CProgramListEx() {}

	virtual std::wstring GetPath() const = 0;
	virtual bool MatchFileName(const std::wstring& FileName) const = 0;
};

typedef std::shared_ptr<CProgramListEx> CProgramListExPtr;
typedef std::weak_ptr<CProgramListEx> CProgramListExRef;

///////////////////////////////////////////////////////////////////////////////////////
// CProgramGroup

class CProgramGroup: public CProgramList
{
	TRACK_OBJECT(CProgramGroup)
public:
	CProgramGroup(const std::wstring& Guid);
	
	virtual EProgramType GetType() const {return EProgramType::eProgramGroup; }
};

typedef std::shared_ptr<CProgramGroup> CProgramGroupPtr;
typedef std::weak_ptr<CProgramGroup> CProgramGroupRef;

///////////////////////////////////////////////////////////////////////////////////////
// CProgramRoot

class CProgramRoot : public CProgramList
{
public:
	CProgramRoot();



	virtual EProgramType GetType() const {return EProgramType::eProgramRoot; }

};
