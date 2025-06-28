// Source: https://github.com/microsoft/vscode-uri/blob/main/src/test/uri.test.ts
#include "doctest.h"
#include "nlohmann/json.hpp"
#include "LSP/Uri.hpp"

#ifdef _WIN32
#define IF_WINDOWS(X, Y) X
#endif
#ifndef _WIN32
#define IF_WINDOWS(X, Y) Y
#endif

std::ostream& operator<<(std::ostream& stream, const Uri& uri)
{
    return stream << uri.toString();
}

TEST_SUITE_BEGIN("UriTests");

TEST_CASE("file#toString")
{
    CHECK_EQ(Uri::file("c:/win/path").toString(), "file:///c%3A/win/path");
    CHECK_EQ(Uri::file("C:/win/path").toString(), "file:///c%3A/win/path");
    CHECK_EQ(Uri::file("c:/win/path/").toString(), "file:///c%3A/win/path/");
    CHECK_EQ(Uri::file("/c:/win/path").toString(), "file:///c%3A/win/path");
}

TEST_CASE("Uri::file (win-special)")
{
#ifdef _WIN32
    CHECK_EQ(Uri::file("c:\\win\\path").toString(), "file:///c%3A/win/path");
    CHECK_EQ(Uri::file("c:\\win/path").toString(), "file:///c%3A/win/path");
#endif

#ifndef _WIN32
    CHECK_EQ(Uri::file("c:\\win\\path").toString(), "file:///c%3A%5Cwin%5Cpath");
    CHECK_EQ(Uri::file("c:\\win/path").toString(), "file:///c%3A%5Cwin/path");
#endif
}

TEST_CASE("file#fsPath (win-special)")
{
#ifdef _WIN32

    CHECK_EQ(Uri::file("c:\\win\\path").fsPath(), "c:\\win\\path");
    CHECK_EQ(Uri::file("c:\\win/path").fsPath(), "c:\\win\\path");

    CHECK_EQ(Uri::file("c:/win/path").fsPath(), "c:\\win\\path");
    CHECK_EQ(Uri::file("c:/win/path/").fsPath(), "c:\\win\\path\\");
    CHECK_EQ(Uri::file("C:/win/path").fsPath(), "c:\\win\\path");
    CHECK_EQ(Uri::file("/c:/win/path").fsPath(), "c:\\win\\path");
    CHECK_EQ(Uri::file("./c/win/path").fsPath(), "\\.\\c\\win\\path");
#endif

#ifndef _WIN32
    CHECK_EQ(Uri::file("c:/win/path").fsPath(), "c:/win/path");
    CHECK_EQ(Uri::file("c:/win/path/").fsPath(), "c:/win/path/");
    CHECK_EQ(Uri::file("C:/win/path").fsPath(), "C:/win/path"); // DEVIATION: c:/win/path
    CHECK_EQ(Uri::file("/c:/win/path").fsPath(), "c:/win/path");
    CHECK_EQ(Uri::file("./c/win/path").fsPath(), "/./c/win/path");
#endif
}

TEST_CASE("URI#fsPath - no `fsPath` when no `path`")
{
    auto value = Uri::parse("file://%2Fhome%2Fticino%2Fdesktop%2Fcpluscplus%2Ftest.cpp");
    CHECK_EQ(value.authority, "/home/ticino/desktop/cpluscplus/test.cpp");
    CHECK_EQ(value.path, "/");
#ifdef _WIN32
    CHECK_EQ(value.fsPath(), "\\");
#endif
#ifndef _WIN32
    CHECK_EQ(value.fsPath(), "/");
#endif
}

TEST_CASE("http#toString")
{
    CHECK_EQ(Uri("http", "www.msft.com", "/my/path").toString(), "http://www.msft.com/my/path");
    CHECK_EQ(Uri("http", "www.msft.com", "/my/path").toString(), "http://www.msft.com/my/path");
    CHECK_EQ(Uri("http", "www.MSFT.com", "/my/path").toString(), "http://www.msft.com/my/path");
    CHECK_EQ(Uri("http", "", "my/path").toString(), "http:/my/path");
    CHECK_EQ(Uri("http", "", "/my/path").toString(), "http:/my/path");
    // http://a-test-site.com/#test=true
    CHECK_EQ(Uri("http", "a-test-site.com", "/", "test=true").toString(), "http://a-test-site.com/?test%3Dtrue");
    CHECK_EQ(Uri("http", "a-test-site.com", "/", "", "test=true").toString(), "http://a-test-site.com/#test%3Dtrue");
}

