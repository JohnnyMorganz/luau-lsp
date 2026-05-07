#pragma once

#include <string>

// Sourcemap where file paths intentionally DON'T mirror the DataModel hierarchy.
// This tests the core value of sourcemap-aware requires: files that are neighbors
// in the DataModel but scattered across the filesystem.
static const std::string SOURCEMAP_FOR_STRING_REQUIRES = R"(
{
    "name": "Game",
    "className": "DataModel",
    "children": [
        {
            "name": "ReplicatedStorage",
            "className": "ReplicatedStorage",
            "children": [
                {
                    "name": "Shared",
                    "className": "Folder",
                    "children": [
                        {"name": "ModuleA", "className": "ModuleScript", "filePaths": ["packages/core/ModuleA.luau"]},
                        {"name": "ModuleB", "className": "ModuleScript", "filePaths": ["packages/combat/ModuleB.luau"]},
                        {
                            "name": "Nested",
                            "className": "Folder",
                            "children": [
                                {"name": "DeepModule", "className": "ModuleScript", "filePaths": ["packages/ui/components/DeepModule.luau"]}
                            ]
                        }
                    ]
                },
                {"name": "Utils", "className": "ModuleScript", "filePaths": ["lib/Utils.luau"]}
            ]
        },
        {
            "name": "ServerScriptService",
            "className": "ServerScriptService",
            "children": [
                {"name": "ServerModule", "className": "ModuleScript", "filePaths": ["src/server/ServerModule.luau"]}
            ]
        }
    ]
}
)";

static const std::string SOURCEMAP_FOR_INSTANCE_REQUIRE_SERVER_CLIENT_FILTERING = R"(
{
    "name": "game",
    "className": "DataModel",
    "children": [
        {
            "name": "ServerScriptService",
            "className": "ServerScriptService",
            "children": [{"name": "ServerModule", "className": "ModuleScript", "filePaths": ["server/ServerModule.luau"]}]
        },
        {
            "name": "StarterPlayer",
            "className": "StarterPlayer",
            "children": [
                {
                    "name": "StarterPlayerScripts",
                    "className": "StarterPlayerScripts",
                    "children": [{"name": "ClientModule", "className": "ModuleScript", "filePaths": ["client/ClientModule.luau"]}]
                }
            ]
        },
        {
            "name": "ReplicatedStorage",
            "className": "ReplicatedStorage",
            "children": [{"name": "SharedModule", "className": "ModuleScript", "filePaths": ["shared/SharedModule.luau"]}]
        }
    ]
}
)";

static const std::string SOURCEMAP_FOR_STRING_REQUIRES_SERVER_CLIENT_FILTERING = R"(
{
    "name": "game",
    "className": "DataModel",
    "children": [
        {
            "name": "ServerScriptService",
            "className": "ServerScriptService",
            "children": [{"name": "ServerModule", "className": "ModuleScript", "filePaths": ["server/ServerModule.luau"]}]
        },
        {
            "name": "StarterPlayer",
            "className": "StarterPlayer",
            "children": [
                {
                    "name": "StarterPlayerScripts",
                    "className": "StarterPlayerScripts",
                    "children": [{"name": "ClientModule", "className": "ModuleScript", "filePaths": ["client/ClientModule.luau"]}]
                }
            ]
        },
        {
            "name": "ReplicatedStorage",
            "className": "ReplicatedStorage",
            "children": [{"name": "SharedModule", "className": "ModuleScript", "filePaths": ["shared/SharedModule.luau"]}]
        }
    ]
}
)";
