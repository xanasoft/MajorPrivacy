#include <ph.h>

#include <shellapi.h>
#include <shlobj.h>

#include <appresolver.h>

VOID KphpTpSetPoolThreadBasePriority(
    _Inout_ PTP_POOL Pool,
    _In_ ULONG BasePriority
    )
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static NTSTATUS (NTAPI* TpSetPoolThreadBasePriority_I)(
        _Inout_ PTP_POOL Pool,
        _In_ ULONG BasePriority
        ) = NULL;

    if (PhBeginInitOnce(&initOnce))
    {
        PVOID baseAddress;

        if (baseAddress = PhGetLoaderEntryDllBaseZ(L"ntdll.dll"))
        {
            TpSetPoolThreadBasePriority_I = PhGetDllBaseProcedureAddress(baseAddress, "TpSetPoolThreadBasePriority", 0);
        }

        PhEndInitOnce(&initOnce);
    }

    if (TpSetPoolThreadBasePriority_I)
    {
        TpSetPoolThreadBasePriority_I(Pool, BasePriority);
    }
}

NTSTATUS PhSetFileCompletionNotificationMode(
    _In_ HANDLE FileHandle,
    _In_ ULONG Flags
    )
{
    FILE_IO_COMPLETION_NOTIFICATION_INFORMATION completionMode;
    IO_STATUS_BLOCK isb;

    completionMode.Flags = Flags;

    return NtSetInformationFile(
        FileHandle,
        &isb,
        &completionMode,
        sizeof(FILE_IO_COMPLETION_NOTIFICATION_INFORMATION),
        FileIoCompletionNotificationInformation
        );
}

