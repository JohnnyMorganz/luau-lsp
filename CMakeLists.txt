cmake_minimum_required(VERSION 3.19)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

option(LUAU_ENABLE_TIME_TRACE "Build with Luau TimeTrace" OFF)
option(LSP_BUILD_ASAN "Build with ASAN" OFF)
option(LSP_STATIC_CRT "Link with the static CRT (/MT)" OFF)

if (LSP_STATIC_CRT)
    cmake_policy(SET CMP0091 NEW)
    set(LUAU_STATIC_CRT ON)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

if (LUAU_ENABLE_TIME_TRACE)
    add_definitions(-DLUAU_ENABLE_TIME_TRACE)
endif ()

project(Luau.LanguageServer LANGUAGES CXX)
add_subdirectory(luau)
add_library(Luau.LanguageServer STATIC)
add_executable(Luau.LanguageServer.CLI)
add_executable(Luau.LanguageServer.Test)

target_sources(Luau.LanguageServer PRIVATE
        src/LanguageServer.cpp
        src/JsonRpc.cpp
        src/Uri.cpp
        src/WorkspaceFileResolver.cpp
        src/Workspace.cpp
        src/TextDocument.cpp
        src/Client.cpp
        src/DocumentationParser.cpp
        src/LuauExt.cpp
        src/IostreamHelpers.cpp
        src/Utils.cpp
        src/Flags.cpp
        src/JsonTomlSyntaxParser.cpp
        src/CliConfigurationParser.cpp
        src/platform/LSPPlatform.cpp
        src/platform/roblox/RobloxCodeAction.cpp
        src/platform/roblox/RobloxColorProvider.cpp
        src/platform/roblox/RobloxCompletion.cpp
        src/platform/roblox/RobloxFileResolver.cpp
        src/platform/roblox/RobloxLanguageServer.cpp
        src/platform/roblox/RobloxLuauExt.cpp
        src/platform/roblox/RobloxSourcemap.cpp
        src/platform/roblox/RobloxSourceNode.cpp
        src/platform/roblox/RobloxStudioPlugin.cpp
        src/operations/Diagnostics.cpp
        src/operations/Completion.cpp
        src/operations/DocumentSymbol.cpp
        src/operations/DocumentLink.cpp
        src/operations/ColorProvider.cpp
        src/operations/GotoDefinition.cpp
        src/operations/Hover.cpp
        src/operations/SignatureHelp.cpp
        src/operations/References.cpp
        src/operations/Rename.cpp
        src/operations/InlayHints.cpp
        src/operations/SemanticTokens.cpp
        src/operations/CodeAction.cpp
        src/operations/WorkspaceSymbol.cpp
        src/operations/FoldingRange.cpp
        src/operations/CallHierarchy.cpp
        src/operations/Bytecode.cpp
)

target_sources(Luau.LanguageServer.CLI PRIVATE
        src/main.cpp
        src/AnalyzeCli.cpp
)

target_sources(Luau.LanguageServer.Test PRIVATE
        tests/main.cpp
        tests/Fixture.cpp
        tests/TempDir.cpp
        tests/Autocomplete.test.cpp
        tests/MagicFunctions.test.cpp
        tests/Documentation.test.cpp
        tests/TextDocument.test.cpp
        tests/Uri.test.cpp
        tests/Utils.test.cpp
        tests/WorkspaceFileResolver.test.cpp
        tests/SemanticTokens.test.cpp
        tests/Sourcemap.test.cpp
        tests/References.test.cpp
        tests/Rename.test.cpp
        tests/ColorProvider.test.cpp
        tests/LuauExt.test.cpp
        tests/CliConfigurationParser.test.cpp
        tests/Hover.test.cpp
        tests/LanguageServer.test.cpp
        tests/InlayHints.test.cpp
        tests/JsonTomlSyntaxParser.test.cpp
        tests/Definitions.test.cpp
        tests/SignatureHelp.test.cpp
)

