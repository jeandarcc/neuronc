// Neuron Minimal Test Framework
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <functional>

struct TestCase {
    std::string name;
    std::function<bool()> fn;
};

inline std::vector<TestCase>& getTests() {
    static std::vector<TestCase> tests;
    return tests;
}

#define TEST(name) \
    static bool test_##name(); \
    static bool _reg_##name = (getTests().push_back({#name, test_##name}), true); \
    static bool test_##name()

#define ASSERT_TRUE(cond)  do { if (!(cond))  { std::cerr << "  FAIL: " << #cond  << " at " << __FILE__ << ":" << __LINE__ << std::endl; return false; } } while(0)
#define ASSERT_FALSE(cond) do { if ((cond))   { std::cerr << "  FAIL: !" << #cond << " at " << __FILE__ << ":" << __LINE__ << std::endl; return false; } } while(0)
#define ASSERT_EQ(a, b)    do { if ((a) != (b)) { std::cerr << "  FAIL: " << #a << " == " << #b << " at " << __FILE__ << ":" << __LINE__ << std::endl; return false; } } while(0)

// Single-TU test binary: touching this file forces include-only test changes
// to rebuild reliably. Refreshed for multiline implicit-body parser/sema coverage.
#include "lexer/test_lexer.cpp"
#include "parser/test_parser.cpp"
#include "cli/test_project_config.cpp"
#include "cli/test_project_commands.cpp"
#include "cli/test_project_generator.cpp"
#include "cli/test_package_cli.cpp"
#include "cli/test_repl.cpp"
#include "cli/test_package_query_commands.cpp"
#include "cli/test_web_build_pipeline.cpp"
#include "cli/test_modulecpp_manifest.cpp"
#include "cli/test_package_lock.cpp"
#include "cli/test_package_manager.cpp"
#include "cli/test_build_minimal.cpp"
#include "cli/test_settings_macros.cpp"
#include "cli/test_language_cli.cpp"
#include "main/test_build_support.cpp"
#include "main/test_diagnostic_engine.cpp"
#include "main/test_usage_text_builder.cpp"
// Keep JIT/provider tests in the single test translation unit.
#include "codegen/test_llvm_codegen.cpp"
#include "codegen/test_jit_engine.cpp"
#include "mir/test_mir.cpp"
#include "mir/test_mir_ownership.cpp"
#include "nir/test_nir_builder.cpp"
#include "nir/test_optimizer.cpp"
#include "nir/test_exceptions.cpp"
#include "ncon/test_bytecode.cpp"
#include "ncon/test_format.cpp"
#include "ncon/test_manifest.cpp"
#include "ncon/test_runtime_bridge.cpp"
#include "ncon/test_sandbox.cpp"
#include "ncon/test_mini_cli.cpp"
#include "ncon/test_vm.cpp"
#include "sema/test_semantic.cpp"
#include "runtime/test_gpu_runtime.cpp"
#include "runtime/test_graphics_runtime.cpp"
#include "runtime/test_tensor_runtime.cpp"
#include "runtime/test_nn_runtime.cpp"
#include "runtime/test_platform_runtime.cpp"
#include "lsp/test_lsp.cpp"
#include "lsp/test_diagnostic_localization.cpp"

namespace {

void printUsage() {
    std::cout << "Usage: neuron_tests [--list-tests] [--filter <name>]" << std::endl;
}

bool matchesFilter(const TestCase& test, const std::string& filter) {
    return filter.empty() || test.name == filter;
}

int runSelectedTests(const std::string& filter) {
    int passed = 0;
    int failed = 0;
    bool found = false;

    for (auto& test : getTests()) {
        if (!matchesFilter(test, filter)) {
            continue;
        }
        found = true;
        std::cout << "[ RUN  ] " << test.name << std::endl;
        if (test.fn()) {
            std::cout << "[ PASS ] " << test.name << std::endl;
            passed++;
        } else {
            std::cout << "[ FAIL ] " << test.name << std::endl;
            failed++;
        }
    }

    if (!filter.empty() && !found) {
        std::cerr << "[ ERROR ] Test not found: " << filter << std::endl;
        return 2;
    }

    std::cout << "\n" << passed << " passed, " << failed << " failed, "
              << (passed + failed) << " total." << std::endl;
    return failed > 0 ? 1 : 0;
}

} // namespace

int main(int argc, char* argv[]) {
    bool listTests = false;
    std::string filter;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--list-tests") {
            listTests = true;
        } else if (arg == "--filter") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --filter" << std::endl;
                printUsage();
                return 2;
            }
            filter = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            printUsage();
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            printUsage();
            return 2;
        }
    }

    if (listTests) {
        for (const auto& test : getTests()) {
            std::cout << test.name << std::endl;
        }
        return 0;
    }

    return runSelectedTests(filter);
}
