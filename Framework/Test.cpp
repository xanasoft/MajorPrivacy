/*
* Copyright (c) 2023-2024 David Xanatos, xanasoft.com
* All rights reserved.
*
* This file is part of MajorPrivacy.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
* 
*/

#include "Header.h"
#include "Framework.h"
#include "Object.h"
#include "String.h"
#include "Array.h"
#include "List.h"
#include "Table.h"
#include "Map.h"
#include "UniquePtr.h"
#include "RetValue.h"
#include "../Library/Common/PathRule.h"

#include "../Driver/SBIE/pool.h"

#include "../Library/Common/Variant.h"
#include "../Library/Common/SVariant.h"

POOL *Driver_Pool = NULL;

using namespace FW;

class MyMemPool : public AbstractMemPool
{
	struct SMemory {
		ULONG Magic;
		ULONG Size;
#pragma warning( push )
#pragma warning( disable : 4200)
		BYTE Block[0];
#pragma warning( pop )
	};

public:

	void Init() {
		Driver_Pool = Pool_Create();
	}
	void Dispose() {
		Pool_Delete(Driver_Pool);
	}

	virtual void* Alloc(size_t Size){
		SMemory *pMem = (SMemory*)Pool_Alloc(Driver_Pool, (ULONG)(Size + sizeof(SMemory)));
		if (!pMem) return nullptr;
		pMem->Magic = 'MEM';
		pMem->Size = (ULONG)Size;
		return pMem->Block;
	}
	virtual void Free(void* ptr) {
		if (!ptr) return;
		SMemory *pMem = (SMemory*)((BYTE*)ptr - sizeof(SMemory));
		Pool_Free(pMem, pMem->Size);
	}

protected:

	POOL *Driver_Pool = nullptr;
};

MyMemPool g_MemPool;

//DefaultMemPool g_MemPool;

class CTestObj : public Object
{
public:
	CTestObj(AbstractMemPool* pMem) : Object(pMem) {
		DBG_MSG("CTestObj::CTestObj()\n");

		m_Test = pMem->New<Object>();
	}
	~CTestObj() {
		DBG_MSG("CTestObj::~CTestObj()\n");
	}

	SharedPtr<Object> m_Test;
};

struct MyKeyHash {
	ULONG operator()(const int& key) const {
		return key;
	}
};

struct MyKeyEqual {
	bool operator()(const int& lhs, const int& rhs) const {
		return lhs == rhs;
	}
};

/*UniquePtr<HANDLE, BOOL(WINAPI*)(HANDLE)> OpenFile(const char* filename)
{
	UniquePtr<HANDLE, BOOL(WINAPI*)(HANDLE)> h(CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL), CloseHandle);
	if(h == INVALID_HANDLE_VALUE)
		return UniquePtr<HANDLE, BOOL(WINAPI*)(HANDLE)>(nullptr, nullptr);
	return h;
}*/

StringW return1(int one)
{
	StringW Str(&g_MemPool, L"test");
	StringW Str2(&g_MemPool, L"test2");
	if(one)
		return Str;
	return Str2;
}

struct STest
{
	int a;
	int b;
};

STest ReturnSTest(int a)
{
	STest s;
	s.a = a;
	s.b = 0xff;
	return s;
}

StringW ReturnStringW()
{
	StringW Str(&g_MemPool, L"test");
	return Str;
}

class CTestRule : public CPathRule
{
public:
	CTestRule(FW::AbstractMemPool* pMemPool) : CPathRule(pMemPool) {}
	CTestRule(FW::AbstractMemPool* pMem, const FW::StringW& Path) : CPathRule(pMem) { m_PathPattern.Set(Path); }
	CTestRule(FW::AbstractMemPool* pMem, const wchar_t* Path) : CTestRule(pMem, FW::StringW(pMem, Path)) {}
	~CTestRule() {}
};

typedef SharedPtr<CTestRule> CTestRulePtr;

