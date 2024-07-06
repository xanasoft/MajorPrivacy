#pragma once

typedef struct _MSG_HEADER {

    //ULONG Magic;
    //USHORT Version;
    //USHORT Offset;    // data offset after ext header

    ULONG 
        Size : 31,
        Flag : 1;
    union
    {
        ULONG MessageId;
        NTSTATUS Status;
    };

} MSG_HEADER, *PMSG_HEADER;



/*typedef struct _MSG_HEADER_EX {

    ULONG RequestId;

    LARGE_INTEGER TimeStamp;

} MSG_HEADER_EX;*/
