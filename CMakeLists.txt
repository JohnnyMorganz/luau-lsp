cmake_minimum_required(VERSION 3.19)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

option(LUAU_ENABLE_TIME_TRACE "Build with Luau TimeTrace" OFF)
option(LSP_BUILD_ASAN "Build with ASAN" OFF)
option(LSP_STATIC_CRT "Link with the static CRT (/MT)" OFF)
option(LSP_BUILD_WITH_SENTRY "Build with Sentry (crash reporting)" OFF)
option(LSP_WERROR "Warnings as errors" ON)

set(LSP_VERSION "1.62.0")
set(LSP_NAME "Luau")

if (LSP_STATIC_CRT)
    cmake_policy(SET CMP0091 NEW)
    set(LUAU_STATIC_CRT ON)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

if (LUAU_ENABLE_TIME_TRACE)
    add_definitions(-DLUAU_ENABLE_TIME_TRACE)
endif ()

if (LSP_BUILD_WITH_SENTRY)
    add_definitions(-DLSP_BUILD_WITH_SENTRY)
endif ()

project(Luau.LanguageServer LANGUAGES CXX)

# Sentry setup
if (LSP_BUILD_WITH_SENTRY)
    add_subdirectory(extern/sentry-native)
    set(BUILD_SHARED_LIBS OFF)
    set(SENTRY_BUILD_RUNTIMESTATIC ON)
    set(CRASHPAD_ENABLE_STACKTRACE ON)
endif ()

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
        src/LuauFileUtils.cpp
        src/IostreamHelpers.cpp
        src/Utils.cpp
        src/Flags.cpp
        src/Thread.cpp
        src/JsonTomlSyntaxParser.cpp
        src/CliConfigurationParser.cpp
        src/platform/AutoImports.cpp
        src/platform/LSPPlatform.cpp
        src/platform/StringRequireAutoImporter.cpp
        src/platform/StringRequireSuggester.cpp
        src/platform/roblox/InstanceRequireAutoImporter.cpp
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
        src/operations/RequireGraph.cpp
        src/operations/OnTypeFormatting.cpp
        src/operations/FileRename.cpp
        src/transport/PipeTransport.cpp
        src/transport/StdioTransport.cpp
        extern/glob/match.cpp
)

target_sources(Luau.LanguageServer.CLI PRIVATE
        src/main.cpp
        src/AnalyzeCli.cpp
)

target_sources(Luau.LanguageServer.Test PRIVATE
        tests/main.cpp
        tests/Fixture.cpp
        tests/TempDir.cpp
        tests/TestClient.cpp
        tests/Autocomplete.test.cpp
        tests/AutoImports.test.cpp
        tests/Bytecode.test.cpp
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
        tests/Diagnostics.test.cpp
        tests/GoToDefinition.test.cpp
        tests/Glob.test.cpp
        tests/Workspace.test.cpp
        tests/RequireGraph.test.cpp
        tests/OnTypeFormatting.test.cpp
        tests/AnalyzeCli.test.cpp
        tests/CodeAction.test.cpp
        tests/FileRename.test.cpp
        src/AnalyzeCli.cpp # For testing
)

# TODO: Set Luau.Analysis at O2 to speed up debugging
if (MSVC)
    list(APPEND LUAU_LSP_OPTIONS /W3 /D_CRT_SECURE_NO_WARNINGS)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        list(APPEND LUAU_LSP_OPTIONS "/EHsc") # The CMake clang-cl target doesn't enable exceptions by default
    else ()
        list(APPEND LUAU_LSP_OPTIONS /MP) # Distribute compilation across multiple cores; doesn't work with clang-cl
    endif ()
else ()
    list(APPEND LUAU_LSP_OPTIONS -Wall)
endif ()

if (LSP_WERROR)
    if (MSVC)
        list(APPEND LUAU_LSP_OPTIONS /WX) # Warnings are errors
    else ()
        list(APPEND LUAU_LSP_OPTIONS -Werror) # Warnings are errors
    endif ()
endif ()

