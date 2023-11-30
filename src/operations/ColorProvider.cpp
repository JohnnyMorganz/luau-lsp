#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/ColorProvider.hpp"

#include "Platform/LSPPlatform.hpp"
#include "Protocol/LanguageFeatures.hpp"

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
    default:
        LUAU_ASSERT(false);
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
    LSPPlatform* platform;
    std::vector<lsp::ColorInformation> colors{};

    explicit DocumentColorVisitor(const TextDocument* textDocument, LSPPlatform* platform)
        : textDocument(textDocument)
        , platform(platform)
    {
    }

    bool visit(Luau::AstExprCall* call) override
    {
        if (auto colorInfo = platform->colorInformation(call, textDocument))
            colors.emplace_back(colorInfo.value());

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
    if (!platform)
        return {};

    auto config = client->getConfiguration(rootUri);

    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(params.textDocument.uri);
    if (!textDocument)
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + params.textDocument.uri.toString());

    frontend.parse(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    if (!sourceModule)
        return {};

    DocumentColorVisitor visitor{textDocument, platform.get()};
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
    return platform->colorPresentation(params);
}

lsp::ColorPresentationResult LanguageServer::colorPresentation(const lsp::ColorPresentationParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->colorPresentation(params);
}
