#pragma once
#include "../../Library/Common/StVariant.h"
#include "../../Library/Common/VariantCache.h"
#include "../../Library/API/PrivacyAPI.h"

//
// The list replies that go out on every refresh, built as deltas.
//
// Each of these walks what the collector holds, writes every field through the
// cache, and lets the cache decide what has actually changed since the last
// round on this connection. The caches live in CServiceCore::SClient, so they
// are made and dropped with the pipe.
//
// Changed holds only the entries with at least one field differing from what was
// last sent, and within each only those fields; Removed holds the keys of the
// entries this round did not mention. How the two are spelt in the reply is the
// protocol's business and stays in ServiceCoreAPI.cpp - see API_V_PROCESSES and
// API_V_OLD_PROCESSES.
//

//
// One client's worth. Kept together rather than as three members of SClient
// because they are one subscription: the handle and socket caches are scoped by
// the PUID of the owning process, so they only make sense alongside the process
// cache that decides which of those processes still exist.
//
struct SListCaches
{
	SListCaches(FW::AbstractMemPool* pMemPool)
		: Processes(pMemPool, API_V_PUID)
		, Handles(pMemPool, API_V_ACCESS_REF)
		, Sockets(pMemPool, API_V_SOCK_REF)
		, Programs(pMemPool, API_V_PROG_UID)
		, Libraries(pMemPool, API_V_PROG_UID)
	{}

	//
	// Everything at once, for a client that says it has lost its copy.
	//
	void Clear()	{ Processes.Clear(); Handles.Clear(); Sockets.Clear(); Programs.Clear(); Libraries.Clear(); }

	CVariantCache	Processes;
	CVariantCache	Handles;
	CVariantCache	Sockets;
	CVariantCache	Programs;
	CVariantCache	Libraries;
};

StVariant BuildProcessDelta(SListCaches& Caches, StVariant& Removed, FW::AbstractMemPool* pMemPool);

StVariant BuildProgramDelta(CVariantCache& Cache, StVariant& Removed, FW::AbstractMemPool* pMemPool);
StVariant BuildLibraryDelta(CVariantCache& Cache, StVariant& Removed, FW::AbstractMemPool* pMemPool);