# TODO: Set Luau.Analysis at O2 to speed up debugging
if (MSVC)
    list(APPEND LUAU_LSP_OPTIONS /W3 /WX /D_CRT_SECURE_NO_WARNINGS)
    list(APPEND LUAU_LSP_OPTIONS /MP) # Distribute compilation across multiple cores
else ()
    list(APPEND LUAU_LSP_OPTIONS -Wall -Werror)
endif ()

if (LSP_BUILD_ASAN)
    if (MSVC)
        list(APPEND LUAU_LSP_OPTIONS /fsanitize=address)
        target_compile_options(Luau.Ast PRIVATE /fsanitize=address)
        target_compile_options(Luau.Analysis PRIVATE /fsanitize=address)
        target_compile_options(Luau.Compiler PRIVATE /fsanitize=address)
        target_compile_options(Luau.Config PRIVATE /fsanitize=address)
        target_compile_options(Luau.VM PRIVATE /fsanitize=address)
        target_compile_options(Luau.EqSat PRIVATE /fsanitize=address)
    else ()
        list(APPEND LUAU_LSP_OPTIONS -fsanitize=address)
        target_compile_options(Luau.Ast PRIVATE -fsanitize=address)
        target_compile_options(Luau.Analysis PRIVATE -fsanitize=address)
        target_compile_options(Luau.Compiler PRIVATE -fsanitize=address)
        target_compile_options(Luau.Config PRIVATE -fsanitize=address)
        target_compile_options(Luau.VM PRIVATE -fsanitize=address)
        target_compile_options(Luau.EqSat PRIVATE -fsanitize=address)
        target_link_options(Luau.LanguageServer PRIVATE -fsanitize=address)
        target_link_options(Luau.LanguageServer.CLI PRIVATE -fsanitize=address)
        target_link_options(Luau.LanguageServer.Test PRIVATE -fsanitize=address)
        target_link_options(Luau.Ast PRIVATE -fsanitize=address)
        target_link_options(Luau.Analysis PRIVATE -fsanitize=address)
        target_link_options(Luau.Compiler PRIVATE -fsanitize=address)
        target_link_options(Luau.Config PRIVATE -fsanitize=address)
        target_link_options(Luau.VM PRIVATE -fsanitize=address)
        target_link_options(Luau.EqSat PRIVATE -fsanitize=address)
    endif ()
endif ()

set(EXTERN_INCLUDES extern/json/include extern/glob/single_include extern/argparse/include extern/toml/include)

target_compile_features(Luau.LanguageServer PUBLIC cxx_std_17)
target_compile_options(Luau.LanguageServer PRIVATE ${LUAU_LSP_OPTIONS})
target_include_directories(Luau.LanguageServer PUBLIC src/include ${EXTERN_INCLUDES})
target_link_libraries(Luau.LanguageServer PRIVATE Luau.Ast Luau.Analysis Luau.Compiler)

set_target_properties(Luau.LanguageServer.CLI PROPERTIES OUTPUT_NAME luau-lsp)
target_compile_features(Luau.LanguageServer.CLI PUBLIC cxx_std_17)
target_compile_options(Luau.LanguageServer.CLI PRIVATE ${LUAU_LSP_OPTIONS})
target_include_directories(Luau.LanguageServer.CLI PRIVATE src/include ${EXTERN_INCLUDES})
target_link_libraries(Luau.LanguageServer.CLI PRIVATE Luau.LanguageServer Luau.Analysis)

target_compile_features(Luau.LanguageServer.Test PRIVATE cxx_std_17)
target_compile_options(Luau.LanguageServer.Test PRIVATE ${LUAU_LSP_OPTIONS})
target_include_directories(Luau.LanguageServer.Test PRIVATE tests ${EXTERN_INCLUDES} extern/doctest)
target_link_libraries(Luau.LanguageServer.Test PRIVATE Luau.Ast Luau.Analysis Luau.LanguageServer)
