#pragma once
#include <vector>
#include "nlohmann/json.hpp"

struct ClientDiagnosticsConfiguration
{
    /// Whether to also compute diagnostics for dependents when a file changes
    bool includeDependents = true;
    /// Whether to compute diagnostics for a whole workspace
    bool workspace = false;
    /// Whether to use expressive DM types in the diagnostics typechecker
    bool strictDatamodelTypes = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientDiagnosticsConfiguration, includeDependents, workspace, strictDatamodelTypes)

struct ClientRobloxSourcemapConfiguration
{
    /// Whether Rojo sourcemap-related features are enabled
    bool enabled = true;
    /// Whether we should autogenerate the Rojo sourcemap by calling `rojo sourcemap`
    bool autogenerate = true;
    /// The project file to generate a sourcemap for
    std::string rojoProjectFile = "default.project.json";
    /// Whether non script instances should be included in the generated sourcemap
    bool includeNonScripts = true;
    // The sourcemap file name
    std::string sourcemapFile = "sourcemap.json";
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    ClientRobloxSourcemapConfiguration, enabled, autogenerate, rojoProjectFile, includeNonScripts, sourcemapFile);

struct ClientTypesConfiguration
{
    /// Whether Roblox-related definitions should be supported
    /// DEPRECATED: USE `platform.type` INSTEAD
    bool roblox = true;
    /// Any definition files to load globally
    std::unordered_map<std::string, std::string> definitionFiles{};
    /// A list of globals to remove from the global scope. Accepts full libraries or particular functions (e.g., `table` or `table.clone`)
    std::vector<std::string> disabledGlobals{};
};

// TODO: ser/de defined manually to retain backwards compatibility of 'definitionsFiles' being an array
inline void to_json(nlohmann::json& nlohmann_json_j, const ClientTypesConfiguration& nlohmann_json_t)
{
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, roblox, definitionFiles, disabledGlobals))
}
inline void from_json(const nlohmann::json& json, ClientTypesConfiguration& object)
{
    if (json.contains("roblox"))
        json["roblox"].get_to(object.roblox);
    if (json.contains("disabledGlobals"))
        json["disabledGlobals"].get_to(object.disabledGlobals);
    if (json.contains("definitionFiles"))
    {
        if (json["definitionFiles"].is_object())
            json["definitionFiles"].get_to(object.definitionFiles);
        else
        {
            // Backwards compatibility, randomise the packageName of the definitionFiles
            size_t backwardsCompatibilityNameSuffix = 1;
            for (const auto& definition : json["definitionFiles"].get<std::vector<std::string>>())
            {
                std::string packageName = "@roblox" + std::to_string(backwardsCompatibilityNameSuffix);
                object.definitionFiles.emplace(packageName, definition);
                backwardsCompatibilityNameSuffix += 1;
            }
        }
    }
}

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
    InlayHintsParameterNamesConfig parameterNames = InlayHintsParameterNamesConfig::None;
    bool variableTypes = false;
    bool parameterTypes = false;
    bool functionReturnTypes = false;
    bool hideHintsForErrorTypes = false;
    bool hideHintsForMatchingParameterNames = true;
    size_t typeHintMaxLength = 50;
    /// Whether type inlay hints should be made insertable
    bool makeInsertable = true;

    inline bool operator==(const ClientInlayHintsConfiguration& rhs) const
    {
        return this->parameterNames == rhs.parameterNames && this->variableTypes == rhs.variableTypes && this->parameterTypes == rhs.parameterTypes &&
               this->functionReturnTypes == rhs.functionReturnTypes && this->hideHintsForErrorTypes == rhs.hideHintsForErrorTypes &&
               this->hideHintsForMatchingParameterNames == rhs.hideHintsForMatchingParameterNames && this->typeHintMaxLength == rhs.typeHintMaxLength;
    }

    inline bool operator!=(const ClientInlayHintsConfiguration& rhs) const
    {
        return !(*this == rhs);
    }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientInlayHintsConfiguration, parameterNames, variableTypes, parameterTypes, functionReturnTypes,
    hideHintsForErrorTypes, hideHintsForMatchingParameterNames, typeHintMaxLength, makeInsertable);

struct ClientHoverConfiguration
{
    bool enabled = true;
    bool showTableKinds = false;
    bool multilineFunctionDefinitions = false;
    bool strictDatamodelTypes = true;
    bool includeStringLength = true;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    ClientHoverConfiguration, enabled, showTableKinds, multilineFunctionDefinitions, strictDatamodelTypes, includeStringLength);

enum struct ImportRequireStyle
{
    Auto,
    AlwaysRelative,
    AlwaysAbsolute,
};
NLOHMANN_JSON_SERIALIZE_ENUM(ImportRequireStyle, {
                                                     {ImportRequireStyle::Auto, "auto"},
                                                     {ImportRequireStyle::AlwaysRelative, "alwaysRelative"},
                                                     {ImportRequireStyle::AlwaysAbsolute, "alwaysAbsolute"},
                                                 })

struct ClientCompletionImportsStringRequiresConfiguration
{
    // Whether to use string requires when auto-importing requires (roblox platform only)
    bool enabled = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientCompletionImportsStringRequiresConfiguration, enabled);

