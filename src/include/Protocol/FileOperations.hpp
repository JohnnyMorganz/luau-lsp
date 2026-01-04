#pragma once

#include <string>
#include <vector>
#include <optional>

#include "Protocol/Base.hpp"
#include "Protocol/Structures.hpp"

namespace lsp
{

/**
 * A pattern kind describing if a glob pattern matches a file a folder or
 * both.
 *
 * @since 3.16.0
 */
enum struct FileOperationPatternKind
{
    /**
	 * The pattern matches a file only.
	 */
    File,
    /**
	 * The pattern matches a folder only.
	 */
    Folder
};
NLOHMANN_JSON_SERIALIZE_ENUM(FileOperationPatternKind, {{FileOperationPatternKind::File, "file"}, {FileOperationPatternKind::Folder, "folder"}})

/**
 * Matching options for the file operation pattern.
 *
 * @since 3.16.0
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
 *
 * @since 3.16.0
 */
struct FileOperationPattern
{
    /**
	 * The glob pattern to match. Glob patterns can have the following syntax:
	 * - `*` to match zero or more characters in a path segment
	 * - `?` to match on one character in a path segment
	 * - `**` to match any number of path segments, including none
	 * - `{}` to group sub patterns into an OR expression. (e.g. `**​/*.{ts,js}`
	 *   matches all TypeScript and JavaScript files)
	 * - `[]` to declare a range of characters to match in a path segment
	 *   (e.g., `example.[0-9]` to match on `example.0`, `example.1`, …)
	 * - `[!...]` to negate a range of characters to match in a path segment
	 *   (e.g., `example.[!0-9]` to match on `example.a`, `example.b`, but
	 *   not `example.0`)
	 */
    std::string glob;

    /**
	 * Whether to match files or folders with this pattern.
	 *
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
 *
 * @since 3.16.0
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
 *
 * @since 3.16.0
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