VOID KphSetServiceSecurity(
    _In_ SC_HANDLE ServiceHandle
    )
{
    PSID administratorsSid = PhSeAdministratorsSid();
    UCHAR securityDescriptorBuffer[SECURITY_DESCRIPTOR_MIN_LENGTH + 0x80];
    PSECURITY_DESCRIPTOR securityDescriptor;
    ULONG sdAllocationLength;
    PACL dacl;

    sdAllocationLength = SECURITY_DESCRIPTOR_MIN_LENGTH +
        (ULONG)sizeof(ACL) +
        (ULONG)sizeof(ACCESS_ALLOWED_ACE) +
        PhLengthSid((PSID)&PhSeServiceSid) +
        (ULONG)sizeof(ACCESS_ALLOWED_ACE) +
        PhLengthSid(administratorsSid) +
        (ULONG)sizeof(ACCESS_ALLOWED_ACE) +
        PhLengthSid((PSID)&PhSeInteractiveSid);

    securityDescriptor = (PSECURITY_DESCRIPTOR)securityDescriptorBuffer;
    dacl = PTR_ADD_OFFSET(securityDescriptor, SECURITY_DESCRIPTOR_MIN_LENGTH);

    RtlCreateSecurityDescriptor(securityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    RtlCreateAcl(dacl, sdAllocationLength - SECURITY_DESCRIPTOR_MIN_LENGTH, ACL_REVISION);
    RtlAddAccessAllowedAce(dacl, ACL_REVISION, SERVICE_ALL_ACCESS, (PSID)&PhSeServiceSid);
    RtlAddAccessAllowedAce(dacl, ACL_REVISION, SERVICE_ALL_ACCESS, administratorsSid);
    RtlAddAccessAllowedAce(dacl, ACL_REVISION,
        SERVICE_QUERY_CONFIG |
        SERVICE_QUERY_STATUS |
        SERVICE_START |
        SERVICE_STOP |
        SERVICE_INTERROGATE |
        DELETE,
        (PSID)&PhSeInteractiveSid
        );
    RtlSetDaclSecurityDescriptor(securityDescriptor, TRUE, dacl, FALSE);

    SetServiceObjectSecurity(ServiceHandle, DACL_SECURITY_INFORMATION, securityDescriptor);

#ifdef DEBUG
    assert(sdAllocationLength < sizeof(securityDescriptorBuffer));
    assert(RtlLengthSecurityDescriptor(securityDescriptor) < sizeof(securityDescriptorBuffer));
#endif
}



//#define RTL_CONSTANT_STRING(s) { wcslen(s) * sizeof(WCHAR), (wcslen(s)+1) * sizeof(WCHAR), s }
#define RTL_CONSTANT_STRING(s) { sizeof(s) - sizeof((s)[0]), sizeof(s), s }

static PH_INITONCE PhPredefineKeyInitOnce = PH_INITONCE_INIT;
static UNICODE_STRING PhPredefineKeyNames[PH_KEY_MAXIMUM_PREDEFINE] =
{
    RTL_CONSTANT_STRING(L"\\Registry\\Machine"),
    RTL_CONSTANT_STRING(L"\\Registry\\User"),
    RTL_CONSTANT_STRING(L"\\Registry\\Machine\\Software\\Classes"),
    { 0, 0, NULL }
};
static HANDLE PhPredefineKeyHandles[PH_KEY_MAXIMUM_PREDEFINE] = { 0 };

/*VOID PhpInitializePredefineKeys(
    VOID
    )
{
    static UNICODE_STRING currentUserPrefix = RTL_CONSTANT_STRING((WCHAR*)L"\\Registry\\User\\");
    NTSTATUS status;
    PTOKEN_USER tokenUser;
    UNICODE_STRING stringSid;
    WCHAR stringSidBuffer[SECURITY_MAX_SID_STRING_CHARACTERS];
    PUNICODE_STRING currentUserKeyName;

    // Get the string SID of the current user.

    if (NT_SUCCESS(status = PhGetTokenUser(PhGetOwnTokenAttributes().TokenHandle, &tokenUser)))
    {
        stringSid.Buffer = stringSidBuffer;
        stringSid.MaximumLength = sizeof(stringSidBuffer);

        status = RtlConvertSidToUnicodeString(
            &stringSid,
            tokenUser->User.Sid,
            FALSE
            );

        PhFree(tokenUser);
    }

    // Construct the current user key name.

    if (NT_SUCCESS(status))
    {
        currentUserKeyName = &PhPredefineKeyNames[PH_KEY_CURRENT_USER_NUMBER];
        currentUserKeyName->Length = currentUserPrefix.Length + stringSid.Length;
        currentUserKeyName->Buffer = PhAllocate(currentUserKeyName->Length + sizeof(UNICODE_NULL));
        memcpy(currentUserKeyName->Buffer, currentUserPrefix.Buffer, currentUserPrefix.Length);
        memcpy(&currentUserKeyName->Buffer[currentUserPrefix.Length / sizeof(WCHAR)], stringSid.Buffer, stringSid.Length);
    }
}*/

NTSTATUS PhpInitializeKeyObjectAttributes(
    _In_opt_ HANDLE RootDirectory,
    _In_ PUNICODE_STRING ObjectName,
    _In_ ULONG Attributes,
    _Out_ POBJECT_ATTRIBUTES ObjectAttributes,
    _Out_ PHANDLE NeedsClose
    )
{
    NTSTATUS status;
    ULONG predefineIndex;
    HANDLE predefineHandle;
    OBJECT_ATTRIBUTES predefineObjectAttributes;

    InitializeObjectAttributes(
        ObjectAttributes,
        ObjectName,
        Attributes | OBJ_CASE_INSENSITIVE,
        RootDirectory,
        NULL
        );

    *NeedsClose = NULL;

    if (RootDirectory && PH_KEY_IS_PREDEFINED(RootDirectory))
    {
        predefineIndex = PH_KEY_PREDEFINE_TO_NUMBER(RootDirectory);

        if (predefineIndex < PH_KEY_MAXIMUM_PREDEFINE)
        {
            if (PhBeginInitOnce(&PhPredefineKeyInitOnce))
            {
                //PhpInitializePredefineKeys();
                PhEndInitOnce(&PhPredefineKeyInitOnce);
            }

            predefineHandle = PhPredefineKeyHandles[predefineIndex];

            if (!predefineHandle)
            {
                // The predefined key has not been opened yet. Do so now.

                if (!PhPredefineKeyNames[predefineIndex].Buffer) // we may have failed in getting the current user key name
                    return STATUS_UNSUCCESSFUL;

                InitializeObjectAttributes(
                    &predefineObjectAttributes,
                    &PhPredefineKeyNames[predefineIndex],
                    OBJ_CASE_INSENSITIVE,
                    NULL,
                    NULL
                    );

                status = NtOpenKey(
                    &predefineHandle,
                    KEY_READ,
                    &predefineObjectAttributes
                    );

                if (!NT_SUCCESS(status))
                    return status;

                if (_InterlockedCompareExchangePointer(
                    &PhPredefineKeyHandles[predefineIndex],
                    predefineHandle,
                    NULL
                    ) != NULL)
                {
                    // Someone else already opened the key and cached it. Indicate that the caller
                    // needs to close the handle later, since it isn't shared.
                    *NeedsClose = predefineHandle;
                }
            }

            ObjectAttributes->RootDirectory = predefineHandle;
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS PhCreateKey(
    _Out_ PHANDLE KeyHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ HANDLE RootDirectory,
    _In_ PPH_STRINGREF ObjectName,
    _In_ ULONG Attributes,
    _In_ ULONG CreateOptions,
    _Out_opt_ PULONG Disposition
    )
{
    NTSTATUS status;
    UNICODE_STRING objectName;
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE needsClose;

    if (!PhStringRefToUnicodeString(ObjectName, &objectName))
        return STATUS_NAME_TOO_LONG;

    if (!NT_SUCCESS(status = PhpInitializeKeyObjectAttributes(
        RootDirectory,
        &objectName,
        Attributes,
        &objectAttributes,
        &needsClose
        )))
    {
        return status;
    }

    status = NtCreateKey(
        KeyHandle,
        DesiredAccess,
        &objectAttributes,
        0,
        NULL,
        CreateOptions,
        Disposition
        );

    
    if (needsClose)
        NtClose(needsClose);

    return status;
}

NTSTATUS PhOpenKey(
    _Out_ PHANDLE KeyHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ HANDLE RootDirectory,
    _In_ PPH_STRINGREF ObjectName,
    _In_ ULONG Attributes
    )
{
    NTSTATUS status;
    UNICODE_STRING objectName;
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE needsClose;

    if (!PhStringRefToUnicodeString(ObjectName, &objectName))
        return STATUS_NAME_TOO_LONG;

    if (!NT_SUCCESS(status = PhpInitializeKeyObjectAttributes(
        RootDirectory,
        &objectName,
        Attributes,
        &objectAttributes,
        &needsClose
        )))
    {
        return status;
    }

    status = NtOpenKey(
        KeyHandle,
        DesiredAccess,
        &objectAttributes
        );

    if (needsClose)
        NtClose(needsClose);

    return status;
}

NTSTATUS PhQueryValueKey(
    _In_ HANDLE KeyHandle,
    _In_opt_ PPH_STRINGREF ValueName,
    _In_ KEY_VALUE_INFORMATION_CLASS KeyValueInformationClass,
    _Out_ PVOID *Buffer
    )
{
    NTSTATUS status;
    UNICODE_STRING valueName;
    PVOID buffer;
    ULONG bufferSize;
    ULONG attempts = 16;

    if (ValueName && ValueName->Length)
    {
        if (!PhStringRefToUnicodeString(ValueName, &valueName))
            return STATUS_NAME_TOO_LONG;
    }
    else
    {
        RtlInitEmptyUnicodeString(&valueName, NULL, 0);
    }

    bufferSize = 0x100;
    buffer = PhAllocate(bufferSize);

    do
    {
        status = NtQueryValueKey(
            KeyHandle,
            &valueName,
            KeyValueInformationClass,
            buffer,
            bufferSize,
            &bufferSize
            );

        if (NT_SUCCESS(status))
            break;

        if (status == STATUS_BUFFER_OVERFLOW)
        {
            PhFree(buffer);
            buffer = PhAllocate(bufferSize);
        }
        else
        {
            PhFree(buffer);
            return status;
        }
    } while (--attempts);

    *Buffer = buffer;

    return status;
}

NTSTATUS PhSetValueKey(
    _In_ HANDLE KeyHandle,
    _In_opt_ PPH_STRINGREF ValueName,
    _In_ ULONG ValueType,
    _In_ PVOID Buffer,
    _In_ ULONG BufferLength
    )
{
    NTSTATUS status;
    UNICODE_STRING valueName;

    if (ValueName && ValueName->Length)
    {
        if (!PhStringRefToUnicodeString(ValueName, &valueName))
            return STATUS_NAME_TOO_LONG;
    }
    else
    {
        RtlInitEmptyUnicodeString(&valueName, NULL, 0);
    }

    status = NtSetValueKey(
        KeyHandle,
        &valueName,
        0,
        ValueType,
        Buffer,
        BufferLength
        );

    return status;
}

NTSTATUS PhGetTokenUser(
    _In_ HANDLE TokenHandle,
    _Out_ PPH_TOKEN_USER User
    )
{
    ULONG returnLength;

    return NtQueryInformationToken(
        TokenHandle,
        TokenUser,
        User,
        sizeof(PH_TOKEN_USER), // SE_TOKEN_USER
        &returnLength
        );
}

FORCEINLINE
NTSTATUS
PhGetTokenElevationType(
    _In_ HANDLE TokenHandle,
    _Out_ PTOKEN_ELEVATION_TYPE ElevationType
    )
{
    ULONG returnLength;

    return NtQueryInformationToken(
        TokenHandle,
        TokenElevationType,
        ElevationType,
        sizeof(TOKEN_ELEVATION_TYPE),
        &returnLength
        );
}

FORCEINLINE
NTSTATUS
PhGetTokenIsElevated(
    _In_ HANDLE TokenHandle,
    _Out_ PBOOLEAN Elevated
    )
{
    NTSTATUS status;
    TOKEN_ELEVATION elevation;
    ULONG returnLength;

    status = NtQueryInformationToken(
        TokenHandle,
        TokenElevation,
        &elevation,
        sizeof(TOKEN_ELEVATION),
        &returnLength
        );

    if (NT_SUCCESS(status))
    {
        *Elevated = !!elevation.TokenIsElevated;
    }

    return status;
}

PH_TOKEN_ATTRIBUTES PhGetOwnTokenAttributes(
    VOID
    )
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static PH_TOKEN_ATTRIBUTES attributes = { 0 };

    if (PhBeginInitOnce(&initOnce))
    {
        BOOLEAN elevated = TRUE;
        TOKEN_ELEVATION_TYPE elevationType = TokenElevationTypeFull;

        if (WindowsVersion >= WINDOWS_8_1)
        {
            attributes.TokenHandle = NtCurrentProcessToken();
        }
        else
        {
            HANDLE tokenHandle;

            if (NT_SUCCESS(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tokenHandle)))
                attributes.TokenHandle = tokenHandle;
        }

        if (attributes.TokenHandle)
        {
            PH_TOKEN_USER tokenUser;

            PhGetTokenIsElevated(attributes.TokenHandle, &elevated);
            PhGetTokenElevationType(attributes.TokenHandle, &elevationType);

            if (NT_SUCCESS(PhGetTokenUser(attributes.TokenHandle, &tokenUser)))
            {
                attributes.TokenSid = PhAllocateCopy(tokenUser.User.Sid, PhLengthSid(tokenUser.User.Sid));
            }
        }

        attributes.Elevated = elevated;
        attributes.ElevationType = elevationType;

        PhEndInitOnce(&initOnce);
    }

    return attributes;
}

NTSTATUS PhGetSystemLogicalProcessorInformation(
    _In_ LOGICAL_PROCESSOR_RELATIONSHIP RelationshipType,
    _Out_ PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *Buffer,
    _Out_ PULONG BufferLength
    )
{
    static ULONG initialBufferSize[] = { 0x200, 0x80, 0x100, 0x1000 };
    NTSTATUS status;
    ULONG classIndex;
    PVOID buffer;
    ULONG bufferSize;
    ULONG attempts;

    switch (RelationshipType)
    {
    case RelationProcessorCore:
        classIndex = 0;
        break;
    case RelationProcessorPackage:
        classIndex = 1;
        break;
    case RelationGroup:
        classIndex = 2;
        break;
    case RelationAll:
        classIndex = 3;
        break;
    default:
        return STATUS_INVALID_INFO_CLASS;
    }

    bufferSize = initialBufferSize[classIndex];
    buffer = PhAllocate(bufferSize);

    status = NtQuerySystemInformationEx(
        SystemLogicalProcessorAndGroupInformation,
        &RelationshipType,
        sizeof(LOGICAL_PROCESSOR_RELATIONSHIP),
        buffer,
        bufferSize,
        &bufferSize
        );
    attempts = 0;

    while (status == STATUS_INFO_LENGTH_MISMATCH && attempts < 8)
    {
        PhFree(buffer);
        buffer = PhAllocate(bufferSize);

        status = NtQuerySystemInformationEx(
            SystemLogicalProcessorAndGroupInformation,
            &RelationshipType,
            sizeof(LOGICAL_PROCESSOR_RELATIONSHIP),
            buffer,
            bufferSize,
            &bufferSize
            );
        attempts++;
    }

    if (!NT_SUCCESS(status))
    {
        PhFree(buffer);
        return status;
    }

    if (bufferSize <= 0x100000) initialBufferSize[classIndex] = bufferSize;
    *Buffer = buffer;
    *BufferLength = bufferSize;

    return status;
}


NTSTATUS PhSetThreadBasePriority(
    _In_ HANDLE ThreadHandle,
    _In_ KPRIORITY Increment
    )
{
    NTSTATUS status;

    status = NtSetInformationThread(
        ThreadHandle,
        ThreadBasePriority,
        &Increment,
        sizeof(KPRIORITY)
        );

    return status;
}

NTSTATUS PhSetThreadIoPriority(
    _In_ HANDLE ThreadHandle,
    _In_ IO_PRIORITY_HINT IoPriority
    )
{
    NTSTATUS status;

    status = NtSetInformationThread(
        ThreadHandle,
        ThreadIoPriority,
        &IoPriority,
        sizeof(IO_PRIORITY_HINT)
        );

    return status;
}

NTSTATUS PhSetThreadPagePriority(
    _In_ HANDLE ThreadHandle,
    _In_ ULONG PagePriority
    )
{
    NTSTATUS status;
    PAGE_PRIORITY_INFORMATION pagePriorityInfo;

    pagePriorityInfo.PagePriority = PagePriority;

    status = NtSetInformationThread(
        ThreadHandle,
        ThreadPagePriority,
        &pagePriorityInfo,
        sizeof(PAGE_PRIORITY_INFORMATION)
        );

    return status;
}


PPH_STRING PhSidToStringSid(
    _In_ PSID Sid
    )
{
    PPH_STRING string;
    UNICODE_STRING us;

    string = PhCreateStringEx(NULL, SECURITY_MAX_SID_STRING_CHARACTERS * sizeof(WCHAR));
    PhStringRefToUnicodeString(&string->sr, &us);

    if (NT_SUCCESS(RtlConvertSidToUnicodeString(
        &us,
        Sid,
        FALSE
        )))
    {
        string->Length = us.Length;
        string->Buffer[us.Length / sizeof(WCHAR)] = UNICODE_NULL;

        return string;
    }
    else
    {
        PhDereferenceObject(string);

        return NULL;
    }
}


DECLSPEC_SELECTANY WCHAR *PhSizeUnitNames[7] = { L"B", L"kB", L"MB", L"GB", L"TB", L"PB", L"EB" };
DECLSPEC_SELECTANY ULONG PhMaxSizeUnit = ULONG_MAX;
DECLSPEC_SELECTANY USHORT PhMaxPrecisionUnit = 2;

PPH_STRING PhQueryRegistryString(
    _In_ HANDLE KeyHandle,
    _In_opt_ PPH_STRINGREF ValueName
    )
{
    PPH_STRING string = NULL;
    PKEY_VALUE_PARTIAL_INFORMATION buffer;

    if (NT_SUCCESS(PhQueryValueKey(KeyHandle, ValueName, KeyValuePartialInformation, &buffer)))
    {
        if (buffer->Type == REG_SZ ||
            buffer->Type == REG_MULTI_SZ ||
            buffer->Type == REG_EXPAND_SZ)
        {
            if (buffer->DataLength >= sizeof(UNICODE_NULL))
                string = PhCreateStringEx((PWCHAR)buffer->Data, buffer->DataLength - sizeof(UNICODE_NULL));
            else
                string = PhReferenceEmptyString();
        }

        PhFree(buffer);
    }

    return string;
}


HRESULT PhShellGetKnownFolderItem(
    _In_ PCGUID rfid,
    _In_ ULONG Flags,
    _In_opt_ HANDLE TokenHandle,
    _In_ REFIID riid,
    _Outptr_ PVOID* ppv
    )
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static HRESULT (WINAPI *SHGetKnownFolderItem_I)(
        _In_ REFKNOWNFOLDERID rfid,
        _In_ ULONG flags, // KNOWN_FOLDER_FLAG
        _In_opt_ HANDLE hToken,
        _In_ REFIID riid,
        _Outptr_ void** ppv
        ) = NULL;

    if (PhBeginInitOnce(&initOnce))
    {
        PVOID baseAddress;

        if (baseAddress = PhLoadLibrary(L"shell32.dll"))
        {
            SHGetKnownFolderItem_I = PhGetDllBaseProcedureAddress(baseAddress, "SHGetKnownFolderItem", 0);
        }

        PhEndInitOnce(&initOnce);
    }

    if (!SHGetKnownFolderItem_I)
        return E_FAIL;

    return SHGetKnownFolderItem_I(rfid, Flags, TokenHandle, riid, ppv);
}






HRESULT PhGetClassObjectDllBase(
    _In_ PVOID DllBase,
    _In_ REFCLSID Rclsid,
    _In_ REFIID Riid,
    _Out_ PVOID* Ppv
    )
{
    HRESULT (WINAPI* DllGetClassObject_I)(_In_ REFCLSID rclsid, _In_ REFIID riid, _COM_Outptr_ PVOID* ppv);
    HRESULT status;
    IClassFactory* classFactory;

    if (!(DllGetClassObject_I = PhGetDllBaseProcedureAddress(DllBase, "DllGetClassObject", 0)))
        return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);

    status = DllGetClassObject_I(
        Rclsid,
        &IID_IClassFactory,
        &classFactory
        );

    if (SUCCEEDED(status))
    {
        status = IClassFactory_CreateInstance(
            classFactory,
            NULL,
            Riid,
            Ppv
            );
        IClassFactory_Release(classFactory);
    }

    return status;
}

