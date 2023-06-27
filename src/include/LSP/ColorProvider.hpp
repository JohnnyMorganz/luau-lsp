#include <string>

// RGB are of values [0, 255]
struct RGB
{
    int r = 0;
    int g = 0;
    int b = 0;
};

// HSV are of values [0, 1]
struct HSV
{
    double h = 0;
    double s = 0;
    double v = 0;
};

RGB hsvToRgb(HSV in);
HSV rgbToHsv(RGB in);
RGB hexToRgb(std::string hex);
std::string rgbToHex(RGB in);