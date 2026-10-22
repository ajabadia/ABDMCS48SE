// TestFramework.h - Micro framework de pruebas.
//
// Sin dependencias externas a proposito: la suite tiene que compilar y
// ejecutarse en cualquier maquina con un compilador de C++20, sin red y sin
// gestor de paquetes. Si un test falla, se lanza una excepcion con el
// mensaje y el runner la convierte en un fallo con su fichero y su linea.

#pragma once

#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace test {

class Failure : public std::exception {
public:
    explicit Failure(std::string message) : message_(std::move(message)) {}
    const char* what() const noexcept override { return message_.c_str(); }
    const std::string& message() const { return message_; }

private:
    std::string message_;
};

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> body;
};

std::vector<TestCase>& registry();

struct Registrar {
    Registrar(const char* suite, const char* name, std::function<void()> body);
};

int runAll(const std::string& filter);

template <typename T>
std::string describe(const T& value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

inline std::string describe(bool value) { return value ? "true" : "false"; }
inline std::string describe(const std::string& value) { return "\"" + value + "\""; }

} // namespace test

#define TEST(suite_name, test_name)                                                             \
    static void suite_name##_##test_name##_body();                                              \
    static const ::test::Registrar suite_name##_##test_name##_registrar(                       \
        #suite_name, #test_name, &suite_name##_##test_name##_body);                             \
    static void suite_name##_##test_name##_body()

#define CHECK(condition)                                                                        \
    do {                                                                                        \
        if (!(condition)) {                                                                     \
            throw ::test::Failure(std::string("CHECK failed: ") + #condition + " (" +          \
                                  __FILE__ + ":" + std::to_string(__LINE__) + ")");              \
        }                                                                                       \
    } while (false)

#define CHECK_EQ(actual, expected)                                                              \
    do {                                                                                        \
        const auto& actual_value = (actual);                                                    \
        const auto& expected_value = (expected);                                                \
        if (!(actual_value == expected_value)) {                                                \
            throw ::test::Failure(std::string("CHECK_EQ failed: ") + #actual + " == " +          \
                                  #expected + "\n  actual:   " +                                \
                                  ::test::describe(actual_value) + "\n  esperado: " +           \
                                  ::test::describe(expected_value) + " (" + __FILE__ + ":" +     \
                                  std::to_string(__LINE__) + ")");                              \
        }                                                                                       \
    } while (false)

#define CHECK_NEAR(actual, expected, tolerance)                                                \
    do {                                                                                        \
        const double actual_value = static_cast<double>(actual);                                \
        const double expected_value = static_cast<double>(expected);                            \
        const double allowed = static_cast<double>(tolerance);                                   \
        const double difference = actual_value > expected_value ? actual_value - expected_value  \
                                                                : expected_value - actual_value; \
        if (!(difference <= allowed)) {                                                         \
            throw ::test::Failure(std::string("CHECK_NEAR failed: ") + #actual + " ~ " +        \
                                  #expected + "\n  actual:   " +                                \
                                  ::test::describe(actual_value) + "\n  esperado: " +           \
                                  ::test::describe(expected_value) + " +/- " +                   \
                                  ::test::describe(allowed) + " (" + __FILE__ + ":" +            \
                                  std::to_string(__LINE__) + ")");                              \
        }                                                                                       \
    } while (false)
