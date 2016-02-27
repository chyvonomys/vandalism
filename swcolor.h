const u32 RO = 2;
const u32 GO = 1;
const u32 BO = 0;
const u32 AO = 3;

struct color
{
    u8 r;
    u8 g;
    u8 b;
    u8 a;
    color(u32 c) :
        r(c & 0xFF),
        g((c >> 8) & 0xFF),
        b((c >> 16) & 0xFF),
        a(c >> 24)
    {}
    color(u8 rr, u8 gg, u8 bb, u8 aa)
        : r(rr), g(gg), b(bb), a(aa) {}
};

u32 pack_color(const color &c)
{
    u32 res = static_cast<u32>(c.a);
    res <<= 8;
    res |= static_cast<u32>(c.r);
    res <<= 8;
    res |= static_cast<u32>(c.g);
    res <<= 8;
    res |= static_cast<u32>(c.b);
    return res;
}

color lerp_color(const color &a, const color &b, float t)
{
    return color(
        static_cast<u8>(lerp(a.r, b.r, t)),
        static_cast<u8>(lerp(a.g, b.g, t)),
        static_cast<u8>(lerp(a.b, b.b, t)),
        static_cast<u8>(lerp(a.a, b.a, t)));
}

color modulate(const color &a, float k)
{
    return color(
        static_cast<u8>(a.r * k),
        static_cast<u8>(a.g * k),
        static_cast<u8>(a.b * k),
        a.a);
}

color uint32_to_color(u32 i)
{
    return color(
        static_cast<u8>(127 * ((i >> 0) & 1) + 255 * ((i >> 3) & 1)),
        static_cast<u8>(127 * ((i >> 1) & 1) + 255 * ((i >> 3) & 1)),
        static_cast<u8>(127 * ((i >> 2) & 1) + 255 * ((i >> 3) & 1)),
        static_cast<u8>(255));
}

const color COLOR_BLACK  (0xFF000000);
const color COLOR_RED    (0xFF0000FF);
const color COLOR_CYAN   (0xFFFFFF00);
const color COLOR_YELLOW (0xFF00FFFF);
const color COLOR_MAGENTA(0xFFFF00FF);
const color COLOR_WHITE  (0xFFFFFFFF);
const color COLOR_GRAY   (0xFF444444);