TEST_CASE("http#toString, encode=FALSE")
{
    CHECK_EQ(Uri("http", "a-test-site.com", "/", "test=true").toString(true), "http://a-test-site.com/?test=true");
    CHECK_EQ(Uri("http", "a-test-site.com", "/", "", "test=true").toString(true), "http://a-test-site.com/#test=true");
    CHECK_EQ(Uri("http", "", "/api/files/test.me", "t=1234").toString(true), "http:/api/files/test.me?t=1234");

    auto value = Uri::parse("file://shares/pröjects/c%23/#l12");
    CHECK_EQ(value.authority, "shares");
    CHECK_EQ(value.path, "/pröjects/c#/");
    CHECK_EQ(value.fragment, "l12");
    CHECK_EQ(value.toString(), "file://shares/pr%C3%B6jects/c%23/#l12");
    CHECK_EQ(value.toString(true), "file://shares/pröjects/c%23/#l12");

    auto uri2 = Uri::parse(value.toString(true));
    auto uri3 = Uri::parse(value.toString());
    CHECK_EQ(uri2.authority, uri3.authority);
    CHECK_EQ(uri2.path, uri3.path);
    CHECK_EQ(uri2.query, uri3.query);
    CHECK_EQ(uri2.fragment, uri3.fragment);
}

// DEVIATION: with testcases removed - NOT REQUIRED

TEST_CASE("parse")
{
    auto value = Uri::parse("http:/api/files/test.me?t=1234");
    CHECK_EQ(value.scheme, "http");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "/api/files/test.me");
    CHECK_EQ(value.query, "t=1234");
    CHECK_EQ(value.fragment, "");

    value = Uri::parse("http://api/files/test.me?t=1234");
    CHECK_EQ(value.scheme, "http");
    CHECK_EQ(value.authority, "api");
    CHECK_EQ(value.path, "/files/test.me");
    CHECK_EQ(value.query, "t=1234");
    CHECK_EQ(value.fragment, "");

    value = Uri::parse("file:///c:/test/me");
    CHECK_EQ(value.scheme, "file");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "/c:/test/me");
    CHECK_EQ(value.fragment, "");
    CHECK_EQ(value.query, "");
    CHECK_EQ(value.fsPath(), IF_WINDOWS("c:\\test\\me", "c:/test/me"));

    value = Uri::parse("file://shares/files/c%23/p.cs");
    CHECK_EQ(value.scheme, "file");
    CHECK_EQ(value.authority, "shares");
    CHECK_EQ(value.path, "/files/c#/p.cs");
    CHECK_EQ(value.fragment, "");
    CHECK_EQ(value.query, "");
    CHECK_EQ(value.fsPath(), IF_WINDOWS("\\\\shares\\files\\c#\\p.cs", "//shares/files/c#/p.cs"));

    value = Uri::parse("file:///c:/Source/Z%C3%BCrich%20or%20Zurich%20(%CB%88zj%CA%8A%C9%99r%C9%AAk,/Code/"
                       "resources/app/plugins/c%23/plugin.json");
    CHECK_EQ(value.scheme, "file");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "/c:/Source/Zürich or Zurich (ˈzjʊərɪk,/Code/resources/app/plugins/c#/plugin.json");
    CHECK_EQ(value.fragment, "");
    CHECK_EQ(value.query, "");

    value = Uri::parse("file:///c:/test %25/path");
    CHECK_EQ(value.scheme, "file");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "/c:/test %/path");
    CHECK_EQ(value.fragment, "");
    CHECK_EQ(value.query, "");

    value = Uri::parse("inmemory:");
    CHECK_EQ(value.scheme, "inmemory");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "");
    CHECK_EQ(value.query, "");
    CHECK_EQ(value.fragment, "");

    value = Uri::parse("foo:api/files/test");
    CHECK_EQ(value.scheme, "foo");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "api/files/test");
    CHECK_EQ(value.query, "");
    CHECK_EQ(value.fragment, "");

    value = Uri::parse("file:?q");
    CHECK_EQ(value.scheme, "file");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "/");
    CHECK_EQ(value.query, "q");
    CHECK_EQ(value.fragment, "");

    value = Uri::parse("file:#d");
    CHECK_EQ(value.scheme, "file");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "/");
    CHECK_EQ(value.query, "");
    CHECK_EQ(value.fragment, "d");

    value = Uri::parse("f3ile:#d");
    CHECK_EQ(value.scheme, "f3ile");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "");
    CHECK_EQ(value.query, "");
    CHECK_EQ(value.fragment, "d");

    value = Uri::parse("foo+bar:path");
    CHECK_EQ(value.scheme, "foo+bar");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "path");
    CHECK_EQ(value.query, "");
    CHECK_EQ(value.fragment, "");

    value = Uri::parse("foo-bar:path");
    CHECK_EQ(value.scheme, "foo-bar");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "path");
    CHECK_EQ(value.query, "");
    CHECK_EQ(value.fragment, "");

    value = Uri::parse("foo.bar:path");
    CHECK_EQ(value.scheme, "foo.bar");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "path");
    CHECK_EQ(value.query, "");
    CHECK_EQ(value.fragment, "");
}

