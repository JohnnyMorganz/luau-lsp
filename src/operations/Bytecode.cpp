#include "LSP/Workspace.hpp"
#include "Luau/BytecodeBuilder.h"
#include "Luau/Parser.h"
#include "Luau/Compiler.h"

static std::string constructError(const std::string& type, const Luau::Location& location, const std::string& message)
{
    return type + "(" + std::to_string(location.begin.line + 1) + "," + std::to_string(location.begin.column + 1) + "): " + message + "\n";
}

lsp::CompilerRemarksResult WorkspaceFolder::compilerRemarks(const lsp::CompilerRemarksParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    try
    {
        auto source = textDocument->getText();

        Luau::BytecodeBuilder bcb;
        bcb.setDumpFlags(Luau::BytecodeBuilder::Dump_Source | Luau::BytecodeBuilder::Dump_Remarks);
        bcb.setDumpSource(source);

        Luau::Allocator allocator;
        Luau::AstNameTable names(allocator);
        Luau::ParseResult result = Luau::Parser::parse(source.c_str(), source.size(), names, allocator);

        if (!result.errors.empty())
            throw Luau::ParseErrors(result.errors);

        auto config = client->getConfiguration(rootUri);
        Luau::CompileOptions options = {};
        options.optimizationLevel = params.optimizationLevel;
        options.debugLevel = config.bytecode.debugLevel;
        options.vectorLib = config.bytecode.vectorLib.empty() ? nullptr : config.bytecode.vectorLib.c_str();
        options.vectorCtor = config.bytecode.vectorCtor.empty() ? nullptr : config.bytecode.vectorCtor.c_str();
        options.vectorType = config.bytecode.vectorType.empty() ? nullptr : config.bytecode.vectorType.c_str();

        Luau::compileOrThrow(bcb, result, names, options);
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
