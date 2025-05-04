#pragma once
#include "..\Library\Status.h"
#include "../Library/Helpers/WinUtil.h"
#include "..\Library\Common\StVariant.h"
#include "..\Programs\ProgramID.h"

/*
CAbstractTweak
  CTweak
    CRegTweak
    CGpoTweak
    CSvcTweak
    CTaskTweak
    CResTweak
    CExecTweak
    CFwTweak
  CTweakList
    CTweakSet (contains single tweaks)
    CTweakGroup (contains tweak sets or other groups)
*/

#include "../Library/API/PrivacyDefs.h"

struct SWinVer
{
    SWinVer(uint32 MinBuild, uint32 MaxBuild, EWinEd Win10Ed = EWinEd::Any)
        : MinBuild(MinBuild), MaxBuild(MaxBuild), Win10Ed(Win10Ed) {}
    SWinVer(uint32 MinBuild = 0, EWinEd Win10Ed = EWinEd::Any)
        : MinBuild(MinBuild), MaxBuild(0), Win10Ed(Win10Ed) {}

    bool Test() const;

    EWinEd      Win10Ed;

    uint32      MinBuild;
    uint32      MaxBuild;
};

///////////////////////////////////////////////////////////////////////////////////////
// CAbstractTweak

class CAbstractTweak
{
    TRACK_OBJECT(CAbstractTweak)
public:
    CAbstractTweak(const std::wstring& Id);
    virtual ~CAbstractTweak() {}

    virtual bool IsAvailable() const = 0;

    virtual std::wstring GetId() const { std::unique_lock Lock(m_Mutex); return m_Id; }
    virtual std::wstring GetParentId() const { std::unique_lock Lock(m_Mutex); return m_ParentId; }
	virtual void SetParentId(const std::wstring& Id) { std::unique_lock Lock(m_Mutex); m_ParentId = Id; }

    virtual std::wstring GetName() const { std::unique_lock Lock(m_Mutex); return m_Name; }
	virtual void SetIndex(int Index) { std::unique_lock Lock(m_Mutex); m_Index = Index; }

    virtual ETweakType GetType() const = 0;

    //virtual ETweakPreset GetPreset() const = 0;
    virtual ETweakHint GetHint() const { std::unique_lock Lock(m_Mutex); return m_Hint; }
    
    virtual ETweakStatus GetStatus(uint32 CallerPID) const = 0;

    virtual STATUS Apply(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault) = 0;
    virtual STATUS Undo(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault) = 0;

    //virtual void SetParent(std::shared_ptr<class CAbstractTweak> pParent) { std::unique_lock Lock(m_Mutex); m_Parent = pParent; }
    //virtual std::shared_ptr<class CAbstractTweak> GetParent() { std::unique_lock Lock(m_Mutex); return m_Parent.lock(); }

	virtual StVariant ToVariant(const SVarWriteOpt& Opts) const;
    virtual NTSTATUS FromVariant(const class StVariant& Data);

    static ETweakType ReadType(const StVariant& Data, SVarWriteOpt::EFormat &Format);

    //virtual std::wstring ToString() const = 0;
    virtual void Dump(class CConfigIni* pIni);

	static std::shared_ptr<CAbstractTweak> CreateFromIni(const std::string& Id, class CConfigIni* pIni);

protected:
    friend class CTweakManager;
	
    virtual void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const;
    virtual void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const;
    virtual void ReadIValue(uint32 Index, const StVariant& Data);
    virtual void ReadMValue(const SVarName& Name, const StVariant& Data);

    mutable std::recursive_mutex m_Mutex;

    std::wstring m_Id;
    std::wstring m_ParentId;
    std::wstring m_Name;
    int m_Index = 0;
    //std::weak_ptr<class CAbstractTweak> m_Parent;
    ETweakHint m_Hint = ETweakHint::eNone;
};

typedef std::shared_ptr<CAbstractTweak> CTweakPtr;

///////////////////////////////////////////////////////////////////////////////////////
// CTweakList

class CTweakList : public CAbstractTweak
{
public:
    CTweakList(const std::wstring& Id) 
    : CAbstractTweak(Id) { }

