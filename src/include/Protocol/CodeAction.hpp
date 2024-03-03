#pragma once
#include <optional>
#include <vector>
#include <string>

#include "LSP/Utils.hpp"
#include "Protocol/Diagnostics.hpp"
#include "Protocol/Structures.hpp"

namespace lsp
{
/**
 * The kind of a code action.
 *
 * Kinds are a hierarchical list of identifiers separated by `.`,
 * e.g. `"refactor.extract.function"`.
 *
 * The set of kinds is open and client needs to announce the kinds it supports
 * to the server during initialization.
 */
enum struct CodeActionKind
{
    /**
     * Empty kind.
     */
    Empty,
    /**
     * Base kind for quickfix actions: 'quickfix'.
     */
    QuickFix,
    /**
     * Base kind for refactoring actions: 'refactor'.
     */
    Refactor,
    /**
     * Base kind for refactoring extraction actions: 'refactor.extract'.
     *
     * Example extract actions:
     *
     * - Extract method
     * - Extract function
     * - Extract variable
     * - Extract interface from class
     * - ...
     */
    RefactorExtract,
    /**
     * Base kind for refactoring inline actions: 'refactor.inline'.
     *
     * Example inline actions:
     *
     * - Inline function
     * - Inline variable
     * - Inline constant
     * - ...
     */
    RefactorInline,
    /**
     * Base kind for refactoring rewrite actions: 'refactor.rewrite'.
     *
     * Example rewrite actions:
     *
     * - Convert JavaScript function to class
     * - Add or remove parameter
     * - Encapsulate field
     * - Make method static
     * - Move method to base class
     * - ...
     */
    RefactorRewrite,
    /**
     * Base kind for source actions: `source`.
     *
     * Source code actions apply to the entire file.
     */
    Source,
    /**
     * Base kind for an organize imports source action:
     * `source.organizeImports`.
     */
    SourceOrganizeImports,
    /**
     * Base kind for a 'fix all' source action: `source.fixAll`.
     *
     * 'Fix all' actions automatically fix errors that have a clear fix that
     * do not require user input. They should not suppress errors or perform
     * unsafe fixes such as generating new types or classes.
     *
     * @since 3.17.0
     */
    SourceFixAll,
};
NLOHMANN_JSON_SERIALIZE_ENUM(CodeActionKind, {
                                                 {CodeActionKind::Empty, ""},
                                                 {CodeActionKind::QuickFix, "quickfix"},
                                                 {CodeActionKind::Refactor, "refactor"},
                                                 {CodeActionKind::RefactorExtract, "refactor.extract"},
                                                 {CodeActionKind::RefactorInline, "refactor.inline"},
                                                 {CodeActionKind::RefactorRewrite, "refactor.rewrite"},
                                                 {CodeActionKind::Source, "source"},
                                                 {CodeActionKind::SourceOrganizeImports, "source.organizeImports"},
                                                 {CodeActionKind::SourceFixAll, "source.fixAll"},
                                             })


/**
 * The reason why code actions were requested.
 *
 * @since 3.17.0
 */
enum struct CodeActionTriggerKind
{
    /**
     * Code actions were explicitly requested by the user or by an extension.
     */
    Invoked = 1,

    /**
     * Code actions were requested automatically.
     *
     * This typically happens when current selection in a file changes, but can
     * also be triggered when file content changes.
     */
    Automatic = 2,
};

/**
 * Contains additional diagnostic information about the context in which
 * a code action is run.
 */
struct CodeActionContext
{
    /**
     * An array of diagnostics known on the client side overlapping the range
     * provided to the `textDocument/codeAction` request. They are provided so
     * that the server knows which errors are currently presented to the user
     * for the given range. There is no guarantee that these accurately reflect
     * the error state of the resource. The primary parameter
     * to compute code actions is the provided range.
     */

    std::vector<Diagnostic> diagnostics{};
    /**
     * Requested kind of actions to return.
     *
     * Actions not of this kind are filtered out by the client before being
     * shown. So servers can omit computing them.
     */
    std::vector<CodeActionKind> only{};
    /**
     * The reason why code actions were requested.
     *
     * @since 3.17.0
     */
    // TODO: this is technicall optional, but it causes build issues
    CodeActionTriggerKind triggerKind = CodeActionTriggerKind::Invoked;

    [[nodiscard]] bool wants(lsp::CodeActionKind kind) const {
        return only.empty() || contains(only, kind);
    }
};
NLOHMANN_DEFINE_OPTIONAL(CodeActionContext, diagnostics, only, triggerKind)

/**
 * A code action represents a change that can be performed in code, e.g. to fix
 * a problem or to refactor code.
 *
 * A CodeAction must set either `edit` and/or a `command`. If both are supplied,
 * the `edit` is applied first, then the `command` is executed.
 */
struct CodeAction
{
    /**
     * A short, human-readable, title for this code action.
     */
    std::string title;

    /**
     * The kind of the code action.
     *
     * Used to filter code actions.
     */
    std::optional<CodeActionKind> kind = std::nullopt;

    /**
     * The diagnostics that this code action resolves.
     */
    std::vector<Diagnostic> diagnostics{};

    /**
     * Marks this as a preferred action. Preferred actions are used by the
     * `auto fix` command and can be targeted by keybindings.
     *
     * A quick fix should be marked preferred if it properly addresses the
     * underlying error. A refactoring should be marked preferred if it is the
     * most reasonable choice of actions to take.
     *
     * @since 3.15.0
     */
    bool isPreferred = false;

    struct CodeActionDisabled
    {
        /**
         * Human readable description of why the code action is currently
         * disabled.
         *
         * This is displayed in the code actions UI.
         */
        std::string reason;
    };

    /**
     * Marks that the code action cannot currently be applied.
     *
     * Clients should follow the following guidelines regarding disabled code
     * actions:
     *
     * - Disabled code actions are not shown in automatic lightbulbs code
     *   action menus.
     *
     * - Disabled actions are shown as faded out in the code action menu when
     *   the user request a more specific type of code action, such as
     *   refactorings.
     *
     * - If the user has a keybinding that auto applies a code action and only
     *   a disabled code actions are returned, the client should show the user
     *   an error message with `reason` in the editor.
     *
     * @since 3.16.0
     */
    std::optional<CodeActionDisabled> disabled = std::nullopt;

    /**
     * The workspace edit this code action performs.
     */
    std::optional<WorkspaceEdit> edit = std::nullopt;

    /**
     * A command this code action executes. If a code action
     * provides an edit and a command, first the edit is
     * executed and then the command.
     */
    std::optional<Command> command = std::nullopt;

    /**
     * A data entry field that is preserved on a code action between
     * a `textDocument/codeAction` and a `codeAction/resolve` request.
     *
     * @since 3.16.0
     */
    std::optional<LSPAny> data = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(CodeAction::CodeActionDisabled, reason)
NLOHMANN_DEFINE_OPTIONAL(CodeAction, title, kind, diagnostics, isPreferred, disabled, edit, command, data)


struct CodeActionParams
{
    /**
     * The document in which the command was invoked.
     */
    TextDocumentIdentifier textDocument;
    /**
     * The range for which the command was invoked.
     */
    Range range;
    /**
     * Context carrying additional information.
     */
    CodeActionContext context;
};
NLOHMANN_DEFINE_OPTIONAL(CodeActionParams, textDocument, range, context)

using CodeActionResult = std::optional<std::vector<CodeAction>>;
} // namespace lsp