/**
 * \brief Provides a pointer to an interface on a class object associated with a specified CLSID.
 * Most class objects implement the IClassFactory interface. You would then call CreateInstance to
 * create an uninitialized object. It is not always necessary to go through this process however.
 *
 * \param DllName The file containing the object to initialize.
 * \param Rclsid A pointer to the class identifier of the object to be created.
 * \param Riid Reference to the identifier of the interface to communicate with the class object.
 * \a Typically this value is IID_IClassFactory, although other values such as IID_IClassFactory2 which supports a form of licensing are allowed.
 * \param Ppv The address of pointer variable that contains the requested interface.
 *
 * \return Successful or errant status.
 */
HRESULT PhGetClassObject(
    _In_ PCWSTR DllName,
    _In_ REFCLSID Rclsid,
    _In_ REFIID Riid,
    _Out_ PVOID* Ppv
    )
{
#if (PH_NATIVE_COM_CLASS_FACTORY)
    HRESULT status;
    IClassFactory* classFactory;

    status = CoGetClassObject(
        Rclsid,
        CLSCTX_INPROC_SERVER,
        NULL,
        &IID_IClassFactory,
        &classFactory
        );

    if (SUCCEEDED(status))
    {
        status = IClassFactory_CreateInstance(
            classFactory,
            NULL,
            Riid,
            Ppv
            );
        IClassFactory_Release(classFactory);
    }

    return status;
#else
    PVOID baseAddress;

    if (!(baseAddress = PhGetLoaderEntryDllBaseZ((PWSTR)DllName)))
    {
        if (!(baseAddress = PhLoadLibrary(DllName)))
            return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    }

    return PhGetClassObjectDllBase(baseAddress, Rclsid, Riid, Ppv);
#endif
}

