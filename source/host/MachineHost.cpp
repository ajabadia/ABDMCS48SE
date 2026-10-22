#include "host/MachineHost.h"

#include <cstring>
#include <vector>

#include "firmware/FirmwarePackage.h"
#include "firmware/PackageLoader.h"
#include "machines/MachineFactory.h"

namespace bdmcs48::host {
namespace {

constexpr std::uint32_t kLogIdFirmwareLoaded = 1;
constexpr std::uint32_t kLogIdFirmwareFailed = 2;
constexpr std::uint32_t kLogIdHotSwap = 3;
constexpr std::uint32_t kLogIdHotSwapRejected = 4;

void appendText(core::LogRecord& record, const std::string& text) {
    const std::size_t count = text.size() < sizeof(record.text) - 1 ? text.size() : sizeof(record.text) - 1;
    std::memcpy(record.text, text.data(), count);
    record.text[count] = '\0';
}

} // namespace

MachineHost::MachineHost(Config config) : config_(config) {}

MachineHost::~MachineHost() {
    // La maquina activa la destruye el host: el hilo de audio ya termino.
    owned_.reset();
    active_ = nullptr;
    exchange_.drainRetired();
}

bool MachineHost::loadPackage(const std::string& packageRoot, std::string& error) {
    firmware::LoadedPackage package;
    std::vector<firmware::ValidationIssue> issues;
    if (!firmware::PackageLoader::load(packageRoot, package, issues)) {
        error = firmware::FirmwarePackage::formatIssues(issues);
        log(kLogIdFirmwareFailed, 0, error);
        return false;
    }

    return loadPackage(package, error);
}

bool MachineHost::loadPackage(const firmware::LoadedPackage& package, std::string& error) {
    std::vector<firmware::ValidationIssue> issues;
    if (!firmware::FirmwarePackage::validate(package.manifest, issues)) {
        error = firmware::FirmwarePackage::formatIssues(issues);
        log(kLogIdFirmwareFailed, 0, error);
        return false;
    }

    std::string buildError;
    auto machine = machines::createMachine(package.manifest, package, buildError);
    if (!machine) {
        error = buildError;
        log(kLogIdFirmwareFailed, 0, error);
        return false;
    }

    manifest_ = package.manifest;
    profileId_ = package.manifest.machine.profile;

    if (!publishMachine(std::move(machine))) {
        error = "ya hay una maquina pendiente de instalar";
        log(kLogIdHotSwapRejected, 0, error);
        return false;
    }

    log(kLogIdFirmwareLoaded, static_cast<std::int32_t>(package.manifest.images.size()),
        "firmware '" + package.manifest.id + "' " + package.manifest.version + " cargado");
    return true;
}

bool MachineHost::publishMachine(std::unique_ptr<core::MachineInstance> machine) {
    if (!machine) {
        return false;
    }
    return exchange_.publish(std::move(machine));
}

void MachineHost::renderBlock(std::uint32_t frames, float* const* outputs, std::uint32_t channels) {
    // Hilo de audio. Cambio de maquina en el limite de bloque: es el unico
    // punto donde se puede cambiar sin que el bloque suene a mitad de dos
    // maquinas.
    core::MachineInstance* current = active_;
    core::MachineInstance* next = exchange_.swapAtBlockBoundary(current);
    if (next != current) {
        // La maquina retirada pasa a la cola del intercambio, que la
        // destruye el hilo de control. Aqui no se ejecuta ningun destructor.
        if (owned_ && owned_.get() == current) {
            owned_.release();
        }
        active_ = next;
    }

    if (active_ != nullptr) {
        applyPendingCommands();
        active_->runBlock(frames, outputs, channels);
        framesRendered_.fetch_add(frames, std::memory_order_relaxed);
    }

    core::MachineSnapshot snapshot;
    if (active_ != nullptr) {
        const core::EngineStatus status = active_->status();
        snapshot.frameCounter = framesRendered_.load(std::memory_order_relaxed);
        snapshot.cpuCycles = status.cpuCycles;
        snapshot.programCounter = status.programCounter;
        snapshot.accumulator = status.accumulator;
        snapshot.psw = status.psw;
        snapshot.bank = status.bank;
        snapshot.faulted = status.faulted;
        snapshot.message = status.faultMessage;
        if (outputs != nullptr && channels > 0 && outputs[0] != nullptr && frames > 0) {
            snapshot.outputLevel = outputs[0][frames - 1];
        }
    }
    snapshots_.publish(snapshot);
}

// Hilo de control: estado de la maquina, reconstruido desde la instantanea
// publicada por el hilo de audio. No toca la maquina activa.
core::EngineStatus MachineHost::status() {
    core::EngineStatus status;
    core::MachineSnapshot snapshot;
    if (!snapshots_.read(snapshot)) {
        return status;
    }
    status.framesRendered = snapshot.frameCounter;
    status.cpuCycles = snapshot.cpuCycles;
    status.programCounter = snapshot.programCounter;
    status.accumulator = snapshot.accumulator;
    status.psw = snapshot.psw;
    status.bank = snapshot.bank;
    status.faulted = snapshot.faulted;
    status.faultMessage = snapshot.message;
    return status;
}

void MachineHost::applyPendingCommands() {
    core::Command command;
    while (commands_.pop(command)) {
        if (active_ == nullptr) {
            continue;
        }
        // Los comandos discretos usan el rango 0x1000+ para no chocar con
        // los ids de parametro del perfil.
        if (command.id >= 0x1000u) {
            active_->setParameter(command.id - 0x1000u, static_cast<float>(command.value) / 32767.0f);
        } else {
            active_->writeInput(command.id, static_cast<float>(command.value) / 32767.0f);
        }
    }
}

std::size_t MachineHost::service() {
    std::size_t destroyed = 0;
    while (exchange_.drainRetired() != 0) {
        ++destroyed;
    }
    return destroyed;
}

bool MachineHost::snapshot(core::MachineSnapshot& out) const {
    return snapshots_.read(out);
}

core::EngineStatus MachineHost::status() const {
    core::EngineStatus status;
    core::MachineSnapshot snapshot;
    if (!snapshots_.read(snapshot)) {
        return status;
    }
    status.framesRendered = snapshot.frameCounter;
    status.cpuCycles = snapshot.cpuCycles;
    status.programCounter = snapshot.programCounter;
    status.accumulator = snapshot.accumulator;
    status.psw = snapshot.psw;
    status.bank = snapshot.bank;
    status.faulted = snapshot.faulted;
    status.faultMessage = snapshot.message;
    return status;
}

DiagnosticsContext MachineHost::context() const {
    DiagnosticsContext context;
    context.firmwareId = manifest_.id;
    context.firmwareName = manifest_.name;
    context.firmwareVersion = manifest_.version;
    context.profileId = profileId_;
    context.sampleRateHz = config_.sampleRateHz;
    context.blockSize = config_.blockSize;
    context.framesRendered = framesRendered_.load(std::memory_order_relaxed);
    context.hotSwaps = exchange_.swapCount();
    context.rejectedSwaps = exchange_.rejectedCount();
    context.droppedCommands = commands_.dropped();
    context.droppedLogs = log_.dropped();
    context.retiredPending = exchange_.retiredPending();
    return context;
}

bool MachineHost::postCommand(std::uint32_t id, std::int32_t value) {
    core::Command command;
    command.id = id;
    command.value = value;
    return commands_.push(command);
}

void MachineHost::log(std::uint32_t id, std::int32_t value, const std::string& text) {
    core::LogRecord record;
    record.timestampUs = timestampUs_.fetch_add(1, std::memory_order_relaxed);
    record.id = id;
    record.value = value;
    appendText(record, text);
    log_.push(record);
}

} // namespace bdmcs48::host
