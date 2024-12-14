#include "../../luau/CodeGen/include/Luau/CodeGen.h"
#include "LSP/Workspace.hpp"
#include "Luau/BytecodeBuilder.h"
#include "Luau/Parser.h"
#include "Luau/Compiler.h"

#include "lua.h"
#include "lualib.h"

static std::string constructError(const std::string& type, const Luau::Location& location, const std::string& message)
{
    return type + "(" + std::to_string(location.begin.line + 1) + "," + std::to_string(location.begin.column + 1) + "): " + message + "\n";
}

static std::string getCodegenAssembly(
    const char* name,
    const std::string& bytecode,
    Luau::CodeGen::AssemblyOptions options
)
{
    std::unique_ptr<lua_State, void (*)(lua_State*)> globalState(luaL_newstate(), lua_close);
    lua_State* L = globalState.get();

    if (luau_load(L, name, bytecode.data(), bytecode.size(), 0) == 0)
        return Luau::CodeGen::getAssembly(L, -1, options, nullptr);

    return "Error loading bytecode";
}

static void annotateInstruction(void* context, std::string& text, int fid, int instpos)
{
    Luau::BytecodeBuilder& bcb = *(Luau::BytecodeBuilder*)context;

    bcb.annotateInstruction(text, fid, instpos);
}

enum class BytecodeOutputType
{
    Textual,
    CompilerRemarks,
    CodeGen,
};

static uint32_t flagsForType(BytecodeOutputType type)
{
    switch (type)
    {
    case BytecodeOutputType::Textual:
        return Luau::BytecodeBuilder::Dump_Code | Luau::BytecodeBuilder::Dump_Source | Luau::BytecodeBuilder::Dump_Locals |
               Luau::BytecodeBuilder::Dump_Remarks | Luau::BytecodeBuilder::Dump_Types;
    case BytecodeOutputType::CompilerRemarks:
        return Luau::BytecodeBuilder::Dump_Source | Luau::BytecodeBuilder::Dump_Remarks;
    case BytecodeOutputType::CodeGen:
        return Luau::BytecodeBuilder::Dump_Code | Luau::BytecodeBuilder::Dump_Source | Luau::BytecodeBuilder::Dump_Locals |
            Luau::BytecodeBuilder::Dump_Remarks;
    }
    return 0;
}

static std::string computeBytecodeOutput(const Luau::ModuleName& moduleName, const std::string& source, const ClientConfiguration& config, int optimizationLevel, BytecodeOutputType type)
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
        options.typeInfoLevel = config.bytecode.typeInfoLevel;
        options.vectorLib = config.bytecode.vectorLib.empty() ? nullptr : config.bytecode.vectorLib.c_str();
        options.vectorCtor = config.bytecode.vectorCtor.empty() ? nullptr : config.bytecode.vectorCtor.c_str();
        options.vectorType = config.bytecode.vectorType.empty() ? nullptr : config.bytecode.vectorType.c_str();

        Luau::compileOrThrow(bcb, result, names, options);

        if (type == BytecodeOutputType::CodeGen)
        {
            Luau::CodeGen::AssemblyOptions assemblyOptions;
            // TODO: assemblyOptions target
            assemblyOptions.outputBinary = false;
            assemblyOptions.includeAssembly = true;
            assemblyOptions.includeIr = true;
            assemblyOptions.includeIrTypes = true;
            assemblyOptions.includeOutlinedCode = true;
            assemblyOptions.annotator = annotateInstruction;
            assemblyOptions.annotatorContext = &bcb;
            return getCodegenAssembly(moduleName.c_str(), bcb.getBytecode(), assemblyOptions);
        }
        else if (type == BytecodeOutputType::Textual)
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
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    auto config = client->getConfiguration(rootUri);
    return computeBytecodeOutput(moduleName, textDocument->getText(), config, params.optimizationLevel, BytecodeOutputType::Textual);
}

lsp::CompilerRemarksResult WorkspaceFolder::compilerRemarks(const lsp::CompilerRemarksParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    auto config = client->getConfiguration(rootUri);
    return computeBytecodeOutput(moduleName, textDocument->getText(), config, params.optimizationLevel, BytecodeOutputType::CompilerRemarks);
}

lsp::CompilerRemarksResult WorkspaceFolder::codeGen(const lsp::CodegenParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    auto config = client->getConfiguration(rootUri);
    return computeBytecodeOutput(moduleName, textDocument->getText(), config, params.optimizationLevel, BytecodeOutputType::CodeGen);
}