    virtual void AddTweak(const CTweakPtr& pTweak) { std::unique_lock Lock(m_Mutex); m_List.push_back(pTweak); }
    virtual std::list<CTweakPtr> GetList() { std::unique_lock Lock(m_Mutex); return m_List; }

    virtual bool IsAvailable() const;

    virtual STATUS Apply(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);

protected:
	
    void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void ReadIValue(uint32 Index, const StVariant& Data) override;
    void ReadMValue(const SVarName& Name, const StVariant& Data) override;


    std::list<CTweakPtr> m_List;
};

///////////////////////////////////////////////////////////////////////////////////////
// CTweakGroup

class CTweakGroup: public CTweakList
{
public:
    CTweakGroup(const std::wstring& Id) : CTweakList(Id) {}
    CTweakGroup(const std::wstring& Name, const std::wstring& Id) : CTweakList(Id) { m_Name = Name; }

	virtual ETweakType GetType() const { return ETweakType::eGroup; }

    virtual ETweakStatus GetStatus(uint32 CallerPID) const { return ETweakStatus::eGroup; }

    //virtual std::wstring ToString() const { return L"grp://" + m_Id; } 

    virtual void Dump(class CConfigIni* pIni);

protected:
	
    void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void ReadIValue(uint32 Index, const StVariant& Data) override;
    void ReadMValue(const SVarName& Name, const StVariant& Data) override;
};

///////////////////////////////////////////////////////////////////////////////////////
// CTweakSet

class CTweakSet : public CTweakList
{
public:
    CTweakSet(const std::wstring& Id) : CTweakList(Id) {}
    CTweakSet(const std::wstring& Name, const std::wstring& Id, ETweakHint Hint = ETweakHint::eNone) 
    : CTweakList(Id)  { m_Name = Name; m_Hint = Hint; }

	virtual ETweakType GetType() const { return ETweakType::eSet; }

    virtual ETweakStatus GetStatus(uint32 CallerPID) const;

    //virtual std::wstring ToString() const { return L"set://" + m_Id; } 

    virtual void Dump(class CConfigIni* pIni);

protected:
	
    void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void ReadIValue(uint32 Index, const StVariant& Data) override;
    void ReadMValue(const SVarName& Name, const StVariant& Data) override;
};

///////////////////////////////////////////////////////////////////////////////////////
// CTweak

class CTweak : public CAbstractTweak
{
public:
    CTweak(const std::wstring& Name, const std::wstring& Id, const SWinVer& Ver = SWinVer(), ETweakHint Hint = ETweakHint::eNone) 
    : CAbstractTweak(Id) { m_Name = Name; m_Ver = Ver; m_Hint = Hint; }

    //virtual ETweakPreset GetPreset() const { return m_Set ? ETweakPreset::eSet : ETweakPreset::eNotSet; }

	virtual bool IsSet() const { std::unique_lock Lock(m_Mutex); return m_Set; }
	virtual void SetSet(bool Set) { std::unique_lock Lock(m_Mutex); m_Set = Set; }

    virtual bool IsAvailable() const { return m_Ver.Test(); }

    //virtual ETweakStatus GetStatus(uint32 CallerPID) const;

    virtual STATUS Apply(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);

    virtual void Dump(class CConfigIni* pIni);

    static std::wstring Ver2Str(const SWinVer& Ver);
    static SWinVer Str2Ver(const std::wstring& Str);

	static std::wstring Var2Str(const StVariant& Var);
	static StVariant Str2Var(const std::wstring& Str);

protected:
	
    void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void ReadIValue(uint32 Index, const StVariant& Data) override;
    void ReadMValue(const SVarName& Name, const StVariant& Data) override;

    bool        m_Set = false;
    //bool        m_Applied = false;
    SWinVer     m_Ver;
};

//
// ************************************************************************************
// Tweaks
//

////////////////////////////////////////////
// CRegTweak

