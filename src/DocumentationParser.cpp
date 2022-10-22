#include "LSP/DocumentationParser.hpp"
#include "LSP/Workspace.hpp"
#include <regex>
#include <algorithm>

Luau::FunctionParameterDocumentation parseDocumentationParameter(const json& j)
{
    std::string name;
    std::string documentation;
    if (j.contains("name"))
        j.at("name").get_to(name);
    if (j.contains("documentation"))
        j.at("documentation").get_to(documentation);
    return Luau::FunctionParameterDocumentation{name, documentation};
}

/// Converts an HTML string provided as an input to markdown
/// TODO: currently, this just strips tags, rather than do anything special
std::string convertHtmlToMarkdown(const std::string& input)
{
    // TODO - Tags to support:
    // <code></code> - as well as <code class="language-lua">
    // <pre></pre>
    // <em></em> / <i></i>
    // <ul><li></li></ul>
    // <ol><li></li></ol>
    // <a href=""></a>
    // <strong></strong> / <b></b>
    // <img alt="" src="" />
    // Also need to unescape HTML characters


    // Yes, regex is bad, but I really cannot be bothered right now
    std::regex strip("<[^>]*>");
    return std::regex_replace(input, strip, "");
}


void parseDocumentation(std::optional<std::filesystem::path> documentationFile, Luau::DocumentationDatabase& database, std::shared_ptr<Client> client)
{
    if (!documentationFile)
    {
        client->sendLogMessage(lsp::MessageType::Warning, "No documentation file given. Documentation will not be provided");
        return;
    };

    if (auto contents = readFile(*documentationFile))
    {
        try
        {
            auto docs = json::parse(*contents);
            for (auto& [symbol, info] : docs.items())
            {
                std::string documentation;
                std::string learnMoreLink;
                std::string codeSample;
                if (info.contains("documentation"))
                    info.at("documentation").get_to(documentation);
                if (info.contains("learn_more_link"))
                    info.at("learn_more_link").get_to(learnMoreLink);
                if (info.contains("code_sample"))
                    info.at("code_sample").get_to(codeSample);

                documentation = convertHtmlToMarkdown(documentation);

                if (info.contains("keys"))
                {
                    Luau::DenseHashMap<std::string, Luau::DocumentationSymbol> keys{""};
                    for (auto& [k, v] : info.at("keys").items())
                    {
                        keys[k] = v;
                    }
                    database[symbol] = Luau::TableDocumentation{documentation, keys, learnMoreLink, codeSample};
                }
                else if (info.contains("overloads"))
                {
                    Luau::DenseHashMap<std::string, Luau::DocumentationSymbol> overloads{""};
                    for (auto& [sig, sym] : info.at("overloads").items())
                    {
                        overloads[sig] = sym;
                    }
                    database[symbol] = Luau::OverloadedFunctionDocumentation{overloads};
                }
                else if (info.contains("params") || info.contains("returns"))
                {
                    std::vector<Luau::FunctionParameterDocumentation> parameters;
                    std::vector<std::string> returns;
                    for (auto& param : info.at("params"))
                    {
                        parameters.push_back(parseDocumentationParameter(param));
                    }
                    if (info.contains("returns"))
                        info.at("returns").get_to(returns);
                    database[symbol] = Luau::FunctionDocumentation{documentation, parameters, returns, learnMoreLink, codeSample};
                }
                else
                {
                    database[symbol] = Luau::BasicDocumentation{documentation, learnMoreLink, codeSample};
                }
            }
        }
        catch (const std::exception& e)
        {
            client->sendLogMessage(lsp::MessageType::Error, "Failed to load documentation database: " + std::string(e.what()));
            client->sendWindowMessage(lsp::MessageType::Error, "Failed to load documentation database: " + std::string(e.what()));
        }
    }
    else
    {
        client->sendLogMessage(lsp::MessageType::Error, "Failed to read documentation file. Documentation will not be provided");
        client->sendWindowMessage(lsp::MessageType::Error, "Failed to read documentation file. Documentation will not be provided");
    }
}

