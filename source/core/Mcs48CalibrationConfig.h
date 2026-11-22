#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bdmcs48::calibration {

/**
 * Representacion de una constante o parametro de calibracion desacoplado.
 * Permite introspeccion y modificacion sin romper los valores de fabrica.
 */
struct CalibrationConstant {
    std::string id;
    std::string label;
    std::string category;
    std::string unit;
    std::string description;
    double defaultValue = 0.0;
    double currentValue = 0.0;
    double minValue = 0.0;
    double maxValue = 0.0;
    double stepSize = 0.0;
    bool isInteger = false;

    bool isModified() const noexcept {
        return std::abs(currentValue - defaultValue) > (stepSize > 0.0 ? stepSize * 0.5 : 1e-6);
    }
};

/**
 * Valores canonicos del motor MCS-48 centralizados para trazabilidad y calibracion.
 */
struct Mcs48CalibrationConfig {
    static constexpr double kMinClockScale = 0.0625;
    static constexpr double kMaxClockScale = 8.0;
    static constexpr std::uint32_t kDefaultClockHz = 6'000'000;
    static constexpr std::size_t kCommandQueueCapacity = 256;
    static constexpr std::size_t kMaxRomFileSize = 16 * 1024 * 1024;

    static constexpr double kDemoBaseFreqHz = 443.4590;
    static constexpr double kReferenceNote = 69.0;
    static constexpr double kReferenceHz = 440.0;
    static constexpr std::size_t kMaxHeldNotes = 8;
    static constexpr double kDefaultAttackMs = 4.0;
    static constexpr double kDefaultReleaseMs = 80.0;
    static constexpr double kDefaultGlideMs = 0.0;
    static constexpr double kMaxGlideMs = 2000.0;
    static constexpr double kDefaultDacTolerance = 0.0;
    static constexpr double kMaxDacTolerance = 1.0;

    static constexpr float kDcBlockerFeedback = 0.995f;
    static constexpr float kDiagToneFreqHz = 440.0f;
    static constexpr float kDiagToneAmplitude = 0.2f;
    static constexpr float kDefaultGainDb = -6.0f;

    static constexpr std::size_t kMatrixRows = 8;
    static constexpr double kMatrixMaxDelayMs = 5000.0;
    static constexpr std::int32_t kClockCommandScale = 32767;

    static inline std::vector<CalibrationConstant> catalog() {
        return {
            {"minClockScale", "Min Clock Scale Factor", "Engine & Clock", "x",
             "Factor de escala minima de reloj de CPU", kMinClockScale, kMinClockScale, 0.01, 0.5, 0.001, false},
            {"maxClockScale", "Max Clock Scale Factor", "Engine & Clock", "x",
             "Factor de escala maxima de reloj de CPU", kMaxClockScale, kMaxClockScale, 1.0, 16.0, 0.1, false},
            {"defaultClockHz", "Default CPU Crystal Clock", "Engine & Clock", "Hz",
             "Reloj de cuarzo tipico del Intel 8049", static_cast<double>(kDefaultClockHz),
             static_cast<double>(kDefaultClockHz), 1000000.0, 12000000.0, 100000.0, true},
            {"demoBaseFreqHz", "Demo Firmware Base Frequency", "Voice & Tuning", "Hz",
             "Frecuencia nativa de la ROM de voz (902 ciclos a 400 kHz)", kDemoBaseFreqHz, kDemoBaseFreqHz,
             100.0, 1000.0, 0.001, false},
            {"referenceHz", "Master A4 Tuning Frequency", "Voice & Tuning", "Hz",
             "Frecuencia de afinacion La4 estándar", kReferenceHz, kReferenceHz, 432.0, 448.0, 0.1, false},
            {"defaultAttackMs", "Default Voice Attack Time", "Voice & Tuning", "ms",
             "Rampa de ataque de puerta de voz", kDefaultAttackMs, kDefaultAttackMs, 0.1, 100.0, 0.1, false},
            {"defaultReleaseMs", "Default Voice Release Time", "Voice & Tuning", "ms",
             "Tiempo de liberacion de la voz", kDefaultReleaseMs, kDefaultReleaseMs, 10.0, 500.0, 1.0, false},
            {"glideTime", "Fingered Portamento Glide Time", "Voice & Tuning", "ms",
             "Tiempo de transicion continua entre notas legato", kDefaultGlideMs, kDefaultGlideMs, 0.0, kMaxGlideMs, 1.0, false},
            {"velocityCurve", "Velocity Dynamic Curve Profile", "Voice & Tuning", "index",
             "Perfil de sensibilidad de pulsacion (0: Lin, 1: Exp, 2: Log, 3: Stepped)", 0.0, 0.0, 0.0, 3.0, 1.0, true},
            {"dacTolerance", "R-2R DAC Resistor Network Tolerance", "Output & Circuit", "ratio",
             "Desajuste y no-linealidad vintage de red R-2R", kDefaultDacTolerance, kDefaultDacTolerance, 0.0, kMaxDacTolerance, 0.01, false},
            {"dcBlockerFeedback", "DC Blocker Pole Feedback", "Output & Circuit", "coeff",
             "Polo del filtro de paso alto de salida", static_cast<double>(kDcBlockerFeedback),
             static_cast<double>(kDcBlockerFeedback), 0.900, 0.999, 0.001, false},
            {"diagToneFreqHz", "Diagnostic Tone Frequency", "Output & Circuit", "Hz",
             "Frecuencia del tono de prueba acustica", static_cast<double>(kDiagToneFreqHz),
             static_cast<double>(kDiagToneFreqHz), 100.0, 1000.0, 1.0, false},
            {"diagToneAmplitude", "Diagnostic Tone Headroom Gain", "Output & Circuit", "x",
             "Nivel relativo del oscilador de prueba", static_cast<double>(kDiagToneAmplitude),
             static_cast<double>(kDiagToneAmplitude), 0.05, 1.0, 0.05, false},
            {"matrixMaxDelayMs", "Max Step Trigger Delay", "Command Matrix", "ms",
             "Tiempo maximo de retardo por paso de matriz", kMatrixMaxDelayMs, kMatrixMaxDelayMs, 100.0, 10000.0, 10.0, false}
        };
    }
};

} // namespace bdmcs48::calibration
