// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Fixture.h"

#include "Luau/Parser.h"
#include "Luau/BuiltinDefinitions.h"
#include "LSP/LuauExt.hpp"

#include "doctest.h"
#include <string_view>

static const char* mainModuleName = "MainModule";

Fixture::Fixture()
    : client(std::make_shared<Client>(Client{}))
    , workspace(client, "$TEST_WORKSPACE", Uri())
{
    workspace.fileResolver.defaultConfig.mode = Luau::Mode::Strict;

    workspace.initialize();
    Luau::unfreeze(workspace.frontend.typeChecker.globalTypes);
    auto result = types::registerDefinitions(workspace.frontend.typeChecker, std::string(R"BUILTIN_SRC(
        declare class RBXScriptConnection
            Connected: boolean
            function Disconnect(self): nil
        end

        export type RBXScriptSignal<T... = ...any> = {
            Wait: (self: RBXScriptSignal<T...>) -> T...,
            Connect: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
            ConnectParallel: (self: RBXScriptSignal<T...>, callback: (T...) -> ()) -> RBXScriptConnection,
        }

        declare class Instance
            AncestryChanged: RBXScriptSignal<Instance, Instance?>
            AttributeChanged: RBXScriptSignal<string>
            Changed: RBXScriptSignal<string>
            ChildAdded: RBXScriptSignal<Instance>
            ChildRemoved: RBXScriptSignal<Instance>
            ClassName: string
            DescendantAdded: RBXScriptSignal<Instance>
            DescendantRemoving: RBXScriptSignal<Instance>
            Destroying: RBXScriptSignal<>
            Name: string
            Parent: Instance?
            function ClearAllChildren(self): nil
            function Clone(self): Instance
            function Destroy(self): nil
            function FindFirstAncestor(self, name: string): Instance?
            function FindFirstAncestorOfClass(self, className: string): Instance?
            function FindFirstAncestorWhichIsA(self, className: string): Instance?
            function FindFirstChild(self, name: string, recursive: boolean?): Instance?
            function FindFirstChildOfClass(self, className: string): Instance?
            function FindFirstChildWhichIsA(self, className: string, recursive: boolean?): Instance?
            function FindFirstDescendant(self, name: string): Instance?
            function GetAttribute(self, attribute: string): any
            function GetAttributeChangedSignal(self, attribute: string): RBXScriptSignal<>
            function GetAttributes(self): { [string]: any }
            function GetChildren(self): { Instance }
            function GetDescendants(self): { Instance }
            function GetFullName(self): string
            function GetPropertyChangedSignal(self, property: string): RBXScriptSignal<>
            function IsA(self, className: string): boolean
            function IsAncestorOf(self, descendant: Instance): boolean
            function IsDescendantOf(self, ancestor: Instance): boolean
            function SetAttribute(self, attribute: string, value: any): nil
            function WaitForChild(self, name: string): Instance
            function WaitForChild(self, name: string, timeout: number): Instance?
        end

        declare class Part extends Instance
            Anchored: boolean
        end

        declare class TextLabel extends Instance
            Text: string
        end

        declare Instance: {
            new: (("Part") -> Part) & (("TextLabel") -> TextLabel)
        }
    )BUILTIN_SRC"));
    REQUIRE_MESSAGE(result.success, "loadDefinition failed");
    Luau::freeze(workspace.frontend.typeChecker.globalTypes);

    ClientConfiguration config;
    config.sourcemap.enabled = false;
    workspace.setupWithConfiguration(config);

    Luau::setPrintLine([](auto s) {});
}

Fixture::~Fixture()
{
    Luau::resetPrintLine();
}

Luau::ModuleName fromString(std::string_view name)
{
    return Luau::ModuleName(name);
}

void Fixture::newDocument(const std::string& name, const std::string& source)
{
    Uri uri("file", "", name);
    workspace.openTextDocument(uri, {{uri, "luau", 0, source}});
}

Luau::CheckResult Fixture::check(Luau::Mode mode, std::string source)
{
    Luau::ModuleName mm = fromString(mainModuleName);
    workspace.fileResolver.defaultConfig.mode = mode;
    workspace.fileResolver.managedFiles.emplace(std::make_pair(mm, TextDocument(Uri("file", "", mainModuleName), "luau", 0, std::move(source))));
    workspace.frontend.markDirty(mm);

    return workspace.frontend.check(mm);
}

Luau::CheckResult Fixture::check(const std::string& source)
{
    return check(Luau::Mode::Strict, source);
}

Luau::ModulePtr Fixture::getMainModule()
{
    return workspace.frontend.moduleResolver.getModule(fromString(mainModuleName));
}

Luau::SourceModule* Fixture::getMainSourceModule()
{
    return workspace.frontend.getSourceModule(fromString(mainModuleName));
}

std::optional<Luau::TypeId> lookupName(Luau::ScopePtr scope, const std::string& name)
{
    auto binding = scope->linearSearchForBinding(name);
    if (binding)
        return binding->typeId;
    else
        return std::nullopt;
}

std::optional<Luau::TypeId> Fixture::getType(const std::string& name)
{
    Luau::ModulePtr module = getMainModule();
    REQUIRE(module);

    return lookupName(module->getModuleScope(), name);
}

Luau::TypeId Fixture::requireType(const std::string& name)
{
    std::optional<Luau::TypeId> ty = getType(name);
    REQUIRE_MESSAGE(bool(ty), "Unable to requireType \"" << name << "\"");
    return Luau::follow(*ty);
}