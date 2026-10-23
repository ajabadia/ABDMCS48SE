#include "TestFramework.h"

#include <array>
#include <cmath>
#include <cstdint>

#include "instrument/CommandMatrix.h"
#include "machines/GenericMcs48Machine.h"

namespace {

using bdmcs48::instrument::CommandMatrix;
using bdmcs48::instrument::MatrixMutation;
using bdmcs48::instrument::MatrixRow;
using bdmcs48::instrument::MatrixTarget;

constexpr double kSampleRate = 48'000.0;

// Valor nativo de un comando de la matriz, deshaciendo la normalizacion que
// aplica MachineHost (valor / 32767).
double nativeValue(MatrixTarget target, std::int32_t command) {
    const double normalized = static_cast<double>(command) / 32'767.0;
    const double min = CommandMatrix::minFor(target);
    const double max = CommandMatrix::maxFor(target);
    return min + normalized * (max - min);
}

std::array<MatrixRow, CommandMatrix::kRows> singleRow(const MatrixRow& row) {
    std::array<MatrixRow, CommandMatrix::kRows> rows{};
    rows[0] = row;
    rows[0].enabled = true;
    return rows;
}

} // namespace

TEST(CommandMatrix, LasConstantesCoincidenConElPerfil) {
    CHECK_NEAR(CommandMatrix::kEngineClockMinScale,
               static_cast<double>(bdmcs48::machines::GenericMcs48Machine::kMinClockScale),
               1e-6);
    CHECK_NEAR(CommandMatrix::kEngineClockMaxScale,
               static_cast<double>(bdmcs48::machines::GenericMcs48Machine::kMaxClockScale),
               1e-6);
}

TEST(CommandMatrix, UnaNotaDisparaSuFila) {
    CommandMatrix matrix(kSampleRate);
    MatrixRow row;
    row.enabled = true;
    row.note = 60;
    row.target = MatrixTarget::LatchWrite;
    row.value = 18.0f;
    row.currentValue = 18.0f;
    matrix.publishRows(singleRow(row));
    CHECK(matrix.consumePublishedRows());

    matrix.noteOn(60);
    CHECK_EQ(matrix.pendingCount(), 1u);

    CommandMatrix::Command command;
    CHECK(matrix.popDue(command));
    CHECK_EQ(command.id, CommandMatrix::kLatchWriteCommandId);
    CHECK_NEAR(nativeValue(MatrixTarget::LatchWrite, command.value), 18.0, 0.01);
    CHECK_EQ(matrix.popDue(command), false);
}

TEST(CommandMatrix, UnaNotaDesconocidaNoDisparaNada) {
    CommandMatrix matrix(kSampleRate);
    MatrixRow row;
    row.enabled = true;
    row.note = 60;
    matrix.publishRows(singleRow(row));
    CHECK(matrix.consumePublishedRows());

    matrix.noteOn(61);
    CHECK_EQ(matrix.pendingCount(), 0u);
}

TEST(CommandMatrix, ProbabilidadCeroNoAplicaElComando) {
    CommandMatrix matrix(kSampleRate);
    MatrixRow row;
    row.enabled = true;
    row.note = 60;
    row.probability = 0.0f;
    matrix.publishRows(singleRow(row));
    CHECK(matrix.consumePublishedRows());

    for (int i = 0; i < 16; ++i) {
        matrix.noteOn(60);
    }
    CHECK_EQ(matrix.pendingCount(), 0u);

    CommandMatrix::Command command;
    CHECK_EQ(matrix.popDue(command), false);
}

