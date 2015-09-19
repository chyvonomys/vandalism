const uint32 RO = 2;
const uint32 GO = 1;
const uint32 BO = 0;
const uint32 AO = 3;

struct color
{
    uint8 r;
    uint8 g;
    uint8 b;
    uint8 a;
    color(uint32 c) :
        a(c >> 24),
        b((c >> 16) & 0xFF),
        g((c >> 8) & 0xFF),
        r(c & 0xFF)
    {
        b = (b * a) / 255;
        g = (g * a) / 255;
        r = (r * a) / 255;
        a = 255;
    }
    color(uint8 rr, uint8 gg, uint8 bb, uint8 aa)
        : r(rr), g(gg), b(bb), a(aa) {}
};

uint32 pack_color(const color &c)
{
    return (((((c.a << 8) | c.r) << 8) | c.g) << 8) | c.b;
}

color lerp_color(const color &a, const color &b, float t)
{
    return color(
        static_cast<uint8>(lerp(a.r, b.r, t)),
        static_cast<uint8>(lerp(a.g, b.g, t)),
        static_cast<uint8>(lerp(a.b, b.b, t)),
        static_cast<uint8>(lerp(a.a, b.a, t)));
}

color modulate(const color &a, float k)
{
    return color(
        static_cast<uint8>(a.r * k),
        static_cast<uint8>(a.g * k),
        static_cast<uint8>(a.b * k),
        a.a);
}

color uint32_to_color(uint32 i)
{
    return color(
        static_cast<uint8>(127 * ((i >> 0) & 1) + 255 * ((i >> 3) & 1)),
        static_cast<uint8>(127 * ((i >> 1) & 1) + 255 * ((i >> 3) & 1)),
        static_cast<uint8>(127 * ((i >> 2) & 1) + 255 * ((i >> 3) & 1)),
        static_cast<uint8>(255));
}

const color COLOR_BLACK  (0xFF000000);
const color COLOR_RED    (0xFF0000FF);
const color COLOR_CYAN   (0xFFFFFF00);
const color COLOR_YELLOW (0xFF00FFFF);
const color COLOR_MAGENTA(0xFFFF00FF);
const color COLOR_WHITE  (0xFFFFFFFF);
const color COLOR_GRAY   (0xFF444444);
