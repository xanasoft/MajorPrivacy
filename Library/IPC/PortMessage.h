#pragma once

#define USE_ALPC

typedef struct _MSG_HEADER {

    //ULONG Magic;
    //USHORT Version;
    //USHORT Offset;    // data offset after ext header

    ULONG Size;

    union
    {
        ULONG MessageId;
        NTSTATUS Status;
    };

    /*union
    {
      ULONG Flag      : 1,
            Unused    : 31;
    };

	ULONG Reserved;*/

} MSG_HEADER, *PMSG_HEADER;

/*typedef struct _MSG_HEADER_EX {

ULONG RequestId;

LARGE_INTEGER TimeStamp;

} MSG_HEADER_EX;*/

typedef struct _PIPE_MSG_HEADER {

    MSG_HEADER h;

    union
    {
        ULONG IsEvent : 1,
            Unused    : 31;
    };

    ULONG Reserved;

} PIPE_MSG_HEADER, *PPIPE_MSG_HEADER;

typedef struct _PORT_MSG_HEADER {

    MSG_HEADER h;

    ULONG Sequence;

    ULONG Reserved;

} PORT_MSG_HEADER, *PPORT_MSG_HEADER;

