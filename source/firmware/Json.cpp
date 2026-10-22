#include "firmware/Json.h"

#include <cmath>
#include <cstdlib>
#include <cstdio>

namespace bdmcs48::firmware {
namespace {

class Parser {
public:
    Parser(const std::string& text) : text_(text) {}

    bool parse(JsonValue& out) {
        skipWhitespace();
        if (!parseValue(out, 0)) {
            return false;
        }
        skipWhitespace();
        if (position_ != text_.size()) {
            fail("se esperaba el final del documento");
            return false;
        }
        return true;
    }

    const std::string& error() const { return error_; }

private:
    bool fail(const std::string& message) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%zu", position_);
        error_ = message + " (posicion " + buffer + ")";
        return false;
    }

    void skipWhitespace() {
        while (position_ < text_.size()) {
            const char c = text_[position_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++position_;
            } else {
                break;
            }
        }
    }

    bool parseValue(JsonValue& out, std::size_t depth) {
        if (depth > JsonParser::kMaxDepth) {
            return fail("profundidad maxima superada");
        }
        if (position_ >= text_.size()) {
            return fail("documento incompleto");
        }

        const char c = text_[position_];
        switch (c) {
        case '{': return parseObject(out, depth);
        case '[': return parseArray(out, depth);
        case '"': {
            std::string value;
            if (!parseString(value)) {
                return false;
            }
            out = JsonValue::makeString(std::move(value));
            return true;
        }
        case 't':
            if (text_.compare(position_, 4, "true") == 0) {
                position_ += 4;
                out = JsonValue::makeBoolean(true);
                return true;
            }
            return fail("literal invalido");
        case 'f':
            if (text_.compare(position_, 5, "false") == 0) {
                position_ += 5;
                out = JsonValue::makeBoolean(false);
                return true;
            }
            return fail("literal invalido");
        case 'n':
            if (text_.compare(position_, 4, "null") == 0) {
                position_ += 4;
                out = JsonValue::makeNull();
                return true;
            }
            return fail("literal invalido");
        default:
            return parseNumber(out);
        }
    }

    bool parseObject(JsonValue& out, std::size_t depth) {
        ++position_; // '{'
        JsonValue::Object object;

        skipWhitespace();
        if (position_ < text_.size() && text_[position_] == '}') {
            ++position_;
            out = JsonValue::makeObject(std::move(object));
            return true;
        }

        while (true) {
            skipWhitespace();
            if (position_ >= text_.size() || text_[position_] != '"') {
                return fail("se esperaba una clave de objeto");
            }
            std::string key;
            if (!parseString(key)) {
                return false;
            }
            skipWhitespace();
            if (position_ >= text_.size() || text_[position_] != ':') {
                return fail("se esperaba ':' tras la clave");
            }
            ++position_;
            skipWhitespace();

            JsonValue value;
            if (!parseValue(value, depth + 1)) {
                return false;
            }
            // Claves duplicadas: se rechazan. Un manifiesto con la misma clave
            // dos veces es ambiguo y no debe depender del orden del parser.
            for (const auto& member : object) {
                if (member.first == key) {
                    return fail("clave duplicada: " + key);
                }
            }
            object.emplace_back(std::move(key), std::move(value));

            skipWhitespace();
            if (position_ >= text_.size()) {
                return fail("objeto sin cerrar");
            }
            if (text_[position_] == ',') {
                ++position_;
                continue;
            }
            if (text_[position_] == '}') {
                ++position_;
                out = JsonValue::makeObject(std::move(object));
                return true;
            }
            return fail("se esperaba ',' o '}'");
        }
    }

    bool parseArray(JsonValue& out, std::size_t depth) {
        ++position_; // '['
        JsonValue::Array array;

        skipWhitespace();
        if (position_ < text_.size() && text_[position_] == ']') {
            ++position_;
            out = JsonValue::makeArray(std::move(array));
            return true;
        }

        while (true) {
            skipWhitespace();
            JsonValue value;
            if (!parseValue(value, depth + 1)) {
                return false;
            }
            array.push_back(std::move(value));

            skipWhitespace();
            if (position_ >= text_.size()) {
                return fail("array sin cerrar");
            }
            if (text_[position_] == ',') {
                ++position_;
                continue;
            }
            if (text_[position_] == ']') {
                ++position_;
                out = JsonValue::makeArray(std::move(array));
                return true;
            }
            return fail("se esperaba ',' o ']'");
        }
    }

