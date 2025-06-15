#include "TempDir.h"

#include "Luau/Common.h"

#include <iostream>
#include <fstream>

TempDir::TempDir(const std::string& name)
    : fullPath(std::filesystem::temp_directory_path() / name)
{
    std::filesystem::remove_all(fullPath);
    std::filesystem::create_directories(fullPath);
    // Note: on macOS, TEMPDIR points to /var/, but it's a symlink to /private/var
    // For simplicity, let's resolve the symlink so that other code works correctly
    fullPath = std::filesystem::canonical(fullPath);
}

TempDir::~TempDir()
{
    //    std::filesystem::remove_all(fullPath);
}

std::string TempDir::path()
{
    return fullPath.generic_string();
}

static std::filesystem::path create_child(const std::filesystem::path& base, const std::filesystem::path& child_path)
{
    LUAU_ASSERT(!child_path.is_absolute());

    auto child_location = base / child_path; // TODO: should we check child_location is still within base?
    std::filesystem::create_directories(child_location.parent_path());

    return child_location;
}

std::string TempDir::touch_child(const std::filesystem::path& child_path)
{
    auto child_location = create_child(fullPath, child_path);

    std::ofstream file(child_location);
    file.close();

    return child_location.generic_string();
}

std::string TempDir::write_child(const std::filesystem::path& child_path, const std::string& contents)
{
    auto child_location = create_child(fullPath, child_path);

    std::ofstream file(child_location);
    file << contents;
    file.close();

    return child_location.generic_string();
}
