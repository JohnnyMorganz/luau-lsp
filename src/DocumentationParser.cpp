#include "LSP/DocumentationParser.hpp"
#include <regex>

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

struct AttachCommentsVisitor : public Luau::AstVisitor
{
    Luau::Position pos;
    std::vector<Luau::Comment> moduleComments; // A list of all comments in the module
    Luau::AstStat* closestPreviousNode = nullptr;

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
                if (!closestPreviousNode || comment.location.begin >= closestPreviousNode->location.end)
                    result.emplace_back(comment);
        return result;
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        for (Luau::AstStat* stat : block->body)
        {
            if (stat->location.end > pos)
                continue;
            if (closestPreviousNode)
            {
                if (stat->location.end <= pos && stat->location.end > closestPreviousNode->location.end)
                    closestPreviousNode = stat;
            }
            else
            {
                closestPreviousNode = stat;
            }
        }

        return true;
    }
};

std::vector<Luau::Comment> getCommentLocations(Luau::SourceModule* module, Luau::Location node)
{
    if (!module)
        return {};

    AttachCommentsVisitor visitor{node, module->commentLocations};
    visitor.visit(module->root);
    return visitor.attachComments();
}