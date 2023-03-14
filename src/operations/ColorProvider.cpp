#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"

#include "Luau/Transpiler.h"
#include "Protocol/LanguageFeatures.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/Utils.hpp"

#include <cmath>

// https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
struct RGB
{
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
};

struct HSV
{
    double h = 0;
    double s = 0;
    double v = 0;
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

    auto i = (size_t)std::floor(h);
    double f = h - i;
    double p = v * (1.0 - s);
    double q = v * (1.0 - f * s);
    double t = v * (1.0 - (1.0 - f) * s);

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

    return {(unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255)};
}

static HSV rgbToHsv(RGB in)
{
    HSV out;
    double min, max, delta;

    min = in.r < in.g ? in.r : in.g;
    min = min < in.b ? min : in.b;

    max = in.r > in.g ? in.r : in.g;
    max = max > in.b ? max : in.b;

    out.v = max; // v
    delta = max - min;
    if (delta < 0.00001)
    {
        out.s = 0;
        out.h = 0; // undefined, maybe nan?
        return out;
    }
    if (max > 0.0)
    {                          // NOTE: if Max is == 0, this divide would cause a crash
        out.s = (delta / max); // s
    }
    else
    {
        // if max is 0, then r = g = b = 0
        // s = 0, h is undefined
        out.s = 0.0;
        out.h = NAN; // its now undefined
        return out;
    }
    if (in.r >= max)                   // > is bogus, just keeps compilor happy
        out.h = (in.g - in.b) / delta; // between yellow & magenta
    else if (in.g >= max)
        out.h = 2.0 + (in.b - in.r) / delta; // between cyan & yellow
    else
        out.h = 4.0 + (in.r - in.g) / delta; // between magenta & cyan

    out.h *= 60.0; // degrees

    if (out.h < 0.0)
        out.h += 360.0;

    return out;
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
    // Create color presentations
    lsp::ColorPresentationResult presentations;

    // Add Color3.new
    presentations.emplace_back(lsp::ColorPresentation{"Color3.new(" + std::to_string(params.color.red) + ", " + std::to_string(params.color.green) +
                                                      ", " + std::to_string(params.color.blue) + ")"});

    // Convert to RGB values
    RGB rgb{(unsigned char)std::floor(params.color.red * 255), (unsigned char)std::floor(params.color.green * 255),
        (unsigned char)std::floor(params.color.blue * 255)};

    // Add Color3.fromRGB
    presentations.emplace_back(
        lsp::ColorPresentation{"Color3.fromRGB(" + std::to_string(rgb.r) + ", " + std::to_string(rgb.g) + ", " + std::to_string(rgb.b) + ")"});

    // Add Color3.fromHSV
    HSV hsv = rgbToHsv(rgb);
    presentations.emplace_back(
        lsp::ColorPresentation{"Color3.fromHSV(" + std::to_string(hsv.h) + ", " + std::to_string(hsv.s) + ", " + std::to_string(hsv.v) + ")"});

    // Add Color3.fromHex
    std::stringstream hexString;
    hexString << "#";
    hexString << std::hex << (rgb.r << 16 | rgb.g << 8 | rgb.b);
    presentations.emplace_back(lsp::ColorPresentation{"Color3.fromHex(\"" + hexString.str() + "\")"});

    return presentations;
}

lsp::ColorPresentationResult LanguageServer::colorPresentation(const lsp::ColorPresentationParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->colorPresentation(params);
}