// DEVIATION: No validation
// TEST_CASE("parse, disallow //path when no authority")
// {
//     CHECK_THROWS(Uri::parse("file:////shares/files/p.cs"));
// };

TEST_CASE("URI#file, win-speciale")
{
#ifdef _WIN32
    auto value = Uri::file("c:\\test\\drive");
    CHECK_EQ(value.path, "/c:/test/drive");
    CHECK_EQ(value.toString(), "file:///c%3A/test/drive");

    value = Uri::file("\\\\shäres\\path\\c#\\plugin.json");
    CHECK_EQ(value.scheme, "file");
    CHECK_EQ(value.authority, "shäres");
    CHECK_EQ(value.path, "/path/c#/plugin.json");
    CHECK_EQ(value.fragment, "");
    CHECK_EQ(value.query, "");
    CHECK_EQ(value.toString(), "file://sh%C3%A4res/path/c%23/plugin.json");

    value = Uri::file("\\\\localhost\\c$\\GitDevelopment\\express");
    CHECK_EQ(value.scheme, "file");
    CHECK_EQ(value.path, "/c$/GitDevelopment/express");
    CHECK_EQ(value.fsPath(), "\\\\localhost\\c$\\GitDevelopment\\express");
    CHECK_EQ(value.query, "");
    CHECK_EQ(value.fragment, "");
    CHECK_EQ(value.toString(), "file://localhost/c%24/GitDevelopment/express");

    value = Uri::file("c:\\test with %\\path");
    CHECK_EQ(value.path, "/c:/test with %/path");
    CHECK_EQ(value.toString(), "file:///c%3A/test%20with%20%25/path");

    value = Uri::file("c:\\test with %25\\path");
    CHECK_EQ(value.path, "/c:/test with %25/path");
    CHECK_EQ(value.toString(), "file:///c%3A/test%20with%20%2525/path");

    value = Uri::file("c:\\test with %25\\c#code");
    CHECK_EQ(value.path, "/c:/test with %25/c#code");
    CHECK_EQ(value.toString(), "file:///c%3A/test%20with%20%2525/c%23code");

    value = Uri::file("\\\\shares");
    CHECK_EQ(value.scheme, "file");
    CHECK_EQ(value.authority, "shares");
    CHECK_EQ(value.path, "/"); // slash is always there

    value = Uri::file("\\\\shares\\");
    CHECK_EQ(value.scheme, "file");
    CHECK_EQ(value.authority, "shares");
    CHECK_EQ(value.path, "/");
#endif
}

TEST_CASE("VSCode URI module\'s driveLetterPath regex is incorrect, #32961'")
{
    auto uri = Uri::parse("file:///_:/path");
    CHECK_EQ(uri.fsPath(), IF_WINDOWS("\\_:\\path", "/_:/path"));
}

