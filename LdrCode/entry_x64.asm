;------------------------------------------------------------------------
; Copyright 2021-2023 David Xanatos, xanasoft.com
;----------------------------------------------------------------------------


.code 


;----------------------------------------------------------------------------
; Entrypoint
;----------------------------------------------------------------------------


EXTERN 		EntrypointC : PROC

Start:
	sub		rsp, 28h
	mov		qword ptr [rsp+4*8], rcx
	mov		qword ptr [rsp+5*8], rdx
	mov		qword ptr [rsp+6*8], r8
	mov		qword ptr [rsp+7*8], r9

    db 48h, 8dh, 0dh, 0, 0, 0, 0	; lea rcx, [rip+0]
Pos:

	;
	; call EntrypointC(LdrCodeData)
	;

	add		rcx, offset LdrCodeData - Pos
	call	EntrypointC
		
	;
	; restore registers and jump to LdrInitializeThunk trampoline
	;

	mov		rcx, qword ptr [rsp+4*8]
	mov		rdx, qword ptr [rsp+5*8]
	mov		r8, qword ptr [rsp+6*8]
	mov		r9, qword ptr [rsp+7*8]
	add		rsp, 28h

	jmp		rax


;----------------------------------------------------------------------------
; Detour Code loading HookDll
;----------------------------------------------------------------------------


InjectData		struct				; keep in sync with INJECT_DATA
LdrData			dq	?				; 0x00
HookTarget		dq	?				; 0x08
InjectData		ends

EXTERN 		DetourFunc : PROC

	dq 0h							; inject data area address
DetourCode:
	mov		rax, qword ptr [$-8]	; inject data area -> rax
		
	push	rsi						; save rsi, and align stack
	sub		rsp, 8*8				; set up local stack
		
	mov		qword ptr [rsp+4*8], rcx
	mov		qword ptr [rsp+5*8], rdx
	mov		qword ptr [rsp+6*8], r8
	mov		qword ptr [rsp+7*8], r9

	mov  	rsi, rax  				; inject data area -> rsi
		
	;
	; call DetourFunc
	;

	mov		rcx, rsi
	xor		rdx, rdx
	xor		r8, r8
	xor		r9, r9
	mov		rsi, qword ptr [rsi].InjectData.HookTarget ; save HookTarget so that ExtraData can be freed
	call	DetourFunc

	;
	; resume execution of original function
	;

	mov		rcx, qword ptr [rsp+4*8]
	mov		rdx, qword ptr [rsp+5*8]
	mov		r8, qword ptr [rsp+6*8]
	mov		r9, qword ptr [rsp+7*8]
		
	add		rsp, 8*8
	mov		rax, rsi
	pop		rsi

	jmp		rax


;----------------------------------------------------------------------------
; Parameters stored by Service
;----------------------------------------------------------------------------


LdrCodeData	LABEL	QWORD

	dq	48 dup (0)					; at least sizeof(LDRCODE_DATA)


;----------------------------------------------------------------------------
; Tail Signature
;----------------------------------------------------------------------------


.code	zzzz
	
	dq	Start
	dq  LdrCodeData
	dq	DetourCode
	

;----------------------------------------------------------------------------
end
