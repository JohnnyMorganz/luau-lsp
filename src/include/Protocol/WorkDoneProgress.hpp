#pragma once

#include <string>
#include "Protocol/Base.hpp"

namespace lsp
{

struct WorkDoneProgressBegin
{
    std::string kind = "begin";

    /**
     * Mandatory title of the progress operation. Used to briefly inform about
     * the kind of operation being performed.
     *
     * Examples: "Indexing" or "Linking dependencies".
     */
    std::string title;

    /**
     * Controls if a cancel button should show to allow the user to cancel the
     * long running operation. Clients that don't support cancellation are
     * allowed to ignore the setting.
     */
    bool cancellable = false;

    /**
     * Optional, more detailed associated progress message. Contains
     * complementary information to the `title`.
     *
     * Examples: "3/25 files", "project/src/module2", "node_modules/some_dep".
     * If unset, the previous progress message (if any) is still valid.
     */
    std::optional<std::string> message = std::nullopt;

    /**
     * Optional progress percentage to display (value 100 is considered 100%).
     * If not provided infinite progress is assumed and clients are allowed
     * to ignore the `percentage` value in subsequent report notifications.
     *
     * The value should be steadily rising. Clients are free to ignore values
     * that are not following this rule. The value range is [0, 100].
     */
    std::optional<uint8_t> percentage = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(WorkDoneProgressBegin, kind, title, cancellable, message, percentage)

struct WorkDoneProgressReport
{
    std::string kind = "report";

    /**
     * Controls enablement state of a cancel button. This property is only valid
     * if a cancel button got requested in the `WorkDoneProgressBegin` payload.
     *
     * Clients that don't support cancellation or don't support control the
     * button's enablement state are allowed to ignore the setting.
     */
    bool cancellable = false;

    /**
     * Optional, more detailed associated progress message. Contains
     * complementary information to the `title`.
     *
     * Examples: "3/25 files", "project/src/module2", "node_modules/some_dep".
     * If unset, the previous progress message (if any) is still valid.
     */
    std::optional<std::string> message = std::nullopt;

    /**
     * Optional progress percentage to display (value 100 is considered 100%).
     * If not provided infinite progress is assumed and clients are allowed
     * to ignore the `percentage` value in subsequent report notifications.
     *
     * The value should be steadily rising. Clients are free to ignore values
     * that are not following this rule. The value range is [0, 100].
     */
    std::optional<uint8_t> percentage = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(WorkDoneProgressReport, kind, cancellable, message, percentage)

struct WorkDoneProgressEnd
{
    std::string kind = "end";

    /**
     * Optional, a final message indicating to for example indicate the outcome
     * of the operation.
     */
    std::optional<std::string> message = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(WorkDoneProgressEnd, kind, message)

struct WorkDoneProgressCreateParams
{
    /**
     * The token to be used to report progress.
     */
    lsp::ProgressToken token;
};
NLOHMANN_DEFINE_OPTIONAL(WorkDoneProgressCreateParams, token)

} // namespace lsp