TEST_CASE("URI#file, no path-is-uri check")
{
    // we don't complain here
    auto value = Uri::file("file://path/to/file");
    CHECK_EQ(value.scheme, "file");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "/file://path/to/file");
}

TEST_CASE("URI#file, always slash")
{
    auto value = Uri::file("a.file");
    CHECK_EQ(value.scheme, "file");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "/a.file");
    CHECK_EQ(value.toString(), "file:///a.file");

    value = Uri::parse(value.toString());
    CHECK_EQ(value.scheme, "file");
    CHECK_EQ(value.authority, "");
    CHECK_EQ(value.path, "/a.file");
    CHECK_EQ(value.toString(), "file:///a.file");
}

TEST_CASE("URI.toString, only scheme and query")
{
    auto value = Uri::parse("stuff:?qüery");
    CHECK_EQ(value.toString(), "stuff:?q%C3%BCery");
};

TEST_CASE("URI#toString, upper-case percent espaces")
{
    auto value = Uri::parse("file://sh%c3%a4res/path");
    CHECK_EQ(value.toString(), "file://sh%C3%A4res/path");
};

TEST_CASE("URI#toString, lower-case windows drive letter")
{
    CHECK_EQ(Uri::parse("untitled:c:/Users/jrieken/Code/abc.txt").toString(), "untitled:c%3A/Users/jrieken/Code/abc.txt");
    CHECK_EQ(Uri::parse("untitled:C:/Users/jrieken/Code/abc.txt").toString(), "untitled:c%3A/Users/jrieken/Code/abc.txt");
}

TEST_CASE("URI#toString, escape all the bits")
{
    auto value = Uri::file(std::string("/Users/jrieken/Code/_samples/18500/Mödel + Other Thîngß/model.js"));
    CHECK_EQ(value.toString(), "file:///Users/jrieken/Code/_samples/18500/M%C3%B6del%20%2B%20Other%20Th%C3%AEng%C3%9F/model.js");
};

TEST_CASE("URI#toString, don\'t encode port")
{
    auto value = Uri::parse("http://localhost:8080/far");
    CHECK_EQ(value.toString(), "http://localhost:8080/far");

    value = Uri("http", "löcalhost:8080", "/far");
    CHECK_EQ(value.toString(), "http://l%C3%B6calhost:8080/far");
}

TEST_CASE("URI#toString, user information in authority")
{
    auto value = Uri::parse("http://foo:bar@localhost/far");
    CHECK_EQ(value.toString(), "http://foo:bar@localhost/far");

    value = Uri::parse("http://foo@localhost/far");
    CHECK_EQ(value.toString(), "http://foo@localhost/far");

    value = Uri::parse("http://foo:bAr@localhost:8080/far");
    CHECK_EQ(value.toString(), "http://foo:bAr@localhost:8080/far");

    value = Uri::parse("http://foo@localhost:8080/far");
    CHECK_EQ(value.toString(), "http://foo@localhost:8080/far");

    value = Uri("http", "föö:bör@löcalhost:8080", "/far");
    CHECK_EQ(value.toString(), "http://f%C3%B6%C3%B6:b%C3%B6r@l%C3%B6calhost:8080/far");
}

TEST_CASE("correctFileUriToFilePath2")
{
    auto test = [](const std::string& input, const std::string& expected)
    {
        auto value = Uri::parse(input);
        CHECK_EQ(value.fsPath(), expected);
        auto value2 = Uri::file(value.fsPath());
        CHECK_EQ(value2.fsPath(), expected);
        CHECK_EQ(value.toString(), value2.toString());
    };

    test("file:///c:/alex.txt", IF_WINDOWS("c:\\alex.txt", "c:/alex.txt"));
    test("file:///c:/Source/Z%C3%BCrich%20or%20Zurich%20(%CB%88zj%CA%8A%C9%99r%C9%AAk,/Code/resources/app/plugins",
        IF_WINDOWS("c:\\Source\\Zürich or Zurich (ˈzjʊərɪk,\\Code\\resources\\app\\plugins",
            "c:/Source/Zürich or Zurich (ˈzjʊərɪk,/Code/resources/app/plugins"));
    test("file://monacotools/folder/isi.txt", IF_WINDOWS("\\\\monacotools\\folder\\isi.txt", "//monacotools/folder/isi.txt"));
    test("file://monacotools1/certificates/SSL/", IF_WINDOWS("\\\\monacotools1\\certificates\\SSL\\", "//monacotools1/certificates/SSL/"));
}

