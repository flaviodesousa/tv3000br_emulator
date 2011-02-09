;----------------------------------------------------------------------------
; Emulador local de terminal SISCO TV3000/BR - Modulo Direcionador
; Flavio de Sousa - Marco de 1.990
;----------------------------------------------------------------------------

                   TITLE     EMTV3000.ASM
                   NAME      EMTV3000

SAVEREGS           MACRO
                   push      bp
                   push      ds
                   push      es
                   push      si
                   push      di
                   pushf
                   ENDM

RESTREGS           MACRO
                   popf
                   pop       di
                   pop       si
                   pop       es
                   pop       ds
                   pop       bp
                   ENDM

_TEXT              SEGMENT   BYTE PUBLIC 'CODE'

DGROUP             GROUP     _DATA, _BSS
                   ASSUME    cs: _TEXT, ds: DGROUP, ss: DGROUP

                   EXTRN     _choosefunction:PROC

                   PUBLIC    _handler
_handler           PROC      FAR

                   SAVEREGS
                   push      ax

                   push      cs
                   pop       ds

                   call      _choosefunction ; ativa codigo C

                   or        ax,ax ; testa se nao houve processamento (ax = 0)
                   jnz       return

                   pop       ax
                   RESTREGS
                   jmp       DWORD PTR cs:_old_vector ; continua...

return:
                   add       sp, 2
                   RESTREGS
                   iret

_handler           ENDP

_TEXT              ENDS

_DATA              SEGMENT   WORD PUBLIC 'DATA'
                   EXTRN     _old_vector:DWORD
_DATA              ENDS

_BSS               SEGMENT   WORD PUBLIC 'BSS'
_BSS               ENDS

END
