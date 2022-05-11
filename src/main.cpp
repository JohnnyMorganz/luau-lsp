#include <iostream>
#include <fstream>
#include <string>
#include <optional>
#include <variant>
#include <exception>
#include <filesystem>
#include "Protocol.hpp"
#include "JsonRpc.hpp"
#include "Luau/Frontend.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/StringUtils.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;
using namespace json_rpc;
using id_type = std::variant<int, std::string>;
using Response = json;

#define REQUIRED_PARAMS(params, method) \
    !params ? throw json_rpc::JsonRpcException(lsp::ErrorCode::InvalidParams, "params not provided for " method) : params.value()

std::optional<std::string> readFile(const std::filesystem::path& filePath)
{
    std::ifstream fileContents;
    fileContents.open(filePath);

    std::string output;
    std::stringstream buffer;

    if (fileContents)
    {
        buffer << fileContents.rdbuf();
        output = buffer.str();
        return output;
    }
    else
    {
        return std::nullopt;
    }
}

bool replace(std::string& str, const std::string& from, const std::string& to)
{
    size_t start_pos = str.find(from);
    if (start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}
struct WorkspaceFileResolver
    : Luau::FileResolver
    , Luau::ConfigResolver
{
    Luau::Config defaultConfig;

    mutable std::unordered_map<std::string, Luau::Config> configCache;
    mutable std::vector<std::pair<std::filesystem::path, std::string>> configErrors;

    WorkspaceFileResolver()
    {
        defaultConfig.mode = Luau::Mode::Nonstrict;
    }

    std::optional<Luau::SourceCode> readSource(const Luau::ModuleName& name) override
    {
        Luau::SourceCode::Type sourceType = Luau::SourceCode::Module;
        std::optional<std::string> source = readFile(name); // TODO: URI

        if (!source)
            return std::nullopt;

        return Luau::SourceCode{*source, sourceType};
    }

    std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node) override
    {
        if (Luau::AstExprConstantString* expr = node->as<Luau::AstExprConstantString>())
        {
            Luau::ModuleName name = std::string(expr->value.data, expr->value.size) + ".luau";
            if (!readFile(name))
            {
                // fall back to .lua if a module with .luau doesn't exist
                name = std::string(expr->value.data, expr->value.size) + ".lua";
            }

            return {{name}};
        }

        return std::nullopt;
    }

    std::string getHumanReadableModuleName(const Luau::ModuleName& name) const override
    {
        return name;
    }

    const Luau::Config& getConfig(const Luau::ModuleName& name) const override
    {
        std::filesystem::path realPath = name;
        if (!realPath.has_parent_path())
            return defaultConfig;

        return readConfigRec(realPath.parent_path());
    }

    const Luau::Config& readConfigRec(const std::filesystem::path& path) const
    {
        auto it = configCache.find(path.generic_string());
        if (it != configCache.end())
            return it->second;

        Luau::Config result = path.has_parent_path() ? readConfigRec(path.parent_path()) : defaultConfig;
        auto configPath = path / Luau::kConfigName;

        if (std::optional<std::string> contents = readFile(configPath))
        {
            std::optional<std::string> error = Luau::parseConfig(*contents, result);
            if (error)
                configErrors.push_back({configPath, *error});
        }

        return configCache[path.generic_string()] = result;
    }
};


class LanguageServer
{
    WorkspaceFileResolver fileResolver;
    Luau::Frontend frontend;

public:
    LanguageServer()
        : fileResolver(WorkspaceFileResolver())
        , frontend(Luau::Frontend(&fileResolver, &fileResolver))
    {
    }

    /// Sets up the frontend
    void setup()
    {
        Luau::registerBuiltinTypes(frontend.typeChecker);
        Luau::freeze(frontend.typeChecker.globalTypes);
    }