HRESULT PhGetActivationFactoryDllBase(
    _In_ PVOID DllBase,
    _In_ PCWSTR RuntimeClass,
    _In_ REFIID Riid,
    _Out_ PVOID* Ppv
    )
{
    HRESULT (WINAPI* DllGetActivationFactory_I)(_In_ HSTRING RuntimeClassId, _Out_ PVOID* ActivationFactory);
    HRESULT status;
    HSTRING_REFERENCE string;
    IActivationFactory* activationFactory;

    if (FAILED(status = PhCreateWindowsRuntimeStringReference(RuntimeClass, &string)))
        return status;

    if (!(DllGetActivationFactory_I = PhGetDllBaseProcedureAddress(DllBase, "DllGetActivationFactory", 0)))
        return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);

    status = DllGetActivationFactory_I(
        HSTRING_FROM_STRING(string),
        &activationFactory
        );

    if (SUCCEEDED(status))
    {
        status = IActivationFactory_QueryInterface(
            activationFactory,
            Riid,
            Ppv
            );
        IActivationFactory_Release(activationFactory);
    }

    return status;
}

/**
 * \brief Activates the specified Windows Runtime class.
 *
 * \param DllName The file containing the object to initialize.
 * \param RuntimeClass The class identifier string that is associated with the activatable runtime class.
 * \param Riid The reference ID of the interface.
 * \param Ppv The activation factory.
 *
 * \return Successful or errant status.
 */