class CRegTweak : public CTweak
{
public:
    CRegTweak(const std::wstring& Id) : CTweak(L"", Id) {}
    CRegTweak(const std::wstring& Name, const std::wstring& Id, const SWinVer& Ver, const std::wstring& Key, const std::wstring& Value, const StVariant& Data, ETweakHint Hint = ETweakHint::eNone)
        : CTweak(Name, Id, Ver, Hint) { m_Key = Key; m_Value = Value; m_Data = Data; }

	virtual ETweakType GetType() const { return ETweakType::eReg; }
    virtual std::wstring GetKey() const { std::unique_lock Lock(m_Mutex); return m_Key; }
    virtual std::wstring GetValue() const { std::unique_lock Lock(m_Mutex); return m_Value; }
    virtual StVariant GetData() const { std::unique_lock Lock(m_Mutex); return m_Data; }

    virtual ETweakStatus GetStatus(uint32 CallerPID) const;

    virtual STATUS Apply(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);

	//virtual std::wstring ToString() const { return L"reg://" + m_Key + L"/" + m_Value + L":" + m_Data.AsStr(); }

    virtual void Dump(class CConfigIni* pIni);

protected:
	
    void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void ReadIValue(uint32 Index, const StVariant& Data) override;
    void ReadMValue(const SVarName& Name, const StVariant& Data) override;

    std::wstring m_Key;
    std::wstring m_Value;
    StVariant m_Data;
};

////////////////////////////////////////////
// CGpoTweak

class CGpoTweak : public CTweak
{
public:
    CGpoTweak(const std::wstring& Id) : CTweak(L"", Id) {}
    CGpoTweak(const std::wstring& Name, const std::wstring& Id, const SWinVer& Ver, const std::wstring& Key, const std::wstring& Value, const StVariant& Data, ETweakHint Hint = ETweakHint::eNone)
        : CTweak(Name, Id, Ver, Hint) { m_Key = Key; m_Value = Value; m_Data = Data; }

	virtual ETweakType GetType() const { return ETweakType::eGpo; }
    virtual std::wstring GetKey() const { std::unique_lock Lock(m_Mutex); return m_Key; }
    virtual std::wstring GetValue() const { std::unique_lock Lock(m_Mutex); return m_Value; }
    virtual StVariant GetData() const { std::unique_lock Lock(m_Mutex); return m_Data; }

    virtual ETweakStatus GetStatus(uint32 CallerPID) const;

    virtual STATUS Apply(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);

    //virtual std::wstring ToString() const { return L"gpo://" + m_Key + L"/" + m_Value + L":" + m_Data.AsStr(); }

    virtual void Dump(class CConfigIni* pIni);

protected:

    void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void ReadIValue(uint32 Index, const StVariant& Data) override;
    void ReadMValue(const SVarName& Name, const StVariant& Data) override;

    std::wstring m_Key;
    std::wstring m_Value;
    StVariant m_Data;
};

////////////////////////////////////////////
// CSvcTweak

class CSvcTweak : public CTweak
{
public:
    CSvcTweak(const std::wstring& Id) : CTweak(L"", Id) {}
    CSvcTweak(const std::wstring& Name, const std::wstring& Id, const SWinVer& Ver, const std::wstring& SvcTag, ETweakHint Hint = ETweakHint::eNone)
        : CTweak(Name, Id, Ver, Hint) { m_SvcTag = SvcTag; }

	virtual ETweakType GetType() const { return ETweakType::eSvc; }

    virtual bool IsAvailable() const;

    virtual ETweakStatus GetStatus(uint32 CallerPID) const;
    
    virtual STATUS Apply(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);

    //virtual std::wstring ToString() const { return L"svc://" + m_SvcTag; }

    virtual void Dump(class CConfigIni* pIni);

protected:
    
    void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void ReadIValue(uint32 Index, const StVariant& Data) override;
    void ReadMValue(const SVarName& Name, const StVariant& Data) override;

    std::wstring m_SvcTag;
};

////////////////////////////////////////////
// CTaskTweak

class CTaskTweak : public CTweak
{
public:
    CTaskTweak(const std::wstring& Id) : CTweak(L"", Id) {}
    CTaskTweak(const std::wstring& Name, const std::wstring& Id, const SWinVer& Ver, const std::wstring& Folder, const std::wstring& Entry, ETweakHint Hint = ETweakHint::eNone)
        : CTweak(Name, Id, Ver, Hint) { m_Folder = Folder; m_Entry = Entry; }

