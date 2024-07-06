#pragma once

enum class EFwDirections // same as FW_DIRECTION
{
    Unknown = 0,
    Outbound = 1,
    Inbound = 2,
    Bidirectional = 3
};

enum class EFwActions // NOT same as FW_RULE_ACTION
{
    Undefined = 0,
    Allow = 1,
    Block = 2
};

enum class EFwProfiles // same as FW_PROFILE_TYPE 
{
    Invalid = 0,
    Domain = 0x0001,
    Private = 0x0002,
    Public = 0x0004,
    All = 0x7FFFFFFF
    //Current = 0x80000000
};
    
enum class EFwInterfaces // same as FW_INTERFACE_TYPE
{
    All = 0,
    Lan = 0x0001,
    Wireless = 0x0002,
    RemoteAccess = 0x0004
};

enum class EFwKnownProtocols
{
    Min = 0,
    ICMP = 1,
    TCP = 6,
    UDP = 17,
    ICMPv6 = 58,
    Max = 142,
    Any = 256
};

enum class EFwEventStates
{
    Undefined = 0,
    FromLog,
    UnRuled, // there was no rule found for this connection
    RuleAllowed,
    RuleBlocked,
    RuleError, // a rule was found but it appears it was not obeyed (!)
};

enum class EFwRealms
{
    Undefined = 0,
    LocalHost,
    MultiCast,
    LocalArea,
    Internet
};

enum class FwFilteringModes
{
    Unknown = 0,
    NoFiltering = 1,
    BlockList = 2,
    AllowList = 3,
    //BlockAll = 4,
};

enum class FwAuditPolicy
{
    Off = 0,
    Allowed = 1,    // AUDIT_POLICY_INFORMATION_TYPE_SUCCESS
    Blocked = 2,    // AUDIT_POLICY_INFORMATION_TYPE_FAILURE
    All = 3
};