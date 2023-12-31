#pragma once
#include "..\Library\Status.h"
#include "../Library/Helpers/WinUtil.h"
#include "..\Library\Common\Variant.h"
#include "..\Programs\ProgramID.h"

/*
CAbstractTweak
  CTweak
    CRegTweak
    CGpoTweak
    CSvcTweak
    CTaskTweak
    CFSTweak
    CExecTweak
    CFwTweak
  CTweakList
    CTweakSet (contains single tweaks)
    CTweakGroup (contains tweak sets or other groups)
*/

//enum class ETweakPreset
//{
//    eGroup = -1,    // tweak is group
//    eNotSet = 0,    // not set
//    eSet,           // recomended
//    eSetCustom,     // group has custom preset
//    eSetAll         // all in group set
//};

enum class ETweakStatus
{
    eGroup = -1,    // tweak is group
    eNotSet = 0,    // tweak is not set
    eApplied,       // tweak was not set by user but is applied
    eSet,           // tweak is set
    eMissing        // tweak was set but is not applied
};

enum class ETweakHint
{
    eNone = 0,
    eRecommended
};

enum class ETweakMode
{
    eDefault    = 0,
    eAll        = 1,
    eDontSet    = 2
};

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
public:
    CAbstractTweak() {}
    virtual ~CAbstractTweak() {}

    virtual bool IsAvailable() const = 0;

    virtual std::wstring GetName() const { std::unique_lock Lock(m_Mutex); return m_Name; }

    //virtual ETweakPreset GetPreset() const = 0;
    virtual ETweakHint GetHint() const { std::unique_lock Lock(m_Mutex); return m_Hint; }
    
    virtual ETweakStatus GetStatus() const = 0;
    virtual STATUS Apply(ETweakMode Mode = ETweakMode::eDefault) = 0;
    virtual STATUS Undo(ETweakMode Mode = ETweakMode::eDefault) = 0;

    virtual void SetParent(std::shared_ptr<class CAbstractTweak> pParent) { std::unique_lock Lock(m_Mutex); m_Parent = pParent; }
    virtual std::shared_ptr<class CAbstractTweak> GetParent() { std::unique_lock Lock(m_Mutex); return m_Parent.lock(); }

	virtual CVariant ToVariant() const;

protected:
	
    virtual void WriteVariant(CVariant& Data) const;

    mutable std::recursive_mutex m_Mutex;

    std::wstring m_Name;
    std::weak_ptr<class CAbstractTweak> m_Parent;
    ETweakHint m_Hint = ETweakHint::eNone;
};

typedef std::shared_ptr<CAbstractTweak> CTweakPtr;

///////////////////////////////////////////////////////////////////////////////////////
// CTweakList

class CTweakList : public CAbstractTweak
{
public:

    virtual void AddTweak(const CTweakPtr& pTweak) { std::unique_lock Lock(m_Mutex); m_List.push_back(pTweak); }
    virtual std::list<CTweakPtr> GetList() { std::unique_lock Lock(m_Mutex); return m_List; }

    virtual bool IsAvailable() const;

    virtual STATUS Apply(ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(ETweakMode Mode = ETweakMode::eDefault);

protected:
	
    virtual void WriteVariant(CVariant& Data) const;

    std::list<CTweakPtr> m_List;
};

///////////////////////////////////////////////////////////////////////////////////////
// CTweakGroup

class CTweakGroup: public CTweakList
{
public:
    CTweakGroup(const std::wstring& Name) { m_Name = Name; }

    virtual ETweakStatus GetStatus() const { return ETweakStatus::eGroup; }

protected:
	
    virtual void WriteVariant(CVariant& Data) const;
};

///////////////////////////////////////////////////////////////////////////////////////
// CTweakSet

class CTweakSet : public CTweakList
{
public:
    CTweakSet(const std::wstring& Name, ETweakHint Hint = ETweakHint::eNone) { m_Name = Name; m_Hint = Hint; }

    virtual ETweakStatus GetStatus() const;

protected:
	
    virtual void WriteVariant(CVariant& Data) const;
};

///////////////////////////////////////////////////////////////////////////////////////
// CTweak

class CTweak : public CAbstractTweak
{
public:
    CTweak(const std::wstring& Name, const SWinVer& Ver, ETweakHint Hint) {
        m_Name = Name;
        m_Ver = Ver;
        m_Hint = Hint;
    }

    //virtual ETweakPreset GetPreset() const { return m_Set ? ETweakPreset::eSet : ETweakPreset::eNotSet; }

    virtual bool IsAvailable() const { return m_Ver.Test(); }

