#include "doctest.h"
#include "Fixture.h"

#include "LSP/IostreamHelpers.hpp"

TEST_SUITE_BEGIN("PrepareRename");

TEST_CASE_FIXTURE(Fixture, "return_nullopt_on_nothing_to_rename")
{
    auto uri = newDocument("foo.luau", "");

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{0, 0}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result == std::nullopt);
}

TEST_CASE_FIXTURE(Fixture, "handle_generic_type_parameter")
{
    auto source = R"(
        type Ty<S> = S
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 16}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "S");
    REQUIRE(result.value().range == lsp::Range{{1, 16}, {1, 17}});
}

TEST_CASE_FIXTURE(Fixture, "handle_generic_type_parameter_reference")
{
    auto source = R"(
        type Ty<S> = S
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 21}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "S");
    REQUIRE(result.value().range == lsp::Range{{1, 21}, {1, 22}});
}

TEST_CASE_FIXTURE(Fixture, "handle_generic_type_pack_parameter")
{
    auto source = R"(
        type Ty<S...> = S... & { ... }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 16}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "S");
    REQUIRE(result.value().range == lsp::Range{{1, 16}, {1, 17}});
}

TEST_CASE_FIXTURE(Fixture, "handle_generic_type_pack_parameter_reference")
{
    auto source = R"(
        type Ty<S...> = S... & { ... }
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 24}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "S");
    REQUIRE(result.value().range == lsp::Range{{1, 24}, {1, 25}});
}

TEST_CASE_FIXTURE(Fixture, "handle_type_prefix")
{
    auto source = R"(
        local Module = require("")
        type Ty = Module.Ty
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{2, 21}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "Module");
    REQUIRE(result.value().range == lsp::Range{{2, 18}, {2, 24}});
}

TEST_CASE_FIXTURE(Fixture, "handle_type_definition")
{
    auto source = R"(
        local Module = require("")
        type Ty = Module.Ty
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{2, 14}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "Ty");
    REQUIRE(result.value().range == lsp::Range{{2, 13}, {2, 15}});
}

TEST_CASE_FIXTURE(Fixture, "handle_type_reference_without_prefix")
{
    auto source = R"(
        type T = number
        type Ty = T
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{2, 18}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "T");
    REQUIRE(result.value().range == lsp::Range{{2, 18}, {2, 19}});
}

TEST_CASE_FIXTURE(Fixture, "handle_type_reference_with_prefix")
{
    auto source = R"(
        local Module = require("")
        type Ty = Module.Ty
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{2, 25}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "Ty");
    REQUIRE(result.value().range == lsp::Range{{2, 25}, {2, 27}});
}

TEST_CASE_FIXTURE(Fixture, "handle_type_ref_in_local_var")
{
    auto source = R"(
        type Ty = number
        local t: Ty = 4
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{2, 19}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "Ty");
    REQUIRE(result.value().range == lsp::Range{{2, 17}, {2, 19}});
}

TEST_CASE_FIXTURE(Fixture, "handle_type_ref_in_local_var_cast")
{
    auto source = R"(
        type Ty = number
        local t = 4 :: Ty
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{2, 25}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "Ty");
    REQUIRE(result.value().range == lsp::Range{{2, 23}, {2, 25}});
}

TEST_CASE_FIXTURE(Fixture, "handle_local_symbol")
{
    auto source = R"(
        local v = 4
        v += 0
        print(v)
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 14}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "v");
    REQUIRE(result.value().range == lsp::Range{{1, 14}, {1, 15}});
}

TEST_CASE_FIXTURE(Fixture, "handle_local_symbol_2")
{
    auto source = R"(
        local v = 4
        v += 0
        print(v)
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{2, 8}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "v");
    REQUIRE(result.value().range == lsp::Range{{2, 8}, {2, 9}});
}

TEST_CASE_FIXTURE(Fixture, "handle_local_symbol_3")
{
    auto source = R"(
        local v = 4
        v += 0
        print(v)
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{3, 14}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "v");
    REQUIRE(result.value().range == lsp::Range{{3, 14}, {3, 15}});
}

TEST_CASE_FIXTURE(Fixture, "handle_function_name")
{
    auto source = R"(
        local function fun<T>(a: T): T
            error("unimplemented")
        end
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 26}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "fun");
    REQUIRE(result.value().range == lsp::Range{{1, 23}, {1, 26}});
}

TEST_CASE_FIXTURE(Fixture, "handle_function_generic")
{
    auto source = R"(
        local function fun<T>(a: T): T
            error("unimplemented")
        end
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 27}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "T");
    REQUIRE(result.value().range == lsp::Range{{1, 27}, {1, 28}});
}

TEST_CASE_FIXTURE(Fixture, "handle_function_generic_reference")
{
    auto source = R"(
        local function fun<T>(a: T): T
            error("unimplemented")
        end
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 33}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "T");
    REQUIRE(result.value().range == lsp::Range{{1, 33}, {1, 34}});
}

TEST_CASE_FIXTURE(Fixture, "handle_global")
{
    auto source = R"(
        g = 4
    )";

    auto uri = newDocument("foo.luau", source);

    lsp::PrepareRenameParams params = {lsp::TextDocumentIdentifier{uri}, lsp::Position{1, 8}};
    auto result = workspace.prepareRename(params);
    REQUIRE(result);
    REQUIRE(result.value().placeholder == "g");
    REQUIRE(result.value().range == lsp::Range{{1, 8}, {1, 9}});
}

TEST_SUITE_END();
