#include "core/ProgramMemory.h"

#include <algorithm>

namespace bdmcs48::core {

void ProgramMemory::clear() {
    bytes_.fill(0);
    loaded_ = {0, 0};
}

bool ProgramMemory::loadBank(std::size_t bank, std::size_t offset, const std::uint8_t* data,
                              std::size_t size) {
    if (bank >= kBankCount) {
        return false;
    }
    if (data == nullptr && size != 0) {
        return false;
    }
    if (offset > kBankSize || size > kBankSize - offset) {
        return false;
    }

    const std::size_t base = bank * kBankSize + offset;
    if (size != 0) {
        std::copy(data, data + size, bytes_.begin() + static_cast<std::ptrdiff_t>(base));
    }
    // Un banco se considera cargado cuando su contenido es integro: si se
    // recarga con menos bytes, lo que habia detras se conserva, asi que el
    // contador es el maximo entre ambas cargas.
    loaded_[bank] = std::max(loaded_[bank], offset + size);
    return true;
}

std::uint8_t ProgramMemory::read(std::uint16_t address) const {
    return bytes_[address & static_cast<std::uint16_t>(kSize - 1)];
}

std::size_t ProgramMemory::bankOf(std::uint16_t address) {
    return (address & 0x0800u) != 0 ? 1u : 0u;
}

bool ProgramMemory::isBankLoaded(std::size_t bank) const {
    return bank < kBankCount && loaded_[bank] != 0;
}

std::size_t ProgramMemory::loadedBytes() const {
    return loaded_[0] + loaded_[1];
}

} // namespace bdmcs48::core
