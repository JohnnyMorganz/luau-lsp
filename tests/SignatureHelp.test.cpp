#include "doctest.h"
#include "Fixture.h"
#include "Platform/RobloxPlatform.hpp"

TEST_SUITE_BEGIN("SignatureHelp");

TEST_CASE_FIXTURE(Fixture, "signature_help_handles_overloaded_functions_and_picks_best_overload_1")
{
    auto [source, marker] = sourceWithMarker(R"(
        type Foo = ((foo: string, bar: number) -> number) & ((bar: number, foo: string) -> string)
        local x: Foo

        x(1|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 2);

    CHECK_EQ(result->signatures[0].label, "function x(foo: string, bar: number): number");
    CHECK_EQ(result->signatures[1].label, "function x(bar: number, foo: string): string");
    CHECK_EQ(result->activeSignature, 1);
    CHECK_EQ(result->activeParameter, 0);
}

TEST_CASE_FIXTURE(Fixture, "signature_help_handles_overloaded_functions_and_picks_best_overload_1")
{
    auto [source, marker] = sourceWithMarker(R"(
        type Foo = ((foo: string, bar: number) -> number) & ((bar: number) -> string)
        local x: Foo

        x(1|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 2);

    CHECK_EQ(result->signatures[0].label, "function x(foo: string, bar: number): number");
    CHECK_EQ(result->signatures[1].label, "function x(bar: number): string");
    CHECK_EQ(result->activeSignature, 1);
    CHECK_EQ(result->activeParameter, 0);
}

TEST_CASE_FIXTURE(Fixture, "signature_help_shows_for_call_metamethod")
{
    auto [source, marker] = sourceWithMarker(R"(
        local mt = {}

        --- some documentation
        function mt.__call(self: any, meow: string)
        end

        local tbl = setmetatable({}, mt)

        tbl(|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);

    CHECK_EQ(result->signatures[0].label, "function tbl(meow: string): ()");
    REQUIRE(result->signatures[0].documentation);
    CHECK_EQ(result->signatures[0].documentation->value, "some documentation\n");
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 1);

    CHECK_EQ(std::get<std::vector<size_t>>(result->signatures[0].parameters->at(0).label), std::vector<size_t>{13, 25});
}

TEST_CASE_FIXTURE(Fixture, "signature_help_does_not_show_implicit_self_on_call_metamethod")
{
    auto [source, marker] = sourceWithMarker(R"(
        local module = setmetatable({}, {
            __call = function(self, value: string)
            end,
        })

        module(|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);

    CHECK_EQ(result->signatures[0].label, "function module(value: string): ()");
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 1);
    CHECK_EQ(std::get<std::vector<size_t>>(result->signatures[0].parameters->at(0).label), std::vector<size_t>{16, 29});
}

TEST_CASE_FIXTURE(Fixture, "signature_help_on_call_metamethod_taking_only_self_has_no_parameters")
{
    auto [source, marker] = sourceWithMarker(R"(
        local module = setmetatable({}, {
            __call = function(self)
            end,
        })

        module(|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);

    CHECK_EQ(result->signatures[0].label, "function module(): ()");
    REQUIRE(result->signatures[0].parameters);
    CHECK_EQ(result->signatures[0].parameters->size(), 0);
}

TEST_CASE_FIXTURE(Fixture, "signature_help_hides_call_metamethod_self_when_accessed_by_index")
{
    std::string call;
    std::string expectedLabel;
    std::vector<size_t> expectedOffsets;
    SUBCASE("dot")
    {
        call = "container.callable(|)";
        expectedLabel = "function container.callable(value: string): ()";
        expectedOffsets = {28, 41};
    }
    SUBCASE("bracket")
    {
        call = "container[\"callable\"](|)";
        expectedLabel = "function container['callable'](value: string): ()";
        expectedOffsets = {31, 44};
    }

    auto [source, marker] = sourceWithMarker(R"(
        local container = {
            callable = setmetatable({}, {
                __call = function(self: any, value: string)
                end,
            }),
        }
    )" + call);
    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);
    CHECK_EQ(result->signatures[0].label, expectedLabel);
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 1);
    CHECK_EQ(std::get<std::vector<size_t>>(result->signatures[0].parameters->at(0).label), expectedOffsets);
}

TEST_CASE_FIXTURE(Fixture, "signature_help_only_hides_method_self_for_colon_calls")
{
    loadDefinition("@test", R"(
        declare extern type Widget with
            function method(self, value: string): ()
        end
        declare object: Widget
    )");

    std::string call;
    std::string expectedLabel;
    std::vector<std::vector<size_t>> expectedOffsets;
    SUBCASE("colon")
    {
        call = "object:method(|)";
        expectedLabel = "function Widget:method(value: string): ()";
        expectedOffsets = {{23, 36}};
    }
    SUBCASE("dot")
    {
        call = "object.method(|)";
        expectedLabel = "function Widget.method(self: Widget, value: string): ()";
        expectedOffsets = {{23, 35}, {37, 50}};
    }
    SUBCASE("detached")
    {
        call = "local method = object.method\nmethod(|)";
        expectedLabel = "function method(self: Widget, value: string): ()";
        expectedOffsets = {{16, 28}, {30, 43}};
    }

    auto [source, marker] = sourceWithMarker(call);
    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);
    CHECK_EQ(result->signatures[0].label, expectedLabel);
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), expectedOffsets.size());
    for (size_t i = 0; i < expectedOffsets.size(); ++i)
        CHECK_EQ(std::get<std::vector<size_t>>(result->signatures[0].parameters->at(i).label), expectedOffsets[i]);
}

