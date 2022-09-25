#pragma once
#include <vector>
#include "nlohmann/json.hpp"

struct ClientDiagnosticsConfiguration
{
    /// Whether to also compute diagnostics for dependents when a file changes
    bool includeDependents = true;
    /// Whether to compute diagnostics for a whole workspace
    bool workspace = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientDiagnosticsConfiguration, includeDependents, workspace);

struct ClientSourcemapConfiguration
{
    /// Whether Rojo sourcemap-related features are enabled
    bool enabled = true;
    /// Whether we should autogenerate the Rojo sourcemap by calling `rojo sourcemap`
    bool autogenerate = true;
    /// The project file to generate a sourcemap for
    std::string rojoProjectFile = "default.project.json";
    /// Whether non script instances should be included in the generated sourcemap
    bool includeNonScripts = true;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientSourcemapConfiguration, enabled, autogenerate, rojoProjectFile, includeNonScripts);

struct ClientTypesConfiguration
{
    /// Whether Roblox-related definitions should be supported
    bool roblox = true;
    /// Any definition files to load globally
    std::vector<std::filesystem::path> definitionFiles;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientTypesConfiguration, roblox, definitionFiles);

enum struct InlayHintsParameterNamesConfig
{
    None,
    Literals,
    All
};
NLOHMANN_JSON_SERIALIZE_ENUM(InlayHintsParameterNamesConfig, {
                                                                 {InlayHintsParameterNamesConfig::None, "none"},
                                                                 {InlayHintsParameterNamesConfig::Literals, "literals"},
                                                                 {InlayHintsParameterNamesConfig::All, "all"},
                                                             })

struct ClientInlayHintsConfiguration
{
    InlayHintsParameterNamesConfig parameterNames;
    bool variableTypes = false;
    bool parameterTypes = false;
    bool functionReturnTypes = false;
    size_t typeHintMaxLength = 50;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    ClientInlayHintsConfiguration, parameterNames, variableTypes, parameterTypes, functionReturnTypes, typeHintMaxLength);

struct ClientHoverConfiguration
{
    bool enabled = true;
    bool showTableKinds = false;
    bool multilineFunctionDefinitions = false;
    bool strictDatamodelTypes = true;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    ClientHoverConfiguration, enabled, showTableKinds, multilineFunctionDefinitions, strictDatamodelTypes);

struct ClientCompletionConfiguration
{
    bool enabled = true;
    /// Whether we should suggest automatic imports in completions
    bool suggestImports = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientCompletionConfiguration, enabled, suggestImports);

struct ClientSignatureHelpConfiguration
{
    bool enabled = true;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientSignatureHelpConfiguration, enabled);


// These are the passed configuration options by the client, prefixed with `luau-lsp.`
// Here we also define the default settings
struct ClientConfiguration
{
    /// Whether to automatically autocomplete end
    bool autocompleteEnd = false;
    std::vector<std::string> ignoreGlobs;
    ClientSourcemapConfiguration sourcemap;
    ClientDiagnosticsConfiguration diagnostics;
    ClientTypesConfiguration types;
    ClientInlayHintsConfiguration inlayHints;
    ClientHoverConfiguration hover;
    ClientCompletionConfiguration completion;
    ClientSignatureHelpConfiguration signatureHelp;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    ClientConfiguration, autocompleteEnd, ignoreGlobs, sourcemap, diagnostics, types, inlayHints, hover, completion, signatureHelp);
