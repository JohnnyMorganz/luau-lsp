#pragma once

#include <string>
#include <vector>
#include <optional>
#include <variant>

#include "Protocol/Structures.hpp"

namespace lsp
{

enum struct DiagnosticSeverity
{
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4,
};

enum struct DiagnosticTag
{
    Unnecessary = 1,
    Deprecated = 2,
};

struct DiagnosticRelatedInformation
{
    Location location;
    std::string message;
};
NLOHMANN_DEFINE_OPTIONAL(DiagnosticRelatedInformation, location, message);

struct CodeDescription
{
    URI href;
};
NLOHMANN_DEFINE_OPTIONAL(CodeDescription, href);

struct Diagnostic
{
    Range range;
    std::optional<DiagnosticSeverity> severity;
    std::optional<std::variant<std::string, int>> code;
    std::optional<CodeDescription> codeDescription;
    std::optional<std::string> source;
    std::string message;
    std::vector<DiagnosticTag> tags;
    std::vector<DiagnosticRelatedInformation> relatedInformation;
    // data?
};
NLOHMANN_DEFINE_OPTIONAL(Diagnostic, range, severity, code, codeDescription, source, message, tags, relatedInformation);

struct PublishDiagnosticsParams
{
    DocumentUri uri;
    std::optional<size_t> version;
    std::vector<Diagnostic> diagnostics;
};
NLOHMANN_DEFINE_OPTIONAL(PublishDiagnosticsParams, uri, version, diagnostics);

struct DocumentDiagnosticParams
{
    TextDocumentIdentifier textDocument;
    std::optional<std::string> identifier;
    std::optional<std::string> previousResultId;
};
NLOHMANN_DEFINE_OPTIONAL(DocumentDiagnosticParams, textDocument, identifier, previousResultId);

enum struct DocumentDiagnosticReportKind
{
    Full,
    Unchanged,
};
NLOHMANN_JSON_SERIALIZE_ENUM(
    DocumentDiagnosticReportKind, {{DocumentDiagnosticReportKind::Full, "full"}, {DocumentDiagnosticReportKind::Unchanged, "unchanged"}});

// TODO: we slightly stray away from the specification here for simplicity
// The specification defines separated types FullDocumentDiagnosticReport and UnchangedDocumentDiagnosticReport, depending on the kind
struct SingleDocumentDiagnosticReport
{
    DocumentDiagnosticReportKind kind = DocumentDiagnosticReportKind::Full;
    std::optional<std::string> resultId; // NB: this MUST be present if kind == Unchanged
    std::vector<Diagnostic> items;       // NB: this MUST NOT be present if kind == Unchanged
};
NLOHMANN_DEFINE_OPTIONAL(SingleDocumentDiagnosticReport, kind, resultId, items);

struct RelatedDocumentDiagnosticReport : SingleDocumentDiagnosticReport
{
    std::unordered_map<std::string /* DocumentUri */, SingleDocumentDiagnosticReport> relatedDocuments;
};
NLOHMANN_DEFINE_OPTIONAL(RelatedDocumentDiagnosticReport, kind, resultId, items, relatedDocuments);

using DocumentDiagnosticReport = RelatedDocumentDiagnosticReport;

// struct FullDocumentDiagnosticReport
// {
//     DocumentDiagnosticReportKind kind = DocumentDiagnosticReportKind::Full;
//     std::optional<std::string> resultId;
//     std::vector<Diagnostic> items;
// };
// struct UnchangedDocumentDiagnosticReport
// {
//     DocumentDiagnosticReportKind kind = DocumentDiagnosticReportKind::Unchanged;
//     std::string resultId;
// };
// using SingleDocumentDiagnosticReport = std::variant<FullDocumentDiagnosticReport, UnchangedDocumentDiagnosticReport>;
// struct RelatedFullDocumentDiagnosticReport : FullDocumentDiagnosticReport
// {
//     std::map<std::string /* DocumentUri */, SingleDocumentDiagnosticReport> relatedDocuments;
// };
// struct RelatedUnchangedDocumentDiagnosticReport : UnchangedDocumentDiagnosticReport
// {
//     std::map<std::string /* DocumentUri */, SingleDocumentDiagnosticReport> relatedDocuments;
// };
// using DocumentDiagnosticReport = std::variant<RelatedFullDocumentDiagnosticReport, RelatedUnchangedDocumentDiagnosticReport>;

struct PreviousResultId
{
    DocumentUri uri;
    std::optional<std::string> value;
};
NLOHMANN_DEFINE_OPTIONAL(PreviousResultId, uri, value);

struct WorkspaceDiagnosticParams : PartialResultParams
{
    std::optional<std::string> identifier;
    std::vector<PreviousResultId> previousResultIds;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceDiagnosticParams, partialResultToken, identifier, previousResultIds);

struct WorkspaceDocumentDiagnosticReport : SingleDocumentDiagnosticReport
{
    DocumentUri uri;
    std::optional<size_t> version;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceDocumentDiagnosticReport, kind, resultId, items, uri, version);

struct WorkspaceDiagnosticReport
{
    std::vector<WorkspaceDocumentDiagnosticReport> items;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceDiagnosticReport, items);

struct WorkspaceDiagnosticReportPartialResult
{
    std::vector<WorkspaceDocumentDiagnosticReport> items;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceDiagnosticReportPartialResult, items);

struct DiagnosticServerCancellationData
{
    bool retriggerRequest = true;
};
NLOHMANN_DEFINE_OPTIONAL(DiagnosticServerCancellationData, retriggerRequest);
} // namespace lsp