/// Returns a markdown string of the provided documentation
std::string printDocumentation(const Luau::DocumentationDatabase& database, const Luau::DocumentationSymbol& symbol)
{
    if (auto documentation = database.find(symbol))
    {
        std::string result = symbol;
        if (auto* basic = documentation->get_if<Luau::BasicDocumentation>())
        {
            result = basic->documentation;
            if (!basic->learnMoreLink.empty())
                result += "\n\n[Learn More](" + basic->learnMoreLink + ")";
            if (!basic->codeSample.empty())
                result += "\n\n" + codeBlock("lua", basic->codeSample);
        }
        else if (auto* func = documentation->get_if<Luau::FunctionDocumentation>())
        {
            result = func->documentation;
            if (!func->learnMoreLink.empty())
                result += "\n\n[Learn More](" + func->learnMoreLink + ")";
            if (!func->codeSample.empty())
                result += "\n\n" + codeBlock("lua", func->codeSample);
        }
        else if (auto* overloaded = documentation->get_if<Luau::OverloadedFunctionDocumentation>())
        {
            if (overloaded->overloads.size() > 0)
            {
                // Use the first overload
                result = printDocumentation(database, overloaded->overloads.begin()->second);

                auto remainingOverloads = overloaded->overloads.size() - 1;
                result += "\n\n*+" + std::to_string(remainingOverloads) + " overload" + (remainingOverloads == 1 ? "*" : "s*");
            }
        }
        else if (auto* tbl = documentation->get_if<Luau::TableDocumentation>())
        {
            result = tbl->documentation;
            if (!tbl->learnMoreLink.empty())
                result += "\n\n[Learn More](" + tbl->learnMoreLink + ")";
            if (!tbl->codeSample.empty())
                result += "\n\n" + codeBlock("lua", tbl->codeSample);
        }
        return result;
    }
    return symbol;
}

std::string printMoonwaveDocumentation(const std::vector<std::string>& comments)
{
    if (comments.empty())
        return "";

    std::string result;
    std::vector<std::string> params;
    std::vector<std::string> returns;
    std::vector<std::string> throws;

    for (auto& comment : comments)
    {
        if (Luau::startsWith(comment, "@param "))
            params.emplace_back(comment);
        else if (Luau::startsWith(comment, "@return "))
            returns.emplace_back(comment);
        else if (Luau::startsWith(comment, "@error "))
            throws.emplace_back(comment);
        else if (comment == "@yields" || comment == "@unreleased")
            // Boldify
            result += "**" + comment + "**\n";
        else if (Luau::startsWith(comment, "@tag ") || Luau::startsWith(comment, "@within "))
            // Ignore
            continue;
        else
            result += comment + "\n";
    }

    if (!params.empty())
    {
        result += "\n\n**Parameters**";
        for (auto& param : params)
        {
            auto paramText = param.substr(7);

            // Parse name
            auto paramName = paramText;
            if (auto space = paramText.find(" "); space != std::string::npos)
            {
                paramName = paramText.substr(0, space);
                paramText = paramText.substr(space);
            }

            if (paramText == paramName)
                result += "\n- `" + paramName + "`";
            else
                result += "\n- `" + paramName + "`" + paramText;
        }
    }

    if (!returns.empty())
    {
        result += "\n\n**Returns**";
        for (auto& ret : returns)
        {
            auto returnText = ret.substr(8);

            // Parse return type
            auto retType = returnText;
            if (auto delim = returnText.find(" --"); delim != std::string::npos)
            {
                retType = returnText.substr(0, delim);
                returnText = returnText.substr(delim);
            }

            if (!retType.empty() && retType != returnText)
                result += "\n- `" + retType + "`" + returnText;
            else
                result += "\n- " + returnText;
        }
    }

    if (!throws.empty())
    {
        result += "\n\n**Throws**";
        for (auto& thr : throws)
        {
            auto throwText = thr.substr(7);

            // Parse throw type
            auto throwType = throwText;
            if (auto delim = throwText.find(" --"); delim != std::string::npos)
            {
                throwType = throwText.substr(0, delim);
                throwText = throwText.substr(delim);
            }

            if (!throwType.empty() && throwType != throwText)
                result += "\n- `" + throwType + "`" + throwText;
            else
                result += "\n- " + throwText;
        }
    }

    return result;
}