TEST_CASE_FIXTURE(Fixture, "signature_help_on_callable_preserves_explicit_metamethod_signature_and_hover")
{
    auto [source, marker] = sourceWithMarker(dedent(R"(
        local mt = {}
        function mt.__call(self: any, value: string)
        end
        local module = setmetatable({}, mt)
        module(|)
        mt.__call(module, "a")
        mt:__call("a")
    )"));
    auto uri = newDocument("foo.luau", source);

    lsp::HoverParams hoverParams;
    hoverParams.textDocument = lsp::TextDocumentIdentifier{uri};
    // A colon call makes an accidental change to the shared function's hasSelf visible in hover.
    hoverParams.position = lsp::Position{marker.line + 2, 4};
    auto hoverBefore = workspace.hover(hoverParams, nullptr);
    REQUIRE(hoverBefore);
    CHECK_EQ(hoverBefore->contents.value, codeBlock("luau", "function mt:__call(self: any, value: string): ()"));

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;
    auto callableResult = workspace.signatureHelp(params, nullptr);
    REQUIRE(callableResult);
    REQUIRE_EQ(callableResult->signatures.size(), 1);
    CHECK_EQ(callableResult->signatures[0].label, "function module(value: string): ()");

    params.position = lsp::Position{marker.line + 1, 10};
    auto explicitResult = workspace.signatureHelp(params, nullptr);
    REQUIRE(explicitResult);
    REQUIRE_EQ(explicitResult->signatures.size(), 1);
    CHECK_EQ(explicitResult->signatures[0].label, "function mt.__call(self: any, value: string): ()");
    REQUIRE(explicitResult->signatures[0].parameters);
    REQUIRE_EQ(explicitResult->signatures[0].parameters->size(), 2);
    CHECK_EQ(std::get<std::vector<size_t>>(explicitResult->signatures[0].parameters->at(0).label), std::vector<size_t>{19, 28});
    CHECK_EQ(std::get<std::vector<size_t>>(explicitResult->signatures[0].parameters->at(1).label), std::vector<size_t>{30, 43});

    auto hoverAfter = workspace.hover(hoverParams, nullptr);
    REQUIRE(hoverAfter);
    CHECK_EQ(hoverAfter->contents.value, hoverBefore->contents.value);
}

TEST_CASE_FIXTURE(Fixture, "signature_help_on_variadic_call_metamethod_keeps_varargs")
{
    auto [source, marker] = sourceWithMarker(R"(
        local module = setmetatable({}, {
            __call = function(self, ...: string)
            end,
        })

        module(|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);

    CHECK_EQ(result->signatures[0].label, "function module(...: string): ()");
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 1);
    CHECK_EQ(std::get<std::vector<size_t>>(result->signatures[0].parameters->at(0).label), std::vector<size_t>{16, 27});
}

TEST_CASE_FIXTURE(Fixture, "signature_help_on_call_metamethod_keeps_original_parameter_documentation_indices")
{
    loadDefinition("@test", R"(
        type Callable = typeof(setmetatable({} :: {}, {} :: { __call: (any, value: string, other: number) -> () }))
        declare foo: Callable
    )");

    client->documentation["@test/global/foo/param/0"] = Luau::BasicDocumentation{"documentation for self", "", ""};
    client->documentation["@test/global/foo/param/1"] = Luau::BasicDocumentation{"documentation for value", "", ""};
    client->documentation["@test/global/foo/param/2"] = Luau::BasicDocumentation{"documentation for other", "", ""};

    auto [source, marker] = sourceWithMarker(R"(
        foo(|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 2);

    CHECK_EQ(result->signatures[0].parameters->at(0).documentation->value, "documentation for value");
    CHECK_EQ(result->signatures[0].parameters->at(1).documentation->value, "documentation for other");
}

TEST_CASE_FIXTURE(Fixture, "signature_help_on_variadic_call_metamethod_keeps_original_documentation_index")
{
    loadDefinition("@test", R"(
        type Callable = typeof(setmetatable({} :: {}, {} :: { __call: (any, ...string) -> () }))
        declare foo: Callable
    )");

    client->documentation["@test/global/foo/param/0"] = Luau::BasicDocumentation{"documentation for self", "", ""};
    client->documentation["@test/global/foo/param/1"] = Luau::BasicDocumentation{"documentation for varargs", "", ""};

    auto [source, marker] = sourceWithMarker(R"(
        foo(|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 1);

    CHECK_EQ(result->signatures[0].parameters->at(0).documentation->value, "documentation for varargs");
}

TEST_CASE_FIXTURE(Fixture, "signature_help_on_ordinary_function_keeps_parameter_documentation_indices")
{
    loadDefinition("@test", R"(
        declare function bar(first: string, second: number): ()
    )");

    client->documentation["@test/global/bar/param/0"] = Luau::BasicDocumentation{"documentation for first", "", ""};
    client->documentation["@test/global/bar/param/1"] = Luau::BasicDocumentation{"documentation for second", "", ""};

    auto [source, marker] = sourceWithMarker(R"(
        bar(|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 2);

    CHECK_EQ(result->signatures[0].parameters->at(0).documentation->value, "documentation for first");
    CHECK_EQ(result->signatures[0].parameters->at(1).documentation->value, "documentation for second");
}

TEST_CASE_FIXTURE(Fixture, "signature_help_on_call_metamethod_with_mixed_named_and_unnamed_arguments")
{
    loadDefinition("@test", R"(
        type Callable = typeof(setmetatable({} :: {}, {} :: { __call: (any, string, other: number) -> () }))
        declare foo: Callable
    )");

    auto [source, marker] = sourceWithMarker(R"(
        foo(|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);

    CHECK_EQ(result->signatures[0].label, "function foo(string, other: number): ()");
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 2);

    CHECK_EQ(std::get<std::vector<size_t>>(result->signatures[0].parameters->at(0).label), std::vector<size_t>{13, 19});
    CHECK_EQ(std::get<std::vector<size_t>>(result->signatures[0].parameters->at(1).label), std::vector<size_t>{21, 34});
}

TEST_CASE_FIXTURE(Fixture, "signature_help_on_call_metamethod_tracks_active_parameter")
{
    auto [source, marker] = sourceWithMarker(R"(
        local module = setmetatable({}, {
            __call = function(self, first: string, second: number)
            end,
        })

        module("a", |)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);

    CHECK_EQ(result->signatures[0].label, "function module(first: string, second: number): ()");
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 2);

    CHECK_EQ(std::get<std::vector<size_t>>(result->signatures[0].parameters->at(0).label), std::vector<size_t>{16, 29});
    CHECK_EQ(std::get<std::vector<size_t>>(result->signatures[0].parameters->at(1).label), std::vector<size_t>{31, 45});

    CHECK_EQ(result->activeParameter, 1);
}

TEST_CASE_FIXTURE(Fixture, "signature_help_respects_cancellation")
{
    auto cancellationToken = std::make_shared<Luau::FrontendCancellationToken>();
    cancellationToken->cancel();

    auto document = newDocument("a.luau", "local x = 1");
    CHECK_THROWS_AS(workspace.signatureHelp(lsp::SignatureHelpParams{{{document}}}, cancellationToken), RequestCancelledException);
}

TEST_CASE_FIXTURE(Fixture, "signature_help_does_not_show_first_argument_on_method_calls_from_type_function_ftv")
{
    ENABLE_NEW_SOLVER();

    auto [source, marker] = sourceWithMarker(R"(
        type function repro()
            return types.newfunction({ head = { types.any, types.number } }, { head = { types.boolean } })
        end

        local foo: { bar: repro<> } = {
            bar = function(self: any, n: number): boolean
                return true
            end
        }

        foo:bar(|)
    )");

    auto uri = newDocument("foo.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);

    // TODO: would be nice if this was just `bar(number): boolean` https://github.com/JohnnyMorganz/luau-lsp/issues/1250#issuecomment-3592436704
    CHECK_EQ(result->signatures[0].label, "function foo:bar(any, number): boolean");
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 1);

    CHECK_EQ(std::get<std::vector<size_t>>(result->signatures[0].parameters->at(0).label), std::vector<size_t>{22, 28});
}

TEST_CASE_FIXTURE(Fixture, "signature_help_highlights_param_with_union_type_containing_exported_module_table_type")
{
    // Regression test for https://github.com/JohnnyMorganz/luau-lsp/issues/1507
    // When a parameter type references an exported table type from another module in a union,
    // the parameter label offset computation must still find the param inside the full signature label.
    // Note: sourceWithMarker uses '|' as marker so we write the source manually to avoid conflict
    // with the '|' union operator in the type annotation.
    tempDir.write_child("second.luau", "export type foo = {}\nreturn nil");
    newDocument("second.luau", "export type foo = {}\nreturn nil");

    // sourceWithMarker uses '|' as its marker character, which conflicts with the '|' union operator,
    // so position the cursor manually. After dedent the source becomes:
    //   Line 0: "local second = require("./second")"
    //   Line 1: ""
    //   Line 2: "local function foo(bar: second.foo | string): ()"
    //   Line 3: "end"
    //   Line 4: ""
    //   Line 5: "foo()"   ← column 4 is `)`, i.e. just inside the call
    std::string source = dedent(R"(
        local second = require("./second")

        local function foo(bar: second.foo | string): ()
        end

        foo()
    )");
    lsp::Position marker{5, 4};

    auto uri = newDocument("first.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 1);

    // The parameter label must be resolved to offsets within the full signature string,
    // not fall back to a plain string (which means the find failed).
    CHECK(std::holds_alternative<std::vector<size_t>>(result->signatures[0].parameters->at(0).label));
}

TEST_CASE_FIXTURE(Fixture, "signature_help_highlights_param_with_intersection_type_containing_exported_module_table_type")
{
    // Regression test for https://github.com/JohnnyMorganz/luau-lsp/issues/1507
    // Same as above but with an intersection type (&).
    tempDir.write_child("second.luau", "export type foo = {}\nreturn nil");
    newDocument("second.luau", "export type foo = {}\nreturn nil");

    auto [source, marker] = sourceWithMarker(R"(
        local second = require("./second")

        local function foo(bar: second.foo & {baz: string}): ()
        end

        foo(|)
    )");

    auto uri = newDocument("first.luau", source);

    lsp::SignatureHelpParams params;
    params.textDocument = lsp::TextDocumentIdentifier{uri};
    params.position = marker;

    auto result = workspace.signatureHelp(params, nullptr);
    REQUIRE(result);
    REQUIRE_EQ(result->signatures.size(), 1);
    REQUIRE(result->signatures[0].parameters);
    REQUIRE_EQ(result->signatures[0].parameters->size(), 1);

    CHECK(std::holds_alternative<std::vector<size_t>>(result->signatures[0].parameters->at(0).label));
}

TEST_SUITE_END();
