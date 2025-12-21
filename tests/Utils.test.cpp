#include "doctest.h"
#include "LSP/Utils.hpp"
#include "LuauFileUtils.hpp"
#include "Platform/RobloxPlatform.hpp"
#include "TempDir.h"

TEST_SUITE_BEGIN("UtilsTest");

TEST_CASE("getAncestorPath finds ancestor from given name")
{
    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "Module", nullptr), "game/ReplicatedStorage/Module");
}

TEST_CASE("getAncestorPath handles when ancestor is not found")
{
    CHECK_FALSE(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "NonExistent", nullptr).has_value());
}

TEST_CASE("getAncestorPath handles when ancestor is root of DataModel node")
{
    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "game", nullptr), "game");
}

TEST_CASE("getAncestorPath handles when ancestor is root of non-DataModel node")
{
    SourceNode node("Foo", "ClassName", {}, {});

    CHECK_EQ(getAncestorPath("ProjectRoot/Bar", "Foo", &node), "ProjectRoot");
}

TEST_CASE("getAncestorPath handles when ancestor is root of non-DataModel node and its name has multiple occurrences")
{
    SourceNode node("Foo", "ClassName", {}, {});

    CHECK_EQ(getAncestorPath("ProjectRoot/Bar/Foo/Baz", "Foo", &node), "ProjectRoot/Bar/Foo");
}

TEST_CASE("getAncestorPath returns nothing when ancestorName == current name, and no ancestor of name found")
{
    CHECK_FALSE(getAncestorPath("game/ReplicatedStorage/Module/Child/Foo", "Foo", nullptr).has_value());
}

TEST_CASE("getAncestorPath handles when ancestor name is the same as current name")
{
    CHECK_EQ(getAncestorPath("game/ReplicatedStorage/Module/Child/Module", "Module", nullptr), "game/ReplicatedStorage/Module");
}

TEST_CASE("convertToScriptPath handles when path is empty")
{
    CHECK_EQ(convertToScriptPath(""), "");
}

TEST_CASE("convertToScriptPath handles when path is root")
{
    CHECK_EQ(convertToScriptPath("game"), "game");
}

TEST_CASE("convertToScriptPath handles when path is a single folder")
{
    CHECK_EQ(convertToScriptPath("game/ReplicatedStorage"), "game.ReplicatedStorage");
}

TEST_CASE("convertToScriptPath handles whitespaces in path")
{
    CHECK_EQ(convertToScriptPath("game/Replicated Storage/Common"), "game[\"Replicated Storage\"].Common");
}

TEST_CASE("convertToScriptPath handles non-identifier characters in path")
{
    CHECK_EQ(convertToScriptPath("game/ReplicatedStorage/Packages/react-spring"), "game.ReplicatedStorage.Packages[\"react-spring\"]");
}

TEST_CASE("convertToScriptPath handles relative paths")
{
    CHECK_EQ(convertToScriptPath("../Child/Foo"), "script.Parent.Child.Foo");
}

TEST_CASE("convertToScriptPath handles path where name contains dot")
{
    CHECK_EQ(convertToScriptPath("../Child.Foo"), "script.Parent[\"Child.Foo\"]");
}

TEST_CASE("getHomeDirectory finds a home directory")
{
    // we cannot confirm *what* the home directory is
    // since it varies per test runner
    CHECK(getHomeDirectory());
}

TEST_CASE("resolvePath resolves paths including tilde expansions")
{
    CHECK_EQ(resolvePath("C:/Users/test/foo.lua"), "C:/Users/test/foo.lua");

    auto home = getHomeDirectory();
    REQUIRE(home);

    CHECK_EQ(resolvePath("~/foo.lua"), Luau::FileUtils::joinPaths(*home, "foo.lua"));
}

TEST_CASE("isDataModel returns true when path starts with game")
{
    CHECK_EQ(isDataModel("game/ReplicatedStorage"), true);
}

TEST_CASE("isDataModel returns false when path starts with ProjectRoot")
{
    CHECK_EQ(isDataModel("ProjectRoot/Foo/Bar"), false);
}

TEST_CASE("getFirstLine returns first line")
{
    CHECK_EQ(getFirstLine("--##{'x':true}\nhello = world"), "--##{'x':true}");
}

TEST_CASE("getFirstLine returns string when there is no newline")
{
    CHECK_EQ(getFirstLine(""), "");
    CHECK_EQ(getFirstLine("testing"), "testing");
}

TEST_CASE("readFile can handle non-ASCII characters in path")
{
    auto path = Luau::FileUtils::joinPaths(*Luau::FileUtils::getCurrentWorkingDirectory(), "tests/testdata/non-ascii/ō.luau");
    auto result = Luau::FileUtils::readFile(path);
    REQUIRE(result);
    CHECK_EQ(*result, "local _ = 1");
}

TEST_CASE("traverseDirectory can handle non-ASCII characters in path")
{
    auto basePath = Luau::FileUtils::joinPaths(*Luau::FileUtils::getCurrentWorkingDirectory(), "tests/testdata/non-ascii");

    std::vector<std::string> paths;
    Luau::FileUtils::traverseDirectoryRecursive(basePath,
        [&](const auto& path)
        {
            paths.push_back(path);
        });

    CHECK_EQ(paths.size(), 2);

    paths.clear();
    auto nonAsciiBasePath = Luau::FileUtils::joinPaths(*Luau::FileUtils::getCurrentWorkingDirectory(), "tests/testdata/non-ascii/Рабочий стол");
    Luau::FileUtils::traverseDirectoryRecursive(nonAsciiBasePath,
        [&](const auto& path)
        {
            paths.push_back(path);
        });

    CHECK_EQ(paths.size(), 1);
}

// Issue #1191: Test UTF-8 command-line argument handling
// On Windows, command-line arguments are encoded in the system code page, not UTF-8.
// These tests verify the fix for handling non-ASCII characters in file paths.

#ifdef _WIN32
TEST_CASE("toUtf8 converts wide string to UTF-8 string")
{
    // Test Polish character 'ż' (U+017C)
    // L"Użytkownik" = { 'U', 0x017C, 'y', 't', 'k', 'o', 'w', 'n', 'i', 'k' }
    std::wstring polish = L"U\u017Cytkownik";
    std::string utf8 = Luau::FileUtils::toUtf8(polish);
    CHECK_EQ(utf8, "Użytkownik");

    // Test Cyrillic: "Рабочий стол" (Russian for "Desktop")
    std::wstring cyrillic = L"\u0420\u0430\u0431\u043E\u0447\u0438\u0439 \u0441\u0442\u043E\u043B";
    std::string utf8Cyr = Luau::FileUtils::toUtf8(cyrillic);
    CHECK_EQ(utf8Cyr, "Рабочий стол");
}

TEST_CASE("fromUtf8 and toUtf8 are inverses for Unicode paths")
{
    std::string original = "C:/Users/Użytkownik/Desktop/Рабочий стол/test.luau";
    std::wstring wide = Luau::FileUtils::fromUtf8(original);
    std::string roundTrip = Luau::FileUtils::toUtf8(wide);
    CHECK_EQ(original, roundTrip);
}

TEST_CASE("getUtf8CommandLineArgs returns UTF-8 encoded arguments")
{
    // This test verifies the function exists and returns non-empty results
    // when running from command line with arguments
    auto args = Luau::FileUtils::getUtf8CommandLineArgs();
    // Should have at least the program name
    CHECK_FALSE(args.empty());
}
#endif

TEST_SUITE_END();
