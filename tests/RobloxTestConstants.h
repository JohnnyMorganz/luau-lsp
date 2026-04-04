#pragma once

#include <string>

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
                        {"name": "ModuleA", "className": "ModuleScript", "filePaths": ["src/shared/ModuleA.luau"]},
                        {"name": "ModuleB", "className": "ModuleScript", "filePaths": ["src/shared/ModuleB.luau"]},
                        {
                            "name": "Nested",
                            "className": "Folder",
                            "children": [
                                {"name": "DeepModule", "className": "ModuleScript", "filePaths": ["src/shared/Nested/DeepModule.luau"]}
                            ]
                        }
                    ]
                },
                {"name": "Utils", "className": "ModuleScript", "filePaths": ["src/shared/Utils.luau"]}
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
