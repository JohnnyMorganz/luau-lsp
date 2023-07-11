#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/ColorProvider.hpp"

#include "Luau/Transpiler.h"
#include "Protocol/LanguageFeatures.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/Utils.hpp"

#include <cmath>

// https://github.com/EmmanuelOga/columns/blob/master/utils/color.lua
RGB hsvToRgb(HSV in)
{
    auto h = in.h, s = in.s, v = in.v;
    double r = 0.0, g = 0.0, b = 0.0;

    auto i = (int)std::floor(h * 6);
    auto f = h * 6 - i;
    auto p = v * (1 - s);
    auto q = v * (1 - f * s);
    auto t = v * (1 - (1 - f) * s);

    i = i % 6;

    switch (i)
    {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    case 5:
        r = v;
        g = p;
        b = q;
        break;
    }

    return {(int)(r * 255), (int)(g * 255), (int)(b * 255)};
}

HSV rgbToHsv(RGB in)
{
    double r = in.r / 255.0, g = in.g / 255.0, b = in.b / 255.0;
    auto max = std::max(std::max(r, g), b);
    auto min = std::min(std::min(r, g), b);

    double h;
    double s;
    double v = max;

    auto d = max - min;
    if (max == 0)
        s = 0.0;
    else
        s = d / max;

    if (max == min)
        h = 0.0;
    else
    {
        if (max == r)
        {
            h = (g - b) / d;
            if (g < b)
                h += 6.0;
        }
        else if (max == g)
            h = (b - r) / d + 2.0;
        else // if (max == b) - this condition always holds. we comment it out for compiler to be happy
            h = (r - g) / d + 4.0;
        h = h / 6.0;
    }

    return {h, s, v};
}

RGB hexToRgb(std::string hex)
{
    replace(hex, "#", "");
    unsigned int val = std::stoul(hex, nullptr, 16);
    if (val > 16777215)
        throw std::out_of_range("hex string is larger than #ffffff");
    return {(int)((val >> 16) & 0xFF), (int)((val >> 8) & 0xFF), (int)(val & 0xFF)};
}

std::string rgbToHex(RGB in)
{
    std::stringstream hexString;
    hexString << "#";
    hexString << std::setfill('0') << std::setw(6) << std::hex << (in.r << 16 | in.g << 8 | in.b);
    return hexString.str();
}

struct DocumentColorVisitor : public Luau::AstVisitor
{
    const TextDocument* textDocument;
    std::vector<lsp::ColorInformation> colors{};

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
                        std::array<double, 3> color = {0.0, 0.0, 0.0};

                        if (index->index == "new")
                        {
                            size_t index = 0;
                            for (auto arg : call->args)
                            {
                                if (index >= 3)
                                    return true; // Don't create as the colour is not in the right format
                                if (auto number = arg->as<Luau::AstExprConstantNumber>())
                                    color.at(index) = number->value;
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
                                    color.at(index) = number->value / 255.0;
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
                                    color.at(index) = number->value;
                                else
                                    return true; // Don't create as we can't parse the full colour
                                index++;
                            }
                            RGB data = hsvToRgb({color[0], color[1], color[2]});
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
                                catch (const std::exception&)
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
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    frontend.parse(moduleName);

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
    // Create color presentations
    lsp::ColorPresentationResult presentations;

    // Add Color3.new
    presentations.emplace_back(lsp::ColorPresentation{"Color3.new(" + std::to_string(params.color.red) + ", " + std::to_string(params.color.green) +
                                                      ", " + std::to_string(params.color.blue) + ")"});

    // Convert to RGB values
    RGB rgb{
        (int)std::floor(params.color.red * 255.0),
        (int)std::floor(params.color.green * 255.0),
        (int)std::floor(params.color.blue * 255.0),
    };

    // Add Color3.fromRGB
    presentations.emplace_back(
        lsp::ColorPresentation{"Color3.fromRGB(" + std::to_string(rgb.r) + ", " + std::to_string(rgb.g) + ", " + std::to_string(rgb.b) + ")"});

    // Add Color3.fromHSV
    HSV hsv = rgbToHsv(rgb);
    presentations.emplace_back(
        lsp::ColorPresentation{"Color3.fromHSV(" + std::to_string(hsv.h) + ", " + std::to_string(hsv.s) + ", " + std::to_string(hsv.v) + ")"});

    // Add Color3.fromHex
    presentations.emplace_back(lsp::ColorPresentation{"Color3.fromHex(\"" + rgbToHex(rgb) + "\")"});

    return presentations;
}

lsp::ColorPresentationResult LanguageServer::colorPresentation(const lsp::ColorPresentationParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->colorPresentation(params);
}