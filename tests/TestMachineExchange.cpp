#include "TestFramework.h"

#include <memory>

#include "core/MachineExchange.h"
#include "core/MachineInstance.h"

using bdmcs48::core::MachineExchange;
using bdmcs48::core::MachineInstance;

namespace {

int gAlive = 0;
int gDestroyed = 0;

class StubMachine : public MachineInstance {
public:
    explicit StubMachine(int identifier) : identifier_(identifier) { ++gAlive; }
    ~StubMachine() override {
        --gAlive;
        ++gDestroyed;
    }

    std::string profileId() const override { return "stub"; }
    void reset() override {}
    void setParameter(std::uint32_t, float) override {}
    void writeInput(std::uint32_t, float) override {}
    void runBlock(std::uint32_t, float* const*, std::uint32_t) override {}
    EngineStatus status() override { return EngineStatus{}; }
    std::uint64_t retiredPending() const override { return 0; }
    void drainRetired() override {}

    int identifier() const { return identifier_; }

private:
    int identifier_ = 0;
};

} // namespace

TEST(MachineExchange, PublicaEInstala) {
    gAlive = 0;
    gDestroyed = 0;
    MachineExchange exchange;
    auto machine = std::make_unique<StubMachine>(1);
    MachineInstance* raw = machine.get();
    CHECK(exchange.publish(std::move(machine)));
    CHECK(exchange.hasPending());

    MachineInstance* installed = exchange.swapAtBlockBoundary(nullptr);
    CHECK_EQ(installed, raw);
    CHECK(!exchange.hasPending());
    CHECK_EQ(exchange.swapCount(), std::uint64_t{1});
    // La maquina nueva sigue viva: la destruiria el intercambio.
    CHECK_EQ(gAlive, 1);
}

TEST(MachineExchange, LaMaquinaRetiradaNoSeDestruyeHastaQueSeDrena) {
    gAlive = 0;
    gDestroyed = 0;
    MachineExchange exchange;
    auto first = std::make_unique<StubMachine>(1);
    MachineInstance* firstRaw = first.get();
    MachineInstance* current = exchange.publish(std::move(first)) ? firstRaw : nullptr;
    CHECK(current != nullptr);
    // El intercambio no publica si ya hay algo pendiente; aqui se instala
    // directamente para simular el estado "hay una maquina activa".
    current = exchange.swapAtBlockBoundary(nullptr);

    auto second = std::make_unique<StubMachine>(2);
    MachineInstance* secondRaw = second.get();
    CHECK(exchange.publish(std::move(second)));

    MachineInstance* installed = exchange.swapAtBlockBoundary(current);
    CHECK_EQ(installed, secondRaw);
    CHECK_EQ(exchange.swapCount(), std::uint64_t{1});
    // La maquina retirada sigue viva: el hilo de audio no destruye nada.
    CHECK_EQ(gAlive, 2);
    CHECK_EQ(gDestroyed, 0);

    CHECK_EQ(exchange.drainRetired(), std::size_t{1});
    CHECK_EQ(gDestroyed, 1);
    CHECK_EQ(gAlive, 1);
    CHECK_EQ(exchange.retiredPending(), std::size_t{0});
}

TEST(MachineExchange, SoloSeAceptaUnaPendiante) {
    gAlive = 0;
    gDestroyed = 0;
    MachineExchange exchange;
    CHECK(exchange.publish(std::make_unique<StubMachine>(1)));
    CHECK(!exchange.publish(std::make_unique<StubMachine>(2)));
    CHECK_EQ(exchange.rejectedCount(), std::uint64_t{1});
    // La maquina rechazada se destruye en el hilo de control, no en el de
    // audio: al perder la propiedad del unique_ptr.
    CHECK_EQ(gAlive, 1);
    CHECK_EQ(gDestroyed, 1);
}

TEST(MachineExchange, ColaDeRetiroLlenaDifiereElCambio) {
    gAlive = 0;
    gDestroyed = 0;
    MachineExchange exchange(2);

    MachineInstance* current = nullptr;
    for (int i = 0; i < 2; ++i) {
        auto machine = std::make_unique<StubMachine>(i);
        MachineInstance* raw = machine.get();
        exchange.publish(std::move(machine));
        current = exchange.swapAtBlockBoundary(current);
        CHECK(current == raw);
    }
    // Ahora la cola de retiro tiene 2 maquinas: esta llena.
    CHECK_EQ(exchange.retiredPending(), std::size_t{2});

    auto machine = std::make_unique<StubMachine>(99);
    MachineInstance* raw = machine.get();
    CHECK(exchange.publish(std::move(machine)));

    const MachineInstance* installed = exchange.swapAtBlockBoundary(current);
    // El cambio se difiere: la maquina activa sigue sonando y la nueva
    // queda pendiente para el siguiente bloque.
    CHECK_EQ(installed, current);
    CHECK(exchange.hasPending());
    CHECK_EQ(exchange.swapCount(), std::uint64_t{2});

    // Al drenar hay sitio y el cambio se produce.
    exchange.drainRetired();
    exchange.drainRetired();
    CHECK_EQ(exchange.retiredPending(), std::size_t{0});
    MachineInstance* installed2 = exchange.swapAtBlockBoundary(current);
    CHECK_EQ(installed2, raw);
    CHECK_EQ(exchange.swapCount(), std::uint64_t{3});
}
