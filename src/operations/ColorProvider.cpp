#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"

#include "Luau/Transpiler.h"
#include "Protocol/LanguageFeatures.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/Utils.hpp"

#include <cmath>

struct RGB
{
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
};

static RGB hsvToRgb(double h, double s, double v)
{
    if (s == 0.0)
        return {0, 0, 0};

    double r = 0, g = 0, b = 0;

    if (h == 360)
        h = 0;
    else
        h = h / 60.0;

    size_t i = (size_t)std::floor(h);
    double f = h - i;
    double p = v * (1.0 - s);
    double q = v * (1.0 - f * s);
    double t = v * (1.0 - (1.0 - f) * s);

    switch (i)
    {
    case 0:
    {
        r, g, b = v, t, p;
        break;
    }
    case 1:
    {
        r, g, b = q, v, p;
        break;
    }
    case 2:
    {
        r, g, b = p, v, t;
        break;
    }
    case 3:
    {
        r, g, b = p, q, v;
        break;
    }
    case 4:
    {
        r, g, b = t, p, v;
        break;
    }
    case 5:
    {
        r, g, b = v, p, q;
        break;
    }
    }

    return {(unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255)};
}

static RGB hexToRgb(std::string hex)
{
    replace(hex, "#", "");
    unsigned int val = std::stoul(hex, nullptr, 16);
    if (val > 16777215)
        throw std::out_of_range("hex string is larger than #ffffff");
    return {(unsigned char)((val >> 16) & 0xFF), (unsigned char)((val >> 8) & 0xFF), (unsigned char)(val & 0xFF)};
}

struct DocumentColorVisitor : public Luau::AstVisitor
{
    const TextDocument* textDocument;
    std::vector<lsp::ColorInformation> colors;

    explicit DocumentColorVisitor(const TextDocument* textDocument)
        : textDocument(textDocument)
    {
    }

    bool visit(Luau::AstExprCall* call) override
    {
        if (auto index = call->func->as<Luau::AstExprIndexName>())
        {
            if (auto global = index->expr->as<Luau::AstExprGlobal>())
            {
                if (global->name == "Color3")
                {
                    if (index->index == "new" || index->index == "fromRGB" || index->index == "fromHSV" || index->index == "fromHex")
                    {
                        double color[3]{0.0, 0.0, 0.0};

                        if (index->index == "new")
                        {
                            size_t index = 0;
                            for (auto arg : call->args)
                            {
                                if (index >= 3)
                                    return true; // Don't create as the colour is not in the right format
                                if (auto number = arg->as<Luau::AstExprConstantNumber>())
                                    color[index] = number->value;
                                else
                                    return true; // Don't create as we can't parse the full colour
                                index++;
                            }
                        }
                        else if (index->index == "fromRGB")
                        {
                            size_t index = 0;
                            for (auto arg : call->args)
                            {
                                if (index >= 3)
                                    return true; // Don't create as the colour is not in the right format
                                if (auto number = arg->as<Luau::AstExprConstantNumber>())
                                    color[index] = number->value / 255.0;
                                else
                                    return true; // Don't create as we can't parse the full colour
                                index++;
                            }
                        }
                        else if (index->index == "fromHSV")
                        {
                            size_t index = 0;
                            for (auto arg : call->args)
                            {
                                if (index >= 3)
                                    return true; // Don't create as the colour is not in the right format
                                if (auto number = arg->as<Luau::AstExprConstantNumber>())
                                    color[index] = number->value;
                                else
                                    return true; // Don't create as we can't parse the full colour
                                index++;
                            }
                            RGB data = hsvToRgb(color[0], color[1], color[2]);
                            color[0] = data.r / 255.0;
                            color[1] = data.g / 255.0;
                            color[2] = data.b / 255.0;
                        }
                        else if (index->index == "fromHex")
                        {
                            if (call->args.size != 1)
                                return true; // Don't create as the colour is not in the right format
                            if (auto string = call->args.data[0]->as<Luau::AstExprConstantString>())
                            {
                                try
                                {
                                    RGB data = hexToRgb(std::string(string->value.data, string->value.size));
                                    color[0] = data.r / 255.0;
                                    color[1] = data.g / 255.0;
                                    color[2] = data.b / 255.0;
                                }
                                catch (std::exception _)
                                {
                                    return true; // Invalid hex string
                                }
                            }
                            else
                                return true; // Don't create as we can't parse the full colour
                        }

                        colors.emplace_back(lsp::ColorInformation{
                            lsp::Range{textDocument->convertPosition(call->location.begin), textDocument->convertPosition(call->location.end)},
                            {color[0], color[1], color[2], 1.0}});
                    }
                }
            }
        }

        return true;
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        for (Luau::AstStat* stat : block->body)
        {
            stat->visit(this);
        }

        return false;
    }
};

lsp::DocumentColorResult WorkspaceFolder::documentColor(const lsp::DocumentColorParams& params)
{
    // Only enabled for Roblox code
    auto config = client->getConfiguration(rootUri);
    if (!config.types.roblox)
        return {};

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(moduleName);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + moduleName);

    // Run the type checker to ensure we are up to date
    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return {};

    DocumentColorVisitor visitor{textDocument};
    visitor.visit(sourceModule->root);
    return visitor.colors;
}

lsp::DocumentColorResult LanguageServer::documentColor(const lsp::DocumentColorParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->documentColor(params);
}

lsp::ColorPresentationResult WorkspaceFolder::colorPresentation(const lsp::ColorPresentationParams& params)
{
    // TODO: for now, we do not bother with color presentation (unsure on use cases)
    // it may be worked on in future
    return {};
}

lsp::ColorPresentationResult LanguageServer::colorPresentation(const lsp::ColorPresentationParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->colorPresentation(params);
}