    void sendRequest(const id_type& id, const std::string& method, std::optional<json> params)
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"method", method},
        };

        if (std::holds_alternative<int>(id))
        {
            msg["id"] = std::get<int>(id);
        }
        else
        {
            msg["id"] = std::get<std::string>(id);
        }

        if (params.has_value())
            msg["params"] = params.value();

        sendRawMessage(msg);
    }

    void sendResponse(const id_type& id, const json& result)
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"result", result},
        };

        if (std::holds_alternative<int>(id))
        {
            msg["id"] = std::get<int>(id);
        }
        else
        {
            msg["id"] = std::get<std::string>(id);
        }

        sendRawMessage(msg);
    }
    void sendNotification(const std::string& method, std::optional<json> params)
    {
        json msg{
            {"jsonrpc", "2.0"},
            {"method", method},
        };

        if (params.has_value())
            msg["params"] = params.value();

        sendRawMessage(msg);
    }

    void sendLogMessage(lsp::MessageType type, std::string message)
    {
        json params{
            {"type", type},
            {"message", message},
        };
        sendNotification("window/logMessage", params);
    }

    void sendTrace(std::string message, std::optional<std::string> verbose)
    {
        if (traceMode == lsp::TraceValue::Off)
            return;
        json params{{"message", message}};
        if (verbose && traceMode == lsp::TraceValue::Verbose)
            params["verbose"] = verbose.value();
        sendNotification("$/logTrace", params);
    };

    lsp::ServerCapabilities getServerCapabilities()
    {
        lsp::TextDocumentSyncKind textDocumentSync = lsp::TextDocumentSyncKind::Full;
        lsp::CompletionOptions completionProvider{};
        return lsp::ServerCapabilities{textDocumentSync, completionProvider};
    }

    Response onRequest(const id_type& id, const std::string& method, std::optional<json> params)
    {
        // Handle request
        // If a request has been sent before the server is initialized, we should error
        if (!isInitialized && method != "initialize")
            throw JsonRpcException(lsp::ErrorCode::ServerNotInitialized, "server not initialized");
        // If we received a request after a shutdown, then we error with InvalidRequest
        if (shutdownRequested)
            throw JsonRpcException(lsp::ErrorCode::InvalidRequest, "server is shutting down");

        if (method == "initialize")
        {
            return onInitialize(id);
        }
        else if (method == "shutdown")
        {
            return onShutdown(id);
        }
        else
        {
            throw JsonRpcException(lsp::ErrorCode::MethodNotFound, "method not found / supported: " + method);
        }
    }

    // // void onResponse(); // id = integer/string/null, result?: string | number | boolean | object | null, error?: ResponseError
    void onNotification(const std::string& method, std::optional<json> params)
    {
        // Handle notification
        // If a notification is sent before the server is initilized or after a shutdown is requested (unless its exit), we should
        // drop it
        if ((!isInitialized || shutdownRequested) && method != "exit")
            return;

        if (method == "exit")
        {
            // Exit the process loop
            std::exit(shutdownRequested ? 0 : 1);
        }
        else if (method == "initialized")
        {
            onInitialized(REQUIRED_PARAMS(params, "initialized"));
        }
        else if (method == "$/setTrace")
        {
            lsp::SetTraceParams setTraceParams = REQUIRED_PARAMS(params, "$/setTrace");
            traceMode = setTraceParams.value;
        }
        else if (method == "textDocument/didOpen")
        {
            onDidOpenTextDocument(REQUIRED_PARAMS(params, "textDocument/didOpen"));
        }
        else if (method == "textDocument/didChange")
        {
            onDidChangeTextDocument(REQUIRED_PARAMS(params, "textDocument/didChange"));
        }
        else if (method == "textDocument/didClose")
        {
            onDidCloseTextDocument(REQUIRED_PARAMS(params, "textDocument/didClose"));
        }
    }

    void processInputLoop()
    {
        std::string jsonString;
        while (std::cin)
        {
            sendTrace(jsonString, std::nullopt);

            if (readRawMessage(jsonString))
            {
                try
                {
                    // Parse the input
                    auto msg = json_rpc::parse(jsonString);

                    if (msg.is_request())
                    {
                        auto response = onRequest(msg.id.value(), msg.method.value(), msg.params);
                        sendTrace(response.dump(), std::nullopt);
                        sendResponse(msg.id.value(), response);
                    }
                    else if (msg.is_response())
                    {
                        // TODO: check error or result
                        continue;
                    }
                    else if (msg.is_notification())
                    {
                        onNotification(msg.method.value(), msg.params);
                    }
                    else
                    {
                        throw JsonRpcException(lsp::ErrorCode::InvalidRequest, "invalid json-rpc message");
                    }
                }
                catch (const JsonRpcException& e)
                {
                    sendRawMessage({{"jsonrpc", "2.0"}, {"id", nullptr}, // TODO: id
                        {"error", {{"code", e.code}, {"message", e.message}, {"data", e.data}}}});
                }
                catch (const json::exception& e)
                {
                    sendRawMessage({{"jsonrpc", "2.0"}, {"id", nullptr}, // TODO: id
                        {"error", {"code", lsp::ErrorCode::ParseError}, {"message", e.what()}}});
                }
                catch (const std::exception& e)
                {
                    sendRawMessage({{"jsonrpc", "2.0"}, {"id", nullptr}, // TODO: id
                        {"error", {"code", lsp::ErrorCode::InternalError}, {"message", e.what()}}});
                }
            }
        }
    }

    bool requestedShutdown()
    {
        return shutdownRequested;
    }

    std::vector<lsp::Diagnostic> findDiagnostics(const std::string& fileName)
    {
        Luau::CheckResult cr;
        if (frontend.isDirty(fileName))
            cr = frontend.check(fileName);

        if (!frontend.getSourceModule(fileName))
        {
            lsp::Diagnostic errorDiagnostic;
            errorDiagnostic.source = "Luau";
            errorDiagnostic.code = "000";
            errorDiagnostic.message = "Failed to resolve source module for this file";
            errorDiagnostic.severity = lsp::DiagnosticSeverity::Error;
            errorDiagnostic.range = {{0, 0}, {0, 0}};
            return {errorDiagnostic};
        }

        std::vector<lsp::Diagnostic> diagnostics;
        for (auto& error : cr.errors)
        {
            lsp::Diagnostic diag;
            diag.source = "Luau";
            diag.code = error.code();
            diag.message = "TypeError: " + Luau::toString(error);
            diag.severity = lsp::DiagnosticSeverity::Error;
            diag.range = {{error.location.begin.line, error.location.begin.column}, {error.location.end.line, error.location.end.column}};
            diagnostics.emplace_back(diag);
        }

        Luau::LintResult lr = frontend.lint(fileName);
        for (auto& error : lr.errors)
        {
            lsp::Diagnostic diag;
            diag.source = "Luau";
            diag.code = error.code;
            diag.message = std::string(Luau::LintWarning::getName(error.code)) + ": " + error.text;
            diag.severity = lsp::DiagnosticSeverity::Error;
            lsp::Position start{error.location.begin.line, error.location.begin.column};
            lsp::Position end{error.location.end.line, error.location.end.column};
            diag.range = {start, end};
            diagnostics.emplace_back(diag);
        }
        for (auto& error : lr.warnings)
        {
            lsp::Diagnostic diag;
            diag.source = "Luau";
            diag.code = error.code;
            diag.message = std::string(Luau::LintWarning::getName(error.code)) + ": " + error.text;
            diag.severity = lsp::DiagnosticSeverity::Error;
            lsp::Position start{error.location.begin.line, error.location.begin.column};
            lsp::Position end{error.location.end.line, error.location.end.column};
            diag.range = {start, end};
            diagnostics.emplace_back(diag);
        }

        return diagnostics;
    }

    // Dispatch handlers
    lsp::InitializeResult onInitialize(const id_type& id) // const lsp::InitializeParams& params
    {
        isInitialized = true;
        lsp::InitializeResult result;
        result.capabilities = getServerCapabilities();
        return result;
    }

    void onInitialized(const lsp::InitializedParams& params)
    {
        // Client received result of initialize
        sendLogMessage(lsp::MessageType::Info, "server initialized!");
    }

    void onDidOpenTextDocument(const lsp::DidOpenTextDocumentParams& params)
    {
        sendLogMessage(lsp::MessageType::Info, "got an opened file");
        // TODO: manage file location

        // Trigger diagnostics
        auto diagnostics = findDiagnostics(params.textDocument.uri);
        lsp::PublishDiagnosticsParams publish{params.textDocument.uri, params.textDocument.version, diagnostics};
        sendNotification("textDocument/publishDiagnostics", publish);
    }

    void onDidChangeTextDocument(const lsp::DidChangeTextDocumentParams& params)
    {
        sendLogMessage(lsp::MessageType::Info, "got a changed file");

        // TODO: update local version of file
        // Mark the module dirty for the typechecker
        frontend.markDirty(params.textDocument.uri);
    }

    void onDidCloseTextDocument(const lsp::DidCloseTextDocumentParams& params)
    {
        sendLogMessage(lsp::MessageType::Info, "got a closed file");
        // TODO: release local version of file
    }

    Response onShutdown(const id_type& id)
    {
        shutdownRequested = true;
        return nullptr;
    }

private:
    bool isInitialized = false;
    bool shutdownRequested = false;
    lsp::TraceValue traceMode = lsp::TraceValue::Off;

    bool readRawMessage(std::string& output)
    {
        return json_rpc::readRawMessage(std::cin, output);
    }

    void sendRawMessage(const json& message)
    {
        json_rpc::sendRawMessage(std::cout, message);
    }
};

int main()
{
    LanguageServer server;

    // Debug loop: uncomment and set a breakpoint on while to attach debugger before init
    // auto d = 4;
    // while (d == 4)
    // {
    //     d = 4;
    // }

    // Setup Luau
    server.setup();

    // Begin input loop
    server.processInputLoop();

    // If we received a shutdown request before exiting, exit normally. Otherwise, it is an abnormal exit
    return server.requestedShutdown() ? 0 : 1;
}