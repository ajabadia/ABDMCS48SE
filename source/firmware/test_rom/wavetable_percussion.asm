; ============================================================================
; MCS-48 Lo-Fi Percussion & Wavetable Synthesizer
; id: mcs48.musical.wavetable_percussion
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
    MOV PSW, A          ; Reset stack pointer, Bank 0
    MOV R0, #0x00       ; R0: Acumulador de fase (0..63)
    MOV R1, #0x02       ; R1: Incremento de pitch
    MOV R2, #0x5A       ; R2: Semilla LFSR de pseudo-ruido (distinta de cero)
    MOV R3, #0x80       ; R3: Nivel de envolvente
    MOV R4, #0x00       ; R4: Registro temporal / copia entrada
    MOV R5, #0x10       ; R5: Contador de barrido de tono para Kick
    MOV R6, #0x00       ; R6: Divisor / acumulador temporal
    MOV R7, #0x00       ; R7: Selector de tabla (0x00, 0x40, 0x80, 0xC0)

    ; Arranca temporizador interno
    MOV A, #0x80
    MOV T, A
    STRT T
    JMP main_engine_loop

    ORG 0x0030
main_engine_loop:
    ; 1. Leer Modo y Control desde Puerto 2
    IN A, P2
    MOV R4, A           ; Guarda copia de P2 en R4
    ANL A, #0xC0        ; Aisla bits 7-6 (Modo)
    JZ run_wavetable    ; 00 -> Modo Wavetable (0x0060)
    XRL A, #0x40
    JZ run_kick         ; 01 -> Modo Kick Drum (0x0090)
    XRL A, #0xC0        ; 0x40 ^ 0x80 = 0xC0
    JZ run_snare        ; 10 -> Modo Snare Drum (0x00C0)
    JMP run_hihat       ; 11 -> Modo Hi-Hat (0x00F0)

    ORG 0x0060
run_wavetable:
    ; Lee Puerto 1 para seleccion de tabla (bits 0-1)
    IN A, P1
    ANL A, #0x03        ; Indice de tabla 0..3
    RR A
    RR A                ; Rota a la derecha 2 veces: (index & 3) << 6
    MOV R7, A           ; R7 = offset base de tabla (0x00, 0x40, 0x80, 0xC0)

    ; Pitch desde Puerto 2 bits 0-4
    MOV A, R4
    ANL A, #0x1F
    INC A               ; Incremento de pitch: 1..32
    MOV R1, A

    ; Avanzar fase
    MOV A, R0
    ADD A, R1
    ANL A, #0x3F        ; Mantiene fase dentro de 64 muestras
    MOV R0, A

    ; Construye direccion para MOVP3 (Pagina 3 = 0x0300)
    ORL A, R7           ; Offset tabla | Fase
    MOVP3 A, @A         ; Lee muestra de ROM desde Pagina 3
    OUTL P1, A          ; Envia muestra al DAC de Puerto 1
    JMP main_engine_loop

    ORG 0x0090
run_kick:
    ; Sintetizador de bombo: oscilador con barrido descendente
    INC R6
    MOV A, R6
    ADD A, R5           ; Anade periodo actual R5
    MOV R6, A
    ANL A, #0x80
    JZ kick_low
    MOV A, #0xD8        ; Muestra alta
    JMP kick_out
kick_low:
    MOV A, #0x28        ; Muestra baja
kick_out:
    OUTL P1, A          ; DAC
    INC R5              ; Incrementa periodo -> disminuye frecuencia
    MOV A, R5
    ANL A, #0x3F
    MOV R5, A
    JMP main_engine_loop

    ORG 0x00C0
run_snare:
    ; Generador LFSR de 8 bits para caja (ruido + cuerpo)
    MOV A, R2
    RRC A               ; Rota a traves de Carry
    JNC snare_no_xor
    XRL A, #0xB4        ; Polinomio de realimentacion LFSR
snare_no_xor:
    MOV R2, A           ; Actualiza estado LFSR
    INC R6
    XRL A, R6           ; Modula con tono de cuerpo
    OUTL P1, A          ; DAC
    JMP main_engine_loop

    ORG 0x00F0
run_hihat:
    ; Generador de sonido metalico para platillo / Hi-Hat
    MOV A, R2
    RLC A
    JNC hihat_no_xor
    XRL A, #0xD5        ; Polinomio metalico de alta frecuencia
hihat_no_xor:
    MOV R2, A
    XRL A, #0xAA        ; Modulacion en anillo
    OUTL P1, A          ; DAC
    JMP main_engine_loop
