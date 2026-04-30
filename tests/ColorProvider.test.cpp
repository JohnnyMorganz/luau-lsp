#include "doctest.h"
#include "LSP/ColorProvider.hpp"
#include "Fixture.h"

void checkRGB(const RGB& lhs, const RGB& rhs)
{
    CHECK(lhs.r == doctest::Approx(rhs.r));
    CHECK(lhs.g == doctest::Approx(rhs.g));
    CHECK(lhs.b == doctest::Approx(rhs.b));
}

void checkHSV(const HSV& lhs, const HSV& rhs)
{
    CHECK(lhs.h == doctest::Approx(rhs.h));
    CHECK(lhs.s == doctest::Approx(rhs.s));
    CHECK(lhs.v == doctest::Approx(rhs.v));
}

TEST_SUITE_BEGIN("ColorProvider");

TEST_CASE("RGB to HSV conversions")
{
    checkHSV(rgbToHsv({0, 0, 0}), {0, 0, 0});
    checkHSV(rgbToHsv({255, 255, 255}), {0, 0, 1});
}

TEST_CASE("HSV to RGB conversions")
{
    checkRGB(hsvToRgb({0, 0, 0}), {0, 0, 0});
    checkRGB(hsvToRgb({0, 0, 1}), {255, 255, 255});
}

TEST_CASE("Hex to RGB conversions")
{
    checkRGB(hexToRgb("#000000"), {0, 0, 0});
    checkRGB(hexToRgb("#ffffff"), {255, 255, 255});
}

TEST_CASE("RGB to Hex conversions")
{
    CHECK_EQ(rgbToHex({0, 0, 0}), "#000000");
    CHECK_EQ(rgbToHex({255, 255, 255}), "#ffffff");
}

TEST_CASE_FIXTURE(Fixture, "color_presentation_uses_simplified_numbers")
{
    auto presentation = workspace.colorPresentation(lsp::ColorPresentationParams{{}, lsp::Color{0.0, 0.0, 0.0}});
    REQUIRE_EQ(presentation.size(), 4);
    CHECK_EQ(presentation[0].label, "Color3.new(0, 0, 0)");
    CHECK_EQ(presentation[1].label, "Color3.fromRGB(0, 0, 0)");
    CHECK_EQ(presentation[2].label, "Color3.fromHSV(0, 0, 0)");
    CHECK_EQ(presentation[3].label, "Color3.fromHex(\"#000000\")");

    presentation = workspace.colorPresentation(lsp::ColorPresentationParams{{}, lsp::Color{1.0, 1.0, 1.0}});
    REQUIRE_EQ(presentation.size(), 4);
    CHECK_EQ(presentation[0].label, "Color3.new(1, 1, 1)");
    CHECK_EQ(presentation[1].label, "Color3.fromRGB(255, 255, 255)");
    CHECK_EQ(presentation[2].label, "Color3.fromHSV(0, 0, 1)");
    CHECK_EQ(presentation[3].label, "Color3.fromHex(\"#ffffff\")");

    presentation = workspace.colorPresentation(lsp::ColorPresentationParams{{}, lsp::Color{0.25, 0.25, 0.25}});
    REQUIRE_EQ(presentation.size(), 4);
    CHECK_EQ(presentation[0].label, "Color3.new(0.25, 0.25, 0.25)");
    CHECK_EQ(presentation[1].label, "Color3.fromRGB(63, 63, 63)");
    CHECK_EQ(presentation[2].label, "Color3.fromHSV(0, 0, 0.247059)");
    CHECK_EQ(presentation[3].label, "Color3.fromHex(\"#3f3f3f\")");
}

TEST_SUITE_END();