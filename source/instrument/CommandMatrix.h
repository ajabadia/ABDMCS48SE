// CommandMatrix.h - Matriz de comandos MIDI -> maquina, sin JUCE.
//
// La matriz vive en el host, no en el core: recibe notas MIDI, decide con
// probabilidad si una fila se ejecuta, retrasa el comando el tiempo pedido y
// muta el valor de la fila para la siguiente ejecucion. Al core solo le llega
// un comando ya resuelto, por el mismo camino que usa el resto del host:
//
//   Note On -> matriz -> programacion -> CommandQueue -> applyPendingCommands()
//           -> GenericMcs48Machine::setParameter() / writeInput()
//
// El hilo de audio llama a noteOn(), advance(), nextDeadline() y popDue();
// ninguna de esas operaciones asigna memoria ni bloquea. Los cambios de fila
// desde la UI se publican como una copia completa (ver publishRows), de modo
// que el hilo de audio nunca lee una fila a medio escribir.

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace bdmcs48::instrument {

// Destinos admitidos en esta primera version. Cada uno se traduce al
// identificador de comando que espera MachineHost: los parametros del perfil
// viajan con 0x1000+ y las entradas de maquina con su id directo.
enum class MatrixTarget : std::uint8_t {
    EngineClock = 0, // escala del reloj de la CPU
    LatchWrite = 1,  // latch de entrada 0 del perfil
};

enum class MatrixMutation : std::uint8_t {
    None = 0,
    Increment = 1,
    Decrement = 2,
    Random = 3,
    Wrap = 4, // incrementa y vuelve al minimo al superar el maximo
};

struct MatrixRow {
    bool enabled = false;
    int note = 60;
    MatrixTarget target = MatrixTarget::EngineClock;
    // Valor en unidades nativas: multiplicador de reloj (0,0625..8) o latch
    // (0..255). La conversion al entero de la cola la hace commandFor().
    float value = 1.0f;
    // Probabilidad de ejecucion al recibir la nota (0..1).
    float probability = 1.0f;
    // Retardo antes de aplicar el comando, en milisegundos.
    float delayMs = 0.0f;
    MatrixMutation mutation = MatrixMutation::None;
    // Paso de la mutacion, en las mismas unidades que `value`.
    float mutationAmount = 0.0f;
    // Valor que se aplicara en la proxima ejecucion. Empieza igual que
    // `value` y solo cambia cuando la fila se ejecuta de verdad.
    float currentValue = 1.0f;
};

class CommandMatrix {
public:
    static constexpr std::size_t kRows = 8;
    static constexpr std::size_t kMaxScheduled = 32;

    // Deben coincidir con GenericMcs48Machine::kMinClockScale/kMaxClockScale;
    // un test lo comprueba.
    static constexpr float kEngineClockMinScale = 0.0625f;
    static constexpr float kEngineClockMaxScale = 8.0f;
    static constexpr float kLatchMin = 0.0f;
    static constexpr float kLatchMax = 255.0f;

    // Identificadores del contrato de MachineHost.
    static constexpr std::uint32_t kEngineClockCommandId = 0x1000u + 1u;
    static constexpr std::uint32_t kLatchWriteCommandId = 0u;

    struct Command {
        std::uint32_t id = 0;
        std::int32_t value = 0;
    };

    CommandMatrix();
    explicit CommandMatrix(double sampleRateHz);

    void prepare(double sampleRateHz);
    // Olvida lo programado y devuelve las filas a sus valores iniciales.
    void reset();
    // Olvida lo programado sin tocar filas ni semilla (reset de maquina).
    void clearSchedule();

    std::array<MatrixRow, kRows>& rows() { return rows_; }
    const std::array<MatrixRow, kRows>& rows() const { return rows_; }

    // Semilla del generador: misma semilla y mismas notas, mismo resultado.
    void setSeed(std::uint32_t seed);
    std::uint32_t seed() const { return seed_; }

    // Hilo de audio: evalua las filas cuya nota coincide y programa el
    // comando. Una nota puede disparar varias filas; Note Off no dispara.
    void noteOn(int note);

    std::uint64_t now() const { return frame_; }
    void advance(std::uint64_t frames) { frame_ += frames; }

    // Deadline absoluto del comando programado mas antiguo, o 0 si no hay
    // ninguno. El processor lo usa para partir el bloque.
    std::uint64_t nextDeadline() const;

    // Extrae el comando que vence en `now()` y aplica la mutacion de su fila.
    bool popDue(Command& out);

    std::uint64_t pendingCount() const { return scheduleCount_; }
    std::uint64_t droppedCommands() const { return dropped_; }

    // Hilo de mensajes: publica una copia completa de las filas. El hilo de
    // audio la consume al principio de un bloque; nunca ve una fila a medio
    // escribir porque se escribe en el buffer inactivo y se publica despues.
    void publishRows(const std::array<MatrixRow, kRows>& rows);
    // Hilo de audio: adopta la ultima copia publicada, si hay alguna.
    bool consumePublishedRows();

    // Conversion fila -> comando de MachineHost.
    static Command commandFor(const MatrixRow& row);

    // Filas de fabrica: la primera activa, el resto preparadas y apagadas.
    static std::array<MatrixRow, kRows> defaults();

    // Mutacion de una fila, expuesta para pruebas y para la UI.
    void mutate(MatrixRow& row);

    static float minFor(MatrixTarget target);
    static float maxFor(MatrixTarget target);
    static float clampForTarget(MatrixTarget target, float value);

private:
    struct Scheduled {
        std::uint64_t deadline = 0;
        std::uint8_t row = 0;
    };

    float nextRandom();
    static bool rowMatches(const MatrixRow& row, int note);

    std::array<MatrixRow, kRows> rows_{};
    // Doble buffer para los cambios que llegan de la UI.
    std::array<MatrixRow, kRows> pendingRows_[2]{};
    std::atomic<std::uint32_t> publishedIndex_{0};
    std::atomic<bool> hasPending_{false};
    std::array<Scheduled, kMaxScheduled> schedule_{};
    std::size_t scheduleCount_ = 0;
    std::uint64_t frame_ = 0;
    std::uint64_t dropped_ = 0;
    double sampleRate_ = 44'100.0;
    std::uint32_t seed_ = 0x9e3779b9u;
    std::uint32_t randomState_ = 0x9e3779b9u;
};

} // namespace bdmcs48::instrument