    virtual STATUS Apply(ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(ETweakMode Mode = ETweakMode::eDefault);

protected:
	
    virtual void WriteVariant(CVariant& Data) const;

    bool        m_Set = false;
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
    CRegTweak(const std::wstring& Name, const SWinVer& Ver, const std::wstring& Key, const std::wstring& Value, const CVariant& Data, ETweakHint Hint = ETweakHint::eRecommended)
        : CTweak(Name, Ver, Hint) { m_Key = Key; m_Value = Value; m_Data = Data; }

    virtual ETweakStatus GetStatus() const;
    virtual STATUS Apply(ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(ETweakMode Mode = ETweakMode::eDefault);

protected:
	
    virtual void WriteVariant(CVariant& Data) const;

    std::wstring m_Key;
    std::wstring m_Value;
    CVariant m_Data;
};

////////////////////////////////////////////
// CGpoTweak

class CGpoTweak : public CTweak
{
public:
    CGpoTweak(const std::wstring& Name, const SWinVer& Ver, const std::wstring& Key, const std::wstring& Value, const CVariant& Data, ETweakHint Hint = ETweakHint::eRecommended)
        : CTweak(Name, Ver, Hint) { m_Key = Key; m_Value = Value; m_Data = Data; }

    virtual ETweakStatus GetStatus() const;
    virtual STATUS Apply(ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(ETweakMode Mode = ETweakMode::eDefault);

protected:

    virtual void WriteVariant(CVariant& Data) const;

    std::wstring m_Key;
    std::wstring m_Value;
    CVariant m_Data;
};

////////////////////////////////////////////
// CSvcTweak

class CSvcTweak : public CTweak
{
public:
    CSvcTweak(const std::wstring& Name, const SWinVer& Ver, const std::wstring& SvcTag, ETweakHint Hint = ETweakHint::eRecommended)
        : CTweak(Name, Ver, Hint) { m_SvcTag = SvcTag; }

    virtual bool IsAvailable() const;

    virtual ETweakStatus GetStatus() const;
    virtual STATUS Apply(ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(ETweakMode Mode = ETweakMode::eDefault);

protected:
    
    virtual void WriteVariant(CVariant& Data) const;

    std::wstring m_SvcTag;
};

////////////////////////////////////////////
// CTaskTweak

class CTaskTweak : public CTweak
{
public:
    CTaskTweak(const std::wstring& Name, const SWinVer& Ver, const std::wstring& Folder, const std::wstring& Entry, ETweakHint Hint = ETweakHint::eRecommended)
        : CTweak(Name, Ver, Hint) { m_Folder = Folder; m_Entry = Entry; }

    virtual bool IsAvailable() const;

    virtual ETweakStatus GetStatus() const;
    virtual STATUS Apply(ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(ETweakMode Mode = ETweakMode::eDefault);

protected:
    
    virtual void WriteVariant(CVariant& Data) const;

    std::wstring m_Folder;
    std::wstring m_Entry;
};

////////////////////////////////////////////
// CFSTweak

class CFSTweak : public CTweak
{
public:
    CFSTweak(const std::wstring& Name, const SWinVer& Ver, const std::wstring& PathPattern, ETweakHint Hint = ETweakHint::eRecommended)
        : CTweak(Name, Ver, Hint) { m_PathPattern = PathPattern; }

    virtual bool IsAvailable() const { return false; } // todo: implement

    virtual ETweakStatus GetStatus() const;
    virtual STATUS Apply(ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(ETweakMode Mode = ETweakMode::eDefault);

protected:
    
    virtual void WriteVariant(CVariant& Data) const;

    std::wstring m_PathPattern;
};

////////////////////////////////////////////
// CExecTweak

class CExecTweak : public CTweak
{
public:
    CExecTweak(const std::wstring& Name, const SWinVer& Ver, const std::wstring& PathPattern, ETweakHint Hint = ETweakHint::eRecommended)
        : CTweak(Name, Ver, Hint) { m_PathPattern = PathPattern; }

    virtual bool IsAvailable() const { return false; } // todo: implement

    virtual ETweakStatus GetStatus() const;
    virtual STATUS Apply(ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(ETweakMode Mode = ETweakMode::eDefault);

protected:
    
    virtual void WriteVariant(CVariant& Data) const;

    std::wstring m_PathPattern;
};

////////////////////////////////////////////
// CFwTweak

class CFwTweak : public CTweak
{
public:
    CFwTweak(const std::wstring& Name, const SWinVer& Ver, const CProgramID& ProgID, ETweakHint Hint = ETweakHint::eRecommended)
        : CTweak(Name, Ver, Hint) { m_ProgID = ProgID; }

    virtual bool IsAvailable() const { return false; } // todo: implement

    virtual ETweakStatus GetStatus() const;
    virtual STATUS Apply(ETweakMode Mode = ETweakMode::eDefault);
    virtual STATUS Undo(ETweakMode Mode = ETweakMode::eDefault);

protected:
    
    virtual void WriteVariant(CVariant& Data) const;

    CProgramID m_ProgID;
};