if (LSP_BUILD_ASAN)
    if (MSVC)
        list(APPEND LUAU_LSP_OPTIONS /fsanitize=address)
        target_compile_options(Luau.Ast PRIVATE /fsanitize=address)
        target_compile_options(Luau.Analysis PRIVATE /fsanitize=address)
        target_compile_options(Luau.Compiler PRIVATE /fsanitize=address)
        target_compile_options(Luau.Config PRIVATE /fsanitize=address)
        target_compile_options(Luau.VM PRIVATE /fsanitize=address)
        target_compile_options(Luau.CodeGen PRIVATE /fsanitize=address)
        target_compile_options(Luau.Common PRIVATE /fsanitize=address)
    else ()
        list(APPEND LUAU_LSP_OPTIONS -fsanitize=address)
        target_compile_options(Luau.Ast PRIVATE -fsanitize=address)
        target_compile_options(Luau.Analysis PRIVATE -fsanitize=address)
        target_compile_options(Luau.Compiler PRIVATE -fsanitize=address)
        target_compile_options(Luau.Config PRIVATE -fsanitize=address)
        target_compile_options(Luau.VM PRIVATE -fsanitize=address)
        target_compile_options(Luau.CodeGen PRIVATE -fsanitize=address)
        target_compile_options(Luau.Common PRIVATE -fsanitize=address)
        target_link_options(Luau.LanguageServer PRIVATE -fsanitize=address)
        target_link_options(Luau.LanguageServer.CLI PRIVATE -fsanitize=address)
        target_link_options(Luau.LanguageServer.Test PRIVATE -fsanitize=address)
        target_link_options(Luau.Ast PRIVATE -fsanitize=address)
        target_link_options(Luau.Analysis PRIVATE -fsanitize=address)
        target_link_options(Luau.Compiler PRIVATE -fsanitize=address)
        target_link_options(Luau.Config PRIVATE -fsanitize=address)
        target_link_options(Luau.VM PRIVATE -fsanitize=address)
        target_link_options(Luau.CodeGen PRIVATE -fsanitize=address)
        target_link_options(Luau.Common PRIVATE -fsanitize=address)
    endif ()
endif ()

set(EXTERN_INCLUDES extern/json/include extern/glob/include extern/argparse/include extern/toml/include extern/ryml/include)

target_compile_features(Luau.LanguageServer PUBLIC cxx_std_17)
target_compile_options(Luau.LanguageServer PRIVATE ${LUAU_LSP_OPTIONS})
target_compile_definitions(Luau.LanguageServer PUBLIC -DLSP_VERSION="${LSP_VERSION}")
target_compile_definitions(Luau.LanguageServer PUBLIC -DLSP_NAME="${LSP_NAME}")
target_include_directories(Luau.LanguageServer PUBLIC src/include)
target_include_directories(Luau.LanguageServer SYSTEM PUBLIC ${EXTERN_INCLUDES})
target_link_libraries(Luau.LanguageServer PRIVATE Luau.Ast Luau.Analysis Luau.Compiler Luau.VM Luau.CodeGen)

set_target_properties(Luau.LanguageServer.CLI PROPERTIES OUTPUT_NAME luau-lsp)
target_compile_features(Luau.LanguageServer.CLI PUBLIC cxx_std_17)
target_compile_options(Luau.LanguageServer.CLI PRIVATE ${LUAU_LSP_OPTIONS})
target_include_directories(Luau.LanguageServer.CLI PRIVATE src/include)
target_include_directories(Luau.LanguageServer.CLI SYSTEM PRIVATE ${EXTERN_INCLUDES})
target_link_libraries(Luau.LanguageServer.CLI PRIVATE Luau.LanguageServer Luau.Analysis)

target_compile_features(Luau.LanguageServer.Test PRIVATE cxx_std_17)
target_compile_options(Luau.LanguageServer.Test PRIVATE ${LUAU_LSP_OPTIONS})
target_include_directories(Luau.LanguageServer.Test PRIVATE tests)
target_include_directories(Luau.LanguageServer.Test SYSTEM PRIVATE ${EXTERN_INCLUDES} extern/doctest)
target_link_libraries(Luau.LanguageServer.Test PRIVATE Luau.Ast Luau.Analysis Luau.LanguageServer)

if (LSP_BUILD_WITH_SENTRY)
    target_link_libraries(Luau.LanguageServer PRIVATE sentry)
    target_link_libraries(Luau.LanguageServer.CLI PRIVATE sentry)

    add_custom_command(
            TARGET Luau.LanguageServer.CLI POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:crashpad_handler>"
            "$<TARGET_FILE_DIR:Luau.LanguageServer.CLI>"
            COMMENT "Copying crashpad_handler to output directory"
    )
endif ()