HRESULT PhGetActivationFactory(
    _In_ PCWSTR DllName,
    _In_ PCWSTR RuntimeClass,
    _In_ REFIID Riid,
    _Out_ PVOID* Ppv
    )
{
#if (PH_NATIVE_COM_CLASS_FACTORY || PH_BUILD_MSIX)
    #include <roapi.h>
    #include <winstring.h>
    HRESULT status;
    HSTRING runtimeClassStringHandle = NULL;
    HSTRING_HEADER runtimeClassStringHeader;

    status = WindowsCreateStringReference(
        RuntimeClass,
        (UINT32)PhCountStringZ((PWSTR)RuntimeClass),
        &runtimeClassStringHeader,
        &runtimeClassStringHandle
        );

    if (SUCCEEDED(status))
    {
        status = RoGetActivationFactory(
            runtimeClassStringHandle,
            Riid,
            Ppv
            );
    }

    return status;
#else
    PVOID baseAddress;

    if (!(baseAddress = PhGetLoaderEntryDllBaseZ((PWSTR)DllName)))
    {
        if (!(baseAddress = PhLoadLibrary(DllName)))
            return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    }

    return PhGetActivationFactoryDllBase(baseAddress, RuntimeClass, Riid, Ppv);
#endif
}

HRESULT PhActivateInstanceDllBase(
    _In_ PVOID DllBase,
    _In_ PCWSTR RuntimeClass,
    _In_ REFIID Riid,
    _Out_ PVOID* Ppv
    )
{
    HRESULT (WINAPI* DllGetActivationFactory_I)(_In_ HSTRING RuntimeClassId, _Out_ PVOID * ActivationFactory);
    HRESULT status;
    HSTRING_REFERENCE string;
    IActivationFactory* activationFactory;
    IInspectable* inspectableObject;

    if (FAILED(status = PhCreateWindowsRuntimeStringReference(RuntimeClass, &string)))
        return status;

    if (!(DllGetActivationFactory_I = PhGetDllBaseProcedureAddress(DllBase, "DllGetActivationFactory", 0)))
        return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);

    status = DllGetActivationFactory_I(
        HSTRING_FROM_STRING(string),
        &activationFactory
        );

    if (SUCCEEDED(status))
    {
        status = IActivationFactory_ActivateInstance(
            activationFactory,
            &inspectableObject
            );

        if (SUCCEEDED(status))
        {
            status = IInspectable_QueryInterface(
                inspectableObject,
                Riid,
                Ppv
                );

            IInspectable_Release(inspectableObject);
        }

        IActivationFactory_Release(activationFactory);
    }

    return status;
}

