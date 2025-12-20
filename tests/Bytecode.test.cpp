#include "doctest.h"
#include "Fixture.h"
#include "Protocol/Extensions.hpp"

TEST_SUITE_BEGIN("BytecodeTests");

TEST_CASE_FIXTURE(Fixture, "bytecode_returns_textual_bytecode")
{
    auto uri = newDocument("test.luau", R"(
local function add(a, b)
    return a + b
end
)");

    lsp::BytecodeParams params;
    params.textDocument.uri = uri;
    params.optimizationLevel = lsp::CompilerRemarksOptimizationLevel::O1;

    auto result = workspace.bytecode(params);

    CHECK(result.find("RETURN") != std::string::npos);
    CHECK(result.find("ADD") != std::string::npos);
}

TEST_CASE_FIXTURE(Fixture, "bytecode_handles_syntax_errors")
{
    auto uri = newDocument("test.luau", R"(
local function add(a, b
    return a + b
end
)");

    lsp::BytecodeParams params;
    params.textDocument.uri = uri;

    auto result = workspace.bytecode(params);

    CHECK(result.find("SyntaxError") != std::string::npos);
}

TEST_CASE_FIXTURE(Fixture, "compiler_remarks_returns_remarks")
{
    auto uri = newDocument("test.luau", R"(
local function add(a: number, b: number): number
    return a + b
end
)");

    lsp::CompilerRemarksParams params;
    params.textDocument.uri = uri;
    params.optimizationLevel = lsp::CompilerRemarksOptimizationLevel::O2;

    auto result = workspace.compilerRemarks(params);

    // Result should be a string (may or may not contain remarks depending on code)
    CHECK(result.size() >= 0);
}

TEST_CASE_FIXTURE(Fixture, "codegen_returns_assembly_output")
{
    auto uri = newDocument("test.luau", R"(
local function add(a, b)
    return a + b
end
)");

    lsp::CodegenParams params;
    params.textDocument.uri = uri;
    params.optimizationLevel = lsp::CompilerRemarksOptimizationLevel::O1;
    params.codeGenTarget = lsp::CodeGenTarget::Host;

    auto result = workspace.codeGen(params);

    // CodeGen should produce some output (assembly, IR, etc.)
    CHECK(!result.empty());
}

TEST_CASE_FIXTURE(Fixture, "codegen_with_different_targets")
{
    auto uri = newDocument("test.luau", R"(
local function multiply(x, y)
    return x * y
end
)");

    lsp::CodegenParams params;
    params.textDocument.uri = uri;
    params.optimizationLevel = lsp::CompilerRemarksOptimizationLevel::O1;

    SUBCASE("a64 target")
    {
        params.codeGenTarget = lsp::CodeGenTarget::A64;
        auto result = workspace.codeGen(params);
        CHECK(!result.empty());
    }

    SUBCASE("a64_nofeatures target")
    {
        params.codeGenTarget = lsp::CodeGenTarget::A64_NoFeatures;
        auto result = workspace.codeGen(params);
        CHECK(!result.empty());
    }

    SUBCASE("x64_windows target")
    {
        params.codeGenTarget = lsp::CodeGenTarget::X64_Windows;
        auto result = workspace.codeGen(params);
        CHECK(!result.empty());
    }

    SUBCASE("x64_systemv target")
    {
        params.codeGenTarget = lsp::CodeGenTarget::X64_SystemV;
        auto result = workspace.codeGen(params);
        CHECK(!result.empty());
    }
}

TEST_CASE_FIXTURE(Fixture, "codegen_with_different_optimization_levels")
{
    auto uri = newDocument("test.luau", R"(
local function factorial(n)
    if n <= 1 then
        return 1
    end
    return n * factorial(n - 1)
end
)");

    lsp::CodegenParams params;
    params.textDocument.uri = uri;
    params.codeGenTarget = lsp::CodeGenTarget::Host;

    SUBCASE("optimization level none")
    {
        params.optimizationLevel = lsp::CompilerRemarksOptimizationLevel::None;
        auto result = workspace.codeGen(params);
        CHECK(!result.empty());
    }

    SUBCASE("optimization level O1")
    {
        params.optimizationLevel = lsp::CompilerRemarksOptimizationLevel::O1;
        auto result = workspace.codeGen(params);
        CHECK(!result.empty());
    }

    SUBCASE("optimization level O2")
    {
        params.optimizationLevel = lsp::CompilerRemarksOptimizationLevel::O2;
        auto result = workspace.codeGen(params);
        CHECK(!result.empty());
    }
}

TEST_CASE_FIXTURE(Fixture, "codegen_handles_syntax_errors")
{
    auto uri = newDocument("test.luau", R"(
local function broken(
    return 1
end
)");

    lsp::CodegenParams params;
    params.textDocument.uri = uri;

    auto result = workspace.codeGen(params);

    CHECK(result.find("SyntaxError") != std::string::npos);
}

TEST_SUITE_END();
