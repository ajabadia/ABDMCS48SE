#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bdmcs48::core {

enum class RegionType {
    ResetVector,
    InterruptVector,
    TimerVector,
    Code,
    DataTable,
    ReservedOrPadding
};

struct MemoryRegion {
    std::string name;
    RegionType type = RegionType::Code;
    std::uint32_t startAddress = 0;
    std::uint32_t endAddress = 0; // inclusive
    std::string description;

    std::uint32_t size() const noexcept {
        return (endAddress >= startAddress) ? (endAddress - startAddress + 1) : 0;
    }

    bool contains(std::uint32_t address) const noexcept {
        return address >= startAddress && address <= endAddress;
    }

    bool isCode() const noexcept {
        return type == RegionType::Code || type == RegionType::ResetVector ||
               type == RegionType::InterruptVector || type == RegionType::TimerVector;
    }

    bool isData() const noexcept {
        return type == RegionType::DataTable || type == RegionType::ReservedOrPadding;
    }
};

struct MachineProfile {
    std::string profileId;
    std::string displayName;
    std::string cpuVariant; // "8048", "8049", "8039", "8035"
    std::uint32_t defaultClockHz = 6'000'000;
    std::uint32_t romSizeBytes = 2048;
    std::vector<MemoryRegion> regions;

    const MemoryRegion* findRegion(std::uint32_t address) const noexcept {
        for (const auto& reg : regions) {
            if (reg.contains(address)) return &reg;
        }
        return nullptr;
    }

    bool isCodeAddress(std::uint32_t address) const noexcept {
        const auto* reg = findRegion(address);
        return reg ? reg->isCode() : true;
    }

    std::uint32_t totalCodeBytes() const noexcept {
        std::uint32_t total = 0;
        for (const auto& reg : regions) {
            if (reg.isCode()) total += reg.size();
        }
        return total;
    }

    std::uint32_t totalDataBytes() const noexcept {
        std::uint32_t total = 0;
        for (const auto& reg : regions) {
            if (reg.isData()) total += reg.size();
        }
        return total;
    }

    static MachineProfile genericMcs48();
    static MachineProfile juno6();
};

} // namespace bdmcs48::core
