#pragma once
#include "../Types.h"
#include "../lib_global.h"

enum class EKnownProtocols
{
    //Min = 0,

    ICMP = 1,
    TCP = 6,
    UDP = 17,
    ICMPv6 = 58,

    //Max = 142,
    Any = 256
};

extern LIBRARY_EXPORT const std::map<uint32, std::wstring> g_KnownProtocols;
extern LIBRARY_EXPORT const std::map<uint32, std::wstring> g_KnownIcmp4Types;
extern LIBRARY_EXPORT const std::map<uint32, std::wstring> g_KnownIcmp6Types;

//LIBRARY_EXPORT std::wstring Protocol2Str(uint32 Protocol);