/**
 * \brief Activates the specified Windows Runtime class.
 *
 * \param DllName The file containing the object to initialize.
 * \param RuntimeClass The class identifier string that is associated with the activatable runtime class.
 * \param Riid The reference ID of the interface.
 * \param Ppv A pointer to the activated instance of the runtime class.
 *
 * \return Successful or errant status.
 */
HRESULT PhActivateInstance(
    _In_ PCWSTR DllName,
    _In_ PCWSTR RuntimeClass,
    _In_ REFIID Riid,
    _Out_ PVOID* Ppv
    )
{
#if (PH_NATIVE_COM_CLASS_FACTORY || PH_BUILD_MSIX)
    #include <roapi.h>
    #include <winstring.h>
    HRESULT status;
    HSTRING runtimeClassStringHandle = NULL;
    HSTRING_HEADER runtimeClassStringHeader;
    IInspectable* inspectableObject;

    status = WindowsCreateStringReference(
        RuntimeClass,
        (UINT32)PhCountStringZ((PWSTR)RuntimeClass),
        &runtimeClassStringHeader,
        &runtimeClassStringHandle
        );

    if (SUCCEEDED(status))
    {
        status = RoActivateInstance(
            runtimeClassStringHandle,
            &inspectableObject
            );

        if (SUCCEEDED(status))
        {
            status = IInspectable_QueryInterface(
                inspectableObject,
                Riid,
                Ppv
                );
            IInspectable_Release(inspectableObject);
        }
    }

    return status;
#else
    PVOID baseAddress;

    if (!(baseAddress = PhGetLoaderEntryDllBaseZ((PWSTR)DllName)))
    {
        if (!(baseAddress = PhLoadLibrary(DllName)))
            return HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
    }

    return PhActivateInstanceDllBase(baseAddress, RuntimeClass, Riid, Ppv);
#endif
}

