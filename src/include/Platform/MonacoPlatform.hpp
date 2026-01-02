#pragma once

#include "LSP/LuauExt.hpp"
#include "Platform/LSPPlatform.hpp"

class WorkspaceFolder;
struct WorkspaceFileResolver;

class MonacoPlatform : public LSPPlatform
{
protected:
    WorkspaceFileResolver* fileResolver;
    WorkspaceFolder* workspaceFolder;

public:
    // void mutateRegisteredDefinitions(Luau::GlobalTypes& globals, std::optional<nlohmann::json> metadata) override;

    // void onDidChangeWatchedFiles(const lsp::FileEvent& change) override;

    // void setupWithConfiguration(const ClientConfiguration& config) override;

    /// The name points to a virtual path (i.e. for Roblox, game/ or ProjectRoot/)
    [[nodiscard]] bool isVirtualPath(const Luau::ModuleName& name) const override
    {
        return true;
    }

    [[nodiscard]] std::optional<Luau::ModuleName> resolveToVirtualPath(const std::string& name) const override
    {
        return name;
    }

    [[nodiscard]] std::optional<std::filesystem::path> resolveToRealPath(const Luau::ModuleName& name) const override
    {
        return name;
    }

    [[nodiscard]] Luau::SourceCode::Type sourceCodeTypeFromPath(const std::filesystem::path& path) const override
    {
        return Luau::SourceCode::Type::Script;
    }

    // [[nodiscard]]  std::optional<std::string> readSourceCode(const Luau::ModuleName& name, const std::filesystem::path& path) const override {
    //     // todo
    // }

    // std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node);

    std::string getName() override { return "MonacoPlatform"; }

    MonacoPlatform(const MonacoPlatform& copyFrom) = delete;
    MonacoPlatform(MonacoPlatform&&) = delete;
    MonacoPlatform& operator=(const MonacoPlatform& copyFrom) = delete;
    MonacoPlatform& operator=(MonacoPlatform&&) = delete;

    MonacoPlatform(WorkspaceFileResolver* fileResolver = nullptr, WorkspaceFolder* workspaceFolder = nullptr)
        : LSPPlatform(fileResolver, workspaceFolder)
    {

    }
    ~MonacoPlatform() = default;
};