TEST_CASE("URI - http, query & toString'")
{
    auto uri = Uri::parse("https://go.microsoft.com/fwlink/?LinkId=518008");
    CHECK_EQ(uri.query, "LinkId=518008");
    CHECK_EQ(uri.toString(true), "https://go.microsoft.com/fwlink/?LinkId=518008");
    CHECK_EQ(uri.toString(), "https://go.microsoft.com/fwlink/?LinkId%3D518008");

    auto uri2 = Uri::parse(uri.toString());
    CHECK_EQ(uri2.query, "LinkId=518008");
    CHECK_EQ(uri2.query, uri.query);

    uri = Uri::parse("https://go.microsoft.com/fwlink/?LinkId=518008&foö&ké¥=üü");
    CHECK_EQ(uri.query, "LinkId=518008&foö&ké¥=üü");
    CHECK_EQ(uri.toString(true), "https://go.microsoft.com/fwlink/?LinkId=518008&foö&ké¥=üü");
    CHECK_EQ(uri.toString(), "https://go.microsoft.com/fwlink/?LinkId%3D518008%26fo%C3%B6%26k%C3%A9%C2%A5%3D%C3%BC%C3%BC");

    uri2 = Uri::parse(uri.toString());
    CHECK_EQ(uri2.query, "LinkId=518008&foö&ké¥=üü");
    CHECK_EQ(uri2.query, uri.query);

    // #24849
    uri = Uri::parse("https://twitter.com/search?src=typd&q=%23tag");
    CHECK_EQ(uri.toString(true), "https://twitter.com/search?src=typd&q=%23tag");
}

TEST_CASE("class URI cannot represent relative file paths #34449'")
{
    auto path = "/foo/bar";
    CHECK_EQ(Uri::file(path).path, path);
    path = "foo/bar";
    CHECK_EQ(Uri::file(path).path, "/foo/bar");
    path = "./foo/bar";
    CHECK_EQ(Uri::file(path).path, "/./foo/bar"); // missing normalization

    auto fileUri1 = Uri::parse("file:foo/bar");
    CHECK_EQ(fileUri1.path, "/foo/bar");
    CHECK_EQ(fileUri1.authority, "");
    auto uri = fileUri1.toString();
    CHECK_EQ(uri, "file:///foo/bar");
    auto fileUri2 = Uri::parse(uri);
    CHECK_EQ(fileUri2.path, "/foo/bar");
    CHECK_EQ(fileUri2.authority, "");
}

TEST_CASE("Ctrl click to follow hash query param url gets urlencoded #49628'")
{
    auto input = "http://localhost:3000/#/foo?bar=baz";
    auto uri = Uri::parse(input);
    CHECK_EQ(uri.toString(true), input);

    input = "http://localhost:3000/foo?bar=baz";
    uri = Uri::parse(input);
    CHECK_EQ(uri.toString(true), input);
}

TEST_CASE("Unable to open \'%A0.txt\': URI malformed #76506'")
{
    auto uri = Uri::file("/foo/%A0.txt");
    auto uri2 = Uri::parse(uri.toString());
    CHECK_EQ(uri.scheme, uri2.scheme);
    CHECK_EQ(uri.path, uri2.path);

    uri = Uri::file("/foo/%2e.txt");
    uri2 = Uri::parse(uri.toString());
    CHECK_EQ(uri.scheme, uri2.scheme);
    CHECK_EQ(uri.path, uri2.path);
}

// TODO: DEVIATION - the following test cases are broken
// They could be resolved, however for our current use cases it doesn't really matter

