#pragma once

#include <string>
#include <vector>
#include <optional>

#include "Protocol/Base.hpp"
#include "Protocol/Structures.hpp"

namespace lsp
{

/**
 * A pattern kind describing if a glob pattern matches a file a folder or both.
 */
enum struct FileOperationPatternKind
{
    File,
    Folder
};
NLOHMANN_JSON_SERIALIZE_ENUM(FileOperationPatternKind, {{FileOperationPatternKind::File, "file"}, {FileOperationPatternKind::Folder, "folder"}})

/**
 * Matching options for the file operation pattern.
 */
struct FileOperationPatternOptions
{
    /**
     * The pattern should be matched ignoring casing.
     */
    bool ignoreCase = false;
};
NLOHMANN_DEFINE_OPTIONAL(FileOperationPatternOptions, ignoreCase)

/**
 * A pattern to describe in which file operation requests or notifications
 * the server is interested in.
 */
struct FileOperationPattern
{
    /**
     * The glob pattern to match. Glob patterns can have the following syntax:
     * - `*` to match one or more characters in a path segment
     * - `?` to match on one character in a path segment
     * - `**` to match any number of path segments, including none
     * - `{}` to group sub patterns into an OR expression
     * - `[]` to declare a range of characters to match in a path segment
     * - `[!...]` to negate a range of characters to match in a path segment
     */
    std::string glob;

    /**
     * Whether to match files or folders with this pattern.
     * Matches both if undefined.
     */
    std::optional<FileOperationPatternKind> matches = std::nullopt;

    /**
     * Additional options used during matching.
     */
    std::optional<FileOperationPatternOptions> options = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(FileOperationPattern, glob, matches, options)

/**
 * A filter to describe in which file operation requests or notifications
 * the server is interested in.
 */
struct FileOperationFilter
{
    /**
     * A Uri like `file` or `untitled`.
     */
    std::optional<std::string> scheme = std::nullopt;

    /**
     * The actual file operation pattern.
     */
    FileOperationPattern pattern;
};
NLOHMANN_DEFINE_OPTIONAL(FileOperationFilter, scheme, pattern)

/**
 * The options to register for file operations.
 */
struct FileOperationRegistrationOptions
{
    /**
     * The actual filters.
     */
    std::vector<FileOperationFilter> filters;
};
NLOHMANN_DEFINE_OPTIONAL(FileOperationRegistrationOptions, filters)

/**
 * The options for file operations on the server side.
 */
struct FileOperationOptions
{
    /**
     * The server is interested in receiving didCreateFiles notifications.
     */
    std::optional<FileOperationRegistrationOptions> didCreate = std::nullopt;

    /**
     * The server is interested in receiving willCreateFiles requests.
     */
    std::optional<FileOperationRegistrationOptions> willCreate = std::nullopt;

    /**
     * The server is interested in receiving didRenameFiles notifications.
     */
    std::optional<FileOperationRegistrationOptions> didRename = std::nullopt;

    /**
     * The server is interested in receiving willRenameFiles requests.
     */
    std::optional<FileOperationRegistrationOptions> willRename = std::nullopt;

    /**
     * The server is interested in receiving didDeleteFiles file notifications.
     */
    std::optional<FileOperationRegistrationOptions> didDelete = std::nullopt;

    /**
     * The server is interested in receiving willDeleteFiles file requests.
     */
    std::optional<FileOperationRegistrationOptions> willDelete = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(FileOperationOptions, didCreate, willCreate, didRename, willRename, didDelete, willDelete)

/**
 * Represents information on a file/folder rename.
 *
 * @since 3.16.0
 */
struct FileRename
{
    /**
     * A file:// URI for the original location of the file/folder being renamed.
     */
    DocumentUri oldUri;

    /**
     * A file:// URI for the new location of the file/folder being renamed.
     */
    DocumentUri newUri;
};
NLOHMANN_DEFINE_OPTIONAL(FileRename, oldUri, newUri)

/**
 * The parameters sent in notifications/requests for user-initiated
 * renames of files.
 *
 * @since 3.16.0
 */
struct RenameFilesParams
{
    /**
	 * An array of all files/folders renamed in this operation. When a folder
	 * is renamed, only the folder will be included, and not its children.
	 */
    std::vector<FileRename> files;
};
NLOHMANN_DEFINE_OPTIONAL(RenameFilesParams, files)

} // namespace lsp
