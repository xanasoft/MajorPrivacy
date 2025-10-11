;------------------------------------------------------------------------
; Copyright 2021-2023 David Xanatos, xanasoft.com
;----------------------------------------------------------------------------


.386p
.model flat

.code


;----------------------------------------------------------------------------
; Entrypoint
;----------------------------------------------------------------------------


EXTERN 		_EntrypointC@4 : PROC

_Start:		
	call	$+5
_Pos:
	pop		eax

	;
	; call _EntrypointC(LdrCodeData)
	;

	add		eax, offset LdrCodeData - _Pos
	push	eax
	call	_EntrypointC@4

	;
	; jump to LdrInitializeThunk trampoline
	;

	jmp		eax						


;----------------------------------------------------------------------------
; Detour Code loading HookDll
;----------------------------------------------------------------------------


InjectData		struct				; keep in sync with INJECT_DATA
LdrData			dq	?				; 0x00
HookTarget		dq	?				; 0x08
InjectData		ends

EXTERN 		_DetourFunc@4 : PROC

_DetourCode:

	mov		edx, 0					; inject data area
		
	push	esi
	mov		esi, edx				; inject data area -> esi

	;
	; call DetourFunc
	;
		
	push	esi
	mov		esi, dword ptr [esi].InjectData.HookTarget ; save HookTarget so that ExtraData can be freed
	call	_DetourFunc@4

	;
	; resume execution of original function
	;

	mov		eax, esi
	pop		esi

	jmp		eax


;----------------------------------------------------------------------------
; Parameters stored by Service
;----------------------------------------------------------------------------


LdrCodeData	LABEL	QWORD

	dq	48 dup (0)					; at least sizeof(LDRCODE_DATA)


;----------------------------------------------------------------------------
; Tail Signature
;----------------------------------------------------------------------------


.code	zzzz

	dq	_Start
	dq   LdrCodeData
	dq	_DetourCode
	

;----------------------------------------------------------------------------
end