// TEST_CASE("Unable to open \'%A0.txt\': URI malformed #76506'")
//{
//     CHECK_EQ(Uri::parse("file://some/%.txt").toString(), "file://some/%25.txt");
//     CHECK_EQ(Uri::parse("file://some/%A0.txt").toString(), "file://some/%25A0.txt");
// };
//
// TEST_CASE("Links in markdown are broken if url contains encoded parameters #79474'")
//{
//     std::string strIn = "https://myhost.com/Redirect?url=http%3A%2F%2Fwww.bing.com%3Fsearch%3Dtom";
//     auto uri1 = Uri::parse(strIn);
//     auto strOut = uri1.toString();
//     auto uri2 = Uri::parse(strOut);
//
//     CHECK_EQ(uri1.scheme, uri2.scheme);
//     CHECK_EQ(uri1.authority, uri2.authority);
//     CHECK_EQ(uri1.path, uri2.path);
//     CHECK_EQ(uri1.query, uri2.query);
//     CHECK_EQ(uri1.fragment, uri2.fragment);
//     CHECK_EQ(strIn, strOut);
// };
//
// TEST_CASE("Uri#parse can break path-component #45515'")
//{
//     std::string strIn = "https://firebasestorage.googleapis.com/v0/b/brewlangerie.appspot.com/o/"
//                         "products%2FzVNZkudXJyq8bPGTXUxx%2FBetterave-Sesame.jpg?alt=media&token=0b2310c4-3ea6-4207-bbde-9c3710ba0437";
//     auto uri1 = Uri::parse(strIn);
//     auto strOut = uri1.toString();
//     auto uri2 = Uri::parse(strOut);
//
//     CHECK_EQ(uri1.scheme, uri2.scheme);
//     CHECK_EQ(uri1.authority, uri2.authority);
//     CHECK_EQ(uri1.path, uri2.path);
//     CHECK_EQ(uri1.query, uri2.query);
//     CHECK_EQ(uri1.fragment, uri2.fragment);
//     CHECK_EQ(strIn, strOut);
// };

TEST_CASE("URI - (de)serialize'")
{
    auto values = {
        Uri::parse("http://localhost:8080/far"),
        Uri::file("c:\\test with %25\\c#code"),
        Uri::file("\\\\shäres\\path\\c#\\plugin.json"),
        Uri::parse("http://api/files/test.me?t=1234"),
        Uri::parse("http://api/files/test.me?t=1234#fff"),
        Uri::parse("http://api/files/test.me#fff"),
    };

    for (auto& value : values)
    {
        json data = value;
        Uri clone = data;

        CHECK_EQ(clone.scheme, value.scheme);
        CHECK_EQ(clone.authority, value.authority);
        CHECK_EQ(clone.path, value.path);
        CHECK_EQ(clone.query, value.query);
        CHECK_EQ(clone.fragment, value.fragment);
        CHECK_EQ(clone.fsPath(), value.fsPath());
        CHECK_EQ(clone.toString(), value.toString());
    }
}

TEST_CASE("luau-lsp custom: encodeURIComponent #555")
{
    auto uri = Uri::file(IF_WINDOWS("c:\\Users\\leoni\\OneDrive\\Рабочий стол\\Creations\\RobloxProjects\\Nelsk",
        "/home/leoni/OneDrive/Рабочий стол/Creations/RobloxProjects/Nelsk"));
    CHECK_EQ(uri.toString(),
        IF_WINDOWS(
            "file:///c%3A/Users/leoni/OneDrive/%D0%A0%D0%B0%D0%B1%D0%BE%D1%87%D0%B8%D0%B9%20%D1%81%D1%82%D0%BE%D0%BB/Creations/RobloxProjects/Nelsk",
            "file:///home/leoni/OneDrive/%D0%A0%D0%B0%D0%B1%D0%BE%D1%87%D0%B8%D0%B9%20%D1%81%D1%82%D0%BE%D0%BB/Creations/RobloxProjects/Nelsk"));
}

TEST_CASE("luau-lsp custom: two file paths are equal on case-insensitive file systems")
{
    auto uri = Uri::file(IF_WINDOWS("c:\\Users\\testing", "/home/testing"));
    auto uri2 = Uri::file(IF_WINDOWS("C:\\USERS\\TESTING", "/HOME/TESTING"));

#if defined(_WIN32) || defined(__APPLE__)
    CHECK(uri == uri2);
    CHECK(UriHash()(uri) == UriHash()(uri2));
#else
    CHECK(uri != uri2);
    CHECK(UriHash()(uri) != UriHash()(uri2));
#endif
}

