#include "LSP/ConfigLuau.hpp"

#include "Luau/Ast.h"
#include "Luau/LinterConfig.h"
#include "Luau/LuauConfig.h"
#include "Luau/Parser.h"

#include <sstream>
#include <utility>
#include <vector>

namespace Luau::LanguageServer::ConfigLuau
{

static void appendLintWarningNames(std::ostringstream& ss, const char* prefix, const char* suffix)
{
    for (int code = Luau::LintWarning::Code_UnknownGlobal; code < Luau::LintWarning::Code__Count; ++code)
        ss << prefix << Luau::LintWarning::getName(static_cast<Luau::LintWarning::Code>(code)) << suffix;
}

bool isConfigLuauFile(const Uri& uri)
{
    return uri.filename() == Luau::kLuauConfigName;
}

const std::string& getDefinitions()
{
    static const std::string definitions = []
    {
        std::ostringstream ss;

        ss << "export type LanguageMode = \"strict\" | \"nonstrict\" | \"nocheck\"\n";
        ss << "export type LintWarning =\n";
        appendLintWarningNames(ss, "\t| \"", "\"\n");

        ss << "\n";
        ss << "export type LuauConfig = {\n";
        ss << "\tlanguagemode: LanguageMode?,\n";
        ss << "\tlint: {\n";
        ss << "\t\t[\"*\"]: boolean?,\n";
        appendLintWarningNames(ss, "\t\t", ": boolean?,\n");
        ss << "\t},\n";
        ss << "\tlinterrors: boolean?,\n";
        ss << "\ttypeerrors: boolean?,\n";
        ss << "\tglobals: { string }?,\n";
        ss << "\taliases: { [string]: string }?,\n";
        ss << "}\n";
        ss << "export type Config = {\n";
        ss << "\tluau: LuauConfig?,\n";
        ss << "}\n";
        ss << "declare function ";
        ss << kCheckFunctionName << "(config: Config): Config";
        ss << "\n";

        return ss.str();
    }();

    return definitions;
}

static Luau::Position endPosition(const std::string& source)
{
    Luau::Position result{0, 0};

    for (char ch : source)
    {
        if (ch == '\n')
        {
            ++result.line;
            result.column = 0;
        }
        else
            ++result.column;
    }

    return result;
}

struct ChunkReturnCollector : Luau::AstVisitor
{
    std::vector<const Luau::AstStatReturn*> returns;

    bool visit(Luau::AstExprFunction*) override
    {
        return false;
    }

    bool visit(Luau::AstStatFunction*) override
    {
        return false;
    }

    bool visit(Luau::AstStatLocalFunction*) override
    {
        return false;
    }

    bool visit(Luau::AstStatReturn* node) override
    {
        returns.push_back(node);
        return false;
    }
};

static std::vector<const Luau::AstStatReturn*> getChunkReturns(Luau::AstStatBlock* root)
{
    if (!root)
        return {};

    ChunkReturnCollector collector;
    root->visit(&collector);
    return std::move(collector.returns);
}

static void appendReturnCheckEdits(const Luau::AstStatReturn* ret, std::vector<Plugin::TextEdit>& edits)
{
    if (ret->list.size == 0)
    {
        edits.push_back(Plugin::TextEdit{
            Luau::Location{ret->location.end, ret->location.end},
            std::string(" ") + kCheckFunctionName + "(nil)",
        });
    }
    else
    {
        const Luau::AstExpr* first = ret->list.data[0];
        const Luau::AstExpr* last = ret->list.data[ret->list.size - 1];

        edits.push_back(Plugin::TextEdit{
            Luau::Location{first->location.begin, first->location.begin},
            std::string(kCheckFunctionName) + "(",
        });
        edits.push_back(Plugin::TextEdit{
            Luau::Location{last->location.end, last->location.end},
            ")",
        });
    }
}

std::optional<Plugin::TransformResult> transformSourceForAnalysis(const std::string& source)
{
    Luau::Allocator allocator;
    Luau::AstNameTable names(allocator);
    Luau::ParseResult parseResult = Luau::Parser::parse(source.data(), source.size(), names, allocator);

    std::vector<Plugin::TextEdit> edits;

    std::vector<const Luau::AstStatReturn*> returns = getChunkReturns(parseResult.root);
    for (const Luau::AstStatReturn* ret : returns)
        appendReturnCheckEdits(ret, edits);

    if (returns.empty())
    {
        const Luau::Position eof = endPosition(source);
        const std::string separator = source.empty() || source.back() == '\n' ? "" : "\n";
        edits.push_back(Plugin::TextEdit{
            Luau::Location{eof, eof},
            separator + "return " + kCheckFunctionName + "(nil)",
        });
    }

    if (edits.empty())
        return std::nullopt;

    return Plugin::SourceMapping::fromEdits(source, edits);
}

} // namespace Luau::LanguageServer::ConfigLuau
