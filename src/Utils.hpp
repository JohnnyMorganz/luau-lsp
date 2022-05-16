#pragma once
#include <optional>
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>

static std::optional<std::string> getParentPath(const std::string& path)
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

std::string& trim_start(std::string& str)
{
    str.erase(0, str.find_first_not_of(" \n\r\t"));
    return str;
}


std::string& trim_end(std::string& str)
{
    str.erase(str.find_last_not_of(" \n\r\t") + 1);
    return str;
}

std::string& trim(std::string& str)
{
    trim_start(str);
    trim_end(str);
    return str;
}