TEST_CASE("luau-lsp custom: lexicallyRelative")
{
    // NOTE: We will only ever deal with absolute URIs
    CHECK_EQ(Uri::file("/a/d").lexicallyRelative(Uri::file("/a/b/c")), "../../d");
    CHECK_EQ(Uri::file("/a/b/c").lexicallyRelative(Uri::file("/a/d")), "../b/c");
    CHECK_EQ(Uri::file("/a/b/c/").lexicallyRelative(Uri::file("/a")), "b/c");
    CHECK_EQ(Uri::file("/a/b/c/").lexicallyRelative(Uri::file("/a/b/c/x/y")), "../..");
    CHECK_EQ(Uri::file("/a/b/c/").lexicallyRelative(Uri::file("/a/b/c")), ".");
    CHECK_EQ(Uri::file("/a/b").lexicallyRelative(Uri::file("/c/d")), "../../a/b");

#ifdef _WIN32
    CHECK_EQ(Uri::file("C:/project/file").lexicallyRelative(Uri::file("c:/project")), "file");
#endif
}

TEST_CASE("Uri::extension")
{
    CHECK_EQ(Uri::parse("file://a/b/init.lua").extension(), ".lua");
    CHECK_EQ(Uri::parse("file://a/b/init.luau").extension(), ".luau");
    CHECK_EQ(Uri::parse("file://a/b/init.server.luau").extension(), ".luau");
    CHECK_EQ(Uri::parse("file://a/b/init.server").extension(), ".server");
    CHECK_EQ(Uri::parse("file://a/b/init").extension(), "");
    CHECK_EQ(Uri::parse("file://a/b/").extension(), "");
    CHECK_EQ(Uri::parse("file://a/b//").extension(), "");
}

TEST_CASE("Uri::resolvePath")
{
    CHECK_EQ(Uri::parse("foo://a/foo/bar").resolvePath("x").toString(), "foo://a/foo/bar/x");
    CHECK_EQ(Uri::parse("foo://a/foo/bar/").resolvePath("x").toString(), "foo://a/foo/bar/x");
    CHECK_EQ(Uri::parse("foo://a/foo/bar/").resolvePath("/x").toString(), "foo://a/x");
    CHECK_EQ(Uri::parse("foo://a/foo/bar/").resolvePath("x/").toString(), "foo://a/foo/bar/x");

    CHECK_EQ(Uri::parse("foo://a").resolvePath("x/").toString(), "foo://a/x");
    CHECK_EQ(Uri::parse("foo://a").resolvePath("/x/").toString(), "foo://a/x");

    CHECK_EQ(Uri::parse("foo://a/b").resolvePath("/x/..//y/.").toString(), "foo://a/y");
    CHECK_EQ(Uri::parse("foo://a/b").resolvePath("x/..//y/.").toString(), "foo://a/b/y");
    CHECK_EQ(Uri::parse("untitled:untitled-1").resolvePath("../foo").toString(), "untitled:foo");
    CHECK_EQ(Uri::parse("untitled:").resolvePath("foo").toString(), "untitled:foo");
    CHECK_EQ(Uri::parse("untitled:").resolvePath("..").toString(), "untitled:");
    // TODO: LSP Deviation
    //    CHECK_EQ(Uri::parse("untitled:").resolvePath("/foo").toString(), "untitled:foo");
    CHECK_EQ(Uri::parse("untitled:/").resolvePath("/foo").toString(), "untitled:/foo");
}

TEST_CASE("Uri::isDirectory handles filesystem errors")
{
    CHECK_FALSE(Uri::file(IF_WINDOWS("c:\\Users\\con", "/home/con")).isDirectory());
}

TEST_CASE("Uri::exists handles filesystem errors")
{
    CHECK_NOTHROW(Uri::file(IF_WINDOWS("c:\\Users\\con", "/home/con")).exists());
}

TEST_SUITE_END();
