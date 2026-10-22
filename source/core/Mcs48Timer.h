// Mcs48Timer.h - Temporizador/contador del MCS-48 (sin interrupciones).
//
// El 8048 tiene un contador de 8 bits que puede avanzar de dos maneras:
//
//   * Timer: cada 32 ciclos de maquina (el prescaler divide el reloj).
//   * EventCounter: un pulso por flanco en la entrada T1, que en este core
//     inyecta el perfil de maquina con externalPulse().
//
// Al pasar de 0xFF a 0x00 el contador marca el flag TF, que es estado
// observable. Despachar una interrupcion por ese desbordamiento NO esta
// implementado: los opcodes de interrupcion siguen fallando de forma
// explicita. Este bloque modela solo el contador y su flag.
//
// La semantica sigue la tabla de MAME (src/devices/cpu/mcs48): STRT T pone el
// prescaler a cero, STRT CNT no lo toca y STOP TCNT detiene la cuenta sin
// borrar el valor del contador.

#pragma once

#include <cstdint>

namespace bdmcs48::core {

enum class TimerMode {
    Stopped,
    Timer,
    EventCounter,
};

class Mcs48Timer {
public:
    void reset();

    void startTimer();     // STRT T
    void startCounter();   // STRT CNT
    void stop();           // STOP TCNT

    void setCounter(std::uint8_t value);   // MOV T,A
    std::uint8_t counter() const;          // MOV A,T

    bool timerFlag() const;   // TF
    void clearTimerFlag();    // lo limpia JTF o la atencion de la interrupcion

    TimerMode mode() const { return mode_; }

    // Un ciclo de maquina mas (el prescaler divide entre 32).
    void tick();

    // Un flanco en T1: solo cuenta en modo EventCounter.
    void externalPulse();

private:
    void increment();

    std::uint8_t counter_ = 0;
    std::uint8_t prescaler_ = 0;
    TimerMode mode_ = TimerMode::Stopped;
    bool timerFlag_ = false;
};

} // namespace bdmcs48::core
