// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Config.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/IostreamHelpers.h"
#include "Luau/Linter.h"
#include "Luau/Location.h"
#include "Luau/ModuleResolver.h"
#include "Luau/Scope.h"
#include "Luau/ToString.h"
#include "Luau/TypeInfer.h"
#include "Luau/TypeVar.h"

#include <iostream>
#include <string>
#include <unordered_map>

#include <optional>

struct Fixture
{
    std::unique_ptr<Luau::SourceModule> sourceModule;

    explicit Fixture();
    ~Fixture();

    Luau::AstStatBlock* parse(const std::string& source, const Luau::ParseOptions& parseOptions = {});
};
