#include "Platform/RobloxPlatform.hpp"

#include "LSP/ColorProvider.hpp"

static std::string formatDouble(double d)
{
    std::string s = std::to_string(d);

    // Strip trailing decimals that are redundant
    size_t decimalPos = s.find('.');
    if (decimalPos != std::string::npos)
    {
        size_t lastNonZero = std::max(s.find_last_not_of('0'), decimalPos);
        if (lastNonZero != std::string::npos)
            s.erase(lastNonZero + 1);
        if (s.back() == '.')
            s.pop_back();
    }
    return s;
}

struct RobloxColorVisitor : public Luau::AstVisitor
{
    const TextDocument* textDocument;
    std::vector<lsp::ColorInformation> colors{};

    explicit RobloxColorVisitor(const TextDocument* textDocument)
        : textDocument(textDocument)
    {
    }

    bool visit(Luau::AstExprCall* call) override
    {
        auto index = call->func->as<Luau::AstExprIndexName>();
        if (!index)
            return true;

        auto global = index->expr->as<Luau::AstExprGlobal>();
        if (!global || global->name != "Color3")
            return true;

        std::array<double, 3> color = {0.0, 0.0, 0.0};

        if (index->index == "new")
        {
            size_t argIndex = 0;
            for (auto arg : call->args)
            {
                if (argIndex >= 3)
                    return true; // Don't create as the colour is not in the right format
                if (auto number = arg->as<Luau::AstExprConstantNumber>())
                    color.at(argIndex) = number->value;
                else
                    return true; // Don't create as we can't parse the full colour
                argIndex++;
            }
        }
        else if (index->index == "fromRGB")
        {
            size_t argIndex = 0;
            for (auto arg : call->args)
            {
                if (argIndex >= 3)
                    return true; // Don't create as the colour is not in the right format
                if (auto number = arg->as<Luau::AstExprConstantNumber>())
                    color.at(argIndex) = number->value / 255.0;
                else
                    return true; // Don't create as we can't parse the full colour
                argIndex++;
            }
        }
        else if (index->index == "fromHSV")
        {
            size_t argIndex = 0;
            for (auto arg : call->args)
            {
                if (argIndex >= 3)
                    return true; // Don't create as the colour is not in the right format
                if (auto number = arg->as<Luau::AstExprConstantNumber>())
                    color.at(argIndex) = number->value;
                else
                    return true; // Don't create as we can't parse the full colour
                argIndex++;
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
        else
        {
            return true;
        }

        colors.emplace_back(
            lsp::ColorInformation{lsp::Range{textDocument->convertPosition(call->location.begin), textDocument->convertPosition(call->location.end)},
                {std::clamp(color[0], 0.0, 1.0), std::clamp(color[1], 0.0, 1.0), std::clamp(color[2], 0.0, 1.0), 1.0}});

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

lsp::DocumentColorResult RobloxPlatform::documentColor(const TextDocument& textDocument, const Luau::SourceModule& module)
{
    RobloxColorVisitor visitor{&textDocument};
    module.root->visit(&visitor);
    return visitor.colors;
}

lsp::ColorPresentationResult RobloxPlatform::colorPresentation(const lsp::ColorPresentationParams& params)
{
    // Create color presentations
    lsp::ColorPresentationResult presentations;

    // Add Color3.new
    presentations.emplace_back(lsp::ColorPresentation{
        "Color3.new(" + formatDouble(params.color.red) + ", " + formatDouble(params.color.green) + ", " + formatDouble(params.color.blue) + ")"});

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
        lsp::ColorPresentation{"Color3.fromHSV(" + formatDouble(hsv.h) + ", " + formatDouble(hsv.s) + ", " + formatDouble(hsv.v) + ")"});

    // Add Color3.fromHex
    presentations.emplace_back(lsp::ColorPresentation{"Color3.fromHex(\"" + rgbToHex(rgb) + "\")"});

    return presentations;
}