    bool parseString(std::string& out) {
        ++position_; // '"'
        out.clear();

        while (position_ < text_.size()) {
            const unsigned char c = static_cast<unsigned char>(text_[position_]);
            if (c == '"') {
                ++position_;
                return true;
            }
            if (c == '\\') {
                ++position_;
                if (position_ >= text_.size()) {
                    return fail("cadena sin cerrar");
                }
                const char escape = text_[position_++];
                switch (escape) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    std::uint32_t code = 0;
                    if (!parseHex4(code)) {
                        return false;
                    }
                    if (code >= 0xd800 && code <= 0xdbff) {
                        // Par alto: se espera un par bajo.
                        if (position_ + 1 < text_.size() && text_[position_] == '\\' &&
                            text_[position_ + 1] == 'u') {
                            position_ += 2;
                            std::uint32_t low = 0;
                            if (!parseHex4(low)) {
                                return false;
                            }
                            if (low < 0xdc00 || low > 0xdfff) {
                                return fail("par UTF-16 invalido");
                            }
                            code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
                        } else {
                            return fail("par UTF-16 sin pareja");
                        }
                    }
                    appendUtf8(code, out);
                    break;
                }
                default:
                    return fail("escape desconocido");
                }
                continue;
            }
            if (c < 0x20) {
                return fail("caracter de control sin escapar en la cadena");
            }
            out.push_back(static_cast<char>(c));
            ++position_;
        }
        return fail("cadena sin cerrar");
    }

    bool parseHex4(std::uint32_t& out) {
        if (position_ + 4 > text_.size()) {
            return fail("escape \\u incompleto");
        }
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[position_++];
            value <<= 4;
            if (c >= '0' && c <= '9') {
                value |= static_cast<std::uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                value |= static_cast<std::uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                value |= static_cast<std::uint32_t>(c - 'A' + 10);
            } else {
                return fail("escape \\u invalido");
            }
        }
        out = value;
        return true;
    }

    static void appendUtf8(std::uint32_t code, std::string& out) {
        if (code < 0x80) {
            out.push_back(static_cast<char>(code));
        } else if (code < 0x800) {
            out.push_back(static_cast<char>(0xc0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else if (code < 0x10000) {
            out.push_back(static_cast<char>(0xe0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        } else {
            out.push_back(static_cast<char>(0xf0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3f)));
        }
    }

    bool parseNumber(JsonValue& out) {
        const std::size_t start = position_;
        if (position_ < text_.size() && text_[position_] == '-') {
            ++position_;
        }
        if (position_ >= text_.size() || !isDigit(text_[position_])) {
            return fail("numero invalido");
        }
        if (text_[position_] == '0') {
            ++position_;
        } else {
            while (position_ < text_.size() && isDigit(text_[position_])) {
                ++position_;
            }
        }
        if (position_ < text_.size() && text_[position_] == '.') {
            ++position_;
            if (position_ >= text_.size() || !isDigit(text_[position_])) {
                return fail("parte decimal invalida");
            }
            while (position_ < text_.size() && isDigit(text_[position_])) {
                ++position_;
            }
        }
        if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
            ++position_;
            if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) {
                ++position_;
            }
            if (position_ >= text_.size() || !isDigit(text_[position_])) {
                return fail("exponente invalido");
            }
            while (position_ < text_.size() && isDigit(text_[position_])) {
                ++position_;
            }
        }

        const std::string literal = text_.substr(start, position_ - start);
        out = JsonValue::makeNumber(std::strtod(literal.c_str(), nullptr));
        return true;
    }

    static bool isDigit(char c) { return c >= '0' && c <= '9'; }

    const std::string& text_;
    std::size_t position_ = 0;
    std::string error_;
};

} // namespace

JsonValue JsonValue::makeBoolean(bool value) {
    JsonValue result;
    result.type_ = Type::Boolean;
    result.bool_ = value;
    return result;
}

JsonValue JsonValue::makeNumber(double value) {
    JsonValue result;
    result.type_ = Type::Number;
    result.number_ = value;
    return result;
}

JsonValue JsonValue::makeString(std::string value) {
    JsonValue result;
    result.type_ = Type::String;
    result.string_ = std::move(value);
    return result;
}

JsonValue JsonValue::makeArray(Array value) {
    JsonValue result;
    result.type_ = Type::Array;
    result.array_ = std::move(value);
    return result;
}

JsonValue JsonValue::makeObject(Object value) {
    JsonValue result;
    result.type_ = Type::Object;
    result.object_ = std::move(value);
    return result;
}

const JsonValue* JsonValue::find(const std::string& key) const {
    if (type_ != Type::Object) {
        return nullptr;
    }
    for (const auto& member : object_) {
        if (member.first == key) {
            return &member.second;
        }
    }
    return nullptr;
}

const char* JsonValue::typeName() const {
    switch (type_) {
    case Type::Null: return "null";
    case Type::Boolean: return "boolean";
    case Type::Number: return "number";
    case Type::String: return "string";
    case Type::Array: return "array";
    case Type::Object: return "object";
    }
    return "desconocido";
}

bool JsonParser::parse(const std::string& text, JsonValue& out, std::string& error) {
    Parser parser(text);
    if (parser.parse(out)) {
        error.clear();
        return true;
    }
    error = parser.error();
    return false;
}

} // namespace bdmcs48::firmware
