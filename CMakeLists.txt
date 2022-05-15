cmake_minimum_required(VERSION 3.0.0)
project(luau-lsp LANGUAGES CXX)

include(CTest)
enable_testing()

add_executable(luau-lsp src/main.cpp src/JsonRpc.hpp src/Protocol.hpp src/Uri.hpp src/Workspace.hpp src/Sourcemap.hpp src/TextDocument.hpp src/Client.hpp)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

add_subdirectory(luau)

target_compile_features(luau-lsp PUBLIC cxx_std_17)
target_include_directories(luau-lsp PRIVATE "${PROJECT_BINARY_DIR}" src/ luau/Analysis/include luau/Ast/include extern/json-rpc-cxx/include extern/json/include)
target_link_libraries(luau-lsp PRIVATE Luau.Ast Luau.Analysis)

if(MSVC)
  target_compile_options(luau-lsp PRIVATE /W3 /WX /D_CRT_SECURE_NO_WARNINGS)
else()
  target_compile_options(luau-lsp PRIVATE -Wall -Werror)
endif()