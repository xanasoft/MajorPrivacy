

/////////////////////////////////////////////////////////////////////////////
// CPreset
//

void CPreset::WriteIVariant(VariantWriter& Preset, const SVarWriteOpt& Opts) const
{
	if(!m_Guid.IsNull())
		Preset.WriteVariant(API_V_GUID, m_Guid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, Preset.Allocator()));
	Preset.WriteEx(API_V_NAME, TO_STR(m_Name));
	Preset.WriteEx(API_V_INFO, TO_STR(m_Description));

	Preset.Write(API_V_IS_ACTIVE, m_bIsActive);
	
	Preset.Write(API_V_USE_SCRIPT, m_bUseScript);
	Preset.WriteEx(API_V_SCRIPT, TO_STR_A(m_Script));

	VariantWriter Items(Preset.Allocator());
	Items.BeginList();
	for(auto I = m_Items.begin(); I != m_Items.end(); ++I)
	{
#ifdef PRESET_SVC
		QFlexGuid ItemGuid = I->first;
		const SItemPreset& ItemPreset = I->second;
#else
		QFlexGuid ItemGuid = I.key();
		const SItemPreset& ItemPreset = I.value();
#endif

		VariantWriter Item(Preset.Allocator());
		Item.BeginIndex();
		Item.WriteVariant(API_V_GUID, ItemGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, Preset.Allocator()));
		Item.Write(API_V_TYPE, (uint32)ItemPreset.Type);
		Item.Write(API_V_ACTION, (uint32)ItemPreset.Activate);
		Items.WriteVariant(Item.Finish());
	}
	Preset.WriteVariant(API_V_ITEMS, Items.Finish());

	Preset.WriteVariant(API_V_DATA, m_Data);
}

void CPreset::ReadIValue(uint32 Index, const XVariant& Data)
{
	switch (Index)
	{
	case API_V_GUID:			m_Guid.FromVariant(Data); break;
	case API_V_NAME:			m_Name = AS_STR(Data); break;
	case API_V_INFO:			m_Description = AS_STR(Data); break;

	case API_V_IS_ACTIVE:		m_bIsActive = Data.To<bool>(); break;
	
	case API_V_USE_SCRIPT:	m_bUseScript = Data.To<bool>(); break;
	case API_V_SCRIPT:		AS_STR_A(m_Script, Data); break;

	case API_V_ITEMS:
	{
		VariantReader(Data).ReadRawList([this](const XVariant& ItemData)
		{
			QFlexGuid ItemGuid;
			SItemPreset Item;

			VariantReader(ItemData).ReadRawIndex([&ItemGuid, &Item](uint32 Index, const XVariant& Value)
			{
				if (Index == API_V_GUID)
					ItemGuid.FromVariant(Value);
				else if (Index == API_V_TYPE)
					Item.Type = (EItemType)Value.To<uint32>();
				else if (Index == API_V_ACTION)
					Item.Activate = (SItemPreset::EActivate)Value.To<uint32>();
			});

			if (!ItemGuid.IsNull() /*&& Item.Type != EItemType::eUnknown*/)
#ifdef PRESET_SVC
				m_Items.insert(std::make_pair(ItemGuid, Item));
#else
				m_Items.insert(ItemGuid, Item);
#endif
		});
		break;
	}

	case API_V_DATA:			m_Data = Data; break;
	//default: ;
	}
}

/////////////////////////////////////////////////////////////////////////////

void CPreset::WriteMVariant(VariantWriter& Preset, const SVarWriteOpt& Opts) const
{
	if (!m_Guid.IsNull())
		Preset.WriteVariant(API_S_GUID, m_Guid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, Preset.Allocator()));
	Preset.WriteEx(API_S_NAME, TO_STR(m_Name));
	Preset.WriteEx(API_S_INFO, TO_STR(m_Description));

	Preset.Write(API_S_IS_ACTIVE, m_bIsActive);
	
	Preset.Write(API_S_USE_SCRIPT, m_bUseScript);
	Preset.WriteEx(API_S_SCRIPT, TO_STR_A(m_Script));

	VariantWriter Items(Preset.Allocator());
	Items.BeginList();
	for(auto I = m_Items.begin(); I != m_Items.end(); ++I)
	{
#ifdef PRESET_SVC
		QFlexGuid ItemGuid = I->first;
		const SItemPreset& ItemPreset = I->second;
#else
		QFlexGuid ItemGuid = I.key();
		const SItemPreset& ItemPreset = I.value();
#endif

		VariantWriter Item(Preset.Allocator());
		Item.BeginMap();
		Item.WriteVariant(API_S_GUID, ItemGuid.ToVariant(Opts.Flags & SVarWriteOpt::eTextGuids, Preset.Allocator()));

		Item.Write(API_S_TYPE, ItemTypeToString(ItemPreset.Type));

		switch (ItemPreset.Activate)
		{
		case SItemPreset::EActivate::eUndefined:	Item.Write(API_S_ACTION, API_S_ACTION_UNDEFINED); break;
		case SItemPreset::EActivate::eEnable:		Item.Write(API_S_ACTION, API_S_ACTION_ENABLE); break;
		case SItemPreset::EActivate::eDisable:		Item.Write(API_S_ACTION, API_S_ACTION_DISABLE); break;
		}
		
		Items.WriteVariant(Item.Finish());
	}
	Preset.WriteVariant(API_S_ITEMS, Items.Finish());

	Preset.WriteVariant(API_S_DATA, m_Data);
}