	virtual ETweakType GetType() const { return ETweakType::eTask; }

    virtual bool IsAvailable() const;

    virtual ETweakStatus GetStatus(uint32 CallerPID) const;
    
    virtual STATUS Apply(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);

    //virtual std::wstring ToString() const { return L"task://" + m_Folder + L"/" + m_Entry; }

    virtual void Dump(class CConfigIni* pIni);

protected:
    
    void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void ReadIValue(uint32 Index, const StVariant& Data) override;
    void ReadMValue(const SVarName& Name, const StVariant& Data) override;

    std::wstring m_Folder;
    std::wstring m_Entry;
};

////////////////////////////////////////////
// CResTweak

class CResTweak : public CTweak
{
public:
    CResTweak(const std::wstring& Id) : CTweak(L"", Id) {}
    CResTweak(const std::wstring& Name, const std::wstring& Id, const SWinVer& Ver, const std::wstring& PathPattern, ETweakHint Hint = ETweakHint::eNone)
        : CTweak(Name, Id, Ver, Hint) { m_PathPattern = PathPattern; }

	virtual ETweakType GetType() const { return ETweakType::eRes; }

    virtual ETweakStatus GetStatus(uint32 CallerPID) const;
    
    virtual STATUS Apply(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);

    //virtual std::wstring ToString() const { return L"fs:///" + m_PathPattern; }

    virtual void Dump(class CConfigIni* pIni);

protected:
    
    void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void ReadIValue(uint32 Index, const StVariant& Data) override;
    void ReadMValue(const SVarName& Name, const StVariant& Data) override;

    std::wstring m_PathPattern;
};

////////////////////////////////////////////
// CExecTweak

class CExecTweak : public CTweak
{
public:
    CExecTweak(const std::wstring& Id) : CTweak(L"", Id) {}
    CExecTweak(const std::wstring& Name, const std::wstring& Id, const SWinVer& Ver, const std::wstring& PathPattern, ETweakHint Hint = ETweakHint::eNone)
        : CTweak(Name, Id, Ver, Hint) { m_PathPattern = PathPattern; }

	virtual ETweakType GetType() const { return ETweakType::eExec; }

    virtual ETweakStatus GetStatus(uint32 CallerPID) const;
    
    virtual STATUS Apply(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);

    //virtual std::wstring ToString() const { return L"exec:///" + m_PathPattern; }

    virtual void Dump(class CConfigIni* pIni);

protected:
    
    void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void ReadIValue(uint32 Index, const StVariant& Data) override;
    void ReadMValue(const SVarName& Name, const StVariant& Data) override;

    std::wstring m_PathPattern;
};

////////////////////////////////////////////
// CFwTweak

class CFwTweak : public CTweak
{
public:
    CFwTweak(const std::wstring& Id) : CTweak(L"", Id) {}
    CFwTweak(const std::wstring& Name, const std::wstring& Id, const SWinVer& Ver,  std::wstring PathPattern, std::wstring SvcTag = L"", ETweakHint Hint = ETweakHint::eNone)
        : CTweak(Name, Id, Ver, Hint) { m_PathPattern = PathPattern; m_SvcTag = SvcTag; }

	virtual ETweakType GetType() const { return ETweakType::eFw; }

    virtual ETweakStatus GetStatus(uint32 CallerPID) const;
    
    virtual STATUS Apply(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(uint32 CallerPID, ETweakMode Mode = ETweakMode::eDefault);

    //virtual std::wstring ToString() const;

    virtual void Dump(class CConfigIni* pIni);

protected:
    
    void WriteIVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void WriteMVariant(StVariantWriter& Data, const SVarWriteOpt& Opts) const override;
    void ReadIValue(uint32 Index, const StVariant& Data) override;
    void ReadMValue(const SVarName& Name, const StVariant& Data) override;

    std::wstring m_PathPattern;
    std::wstring m_SvcTag;
};