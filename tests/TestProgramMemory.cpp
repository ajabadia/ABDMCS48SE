#include "TestFramework.h"

#include "core/ProgramMemory.h"

using bdmcs48::core::ProgramMemory;

TEST(ProgramMemory, EmpiezaVacia) {
    ProgramMemory memory;
    CHECK_EQ(memory.loadedBytes(), std::size_t{0});
    CHECK(!memory.isBankLoaded(0));
    CHECK(!memory.isBankLoaded(1));
    // La memoria sin cargar vale 0x00, no 0xFF: es lo que hace el fetch
    // cuando el paquete no cubre la direccion.
    CHECK_EQ(memory.read(0x000), std::uint8_t{0});
}

TEST(ProgramMemory, CargaEnBanco0) {
    ProgramMemory memory;
    const std::uint8_t data[] = {0x23, 0x5a, 0x39};
    CHECK(memory.loadBank(0, 0, data, sizeof(data)));
    CHECK_EQ(memory.read(0x000), std::uint8_t{0x23});
    CHECK_EQ(memory.read(0x001), std::uint8_t{0x5a});
    CHECK_EQ(memory.read(0x002), std::uint8_t{0x39});
    CHECK(memory.isBankLoaded(0));
    CHECK(!memory.isBankLoaded(1));
    CHECK_EQ(memory.loadedBytes(), std::size_t{3});
}

TEST(ProgramMemory, CargaEnBanco1) {
    ProgramMemory memory;
    const std::uint8_t data[] = {0xe8, 0x00};
    CHECK(memory.loadBank(1, 0, data, sizeof(data)));
    // El banco 1 son las direcciones 0x800-0xFFF: el bit 11 de la direccion.
    CHECK_EQ(memory.read(0x800), std::uint8_t{0xe8});
    CHECK_EQ(memory.read(0x801), std::uint8_t{0x00});
    CHECK_EQ(memory.read(0x000), std::uint8_t{0});
    CHECK(memory.isBankLoaded(1));
    CHECK(!memory.isBankLoaded(0));
}

TEST(ProgramMemory, OffsetDentroDelBanco) {
    ProgramMemory memory;
    const std::uint8_t data[] = {0xaa, 0xbb};
    CHECK(memory.loadBank(0, 0x100, data, sizeof(data)));
    CHECK_EQ(memory.read(0x100), std::uint8_t{0xaa});
    CHECK_EQ(memory.read(0x101), std::uint8_t{0xbb});
    CHECK_EQ(memory.read(0x000), std::uint8_t{0});
}

TEST(ProgramMemory, RechazaBancoInexistente) {
    ProgramMemory memory;
    const std::uint8_t data[] = {0x00};
    CHECK(!memory.loadBank(2, 0, data, sizeof(data)));
    CHECK(!memory.loadBank(99, 0, data, sizeof(data)));
}

TEST(ProgramMemory, RechazaImageQueNoCabe) {
    ProgramMemory memory;
    const std::uint8_t data[] = {0x00, 0x01, 0x02};
    // offset + size no puede pasar de 2048.
    CHECK(!memory.loadBank(0, ProgramMemory::kBankSize - 1, data, sizeof(data)));
    CHECK(memory.loadBank(0, ProgramMemory::kBankSize - sizeof(data), data, sizeof(data)));
    // El puntero nulo con tamano distinto de cero tambien se rechaza.
    CHECK(!memory.loadBank(0, 0, nullptr, 4));
}

TEST(ProgramMemory, LaDireccionSeEnmascaraA12Bits) {
    ProgramMemory memory;
    const std::uint8_t data[] = {0x77};
    CHECK(memory.loadBank(0, 0, data, sizeof(data)));
    CHECK_EQ(memory.read(0x1000), memory.read(0x000));
    CHECK_EQ(memory.read(0x1fff), memory.read(0x0fff));
}

TEST(ProgramMemory, BancoDeDireccion) {
    CHECK_EQ(ProgramMemory::bankOf(0x000), std::size_t{0});
    CHECK_EQ(ProgramMemory::bankOf(0x7ff), std::size_t{0});
    CHECK_EQ(ProgramMemory::bankOf(0x800), std::size_t{1});
    CHECK_EQ(ProgramMemory::bankOf(0xfff), std::size_t{1});
}

TEST(ProgramMemory, RecargarConservaLoQueHabiaDetras) {
    ProgramMemory memory;
    const std::uint8_t first[] = {0x11, 0x22, 0x33, 0x44};
    const std::uint8_t second[] = {0x99};
    CHECK(memory.loadBank(0, 0, first, sizeof(first)));
    CHECK(memory.loadBank(0, 0, second, sizeof(second)));
    CHECK_EQ(memory.read(0x000), std::uint8_t{0x99});
    CHECK_EQ(memory.read(0x001), std::uint8_t{0x22});
    CHECK_EQ(memory.loadedBytes(), std::size_t{4});
}
