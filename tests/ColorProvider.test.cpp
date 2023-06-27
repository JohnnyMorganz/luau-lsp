#include "doctest.h"
#include "LSP/ColorProvider.hpp"

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
};

TEST_CASE("HSV to RGB conversions")
{
    checkRGB(hsvToRgb({0, 0, 0}), {0, 0, 0});
    checkRGB(hsvToRgb({0, 0, 1}), {255, 255, 255});
};

TEST_CASE("Hex to RGB conversions")
{
    checkRGB(hexToRgb("#000000"), {0, 0, 0});
    checkRGB(hexToRgb("#ffffff"), {255, 255, 255});
};

TEST_CASE("RGB to Hex conversions")
{
    CHECK_EQ(rgbToHex({0, 0, 0}), "#000000");
    CHECK_EQ(rgbToHex({255, 255, 255}), "#ffffff");
};

TEST_SUITE_END();