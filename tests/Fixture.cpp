// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Fixture.h"

#include "Platform/RobloxPlatform.hpp"
#include "Luau/Parser.h"
#include "Luau/BuiltinDefinitions.h"
#include "LSP/LuauExt.hpp"

#include "doctest.h"
#include <string_view>

static const char* mainModuleName = "MainModule";

Fixture::Fixture()
    : client(std::make_shared<Client>(Client{}))
    , workspace(client, "$TEST_WORKSPACE", Uri(), std::nullopt)
{
    workspace.fileResolver.defaultConfig.mode = Luau::Mode::Strict;

    workspace.initialize();
    loadDefinition(R"BUILTIN_SRC(
        declare class Enum
            function GetEnumItems(self): { any }
        end
        declare class EnumItem
            Name: string
            Value: number
            EnumType: Enum
            function IsA(self, enumName: string): boolean
        end

        declare class EnumHumanoidRigType extends EnumItem end
        declare class EnumHumanoidRigType_INTERNAL extends Enum
            R6: EnumHumanoidRigType
            R15: EnumHumanoidRigType
        end
        type ENUM_LIST = {
            HumanoidRigType: EnumHumanoidRigType_INTERNAL,
        } & { GetEnums: (self: ENUM_LIST) -> { Enum } }
        declare Enum: ENUM_LIST

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
    )BUILTIN_SRC");

    ClientConfiguration config;
    config.sourcemap.enabled = false;
    config.index.enabled = false;
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

Uri Fixture::newDocument(const std::string& name, const std::string& source)
{
    Uri uri("file", "", name);
    workspace.openTextDocument(uri, {{uri, "luau", 0, source}});
    return uri;
}

Luau::AstStatBlock* Fixture::parse(const std::string& source, const Luau::ParseOptions& parseOptions)
{
    sourceModule.reset(new Luau::SourceModule);

    Luau::ParseResult result = Luau::Parser::parse(source.c_str(), source.length(), *sourceModule->names, *sourceModule->allocator, parseOptions);

    sourceModule->name = fromString(mainModuleName);
    sourceModule->root = result.root;
    sourceModule->mode = parseMode(result.hotcomments);
    sourceModule->hotcomments = std::move(result.hotcomments);

    return result.root;
}

Luau::CheckResult Fixture::check(Luau::Mode mode, std::string source)
{
    newDocument(mainModuleName, source);
    return workspace.frontend.check(mainModuleName);
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

std::vector<std::string> Fixture::getComments(const Luau::Location& node)
{
    return workspace.getComments(fromString(mainModuleName), node);
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
    REQUIRE(module->hasModuleScope());

    return lookupName(module->getModuleScope(), name);
}

Luau::TypeId Fixture::requireType(const std::string& name)
{
    std::optional<Luau::TypeId> ty = getType(name);
    REQUIRE_MESSAGE(bool(ty), "Unable to requireType \"" << name << "\"");
    return Luau::follow(*ty);
}

Luau::LoadDefinitionFileResult Fixture::loadDefinition(const std::string& source)
{
    RobloxPlatform platform;

    Luau::unfreeze(workspace.frontend.globals.globalTypes);
    Luau::LoadDefinitionFileResult result = types::registerDefinitions(workspace.frontend, workspace.frontend.globals, source);
    platform.mutateRegisteredDefinitions(workspace.frontend.globals, std::nullopt);
    Luau::freeze(workspace.frontend.globals.globalTypes);

    REQUIRE_MESSAGE(result.success, "loadDefinition: unable to load definition file");
    return result;
}
