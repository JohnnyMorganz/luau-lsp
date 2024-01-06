#include "LSP/Workspace.hpp"
#include "Luau/BytecodeBuilder.h"
#include "Luau/Parser.h"
#include "Luau/Compiler.h"

static std::string constructError(const std::string& type, const Luau::Location& location, const std::string& message)
{
    return type + "(" + std::to_string(location.begin.line + 1) + "," + std::to_string(location.begin.column + 1) + "): " + message + "\n";
}

enum class BytecodeOutputType
{
    Textual,
    CompilerRemarks
};

static uint32_t flagsForType(BytecodeOutputType type)
{
    switch (type)
    {
    case BytecodeOutputType::Textual:
        return Luau::BytecodeBuilder::Dump_Code | Luau::BytecodeBuilder::Dump_Source | Luau::BytecodeBuilder::Dump_Locals |
               Luau::BytecodeBuilder::Dump_Remarks;
    case BytecodeOutputType::CompilerRemarks:
        return Luau::BytecodeBuilder::Dump_Source | Luau::BytecodeBuilder::Dump_Remarks;
    }
    return 0;
}

static std::string computeBytecodeOutput(const std::string& source, const ClientConfiguration& config, int optimizationLevel, BytecodeOutputType type)
{
    try
    {
        Luau::BytecodeBuilder bcb;
        bcb.setDumpFlags(flagsForType(type));
        bcb.setDumpSource(source);

        Luau::Allocator allocator;
        Luau::AstNameTable names(allocator);
        Luau::ParseResult result = Luau::Parser::parse(source.c_str(), source.size(), names, allocator);

        if (!result.errors.empty())
            throw Luau::ParseErrors(result.errors);

        Luau::CompileOptions options = {};
        options.optimizationLevel = optimizationLevel;
        options.debugLevel = config.bytecode.debugLevel;
        options.vectorLib = config.bytecode.vectorLib.empty() ? nullptr : config.bytecode.vectorLib.c_str();
        options.vectorCtor = config.bytecode.vectorCtor.empty() ? nullptr : config.bytecode.vectorCtor.c_str();
        options.vectorType = config.bytecode.vectorType.empty() ? nullptr : config.bytecode.vectorType.c_str();

        Luau::compileOrThrow(bcb, result, names, options);

        if (type == BytecodeOutputType::Textual)
            return bcb.dumpEverything();
        else
            return bcb.dumpSourceRemarks();
    }
    catch (Luau::ParseErrors& e)
    {
        std::string errorBuilder;
        for (auto& error : e.getErrors())
            errorBuilder += constructError("SyntaxError", error.getLocation(), error.what());
        return errorBuilder;
    }
    catch (Luau::CompileError& e)
    {
        return constructError("CompileError", e.getLocation(), e.what());
    }
}

lsp::CompilerRemarksResult WorkspaceFolder::bytecode(const lsp::BytecodeParams& params)
{
    ensureConfigured();

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    auto config = client->getConfiguration(rootUri);
    return computeBytecodeOutput(textDocument->getText(), config, params.optimizationLevel, BytecodeOutputType::Textual);
}

lsp::CompilerRemarksResult WorkspaceFolder::compilerRemarks(const lsp::CompilerRemarksParams& params)
{
    ensureConfigured();

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    auto config = client->getConfiguration(rootUri);
    return computeBytecodeOutput(textDocument->getText(), config, params.optimizationLevel, BytecodeOutputType::CompilerRemarks);
}
