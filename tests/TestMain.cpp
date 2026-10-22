#include "TestFramework.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace test {

std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

Registrar::Registrar(const char* suite, const char* name, std::function<void()> body) {
    registry().push_back(TestCase{suite, name, std::move(body)});
}

int runAll(const std::string& filter) {
    std::vector<TestCase>& cases = registry();
    std::stable_sort(cases.begin(), cases.end(), [](const TestCase& a, const TestCase& b) {
        if (a.suite != b.suite) {
            return a.suite < b.suite;
        }
        return a.name < b.name;
    });

    std::size_t passed = 0;
    std::size_t failed = 0;
    std::size_t skipped = 0;
    std::string currentSuite;

    const auto start = std::chrono::steady_clock::now();

    for (const TestCase& testCase : cases) {
        const std::string fullName = testCase.suite + "." + testCase.name;
        if (!filter.empty() && fullName.find(filter) == std::string::npos) {
            ++skipped;
            continue;
        }
        if (testCase.suite != currentSuite) {
            currentSuite = testCase.suite;
            std::cout << "\n[" << currentSuite << "]\n";
        }

        try {
            testCase.body();
            ++passed;
            std::cout << "  ok    " << testCase.name << "\n";
        } catch (const Failure& failure) {
            ++failed;
            std::cout << "  FALLO " << testCase.name << "\n        " << failure.message() << "\n";
        } catch (const std::exception& error) {
            ++failed;
            std::cout << "  FALLO " << testCase.name << "\n        excepcion: " << error.what() << "\n";
        } catch (...) {
            ++failed;
            std::cout << "  FALLO " << testCase.name << "\n        excepcion desconocida\n";
        }
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    std::cout << "\n--------------------------------------------------\n";
    std::cout << passed << " correctos, " << failed << " fallidos";
    if (skipped != 0) {
        std::cout << ", " << skipped << " filtrados";
    }
    std::cout << " (" << elapsed.count() << " ms)\n";

    return failed == 0 ? 0 : 1;
}

} // namespace test

int main(int argc, char** argv) {
    std::string filter;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument.rfind("--filter=", 0) == 0) {
            filter = argument.substr(std::strlen("--filter="));
        } else if (argument == "--filter" && i + 1 < argc) {
            filter = argv[++i];
        }
    }
    return test::runAll(filter);
}
