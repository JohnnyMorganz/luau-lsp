cmake_minimum_required(VERSION 3.19)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
project(luau-lsp LANGUAGES CXX)

set(SOURCES
    src/LanguageServer.cpp
    src/JsonRpc.cpp
    src/Uri.cpp
    src/WorkspaceFileResolver.cpp
    src/Workspace.cpp
    src/Sourcemap.cpp
    src/TextDocument.cpp
    src/Client.cpp
    src/DocumentationParser.cpp
    src/LuauExt.cpp
    src/Utils.cpp
    src/operations/Diagnostics.cpp
    src/operations/Completion.cpp
    src/operations/DocumentSymbol.cpp
    src/operations/DocumentLink.cpp
    src/operations/GotoDefinition.cpp
)
add_executable(luau-lsp src/main.cpp src/AnalyzeCli.cpp ${SOURCES})

add_subdirectory(luau)

target_compile_features(luau-lsp PUBLIC cxx_std_17)
target_include_directories(luau-lsp PRIVATE "${PROJECT_BINARY_DIR}" src/include luau/Analysis/include luau/Ast/include extern/json-rpc-cxx/include extern/json/include extern/glob/single_include extern/doctest)
target_link_libraries(luau-lsp PRIVATE Luau.Ast Luau.Analysis)

if(MSVC)
    target_compile_options(luau-lsp PRIVATE /W3 /WX /D_CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(luau-lsp PRIVATE -Wall -Werror)
endif()

add_executable(luau-lsp-tests
    tests/main.cpp
    tests/TextDocument.test.cpp
    tests/Uri.test.cpp
    tests/Utils.test.cpp
    tests/WorkspaceFileResolver.test.cpp
    ${SOURCES}
)
target_compile_features(luau-lsp-tests PRIVATE cxx_std_17)
target_include_directories(luau-lsp-tests PRIVATE "${PROJECT_BINARY_DIR}" src/include luau/Analysis/include luau/Ast/include extern/json-rpc-cxx/include extern/json/include extern/glob/single_include extern/doctest)
target_link_libraries(luau-lsp-tests PRIVATE Luau.Ast Luau.Analysis)