TEST(CommandMatrix, ElRetardoProgramaLaAplicacion) {
    CommandMatrix matrix(kSampleRate);
    MatrixRow row;
    row.enabled = true;
    row.note = 60;
    row.delayMs = 100.0f;
    matrix.publishRows(singleRow(row));
    CHECK(matrix.consumePublishedRows());

    matrix.noteOn(60);
    CHECK_EQ(matrix.nextDeadline(), 4'800u); // 100 ms a 48 kHz

    matrix.advance(4'799);
    CommandMatrix::Command command;
    CHECK_EQ(matrix.popDue(command), false);
    CHECK_EQ(matrix.pendingCount(), 1u);

    matrix.advance(1);
    CHECK(matrix.popDue(command));
    CHECK_EQ(matrix.pendingCount(), 0u);
}

TEST(CommandMatrix, LaMutacionIncrementAfectaALaSiguienteEjecucion) {
    CommandMatrix matrix(kSampleRate);
    MatrixRow row;
    row.enabled = true;
    row.note = 60;
    row.target = MatrixTarget::LatchWrite;
    row.value = 18.0f;
    row.currentValue = 18.0f;
    row.mutation = MatrixMutation::Increment;
    row.mutationAmount = 1.0f;
    matrix.publishRows(singleRow(row));
    CHECK(matrix.consumePublishedRows());

    CommandMatrix::Command first;
    matrix.noteOn(60);
    CHECK(matrix.popDue(first));
    CHECK_NEAR(nativeValue(MatrixTarget::LatchWrite, first.value), 18.0, 0.01);

    CommandMatrix::Command second;
    matrix.noteOn(60);
    CHECK(matrix.popDue(second));
    CHECK_NEAR(nativeValue(MatrixTarget::LatchWrite, second.value), 19.0, 0.01);
}

TEST(CommandMatrix, LaProbabilidadFallidaNoMutaLaFila) {
    CommandMatrix matrix(kSampleRate);
    MatrixRow row;
    row.enabled = true;
    row.note = 60;
    row.target = MatrixTarget::LatchWrite;
    row.value = 18.0f;
    row.currentValue = 18.0f;
    row.probability = 0.0f;
    row.mutation = MatrixMutation::Increment;
    row.mutationAmount = 1.0f;
    matrix.publishRows(singleRow(row));
    CHECK(matrix.consumePublishedRows());

    for (int i = 0; i < 8; ++i) {
        matrix.noteOn(60);
    }
    CHECK_NEAR(matrix.rows()[0].currentValue, 18.0, 1e-6);
}

TEST(CommandMatrix, LaMutacionWrapVuelveAlMinimo) {
    CommandMatrix matrix(kSampleRate);
    MatrixRow row;
    row.enabled = true;
    row.note = 60;
    row.target = MatrixTarget::LatchWrite;
    row.value = 254.0f;
    row.currentValue = 254.0f;
    row.mutation = MatrixMutation::Wrap;
    row.mutationAmount = 2.0f;
    matrix.publishRows(singleRow(row));
    CHECK(matrix.consumePublishedRows());

    CommandMatrix::Command command;
    matrix.noteOn(60);
    CHECK(matrix.popDue(command));
    CHECK_NEAR(matrix.rows()[0].currentValue, CommandMatrix::kLatchMin, 1e-6);
}

TEST(CommandMatrix, LaMutacionAleatoriaEsReproducibleConLaMismaSemilla) {
    const auto runWithSeed = [](std::uint32_t seed) {
        CommandMatrix matrix(kSampleRate);
        matrix.setSeed(seed);
        MatrixRow row;
        row.enabled = true;
        row.note = 60;
        row.target = MatrixTarget::LatchWrite;
        row.value = 10.0f;
        row.currentValue = 10.0f;
        row.mutation = MatrixMutation::Random;
        matrix.publishRows(singleRow(row));
        matrix.consumePublishedRows();

        CommandMatrix::Command command;
        matrix.noteOn(60);
        matrix.popDue(command);
        // El comando usa el valor anterior; lo aleatorio es el valor mutado
        // que se aplicara en la siguiente ejecucion.
        return static_cast<int>(std::lround(matrix.rows()[0].currentValue * 1000.0f));
    };

    CHECK_EQ(runWithSeed(1234), runWithSeed(1234));
    CHECK(runWithSeed(4321) != runWithSeed(1234));
}

TEST(CommandMatrix, DosFilasConLaMismaNotaSeEjecutanAmbas) {
    CommandMatrix matrix(kSampleRate);
    std::array<MatrixRow, CommandMatrix::kRows> rows{};
    rows[0].enabled = true;
    rows[0].note = 60;
    rows[1].enabled = true;
    rows[1].note = 60;
    rows[1].target = MatrixTarget::LatchWrite;
    matrix.publishRows(rows);
    CHECK(matrix.consumePublishedRows());

    matrix.noteOn(60);
    CHECK_EQ(matrix.pendingCount(), 2u);

    CommandMatrix::Command command;
    CHECK(matrix.popDue(command));
    CHECK(matrix.popDue(command));
    CHECK_EQ(matrix.popDue(command), false);
}

TEST(CommandMatrix, UnaColaLlenaDescartaSinBloquear) {
    CommandMatrix matrix(kSampleRate);
    MatrixRow row;
    row.enabled = true;
    row.note = 60;
    matrix.publishRows(singleRow(row));
    CHECK(matrix.consumePublishedRows());

    for (std::size_t i = 0; i < CommandMatrix::kMaxScheduled + 5; ++i) {
        matrix.noteOn(60);
    }
    CHECK_EQ(matrix.pendingCount(), CommandMatrix::kMaxScheduled);
    CHECK_EQ(matrix.droppedCommands(), 5u);

    CommandMatrix::Command command;
    // Sacar todo lo programado no debe desbordar ni dejar restos.
    std::size_t drained = 0;
    while (matrix.popDue(command)) {
        ++drained;
    }
    CHECK_EQ(drained, CommandMatrix::kMaxScheduled);
    CHECK_EQ(matrix.pendingCount(), 0u);
}

TEST(CommandMatrix, LasFilasPublicadasLleganAlHiloDeAudio) {
    CommandMatrix matrix(kSampleRate);
    MatrixRow row;
    row.enabled = true;
    row.note = 72;
    row.target = MatrixTarget::LatchWrite;
    row.value = 200.0f;
    row.currentValue = 200.0f;
    matrix.publishRows(singleRow(row));

    CHECK(matrix.consumePublishedRows());
    CHECK_EQ(matrix.rows()[0].note, 72);
    CHECK(matrix.rows()[0].enabled);

    // Sin publicacion nueva, consumir no cambia nada.
    CHECK_EQ(matrix.consumePublishedRows(), false);
}

TEST(CommandMatrix, ResetOlvidaLoProgramado) {
    CommandMatrix matrix(kSampleRate);
    MatrixRow row;
    row.enabled = true;
    row.note = 60;
    matrix.publishRows(singleRow(row));
    matrix.consumePublishedRows();
    matrix.noteOn(60);
    CHECK_EQ(matrix.pendingCount(), 1u);

    matrix.reset();
    CHECK_EQ(matrix.pendingCount(), 0u);
    CHECK_EQ(matrix.rows()[0].enabled, true); // fila 0 activa por defecto
    CHECK_EQ(matrix.rows()[1].enabled, false);
}
