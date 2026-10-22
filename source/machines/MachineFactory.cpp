#include "machines/MachineFactory.h"

#include <vector>

#include "core/ProgramMemory.h"
#include "firmware/PackageLoader.h"
#include "machines/GenericMcs48Machine.h"

namespace bdmcs48::machines {

std::unique_ptr<core::MachineInstance> createMachine(const firmware::FirmwareManifest& manifest,
                                                      const firmware::LoadedPackage& package,
                                                      std::string& error) {
    if (manifest.machine.profile != "generic-mcs48") {
        error = "perfil '" + manifest.machine.profile + "' sin fabrica registrada";
        return nullptr;
    }

    auto machine = std::make_unique<GenericMcs48Machine>(GenericMcs48Machine::Config{
        44'100,
        manifest.machine.cpu.clockHz,
        15, // MCS-48: 15 oscilaciones por ciclo de maquina
        128});

    core::ProgramMemory program;
    std::vector<std::uint8_t> data(256, 0xff);

    for (const firmware::LoadedImage& image : package.images) {
        if (image.region == "program") {
            if (!program.loadBank(image.bank, image.offset, image.data.data(), image.data.size())) {
                error = "la imagen '" + image.id + "' no cabe en el banco " + std::to_string(image.bank);
                return nullptr;
            }
        } else if (image.region == "data") {
            for (std::size_t i = 0; i < image.data.size(); ++i) {
                const std::size_t target = image.offset + i;
                if (target < data.size()) {
                    data[target] = image.data[i];
                }
            }
        }
    }

    machine->setProgram(program);
    machine->setDataRegion(std::move(data));
    machine->reset();
    return machine;
}

} // namespace bdmcs48::machines
