#include "pch.h"
#include "Variant.h"
#include "SVariant.h"
#include "DbgHelp.h"
#include "stl_helpers.h"

void TestVariant()
{
	CCowPtr<std::list<int>> list1;
	list1.New();
	list1->push_back(1);
	list1->push_back(2);
	list1->push_back(3);

	CCowPtr<std::list<int>> list2 = list1;

	for (auto num : list2.Const())
		printf("%d", num);


	CCowPtr<std::wstring> str1(new std::wstring(L"test"));

	CCowPtr<std::wstring> str2 = str1;

	if(str2->size() > 2)
		str2->insert(2, L"_");

#if 1
	CVariant Test_0;
	Test_0[(uint32)'a'] = 0x12345678;
	Test_0[(uint32)'b'] = 0xABCDABCD;

	CVariant Test_0_1;
	Test_0_1.Append(1);
	Test_0_1.Append(2);
	Test_0_1.Append(3);
	Test_0[(uint32)'c'] = Test_0_1;

	Test_0[(uint32)'c'].Append(4);


	
	CBuffer Buff_0;
	//WritePacket("TST", Test_0, Buff_0);
	Test_0.ToPacket(&Buff_0);

	VARIANT Var_0;
	Variant_FromBuffer(Buff_0.GetBuffer(), Buff_0.GetSize(), &Var_0);

	VARIANT test_0_1;
	Variant_Get(&Var_0, 'a', &test_0_1);
	VARIANT test_0_2;
	Variant_Get(&Var_0, 'b', &test_0_2);
#endif

#if 1
	CVariant Test_1;
	Test_1.Append(1);
	Test_1.Append(2);
	Test_1.Append(3);
	Test_1.Append(4);
	
	CBuffer Buff_1;
	//WritePacket("TST", Test_1, Buff_1);
	Test_1.ToPacket(&Buff_1);

	VARIANT Var_1;
	Variant_FromBuffer(Buff_1.GetBuffer(), Buff_1.GetSize(), &Var_1);

	VARIANT test_1_2;
	Variant_At(&Var_1, 2, &test_1_2);

	uint32 int_1_2;
	Variant_ToInt(&test_1_2, &int_1_2, sizeof(int_1_2));

	VARIANT var_1_x;
	VARIANT_IT it;
	for (Variant_Begin(&Var_1, &it); Variant_Next(&it, &var_1_x); )
	{
		uint32 int_1_x;
		Variant_ToInt(&var_1_x, &int_1_x, sizeof(int_1_x));
		DbgPrint("int_1_x: %u\r\n", int_1_x);
	}

#endif

#if 1
	byte Buffer_2[255];

	VARIANT Var_2;
	Variant_Init(VAR_TYPE_MAP, Buffer_2, sizeof(Buffer_2), &Var_2);

	VARIANT Var_2_1;
	Variant_FromUInt32(0x12345678, &Var_2_1);
	if (!Variant_Insert(&Var_2, "1", &Var_2_1))
		DbgPrint("fail_2_1");

	VARIANT Var_2_2;
	Variant_FromUInt32(0xAABBCCDD, &Var_2_2);
	if(!Variant_Insert(&Var_2, "2", &Var_2_2))
		DbgPrint("fail_2_2");
	
	
	byte Buffer_3[255];

	VARIANT Var_3;
	Variant_Init(VAR_TYPE_LIST, Buffer_3, sizeof(Buffer_3), &Var_3);

	for (int i = 0; i < 10; i++) {
		VARIANT Var_3_x;
		Variant_FromUInt32(i, &Var_3_x);
		Variant_Append(&Var_3, &Var_3_x);
	}


	byte Buffer_4[512];

	VARIANT Var_4;
	Variant_Init(VAR_TYPE_MAP, Buffer_4, sizeof(Buffer_4), &Var_4);

	if (!Variant_Insert(&Var_4, "A", &Var_2))
		DbgPrint("fail_3_A");

	if (!Variant_Insert(&Var_4, "B", &Var_3))
		DbgPrint("fail_3_B");


	CBuffer Buff_2;
	Buff_2.SetSize(0, true, 0x1000);
	//Buff_2.SetSize(Variant_ToBuffer(&Var_2, Buff_2.GetBuffer(), Buff_2.GetLength()));
	Buff_2.SetSize(Variant_ToBuffer(&Var_4, Buff_2.GetBuffer(), Buff_2.GetLength()));

	CVariant CVar_2;
	CVar_2.FromPacket(&Buff_2, true);
	
	uint32 int_2_1 = CVar_2["A"]["1"];
	uint32 int_2_2 = CVar_2["A"]["2"];
	DbgPrint("Test: 1 = %u; 2 = %u\r\n", int_2_1, int_2_2);

	for (uint32 i = 0; i < CVar_2["B"].Count(); i++)
	{
		DbgPrint("Test: %u\r\n", CVar_2["B"][i].To<uint32>());
	}
#endif

#if 1

	CBuffer Buff_3;
	Buff_3.SetSize(0, true, 0x1000);

    VARIANT vOutput;
    Variant_Prepare(VAR_TYPE_INDEX, Buff_3.GetBuffer(), Buff_3.GetLength(), &vOutput);

    Variant_AddUInt64(&vOutput, 'uint', 0x1234);

	VARIANT vList;
	Variant_PrepareAdd(&vOutput, 'list', VAR_TYPE_LIST, &vList);

	for (int i = 0; i < 10; i++)
		Variant_AppendUInt32(&vList, i);

	Variant_FinishEntry(&vOutput, &vList);


	Buff_3.SetSize(Variant_Finish(Buff_3.GetBuffer(), &vOutput));

	CVariant CVar_3;
	CVar_3.FromPacket(&Buff_3, true);

	for (uint32 i = 0; i < CVar_3[(uint32)'list'].Count(); i++)
	{
		DbgPrint("Test: %u\r\n", CVar_3[(uint32)'list'][i].To<uint32>());
	}

#endif

	CVariant Test_x;

	Test_x["1"] = 1;
	Test_x["2"] = 0x1234567890ABCDEF;
	Test_x["3"] = L"3";
	Test_x["4"] = "4";

	CBuffer Buff_x;
	WritePacket("Test", Test_x, Buff_x);


	CVariant Test_RawList;
	Test_RawList.BeginList();
	Test_RawList.Write("1");
	Test_RawList.Write("2");
	Test_RawList.Write("3");
	Test_RawList.Write("4");
	Test_RawList.Finish();

	int Test_RawList_Count = Test_RawList.Count();
}