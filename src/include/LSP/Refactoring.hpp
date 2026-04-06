#pragma once
#include <memory>
#include <vector>

#include "Protocol/CodeAction.hpp"
#include "LSP/TextDocument.hpp"
#include "Luau/Location.h"

namespace Luau
{
struct SourceModule;
struct FrontendCancellationToken;
}

class WorkspaceFolder;
using LSPCancellationToken = std::shared_ptr<Luau::FrontendCancellationToken>;

void computeRefactorings(
    const lsp::CodeActionParams& params,
    const Luau::SourceModule& sourceModule,
    const TextDocument& textDocument,
    const Luau::Location& requestRange,
    std::vector<lsp::CodeAction>& result);

lsp::CodeAction resolveRefactoring(
    const lsp::CodeAction& action,
    WorkspaceFolder& workspace,
    const LSPCancellationToken& cancellationToken);
