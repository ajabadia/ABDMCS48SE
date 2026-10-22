// Json.h - Analizador JSON minimo y estricto.
//
// El core no depende de JSON: este parser vive en la capa de firmware,
// que es la unica que habla con el manifiesto del paquete.
//
// Se implementa aqui, en vez de traer una dependencia, por dos razones:
// el paquete tiene que poder validarse en un host sin red y sin gestor de
// paquetes, y el subconjunto necesario es pequeno. El parser es ESTRICTO
// (rechaza lo que la gramática no permite), tiene limite de profundidad y
// no asigna memoria sin cota.

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace bdmcs48::firmware {

class JsonValue {
public:
    enum class Type { Null, Boolean, Number, String, Array, Object };
    using Array = std::vector<JsonValue>;
    using Member = std::pair<std::string, JsonValue>;
    using Object = std::vector<Member>;

    JsonValue() = default;

    static JsonValue makeNull() { return JsonValue(); }
    static JsonValue makeBoolean(bool value);
    static JsonValue makeNumber(double value);
    static JsonValue makeString(std::string value);
    static JsonValue makeArray(Array value);
    static JsonValue makeObject(Object value);

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBoolean() const { return type_ == Type::Boolean; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBoolean() const { return bool_; }
    double asNumber() const { return number_; }
    const std::string& asString() const { return string_; }
    const Array& asArray() const { return array_; }
    const Object& asObject() const { return object_; }

    // Busca una clave en un objeto. Devuelve nullptr si no existe.
    const JsonValue* find(const std::string& key) const;
    bool has(const std::string& key) const { return find(key) != nullptr; }

    const char* typeName() const;

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    Array array_;
    Object object_;
};

class JsonParser {
public:
    // Limites por defecto: protects el host de un manifiesto hostil.
    static constexpr std::size_t kMaxDepth = 64;

    // Devuelve false y rellena `error` con un mensaje que incluye la
    // posicion del problema.
    static bool parse(const std::string& text, JsonValue& out, std::string& error);
};

} // namespace bdmcs48::firmware
