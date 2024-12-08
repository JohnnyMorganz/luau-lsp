#pragma once

#include <string>
#include <filesystem>

/// Temporary directory for testing. Will automatically cleanup all contents on destruction
class TempDir
{
    std::filesystem::path fullPath;

public:
    explicit TempDir(const std::string& name);
    ~TempDir();

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    /// Full path of the temporary directory
    std::string path();

    /// Creates an empty child within the directory. If the child is nested, will create the necessary directories
    std::string touch_child(const std::filesystem::path& child_path);

    /// Writes a child within the directory. If the child is nested, will create the necessary directories
    std::string write_child(const std::filesystem::path& child_path, const std::string& contents);
};