extern "C" VOID Test_KTL()
{
	Driver_Pool = Pool_Create();

	g_MemPool.Init();


/*
	DefaultMemPool Pool1;
	DefaultMemPool Pool2;


	StringW strTest1(&Pool1, L"test1");
	FW::AbstractContainer* Containers[] = { &strTest1, NULL };
	Pool1.MoveTo(Pool2, Containers);
	return;*/


#if 0
	FW::Table<uint64, int> m_SecureEnclaves(&g_MemPool);

	m_SecureEnclaves.Insert(123,123);
	for (auto I = m_SecureEnclaves.begin(); I != m_SecureEnclaves.end(); ++I)
	{
		DbgPrint("%d\n", I.Key());
	}

	return ;
#endif

#if 1
	CDosPattern patTest(&g_MemPool, StringW(&g_MemPool, L"test"));
	ULONG uMatch = patTest.Match(StringW(&g_MemPool, L"test"));

	CTestRulePtr pRule = g_MemPool.New<CTestRule>();

	CPathRulePtr pRule2 = CPathRulePtr(pRule.Get());

	CPathRuleList list(&g_MemPool);

	/*list.Add(g_MemPool.New<CTestRule>(L"\\Device\\HarddiskVolume3\\Temp"));
	list.Add(g_MemPool.New<CTestRule>(L"\\Device\\HarddiskVolume3\\Tmp"));
	list.Add(g_MemPool.New<CTestRule>(L"*\\test1.exe"));
	list.Add(g_MemPool.New<CTestRule>(L"*\\test2.exe"));
	list.Add(g_MemPool.New<CTestRule>(L"*\\test3.exe"));*/
	//list.Add(g_MemPool.New<CTestRule>(L"\\Device\\HarddiskVolume3\\Windows\\*\\app1.exe"));
	//list.Add(g_MemPool.New<CTestRule>(L"\\Device\\HarddiskVolume3\\Windows\\system32\\*\\app2.exe"));
	//list.Add(g_MemPool.New<CTestRule>(L"\\Device\\HarddiskVolume3\\Windows\\system32\\*\\app3.exe"));
	auto pTestRule = g_MemPool.New<CTestRule>(L"\\Device\\HarddiskVolume3\\Windows\\system32\\config\\test\\*");
	list.Add(pTestRule);
	//list.Add(g_MemPool.New<CTestRule>(L"\\Device\\HarddiskVolume3\\telnet.exe"));
	//list.Add(g_MemPool.New<CTestRule>(L"*\\telnet.exe"));

	//list.Remove(pTestRule->GetGuid());

	//list.Add(g_MemPool.New<CTestRule>(L"\\Device\\*\\telnet.exe"));
	//list.Add(g_MemPool.New<CTestRule>(L"\\Device\\HarddiskVolume*\\telnet.exe"));
	//list.Add(g_MemPool.New<CTestRule>(L"\\Device\\HarddiskVolume3\\*.exe"));
	//list.Add(g_MemPool.New<CTestRule>(L"\\Device*telnet.exe"));
	

	/*list.Enum([](const StringW& Path, const CPathRulePtr& pRule, void* pContext) {
		DbgPrint("%S -> %S\n", Path.ConstData(), pRule->GetPath().ConstData());
	}, nullptr);*/

	auto FoundRules = list.GetRules(L"\\Device\\HarddiskVolume3\\Windows\\system32\\config\\test");
	//auto FoundRules = list.GetRules(L"\\Device\\HarddiskVolume3\\telnet.exe");



	FoundRules.Clear();

#endif

	UNICODE_STRING _string;
	UNICODE_STRING* string = &_string;
	RtlInitUnicodeString(string, L"\\Device\\HarddiskVolume3\\Windows\\system32\\config\\test\\telnet.exe");

#define DRV_API_PID         "Pid"
#define DRV_API_TID         "Tid"
#define DRV_API_ACTOR_PID   "ActPid"
#define DRV_API_ACTOR_TID   "ActTid"
#define DRV_API_ACTOR_SVC   "ActSvc"
#define DRV_API_PATH        "Path"
#define DRV_API_TIME_STAMP  "TimeStamp"
#define DRV_API_OPERATION   "Operation"
#define DRV_API_ACCESS      "Access"
#define DRV_API_ACCESS_BLOCK "AccessBlocked"

	uint64 StartTime = GetTickCount64();

	for (int i = 0; i < 1000000; i++)
	{

#if 1 // this is to slow
		CVariant Event(&g_MemPool);
		Event.BeginMap();

		Event.Write(DRV_API_ACTOR_PID, (uint64)0x1234);
		Event.Write(DRV_API_ACTOR_TID, (uint64)0x1234);
		Event.Write(DRV_API_TIME_STAMP, (uint64)0);
		Event.Write(DRV_API_PATH, string->Buffer, string->Length / sizeof(wchar_t));
		Event.Write(DRV_API_OPERATION, (uint32)1);
		Event.Write(DRV_API_ACCESS, (uint32)1);
		Event.Write(DRV_API_ACCESS_BLOCK, 0);

		Event.Finish();
		//theDriver->PushEvent(KphMsgAccessFile, Event);
#else
		size_t BufferLength = 512 + string->Length; // 128 + woudl be just enough
		void* pBuffer = g_MemPool.Alloc(BufferLength);

		VARIANT vOutput;
		Variant_Prepare(VAR_TYPE_MAP, pBuffer, BufferLength, &vOutput);

		Variant_InsertUInt64(&vOutput, DRV_API_ACTOR_PID, (uint64)0x1234);
		Variant_InsertUInt64(&vOutput, DRV_API_ACTOR_TID, (uint64)0x1234);
		Variant_InsertUInt64(&vOutput, DRV_API_TIME_STAMP, (uint64)0);
		Variant_InsertWStr(&vOutput, DRV_API_PATH, string->Buffer, string->Length / sizeof(wchar_t));
		Variant_InsertUInt32(&vOutput, DRV_API_OPERATION, (uint32)1);
		Variant_InsertUInt32(&vOutput, DRV_API_ACCESS, (uint32)1);
		Variant_InsertUInt8(&vOutput, DRV_API_ACCESS_BLOCK, 0);

		size_t BufferUsedSize = Variant_Finish(pBuffer, &vOutput);
		if (BufferUsedSize) {
			//CBuffer Buffer(&g_MemPool, pBuffer, BufferUsedSize, true);
			//theDriver->PushEvent(KphMsgAccessFile, &Buffer);
		}
		else {
			DbgPrint("BAM: Variant failed in KphpFltPreOp\n"); // fails on not enough size
		}

		g_MemPool.Free(pBuffer);
#endif


	}

	uint64 EndTime = GetTickCount64();
	DbgPrint("Time: %dms\n", EndTime - StartTime);

	/*{
		auto hFile = OpenFile("C:\\test.txt");

	}*/

	STest sTest = ReturnSTest(123);

	StringW strTest = ReturnStringW();

	auto test1 = return1(1);

	StringW str(&g_MemPool);
	str = L"test";

	StringA astr(&g_MemPool);
	astr = "test";

	StringW str1;
	str1 = str;

	StringW str2 = str;

	str2 = L"";

	StringW str3;
	str3 = astr;

	StringW str4 = astr;

	UNICODE_STRING ustr;
	str4.ToNtString(&ustr);


	// auto arr = g_MemPool.New<Array<int>>();
	Array<int> arr(&g_MemPool);
	arr.Append(1);
	arr.Append(2);
	arr.Append(3);

	/*for (auto n : arr) {
		DbgPrint("%d\n", n);
	}*/

	List<int> lst(&g_MemPool);
	lst.Append(1);
	lst.Append(2);
	lst.Append(3);

	List<int> lst1 = lst;

	lst.insert(lst.begin(), 0);	

	/*DbgPrint("-\n");
	for (auto& n : lst) {
		DbgPrint("%d\n", n);
	}

	DbgPrint("-\n");
	for (auto& n : lst1) {
		DbgPrint("%d\n", n);
	}*/

	Table<int, int, MyKeyHash> table(&g_MemPool);
	for(int i=0; i < 10; i++)
		table.Insert(i, 100 + i);

	Table<int, int, MyKeyHash> table2 = table;

	auto I = table.find(5);
	table.erase(I);
	/*auto Keys = table.Keys();
	for (auto& n : Keys) {
		DbgPrint("%d\n", n);
	}*/

	//table.Dump([](const int& key) {DbgPrint("%d", key);}, [](const int& value) {DbgPrint("%d", value);});

	DbgPrint("table:\n");
	for(auto I = table.begin(); I != table.end(); ++I) {
		DbgPrint("%d:%d\n", I.Key(), I.Value());
	}

	DbgPrint("table2:\n");
	for(auto I = table2.begin(); I != table2.end(); ++I) {
		DbgPrint("%d:%d\n", I.Key(), I.Value());
	}


	Map<int, int> map(&g_MemPool);
	for(int i=0; i < 3; i++)
		map.Insert(i, 100 + i);

	map.Remove(9);

	for (int i = 0; i < 3; i++)
		map.Insert(0, 200 + i, EMapInsertMode::eMulti);

	map.Remove(3);

	auto i_ = map.lower_bound(3);

	map.Remove(5);

	auto I_le = map.find_le(5);

	auto I_lower = map.lower_bound(5); // 5
	auto I_upper = map.upper_bound(5); // 6

	auto map2 = map;

	auto I2 = map2.find(3);
	auto I3 = map2.erase(I2);

	DbgPrint("map2:\n");
	for(auto I = map2.begin(); I != map2.end(); ++I) {
		DbgPrint("%d:%d\n", I.Key(), I.Value());
	}

	//map

	//UNICODE_STRING
	//ANSI_STRING

	//void* test = new char[10];
	//delete [] test;


	CVariant v1(&g_MemPool);
	v1["str"] = "test 1";
	v1["int"] = 1;
	v1["3"] = "test 3";
	v1["4"] = "test 4";

	v1.Freeze();

	const CVariant v3 = v1["3"];
	//byte* v3b = v3.GetData();

	v1.Clear();

	DBG_MSG("KTL TEST !!!!\n");

	{
		WeakPtr<CTestObj> obj3;

		{
			DBG_MSG("auto obj = New<CTestObj>();\n");
			auto obj = g_MemPool.New<CTestObj>();
			{
				DBG_MSG("auto obj2 = obj;\n");
				auto obj2 = obj;
				{
					DBG_MSG("WeakPtr<CTestObj> obj3(obj2);\n");
					//WeakPtr<CTestObj> obj3(obj2);
					obj3 = obj2;
					DBG_MSG("obj2.Clear();\n");
					obj2.Clear();
					{
						DBG_MSG("auto obj4 = obj3.Acquire();\n");
						auto obj4 = obj3.Acquire();
						DBG_MSG("}\n");
					}
					DBG_MSG("}\n");
				}
				DBG_MSG("}\n");
			}
			DBG_MSG("}\n");
		}
		DBG_MSG("}\n");
	}

	DBG_MSG("+++\n");
}
