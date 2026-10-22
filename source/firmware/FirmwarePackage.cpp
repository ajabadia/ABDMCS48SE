#include "firmware/FirmwarePackage.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

#include "core/ProgramMemory.h"
#include "firmware/Json.h"

namespace bdmcs48::firmware {
namespace {

void addIssue(std::vector<ValidationIssue>& issues, std::string field, std::string message) {
    issues.push_back(ValidationIssue{std::move(field), std::move(message)});
}

bool readString(const JsonValue& object, const char* key, std::string& out, std::vector<ValidationIssue>& issues,
                bool required) {
    const JsonValue* value = object.find(key);
    if (value == nullptr) {
        if (required) {
            addIssue(issues, key, std::string("falta el campo obligatorio '") + key + "'");
            return false;
        }
        return true;
    }
    if (!value->isString()) {
        addIssue(issues, key, std::string("el campo '") + key + "' debe ser una cadena");
        return false;
    }
    out = value->asString();
    return true;
}

bool readNumber(const JsonValue& object, const char* key, double& out, std::vector<ValidationIssue>& issues,
                bool required, double fallback) {
    const JsonValue* value = object.find(key);
    if (value == nullptr) {
        if (required) {
            addIssue(issues, key, std::string("falta el campo obligatorio '") + key + "'");
            return false;
        }
        out = fallback;
        return true;
    }
    if (!value->isNumber()) {
        addIssue(issues, key, std::string("el campo '") + key + "' debe ser un numero");
        return false;
    }
    out = value->asNumber();
    return true;
}

bool readBool(const JsonValue& object, const char* key, bool& out, std::vector<ValidationIssue>& issues,
              bool required, bool fallback) {
    const JsonValue* value = object.find(key);
    if (value == nullptr) {
        if (required) {
            addIssue(issues, key, std::string("falta el campo obligatorio '") + key + "'");
            return false;
        }
        out = fallback;
        return true;
    }
    if (!value->isBoolean()) {
        addIssue(issues, key, std::string("el campo '") + key + "' debe ser booleano");
        return false;
    }
    out = value->asBoolean();
    return true;
}

bool isHex64(const std::string& text) {
    if (text.size() != 64) {
        return false;
    }
    for (const char c : text) {
        const bool digit = c >= '0' && c <= '9';
        const bool lower = c >= 'a' && c <= 'f';
        const bool upper = c >= 'A' && c <= 'F';
        if (!digit && !lower && !upper) {
            return false;
        }
    }
    return true;
}

} // namespace

bool FirmwarePackage::isKnownProfile(const std::string& profile) {
    return profile == "generic-mcs48";
}

bool FirmwarePackage::isKnownCpuVariant(const std::string& variant) {
    return variant == "8048" || variant == "8049" || variant == "8039" || variant == "8035";
}

bool FirmwarePackage::isSafeRelativePath(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    if (path.front() == '/' || path.front() == '\\') {
        return false; // ruta absoluta
    }
    if (path.size() >= 2 && path[1] == ':') {
        return false; // unidad de Windows
    }
    if (path.find('\0') != std::string::npos) {
        return false;
    }
    // Segmentos: no se admiten "." ni ".." ni separadores duplicados.
    std::string segment;
    const auto checkSegment = [&segment]() -> bool {
        if (segment.empty()) {
            return false;
        }
        if (segment == "." || segment == "..") {
            return false;
        }
        segment.clear();
        return true;
    };
    for (const char c : path) {
        if (c == '/' || c == '\\') {
            if (!checkSegment()) {
                return false;
            }
        } else {
            segment.push_back(c);
        }
    }
    return checkSegment();
}

bool FirmwarePackage::parseManifest(const std::string& text, FirmwareManifest& out,
                                    std::vector<ValidationIssue>& issues) {
    JsonValue root;
    std::string error;
    if (!JsonParser::parse(text, root, error)) {
        addIssue(issues, "manifest", "JSON invalido: " + error);
        return false;
    }
    if (!root.isObject()) {
        addIssue(issues, "manifest", "el manifiesto debe ser un objeto JSON");
        return false;
    }

    bool ok = true;
    ok = readString(root, "format", out.format, issues, true) && ok;
    double formatVersion = 0.0;
    ok = readNumber(root, "formatVersion", formatVersion, issues, true, 0.0) && ok;
    out.formatVersion = static_cast<std::uint32_t>(formatVersion);
    ok = readString(root, "id", out.id, issues, true) && ok;
    ok = readString(root, "name", out.name, issues, true) && ok;
    ok = readString(root, "version", out.version, issues, true) && ok;
    ok = readString(root, "author", out.author, issues, false) && ok;
    ok = readString(root, "description", out.description, issues, false) && ok;
    ok = readString(root, "license", out.license, issues, false) && ok;
    ok = readString(root, "provenance", out.provenance, issues, false) && ok;

    // --- machine ----------------------------------------------------------
    const JsonValue* machine = root.find("machine");
    if (machine == nullptr || !machine->isObject()) {
        addIssue(issues, "machine", "falta el objeto 'machine'");
        ok = false;
    } else {
        ok = readString(*machine, "profile", out.machine.profile, issues, true) && ok;

        const JsonValue* cpu = machine->find("cpu");
        if (cpu == nullptr || !cpu->isObject()) {
            addIssue(issues, "machine.cpu", "falta el objeto 'machine.cpu'");
            ok = false;
        } else {
            ok = readString(*cpu, "family", out.machine.cpu.family, issues, true) && ok;
            ok = readString(*cpu, "variant", out.machine.cpu.variant, issues, true) && ok;
            double clockHz = 0.0;
            ok = readNumber(*cpu, "clockHz", clockHz, issues, true, 0.0) && ok;
            out.machine.cpu.clockHz = static_cast<std::uint32_t>(clockHz);
        }

        const JsonValue* program = machine->find("programMemory");
        if (program == nullptr || !program->isObject()) {
            addIssue(issues, "machine.programMemory", "falta el objeto 'machine.programMemory'");
            ok = false;
        } else {
            double value = 0.0;
            ok = readNumber(*program, "addressBits", value, issues, true, 0.0) && ok;
            out.machine.programMemory.addressBits = static_cast<std::uint32_t>(value);
            ok = readNumber(*program, "bankSizeBytes", value, issues, true, 0.0) && ok;
            out.machine.programMemory.bankSizeBytes = static_cast<std::uint32_t>(value);
            ok = readNumber(*program, "bankCount", value, issues, true, 0.0) && ok;
            out.machine.programMemory.bankCount = static_cast<std::uint32_t>(value);
        }
    }

    // --- images -----------------------------------------------------------
    const JsonValue* images = root.find("images");
    if (images == nullptr || !images->isArray()) {
        addIssue(issues, "images", "falta el array 'images'");
        ok = false;
    } else {
        for (const JsonValue& entry : images->asArray()) {
            if (!entry.isObject()) {
                addIssue(issues, "images", "cada imagen debe ser un objeto");
                ok = false;
                continue;
            }
            ImageDescription image;
            readString(entry, "id", image.id, issues, true);
            readString(entry, "file", image.file, issues, true);
            readString(entry, "region", image.region, issues, true);
            double value = 0.0;
            readNumber(entry, "bank", value, issues, false, 0.0);
            image.bank = static_cast<std::uint32_t>(value);
            readNumber(entry, "offset", value, issues, false, 0.0);
            image.offset = static_cast<std::uint32_t>(value);
            readNumber(entry, "size", value, issues, false, 0.0);
            image.size = static_cast<std::uint32_t>(value);
            readString(entry, "sha256", image.sha256, issues, false);
            out.images.push_back(std::move(image));
        }
    }

    // --- inputs / outputs -------------------------------------------------
    const JsonValue* inputs = root.find("inputs");
    if (inputs != nullptr && inputs->isArray()) {
        for (const JsonValue& entry : inputs->asArray()) {
            if (!entry.isObject()) {
                addIssue(issues, "inputs", "cada entrada debe ser un objeto");
                ok = false;
                continue;
            }
            InputDescription input;
            readString(entry, "id", input.id, issues, true);
            readString(entry, "type", input.type, issues, true);
            double value = 0.0;
            readNumber(entry, "index", value, issues, false, 0.0);
            input.index = static_cast<std::uint32_t>(value);
            readNumber(entry, "bits", value, issues, false, 8.0);
            input.bits = static_cast<std::uint32_t>(value);
            out.inputs.push_back(std::move(input));
        }
    }

    const JsonValue* outputs = root.find("outputs");
    if (outputs != nullptr && outputs->isArray()) {
        for (const JsonValue& entry : outputs->asArray()) {
            if (!entry.isObject()) {
                addIssue(issues, "outputs", "cada salida debe ser un objeto");
                ok = false;
                continue;
            }
            OutputDescription output;
            readString(entry, "id", output.id, issues, true);
            readString(entry, "type", output.type, issues, true);
            out.outputs.push_back(std::move(output));
        }
    }

    // --- parameters -------------------------------------------------------
    const JsonValue* parameters = root.find("parameters");
    if (parameters != nullptr && parameters->isArray()) {
        for (const JsonValue& entry : parameters->asArray()) {
            if (!entry.isObject()) {
                addIssue(issues, "parameters", "cada parametro debe ser un objeto");
                ok = false;
                continue;
            }
            ParameterDescription parameter;
            readString(entry, "id", parameter.id, issues, true);
            readString(entry, "type", parameter.type, issues, false);
            readNumber(entry, "minimum", parameter.minimum, issues, false, 0.0);
            readNumber(entry, "maximum", parameter.maximum, issues, false, 1.0);
            readNumber(entry, "initial", parameter.initial, issues, false, parameter.minimum);
            readBool(entry, "hostAutomatable", parameter.hostAutomatable, issues, false, false);
            out.parameters.push_back(std::move(parameter));
        }
    }

    return ok;
}

bool FirmwarePackage::validate(const FirmwareManifest& manifest, std::vector<ValidationIssue>& issues) {
    bool ok = true;

    if (manifest.format != kFormatId) {
        addIssue(issues, "format", "formato '" + manifest.format + "' no soportado (se esperaba '" +
                                       kFormatId + "')");
        ok = false;
    }
    if (manifest.formatVersion != kSupportedFormatVersion) {
        addIssue(issues, "formatVersion", "version de formato " + std::to_string(manifest.formatVersion) +
                                              " no soportada (este loader entiende la " +
                                              std::to_string(kSupportedFormatVersion) + ")");
        ok = false;
    }
    if (manifest.id.empty()) {
        addIssue(issues, "id", "el id del firmware esta vacio");
        ok = false;
    }
    if (manifest.name.empty()) {
        addIssue(issues, "name", "el nombre del firmware esta vacio");
        ok = false;
    }

    if (!isKnownProfile(manifest.machine.profile)) {
        addIssue(issues, "machine.profile", "perfil '" + manifest.machine.profile + "' desconocido");
        ok = false;
    }
    if (manifest.machine.cpu.family != "mcs48") {
        addIssue(issues, "machine.cpu.family", "familia '" + manifest.machine.cpu.family +
                                                   "' no soportada (solo 'mcs48')");
        ok = false;
    }
    if (!isKnownCpuVariant(manifest.machine.cpu.variant)) {
        addIssue(issues, "machine.cpu.variant", "variante '" + manifest.machine.cpu.variant + "' desconocida");
        ok = false;
    }
    if (manifest.machine.cpu.clockHz == 0 || manifest.machine.cpu.clockHz > 20'000'000u) {
        addIssue(issues, "machine.cpu.clockHz", "reloj fuera de rango");
        ok = false;
    }

    // El mapa de memoria del core es el que manda: si el manifiesto describe
    // otra cosa, el paquete no se puede cargar en esta maquina.
    if (manifest.machine.programMemory.addressBits != core::ProgramMemory::kAddressBits) {
        addIssue(issues, "machine.programMemory.addressBits",
                 "este core implementa " + std::to_string(core::ProgramMemory::kAddressBits) +
                     " bits de direccion de programa");
        ok = false;
    }
    if (manifest.machine.programMemory.bankSizeBytes != core::ProgramMemory::kBankSize) {
        addIssue(issues, "machine.programMemory.bankSizeBytes",
                 "este core implementa bancos de " + std::to_string(core::ProgramMemory::kBankSize) +
                     " bytes");
        ok = false;
    }
    if (manifest.machine.programMemory.bankCount != core::ProgramMemory::kBankCount) {
        addIssue(issues, "machine.programMemory.bankCount",
                 "este core implementa " + std::to_string(core::ProgramMemory::kBankCount) + " bancos");
        ok = false;
    }

    // --- imagenes ---------------------------------------------------------
    std::set<std::string> imageIds;
    bool hasProgramImage = false;
    for (const ImageDescription& image : manifest.images) {
        if (image.id.empty()) {
            addIssue(issues, "images", "hay una imagen sin id");
            ok = false;
            continue;
        }
        if (!imageIds.insert(image.id).second) {
            addIssue(issues, "images." + image.id, "id de imagen duplicado");
            ok = false;
        }
        if (!isSafeRelativePath(image.file)) {
            addIssue(issues, "images." + image.id + ".file",
                     "la ruta '" + image.file + "' no es una ruta relativa segura");
            ok = false;
        }
        if (image.region != "program" && image.region != "data") {
            addIssue(issues, "images." + image.id + ".region",
                     "region '" + image.region + "' desconocida (se esperaba 'program' o 'data')");
            ok = false;
        }
        if (image.region == "program") {
            hasProgramImage = true;
            if (image.bank >= manifest.machine.programMemory.bankCount) {
                addIssue(issues, "images." + image.id + ".bank", "banco fuera de rango");
                ok = false;
            }
            const std::uint64_t end = static_cast<std::uint64_t>(image.offset) + image.size;
            if (end > manifest.machine.programMemory.bankSizeBytes) {
                addIssue(issues, "images." + image.id + ".size",
                         "offset + size (" + std::to_string(end) + ") supera el banco de " +
                             std::to_string(manifest.machine.programMemory.bankSizeBytes) + " bytes");
                ok = false;
            }
        }
        if (!image.sha256.empty() && !isHex64(image.sha256)) {
            addIssue(issues, "images." + image.id + ".sha256", "el hash debe ser 64 caracteres hex");
            ok = false;
        }
    }
    if (!hasProgramImage) {
        addIssue(issues, "images", "el paquete no declara ninguna imagen de programa");
        ok = false;
    }

    // Solapamiento dentro de la misma region y banco.
    for (std::size_t i = 0; i < manifest.images.size(); ++i) {
        for (std::size_t j = i + 1; j < manifest.images.size(); ++j) {
            const ImageDescription& a = manifest.images[i];
            const ImageDescription& b = manifest.images[j];
            if (a.region != b.region || a.bank != b.bank) {
                continue;
            }
            const std::uint64_t aEnd = static_cast<std::uint64_t>(a.offset) + a.size;
            const std::uint64_t bEnd = static_cast<std::uint64_t>(b.offset) + b.size;
            if (a.offset < bEnd && b.offset < aEnd) {
                addIssue(issues, "images." + a.id, "la imagen se solapa con '" + b.id + "'");
                ok = false;
            }
        }
    }

    // --- entradas / salidas / parametros ----------------------------------
    std::set<std::string> inputIds;
    for (const InputDescription& input : manifest.inputs) {
        if (input.id.empty()) {
            addIssue(issues, "inputs", "hay una entrada sin id");
            ok = false;
            continue;
        }
        if (!inputIds.insert(input.id).second) {
            addIssue(issues, "inputs." + input.id, "id de entrada duplicado");
            ok = false;
        }
        if (input.bits == 0 || input.bits > 32) {
            addIssue(issues, "inputs." + input.id + ".bits", "ancho fuera de rango");
            ok = false;
        }
    }

    std::set<std::string> outputIds;
    for (const OutputDescription& output : manifest.outputs) {
        if (output.id.empty()) {
            addIssue(issues, "outputs", "hay una salida sin id");
            ok = false;
            continue;
        }
        if (!outputIds.insert(output.id).second) {
            addIssue(issues, "outputs." + output.id, "id de salida duplicado");
            ok = false;
        }
    }

    std::set<std::string> parameterIds;
    for (const ParameterDescription& parameter : manifest.parameters) {
        if (parameter.id.empty()) {
            addIssue(issues, "parameters", "hay un parametro sin id");
            ok = false;
            continue;
        }
        if (!parameterIds.insert(parameter.id).second) {
            addIssue(issues, "parameters." + parameter.id, "id de parametro duplicado");
            ok = false;
        }
        if (parameter.minimum > parameter.maximum) {
            addIssue(issues, "parameters." + parameter.id, "minimo mayor que maximo");
            ok = false;
        }
        if (parameter.initial < parameter.minimum || parameter.initial > parameter.maximum) {
            addIssue(issues, "parameters." + parameter.id + ".initial",
                     "el valor inicial esta fuera del rango");
            ok = false;
        }
    }

    return ok;
}

std::string FirmwarePackage::formatIssues(const std::vector<ValidationIssue>& issues) {
    std::string result;
    for (const ValidationIssue& issue : issues) {
        if (!result.empty()) {
            result += "; ";
        }
        result += issue.field + ": " + issue.message;
    }
    return result;
}

} // namespace bdmcs48::firmware
