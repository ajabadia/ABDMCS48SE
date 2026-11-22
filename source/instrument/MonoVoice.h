// MonoVoice.h - Voz monofonica sobre una maquina MCS-48.
//
// La maquina no tiene un sintetizador: ejecuta firmware y el firmware decide
// el tono. En el perfil generico el tono del firmware es proporcional al
// reloj de la CPU, asi que la afinacion se controla con el parametro
// engine_clock del perfil (cada semitono es una potencia de dos).
//
// Esta clase NO depende de JUCE ni del host: recibe notas MIDI ya extraidas,
// calcula el valor normalizado del parametro y aplica la envolvente de
// puerta. El host solo tiene que:
//   1. llamar a noteOn/noteOff,
//   2. encolar clockCommandValue() cuando cambie currentNote(),
//   3. multiplicar el audio de la maquina por nextGain().
//
// Monofonia con prioridad de ultima nota: si se mantienen varias teclas, la
// ultima pulsada manda; al soltarla vuelve la anterior que siga pulsada.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace bdmcs48::instrument {

enum class VelocityCurve {
    Linear = 0,
    Exponential = 1,
    Logarithmic = 2,
    Stepped = 3
};

struct VoiceConfig {
    // Frecuencia nativa del firmware de voz con engine_clock a 1.0. El
    // programa alterna el DAC cada 450 ciclos de maquina y anade 2 ciclos de
    // JMP por periodo: 902 ciclos a 400 kHz = 443,459 Hz.
    double baseFrequencyHz = 443.4590;

    // Referencia de afinacion: la nota 69 (A4) suena a 440 Hz.
    double referenceNote = 69.0;
    double referenceHz = 440.0;

    // Envolvente de puerta (lineal, en milisegundos).
    double attackMs = 4.0;
    double releaseMs = 80.0;

    // Deslizamiento legato (portamento) en milisegundos (0 = desactivado).
    double glideMs = 0.0;

    // Perfil de respuesta a la velocidad MIDI.
    VelocityCurve velocityCurve = VelocityCurve::Linear;

    // Limites del parametro engine_clock. Deben coincidir con
    // GenericMcs48Machine::kMinClockScale / kMaxClockScale.
    double minClockScale = 0.0625;
    double maxClockScale = 8.0;
};

class MonoVoice {
public:
    static constexpr std::size_t kMaxHeldNotes = 8;

    void prepare(double sampleRateHz, const VoiceConfig& config = VoiceConfig());
    void reset();

    void noteOn(int note, float velocity);
    void noteOff(int note);
    void allNotesOff();

    // Cambia la liberacion sin reiniciar la nota en curso. Llamarlo con el
    // valor vigente es una operacion nula, para que el host pueda repetir el
    // parametro en cada bloque sin alargar la cola.
    void setReleaseMs(double milliseconds);
    void setAttackMs(double milliseconds);
    void setReferenceHz(double hz);
    void setVelocitySens(float sens);
    void setGlideMs(double milliseconds);
    void setVelocityCurve(VelocityCurve curve);

    // Escala de reloj que corresponde a una nota MIDI (sin recortar), y la
    // frecuencia igualmente temperada de la nota.
    double clockScaleForNote(double note) const;
    static double frequencyForNote(double note, const VoiceConfig& config = VoiceConfig());

    // Valor normalizado (0..1) del parametro engine_clock para la nota actual.
    // Si no hay nota, devuelve el valor de la ultima nota o 0.
    float clockParameter() const;
    // Mismo valor en el entero que espera MachineHost::postCommand.
    std::int32_t clockCommandValue() const;

    // Avanza la envolvente y el glide una muestra y devuelve su ganancia (0..1).
    float nextGain();

    bool sounding() const { return stage_ != Stage::Idle; }
    int currentNote() const { return currentNote_; }
    double currentPitch() const { return currentPitch_; }
    bool isGliding() const { return isGliding_; }
    float velocity() const { return velocity_; }
    float level() const { return gain_; }
    double glideMs() const { return config_.glideMs; }
    VelocityCurve velocityCurve() const { return config_.velocityCurve; }

private:
    enum class Stage { Idle, Attack, Hold, Release };

    void refreshTimes();
    void startAttack();
    void startRelease();
    int lastHeldNote() const;

    double sampleRate_ = 44'100.0;
    VoiceConfig config_{};

    std::array<int, kMaxHeldNotes> held_{};
    std::size_t heldCount_ = 0;

    int currentNote_ = -1;
    double currentPitch_ = 69.0;
    double targetPitch_ = 69.0;
    bool isGliding_ = false;
    double glideStepCoeff_ = 1.0;

    float velocity_ = 0.0f;
    float velocitySens_ = 1.0f;
    float gain_ = 0.0f;
    float attackStep_ = 1.0f;
    float releaseStep_ = 1.0f;
    Stage stage_ = Stage::Idle;
};

} // namespace bdmcs48::instrument
