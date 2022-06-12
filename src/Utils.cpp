#include "LSP/Utils.hpp"
#include <algorithm>

std::optional<std::string> getParentPath(const std::string& path)
{
    if (path == "" || path == "." || path == "/")
        return std::nullopt;

    std::string::size_type slash = path.find_last_of("\\/", path.size() - 1);

    if (slash == 0)
        return "/";

    if (slash != std::string::npos)
        return path.substr(0, slash);

    return "";
}

/// Returns a path at the ancestor point.
/// i.e., for game/ReplicatedStorage/Module/Child/Foo, and ancestor == Module, returns game/ReplicatedStorage/Module
std::optional<std::string> getAncestorPath(const std::string& path, const std::string& ancestorName)
{
    // We want to remove the child from the path name in case the ancestor has the same name as the child
    auto parentPath = getParentPath(path);
    if (!parentPath)
        return std::nullopt;

    // Append a "/" to the end of the parentPath to make searching easier
    auto parentPathWithSlash = *parentPath + "/";


    auto ancestor = parentPathWithSlash.rfind(ancestorName + "/");
    if (ancestor != std::string::npos)
    {
        // We need to ensure that the character before the ancestor is a / (or the ancestor is the very beginning)
        // And also make sure that the character after the ancestor is a / (or the ancestor is at the very end)
        if (ancestor == 0 || parentPathWithSlash.at(ancestor - 1) == '/')
        {
            return parentPathWithSlash.substr(0, ancestor + ancestorName.size());
        }
    }

    return std::nullopt;
}

std::string codeBlock(std::string language, std::string code)
{
    return "```" + language + "\n" + code + "\n" + "```";
}

std::optional<std::string> readFile(const std::filesystem::path& filePath)
{
    std::ifstream fileContents;
    fileContents.open(filePath);

    std::string output;
    std::stringstream buffer;

    if (fileContents)
    {
        buffer << fileContents.rdbuf();
        output = buffer.str();
        return output;
    }
    else
    {
        return std::nullopt;
    }
}

void trim_start(std::string& str)
{
    str.erase(0, str.find_first_not_of(" \n\r\t"));
}


void trim_end(std::string& str)
{
    str.erase(str.find_last_not_of(" \n\r\t") + 1);
}

void trim(std::string& str)
{
    trim_start(str);
    trim_end(str);
}

std::string& toLower(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(),
        [](unsigned char c)
        {
            return std::tolower(c);
        });
    return str;
}

bool endsWith(const std::string_view& str, const std::string_view& suffix)
{
    return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}