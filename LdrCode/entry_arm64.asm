;------------------------------------------------------------------------
; Copyright 2022-2023 David Xanatos, xanasoft.com
;------------------------------------------------------------------------


	AREA |.text|, CODE, READONLY

    IMPORT  EntrypointC

    IMPORT  DetourFunc

    ;EXPORT  InjectDataPtr
    EXPORT  DetourCodeARM64

    EXPORT  LdrCodeData

    EXPORT  _InterlockedCompareExchange64
    EXPORT  _InterlockedExchange64

;----------------------------------------------------------------------------
; _InterlockedCompareExchange64:
;   VOID* _InterlockedCompareExchange64(volatile LONG64* dest, LONG64 exchange, LONG64 comparand)
;   Returns original *dest in X0
;----------------------------------------------------------------------------
_InterlockedCompareExchange64 PROC
    DMB     SY                 ; full barrier

cmp_loop
    LDAXR   X3, [X0]            ; X3 = old = *dest
    CMP     X3, X2              ; compare old vs comparand
    BNE     exit_cmp            ; if not equal, done

    STLXR   W4, X1, [X0]        ; attempt *dest = exchange
    CBNZ    W4, cmp_loop        ; retry on failure

    DMB     SY                  ; barrier after store

exit_cmp
    MOV     X0, X3              ; return old in X0
    RET
_InterlockedCompareExchange64 ENDP

;----------------------------------------------------------------------------
; _InterlockedExchange64:
;   LONG64 _InterlockedExchange64(volatile LONG64* target, LONG64 value)
;   Returns original *target in X0
;----------------------------------------------------------------------------
_InterlockedExchange64 PROC
    DMB     SY                  ; full barrier

exchange_loop
    LDAXR   X3, [X0]            ; X3 = old = *target
    STLXR   W4, X1, [X0]        ; attempt *target = value
    CBNZ    W4, exchange_loop   ; retry on failure

    DMB     SY                  ; barrier after store
    MOV     X0, X3              ; return old in X0
    RET
_InterlockedExchange64 ENDP

;----------------------------------------------------------------------------
; Entrypoint
;----------------------------------------------------------------------------


Start PROC

    ;brk     #0xF000

    stp     x0, x1, [sp, #-0x10]!
    stp     x2, x3, [sp, #-0x10]!
    stp     x4, x5, [sp, #-0x10]!
    stp     x6, x7, [sp, #-0x10]!
    stp     fp, lr, [sp, #-0x10]!  

    ;
    ; call EntrypointC()
    ;

    bl      EntrypointC

    mov     x8, x0

    ;
    ; restore registers and jump to LdrInitializeThunk trampoline
    ;

    ldp     fp, lr, [sp], #0x10
    ldp     x6, x7, [sp], #0x10
    ldp     x4, x5, [sp], #0x10
    ldp     x2, x3, [sp], #0x10
    ldp     x0, x1, [sp], #0x10

    br      x8

 ENDP


;----------------------------------------------------------------------------
; Detour Code loading HookDll
;----------------------------------------------------------------------------


InjectDataPtr
    DCQ 0
DetourCodeARM64 PROC
    
    ;brk     #0xF000
    
    stp     fp, lr, [sp, #-0x10]!  
    stp     x19, x20, [sp, #-0x10]! 
    sub     sp, sp, #0x40

    stp     x0, x1, [sp, #0x00]
    stp     x2, x3, [sp, #0x10]
    stp     x4, x5, [sp, #0x20]
    stp     x6, x7, [sp, #0x30]

    ldr     x19, InjectDataPtr      ; inject data area

	;
	; call DetourFunc
	;

    mov     x0, x19
    ldr     x19, [x19, 0x08]        ; [x19].InjectData.HookTarget ; save HookTarget so that ExtraData can be freed
    bl      DetourFunc

	;
	; resume execution of original function
	;

    ldp     x0, x1, [sp, #0x00]
    ldp     x2, x3, [sp, #0x10]
    ldp     x4, x5, [sp, #0x20]
    ldp     x6, x7, [sp, #0x30]

    mov     x8, x19

    add     sp, sp, #0x40
    ldp     x19, x20, [sp], #0x10
    ldp     fp, lr, [sp], #0x10

    br      x8

 ENDP

 
;----------------------------------------------------------------------------
; Parameters stored by Service
;----------------------------------------------------------------------------


    ALIGN 8

LdrCodeData
    ; at least sizeof(LDRCODE_DATA)
    DCQ 0,0,0,0,0,0,0,0
    DCQ 0,0,0,0,0,0,0,0
    DCQ 0,0,0,0,0,0,0,0
    DCQ 0,0,0,0,0,0,0,0
    DCQ 0,0,0,0,0,0,0,0
    DCQ 0,0,0,0,0,0,0,0
    DCQ 0,0,0,0,0,0,0,0
    DCQ 0,0,0,0,0,0,0,0


;----------------------------------------------------------------------------
; Tail Signature
;----------------------------------------------------------------------------


	AREA |zzzzz|, DATA, READONLY

    DCQ Start                       ; entry point for the detour
    DCQ LdrCodeData                 ; data location
    DCQ DetourCodeARM64             ; detour code location


;----------------------------------------------------------------------------

 END