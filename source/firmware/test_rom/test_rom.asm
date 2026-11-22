; ============================================================================
; MCS-48 Diagnostic Test ROM (mcs48.test.diagnostic)
; Conforme a la especificacion normativa docs/TEST_ROM_SPEC.md
; ============================================================================

    ORG 0x0000
reset_vector:
    JMP init_sequence

    ORG 0x0003
ext_int_vector:
    RETR

    ORG 0x0007
timer_int_vector:
    RETR

    ORG 0x0010
init_sequence:
    CLR A
    MOV PSW, A          ; SP=0, RB0

    ; Prueba y borrado de RAM interna (0x08 .. 0x7F)
    MOV R0, #0x7F
ram_clear_loop:
    MOV @R0, A
    DJNZ R0, ram_clear_loop

    ; Inicializacion de temporizador interno
    MOV A, #0x80
    MOV T, A
    STRT T

    ; Prueba de salto interbanco hacia Banco 1
    SEL MB1
    JMP 0x0820         ; Salto a banco 1 (offset 0x020)

    ORG 0x0040
bank0_reentry:
    ; Comprueba si Banco 1 escribio el testigo esperado en R7 (0xA5)
    MOV A, R7
    XRL A, #0xA5
    JNZ fault_halt

    ; Inicializa acumulador para el generador de rampa de audio
    CLR A
    JMP audio_loop

    ORG 0x0050
audio_loop:
    OUTL P1, A          ; Escribe valor en DAC de Puerto 1
    ADD A, #0x10        ; Incrementa en saltos de 16 (rampa de 16 pasos)
    JMP audio_loop

    ORG 0x0060
fault_halt:
    MOV A, #0xEE
    OUTL P2, A
    JMP fault_halt

; ============================================================================
; BANCO 1 (ORG 0x0800)
; ============================================================================
    ORG 0x0800
bank1_header:
    NOP
    NOP

    ORG 0x0820
bank1_entry:
    MOV R7, #0xA5       ; Testigo de ejecucion en Banco 1
    SEL MB0             ; Prepara conmutacion a Banco 0
    JMP 0x0040          ; Salto de retorno a Banco 0