struct ClientCompletionImportsConfiguration
{
    /// Whether we should suggest automatic imports in completions
    bool enabled = true;
    /// Whether services should be suggested in auto-import
    bool suggestServices = true;
    /// When non-empty, only show the services listed when auto-importing
    std::vector<std::string> includedServices{};
    /// Do not show any of the listed services when auto-importing
    std::vector<std::string> excludedServices{};
    /// Whether requires should be suggested in auto-import
    bool suggestRequires = true;
    /// The style of the auto-imported require
    ImportRequireStyle requireStyle = ImportRequireStyle::Auto;
    ClientCompletionImportsStringRequiresConfiguration stringRequires;
    /// Whether services and requires should be separated by an empty line
    bool separateGroupsWithLine = false;
    /// Files that match these globs will not be shown during auto-import
    std::vector<std::string> ignoreGlobs{};
    /// Whether to suggest imports based on Rotriever package dependencies
    bool suggestRotrieverImports = true;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientCompletionImportsConfiguration, enabled, suggestServices, includedServices, excludedServices,
    suggestRequires, requireStyle, stringRequires, separateGroupsWithLine, ignoreGlobs, suggestRotrieverImports);

struct ClientCompletionConfiguration
{
    bool enabled = true;
    /// Whether to automatically autocomplete end
    bool autocompleteEnd = false;
    /// Whether we should suggest automatic imports in completions
    /// DEPRECATED: USE `completion.imports.enabled` INSTEAD
    bool suggestImports = false;
    /// Automatic imports configuration
    ClientCompletionImportsConfiguration imports{};
    /// Automatically add parentheses to a function call
    bool addParentheses = true;
    /// If parentheses are added, include a $0 tabstop after the parentheses
    bool addTabstopAfterParentheses = true;
    /// If parentheses are added, fill call arguments with parameter names
    bool fillCallArguments = true;
    /// Whether to show non-function properties when performing a method call with a colon
    bool showPropertiesOnMethodCall = false;
    /// Whether to show keywords (`if` / `then` / `and` / etc.) during autocomplete
    bool showKeywords = true;
    /// Enables the experimental fragment autocomplete system for performance improvements
    bool enableFragmentAutocomplete = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientCompletionConfiguration, enabled, autocompleteEnd, suggestImports, imports, addParentheses,
    addTabstopAfterParentheses, fillCallArguments, showPropertiesOnMethodCall, showKeywords, enableFragmentAutocomplete);

struct ClientSignatureHelpConfiguration
{
    bool enabled = true;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientSignatureHelpConfiguration, enabled);

struct ClientRequireConfiguration
{
    // DEPRECATED: USE .luaurc INSTEAD. A mapping of custom require strings to file paths
    std::unordered_map<std::string, std::string> fileAliases;
    // DEPRECATED: USE .luaurc INSTEAD. A mapping of custom require prefixes to directory paths
    std::unordered_map<std::string, std::string> directoryAliases;
    // DEPRECATED: USE NEW REQUIRE-BY-STRING SEMANTICS INSTEAD.
    bool useOriginalRequireByStringSemantics = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientRequireConfiguration, fileAliases, directoryAliases, useOriginalRequireByStringSemantics);

struct ClientIndexConfiguration
{
    /// Whether the whole workspace should be indexed. If disabled, only limited support is
    // available for features such as "Find All References" and "Rename"
    bool enabled = true;
    /// The maximum amount of files that can be indexed
    size_t maxFiles = 10000;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientIndexConfiguration, enabled, maxFiles);

struct ClientFFlagsConfiguration
{
    // NOTE: THE ENABLEBYDEFAULT AND SYNC FLAGS ARE INVERTED IN THE VSCODE DEFAULTS
    // WE USE THIS SO THAT THE DEFAULTS ARE REASONABLE FOR THE CLI

    /// Enable all (boolean) Luau FFlags by default. These flags can later be overriden by `#luau-lsp.fflags.override#` and `#luau-lsp.fflags.sync#`
    bool enableByDefault = true;
    // Sync currently enabled FFlags with Roblox's published FFlags.\nThis currently only syncs FFlags which begin with 'Luau'
    bool sync = false;
    // Override FFlags passed to Luau
    std::unordered_map<std::string, std::string> override;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientFFlagsConfiguration, enableByDefault, sync, override);

struct ClientBytecodeConfiguration
{
    int debugLevel = 1;
    int typeInfoLevel = 1;
    std::string vectorLib = "Vector3";
    std::string vectorCtor = "new";
    std::string vectorType = "Vector3";
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientBytecodeConfiguration, debugLevel, typeInfoLevel, vectorLib, vectorCtor, vectorType)

enum struct LSPPlatformConfig
{
    Standard,
    Roblox
};
NLOHMANN_JSON_SERIALIZE_ENUM(LSPPlatformConfig, {
                                                    {LSPPlatformConfig::Standard, "standard"},
                                                    {LSPPlatformConfig::Roblox, "roblox"},
                                                })

struct ClientPlatformConfiguration
{
    LSPPlatformConfig type = LSPPlatformConfig::Roblox;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientPlatformConfiguration, type);

// These are the passed configuration options by the client, prefixed with `luau-lsp.`
// Here we also define the default settings
struct ClientConfiguration
{
    /// Whether to automatically autocomplete end
    /// DEPRECATED: Use completion.autocompleteEnd instead
    bool autocompleteEnd = false;
    std::vector<std::string> ignoreGlobs{};
    ClientPlatformConfiguration platform{};
    ClientRobloxSourcemapConfiguration sourcemap{};
    ClientDiagnosticsConfiguration diagnostics{};
    ClientTypesConfiguration types{};
    ClientInlayHintsConfiguration inlayHints{};
    ClientHoverConfiguration hover{};
    ClientCompletionConfiguration completion{};
    ClientSignatureHelpConfiguration signatureHelp{};
    ClientRequireConfiguration require{};
    ClientIndexConfiguration index{};
    ClientFFlagsConfiguration fflags{};
    ClientBytecodeConfiguration bytecode{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ClientConfiguration, autocompleteEnd, ignoreGlobs, platform, sourcemap, diagnostics, types,
    inlayHints, hover, completion, signatureHelp, require, index, fflags, bytecode);