void CPreset::ReadMValue(const SVarName& Name, const XVariant& Data)
{
	if (VAR_TEST_NAME(Name, API_S_GUID))			m_Guid.FromVariant(Data);
	else if (VAR_TEST_NAME(Name, API_S_NAME))		m_Name = AS_STR(Data);
	else if (VAR_TEST_NAME(Name, API_S_INFO))		m_Description = AS_STR(Data);

	else if (VAR_TEST_NAME(Name, API_S_IS_ACTIVE))	m_bIsActive = Data.To<bool>();
	
	else if (VAR_TEST_NAME(Name, API_S_USE_SCRIPT))	m_bUseScript = Data.To<bool>();
	else if (VAR_TEST_NAME(Name, API_S_SCRIPT))		AS_STR_A(m_Script, Data);


	else if (VAR_TEST_NAME(Name, API_S_ITEMS))
	{
		m_Items.clear();

		VariantReader(Data).ReadRawList([this](const XVariant& ItemData)
		{
			QFlexGuid ItemGuid;
			SItemPreset Item;
			VariantReader(ItemData).ReadRawMap([&ItemGuid, &Item](const SVarName& Name, const XVariant& Value)
			{
				if (VAR_TEST_NAME(Name, API_S_GUID))
					ItemGuid.FromVariant(Value);
				else if (VAR_TEST_NAME(Name, API_S_TYPE))
				{
					ASTR Type = Value;
#ifdef PRESET_SVC
					Item.Type = ItemTypeFromString(Type.c_str());
#else
					Item.Type = ItemTypeFromString(Type.toStdString().c_str());
#endif
				}
				else if (VAR_TEST_NAME(Name, API_S_ACTION))
				{
					ASTR Action = Value;
					if (Action == API_S_ACTION_UNDEFINED)		Item.Activate = SItemPreset::EActivate::eUndefined;
					else if (Action == API_S_ACTION_ENABLE)		Item.Activate = SItemPreset::EActivate::eEnable;
					else if (Action == API_S_ACTION_DISABLE)	Item.Activate = SItemPreset::EActivate::eDisable;
				}
			});
			
			if (!ItemGuid.IsNull() /*&& Item.Type != EItemType::eUnknown*/)
#ifdef PRESET_SVC
				m_Items.insert(std::make_pair(ItemGuid, Item));
#else
				m_Items.insert(ItemGuid, Item);
#endif
		});
	}

	else if (VAR_TEST_NAME(Name, API_S_DATA))		m_Data = Data;
}

// Helper functions to serialize/deserialize EItemType to/from string
const char* ItemTypeToString(EItemType Type)
{
	switch (Type)
	{
	case EItemType::eUnknown:	return API_S_ITEM_TYPE_UNKNOWN;
	case EItemType::eExecRule:	return API_S_ITEM_TYPE_PROGRAM;
	case EItemType::eResRule:	return API_S_ITEM_TYPE_ACCESS;
	case EItemType::eFwRule:	return API_S_ITEM_TYPE_FIREWALL;
	case EItemType::eDnsRule:	return API_S_ITEM_TYPE_DNS;
	case EItemType::eEnclave:	return API_S_ITEM_TYPE_ENCLAVE;
	case EItemType::eVolume:	return API_S_ITEM_TYPE_VOLUME;
	case EItemType::eTweak:		return API_S_ITEM_TYPE_TWEAK;
	case EItemType::ePreset:	return API_S_ITEM_TYPE_PRESET;
	default:					return API_S_ITEM_TYPE_UNKNOWN;
	}
}

EItemType ItemTypeFromString(const char* Str)
{
	if (!Str)
		return EItemType::eUnknown;

	if (strcmp(Str, API_S_ITEM_TYPE_PROGRAM) == 0)		return EItemType::eExecRule;
	if (strcmp(Str, API_S_ITEM_TYPE_ACCESS) == 0)		return EItemType::eResRule;
	if (strcmp(Str, API_S_ITEM_TYPE_FIREWALL) == 0)		return EItemType::eFwRule;
	if (strcmp(Str, API_S_ITEM_TYPE_DNS) == 0)			return EItemType::eDnsRule;
	if (strcmp(Str, API_S_ITEM_TYPE_ENCLAVE) == 0)		return EItemType::eEnclave;
	if (strcmp(Str, API_S_ITEM_TYPE_VOLUME) == 0)		return EItemType::eVolume;
	if (strcmp(Str, API_S_ITEM_TYPE_TWEAK) == 0)		return EItemType::eTweak;
	if (strcmp(Str, API_S_ITEM_TYPE_PRESET) == 0)		return EItemType::ePreset;
	if (strcmp(Str, API_S_ITEM_TYPE_UNKNOWN) == 0)		return EItemType::eUnknown;

	// Backward compatibility with ExecutionRule
	if (strcmp(Str, API_S_ITEM_TYPE_EXECUTION) == 0)	return EItemType::eExecRule;

	return EItemType::eUnknown;
}