struct AttachCommentsVisitor : public Luau::AstVisitor
{
    Luau::Position pos;
    std::vector<Luau::Comment> moduleComments; // A list of all comments in the module
    Luau::Position closestPreviousNode{0, 0};

    explicit AttachCommentsVisitor(const Luau::Location node, const std::vector<Luau::Comment> moduleComments)
        : pos(node.begin)
        , moduleComments(moduleComments)
    {
    }

    std::vector<Luau::Comment> attachComments()
    {
        std::vector<Luau::Comment> result;
        for (auto& comment : moduleComments)
            // The comment needs to be present before the node for it to be attached
            if (comment.location.begin <= pos)
                // They should be after the closest previous node
                if (comment.location.begin >= closestPreviousNode)
                    result.emplace_back(comment);
        return result;
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        for (Luau::AstStat* stat : block->body)
        {
            if (stat->location.begin >= pos)
                continue;
            if (stat->location.begin > closestPreviousNode)
                closestPreviousNode = stat->location.begin;
            stat->visit(this);
            if (stat->location.end <= pos && stat->location.end > closestPreviousNode)
                closestPreviousNode = stat->location.end;
        }

        return false;
    }
};

std::vector<Luau::Comment> getCommentLocations(const Luau::SourceModule* module, const Luau::Location& node)
{
    if (!module)
        return {};

    AttachCommentsVisitor visitor{node, module->commentLocations};
    visitor.visit(module->root);
    return visitor.attachComments();
}

/// Get all moonwave-style documentation comments
/// Performs transformations so that the comments are normalised to lines inside of it (i.e., trimming whitespace, removing comment start/end)
std::vector<std::string> WorkspaceFolder::getComments(const Luau::ModuleName& moduleName, const Luau::Location& node)
{
    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return {};

    auto commentLocations = getCommentLocations(sourceModule, node);
    if (commentLocations.empty())
        return {};

    // Get relevant text document
    auto textDocument = fileResolver.getTextDocument(moduleName);
    bool tempDocument = false;
    if (!textDocument)
    {
        // Open a temporary text document so we can perform operations on it
        if (auto source = fileResolver.readSource(moduleName))
        {
            tempDocument = true;
            textDocument = new TextDocument{Uri(), "luau", 0, source->source};
        }
        else
            return {};
    }

    std::vector<std::string> comments;
    for (auto& comment : commentLocations)
    {
        if (comment.type == Luau::Lexeme::Type::BrokenComment)
            continue;

        auto commentText = textDocument->getText(
            lsp::Range{textDocument->convertPosition(comment.location.begin), textDocument->convertPosition(comment.location.end)});

        // Trim whitespace
        trim(commentText);

        // Parse the comment text for information
        if (comment.type == Luau::Lexeme::Type::Comment)
        {
            if (Luau::startsWith(commentText, "--- "))
            {
                comments.emplace_back(commentText.substr(4));
            }
        }
        else if (comment.type == Luau::Lexeme::Type::BlockComment)
        {
            if (Luau::startsWith(commentText, "--[=["))
            {
                // Parse each line separately
                for (auto& line : Luau::split(commentText, '\n'))
                {
                    auto lineText = std::string(line);
                    trim_end(lineText);

                    auto trimmedLineText = std::string(line);
                    trim(trimmedLineText);
                    if (trimmedLineText == "--[=[" || trimmedLineText == "]=]")
                        continue;
                    comments.emplace_back(lineText);
                }

                // Trim common indentation, but ignore empty lines
                size_t indentLevel = std::string::npos;
                for (auto& line : comments)
                {
                    if (line == "")
                        continue;
                    indentLevel = std::min(indentLevel, line.find_first_not_of(" \n\r\t"));
                }
                for (auto& line : comments)
                {
                    if (line == "")
                        continue;
                    line.erase(0, indentLevel);
                }
            }
        }
    }

    if (tempDocument)
        delete textDocument;

    return comments;
}