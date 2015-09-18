#include "client.h"
#include "math.h"

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

const color COLOR_BLACK  (0xFF000000);
const color COLOR_RED    (0xFF0000FF);
const color COLOR_CYAN   (0xFFFFFF00);
const color COLOR_YELLOW (0xFF00FFFF);
const color COLOR_MAGENTA(0xFFFF00FF);
const color COLOR_WHITE  (0xFFFFFFFF);
const color COLOR_GRAY   (0xFF444444);

inline void draw_pixel(offscreen_buffer *buffer,
                       uint32 x, uint32 y, const color& c)
{
    uint8 *p = buffer->data + 4 * (buffer->width * y + x);
    p[RO] = c.r;
    p[GO] = c.g;
    p[BO] = c.b;
    p[AO] = c.a;
}

void draw_line(offscreen_buffer *pBuffer,
               uint32 x0, uint32 y0,
               uint32 x1, uint32 y1,
               const color &c)
{
    uint32 h = (y1 > y0 ? y1 - y0 : y0 - y1);
    uint32 w = (x1 > x0 ? x1 - x0 : x0 - x1);

    draw_pixel(pBuffer, x0, y0, c);
    draw_pixel(pBuffer, x1, y1, c);

    if (w > h)
    {
        uint32 tmp;
        if (x0 > x1)
        {
            tmp = x0;
            x0 = x1;
            x1 = tmp;

            tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        float k = static_cast<float>(h) / static_cast<float>(w);
        if (y0 > y1) k = -k;

        for (uint32 x = 1; x < w; ++x)
        {
            uint32 y = y0 + static_cast<uint32>(k * x);
            draw_pixel(pBuffer, x + x0, y, c);
        }
    }
    else
    {
        uint32 tmp;
        if (y0 > y1)
        {
            tmp = x0;
            x0 = x1;
            x1 = tmp;

            tmp = y0;
            y0 = y1;
            y1 = tmp;
        }

        float k = static_cast<float>(w) / static_cast<float>(h);
        if (x0 > x1) k = -k;

        for (uint32 y = 1; y < h; ++y)
        {
            uint32 x = x0 + static_cast<uint32>(k * y);
            draw_pixel(pBuffer, x, y + y0, c);
        }
    }
}

void fill_debug_gradient(offscreen_buffer *buffer)
{
    uint8 *ptr = buffer->data;
    uint32 pitch = buffer->width * 4;
    for (uint32 row = 0; row < buffer->height; ++row)
    {
        for (uint32 col = 0; col < buffer->width; ++col)
        {
            ptr[RO] = col % 256;
            ptr[GO] = row % 256;
            ptr[BO] = 0;
            ptr[AO] = 255;
            ptr += 4;
        }
    }
}

void fill_color(offscreen_buffer *buffer,
                uint8 r, uint8 g, uint8 b, uint8 a)
{
    uint8 *ptr = buffer->data;
    uint8 *end  = ptr + buffer->height * buffer->width * 4;
    while (ptr != end)
    {
        ptr[RO] = r;
        ptr[GO] = g;
        ptr[BO] = b;
        ptr[AO] = a;
        ptr += 4;
    }
}


color uint32_to_color(uint32 i)
{
    return color(
        static_cast<uint8>(127 * ((i >> 0) & 1) + 255 * ((i >> 3) & 1)),
        static_cast<uint8>(127 * ((i >> 1) & 1) + 255 * ((i >> 3) & 1)),
        static_cast<uint8>(127 * ((i >> 2) & 1) + 255 * ((i >> 3) & 1)),
        static_cast<uint8>(255));
}

#include "imgui.h"

offscreen_buffer *current_buffer = 0;

void DrawDot(const ImDrawVert &v)
{
    draw_pixel(current_buffer, v.pos.x, v.pos.y, color(v.col));
}

void DrawLine(const ImDrawVert &v0, const ImDrawVert &v1)
{
    if (v0.pos.x >= 0 && v0.pos.y >= 0 &&
        v1.pos.x >= 0 && v1.pos.y >= 0 &&
        v0.pos.x <= current_buffer->width - 1 &&
        v0.pos.y <= current_buffer->height - 1 &&
        v1.pos.x <= current_buffer->width - 1 &&
        v1.pos.y <= current_buffer->height - 1)
    {
        draw_line(current_buffer,
                  v0.pos.x, current_buffer->height - 1 - v0.pos.y,
                  v1.pos.x, current_buffer->height - 1 - v1.pos.y,
                  color(v0.col));
    }
}

void DrawTri(const ImDrawVert &v0,
             const ImDrawVert &v1,
             const ImDrawVert &v2)
{
    //DrawLine(v0, v1);
    //DrawLine(v1, v2);
    //DrawLine(v2, v0);

    float v0x = v0.pos.x;  float v0y = v0.pos.y;
    float v1x = v1.pos.x;  float v1y = v1.pos.y;
    float v2x = v2.pos.x;  float v2y = v2.pos.y;
    
    int32 minx = si_clampf(si_minf(si_minf(v0x, v1x), v2x),
                           0.0f, current_buffer->width-1);
    int32 miny = si_clampf(si_minf(si_minf(v0y, v1y), v2y),
                           0.0f, current_buffer->height-1);

    int32 maxx = si_clampf(si_maxf(si_maxf(v0x, v1x), v2x),
                           0.0f, current_buffer->width-1);
    int32 maxy = si_clampf(si_maxf(si_maxf(v0y, v1y), v2y),
                           0.0f, current_buffer->height-1);

    color col(v0.col);

    float a01 = v0.pos.y - v1.pos.y;
    float a12 = v1.pos.y - v2.pos.y;
    float a20 = v2.pos.y - v0.pos.y;

    float b01 = v1.pos.x - v0.pos.x;
    float b12 = v2.pos.x - v1.pos.x;
    float b20 = v0.pos.x - v2.pos.x;

    float c01 = v0x * v1y - v0y * v1x;
    float c12 = v1x * v2y - v1y * v2x;
    float c20 = v2x * v0y - v2y * v0x;
    
    for (int32 y = miny; y <= maxy; ++y)
    {
        for (int32 x = minx; x <= maxx; ++x)
        {
            float f01 = a01 * x + b01 * y + c01;
            float f12 = a12 * x + b12 * y + c12;
            float f20 = a20 * x + b20 * y + c20;

            if (f01 >= 0 && f12 >= 0 && f20 >= 0)
            {
                draw_pixel(current_buffer,
                           x, current_buffer->height - 1 - y,
                           col);
            }
        }
    }
}


inline float sample_grayscale_texture(const uint8 *pixels,
                                      uint32 w, uint32 h,
                                      float u, float v)
{
    uint32 r = static_cast<uint32>(h * v);
    uint32 c = static_cast<uint32>(w * u);

    uint8 l = pixels[r * w + c];

    return static_cast<float>(l) / 255.0f;
}

inline void draw_solid_hor_line(offscreen_buffer *buffer,
                                uint32 x0, uint32 x1,
                                uint32 y, uint32 col)
{
    uint32 *b = reinterpret_cast<uint32*>(buffer->data +
                                          y * buffer->width * 4 +
                                          x0 * 4);
    uint32 *e = b + (x1 - x0);
    uint32 *p = b;
    while (p != e)
    {
        *p = col;
        ++p;
    }
}

void draw_solid_rect(offscreen_buffer *buffer,
                     uint32 x0, uint32 x1,
                     uint32 y0, uint32 y1,
                     uint32 col)
{
    for (uint32 y = y0; y < y1; ++y)
    {
        draw_solid_hor_line(buffer, x0, x1, y, col);
    }
}

const uint8* fontpixels;
size_t fontpixelswidth;
size_t fontpixelsheight;


void DrawRect(const ImDrawVert &c0, const ImDrawVert &c1)
{
    float xmin = si_minf(c0.pos.x, c1.pos.x); 
    float xmax = si_maxf(c0.pos.x, c1.pos.x); 

    float ymin = si_minf(c0.pos.y, c1.pos.y); 
    float ymax = si_maxf(c0.pos.y, c1.pos.y);

    xmin = si_clampf(xmin, 0.0f, current_buffer->width - 1);
    xmax = si_clampf(xmax, 0.0f, current_buffer->width - 1);

    ymin = si_clampf(ymin, 0.0f, current_buffer->height - 1);
    ymax = si_clampf(ymax, 0.0f, current_buffer->height - 1);

    if (c0.uv.x == c1.uv.x && c0.uv.y == c1.uv.y && c0.col == c1.col)
    {
        float u = c0.uv.x;
        float v = c0.uv.y;
        float lum = sample_grayscale_texture(fontpixels,
                                             fontpixelswidth,
                                             fontpixelsheight,
                                             u, v);
        color col = c0.col;
        col = modulate(col, lum);

        draw_solid_rect(current_buffer,
                        xmin, xmax+1,
                        current_buffer->height - 1 - ymax,
                        current_buffer->height - ymin,
                        pack_color(col));
    }
    else
    for (uint32 r = ymin; r < ymax; ++r)
        for (uint32 c = xmin; c < xmax; ++c)
        {
            float s = invlerp(c0.pos.x, c1.pos.x, c);
            float t = invlerp(c0.pos.y, c1.pos.y, r);

            float u = lerp(c0.uv.x, c1.uv.x, s);
            float v = lerp(c0.uv.y, c1.uv.y, t);
            color col = lerp_color(color(c0.col), color(c1.col), s);

            float lum = sample_grayscale_texture(fontpixels,
                                                 fontpixelswidth,
                                                 fontpixelsheight,
                                                 u, v);
            col = modulate(col, lum);

            draw_pixel(current_buffer, c, current_buffer->height - 1 - r, col);
        }
}

void DrawRectDbg(const ImDrawVert &c0, const ImDrawVert &c1, const color &col)
{
    uint32 x0 = c0.pos.x;
    uint32 x1 = c1.pos.x;
    uint32 y0 = c0.pos.y;
    uint32 y1 = c1.pos.y;
    
    draw_line(current_buffer, x0, y0, x1, y0, col);
    draw_line(current_buffer, x0, y0, x0, y1, col);

    draw_line(current_buffer, x1, y0, x1, y1, col);
    draw_line(current_buffer, x0, y1, x1, y1, col);

    draw_line(current_buffer, x0, y0, x1, y1, col);
}

void DrawTriN(const ImDrawVert &a,
             const ImDrawVert &b,
             const ImDrawVert &c)
{
    float x0 = a.pos.x; float u0 = a.uv.x;
    float y0 = a.pos.y; float v0 = a.uv.y;

    float x1 = b.pos.x; float u1 = b.uv.x;
    float y1 = b.pos.y; float v1 = b.uv.y;

    float x2 = c.pos.x; float u2 = c.uv.x;
    float y2 = c.pos.y; float v2 = c.uv.y;

    float xmin = si_minf(x0, si_minf(x1, x2));
    float xmax = si_maxf(x0, si_maxf(x1, x2));

    float ymin = si_minf(y0, si_minf(y1, y2));
    float ymax = si_maxf(y0, si_maxf(y1, y2));
}

void RenderImGuiDrawLists(ImDrawData *drawData)
{
    for (size_t li = 0; li < drawData->CmdListsCount; ++li)
    {
        const ImDrawList *drawList = drawData->CmdLists[li];

        const ImDrawIdx *indexBuffer = &drawList->IdxBuffer.front();
        const ImDrawVert *vertexBuffer = &drawList->VtxBuffer.front();

        size_t vtxCnt = drawList->VtxBuffer.size();
        size_t idxCnt = drawList->IdxBuffer.size();

        size_t cmdCnt = drawList->CmdBuffer.size();

        size_t idxOffs = 0;

        for (size_t ci = 0; ci < cmdCnt; ++ci)
        {
            const ImDrawCmd &cmd = drawList->CmdBuffer[ci];
            size_t idxCnt = cmd.ElemCount;

            if (cmd.UserCallback)
            {
                cmd.UserCallback(drawList, &cmd);
            }
            else
            {
                const uint8* texId =
                    static_cast<const uint8*>(cmd.TextureId);

                float xmin = cmd.ClipRect.x;
                float ymin = cmd.ClipRect.y;
                float xmax = cmd.ClipRect.z;
                float ymax = cmd.ClipRect.w;

                for (size_t ti = idxOffs; ti < idxOffs + idxCnt;)
                {
                    const ImDrawIdx &i0 = indexBuffer[ti + 0];
                    const ImDrawVert &v0 = vertexBuffer[i0];

                    const ImDrawIdx &i1 = indexBuffer[ti + 1];
                    const ImDrawVert &v1 = vertexBuffer[i1];

                    const ImDrawIdx &i2 = indexBuffer[ti + 2];
                    const ImDrawVert &v2 = vertexBuffer[i2];

                    // this is last, there is no next
                    if (ti + 3 == idxOffs + idxCnt)
                    {
                        DrawTri(v0, v1, v2);
                        ti += 3;
                    }
                    else
                    {
                        const ImDrawIdx &i3 = indexBuffer[ti + 3];
                        const ImDrawVert &v3 = vertexBuffer[i3];

                        const ImDrawIdx &i4 = indexBuffer[ti + 4];
                        const ImDrawVert &v4 = vertexBuffer[i4];

                        const ImDrawIdx &i5 = indexBuffer[ti + 5];
                        const ImDrawVert &v5 = vertexBuffer[i5];

                        // a)  1 -2:4  b) 2:4- 5
                        //     | / |       | \ | 
                        //    0:3- 5       1 -0:3
                        /*
                        if (i0 == i3 && i2 == i4)
                        {
                            // a)
                            if (v5.pos.x == v4.pos.x && v5.pos.y == v3.pos.y &&
                                v1.pos.x == v0.pos.x && v1.pos.y == v2.pos.y &&
                                v5.uv.x == v4.uv.x && v5.uv.y == v3.uv.y &&
                                v1.uv.x == v0.uv.x && v1.uv.y == v2.uv.y)
                            {
                                DrawRectDbg(v1, v5, COLOR_YELLOW);
                                ti += 6;
                                continue;
                            }
                            // b)
                            if (v5.pos.y == v4.pos.y && v5.pos.x == v3.pos.x &&
                                v1.pos.y == v0.pos.y && v1.pos.x == v2.pos.x &&
                                v5.uv.y == v4.uv.y && v5.uv.x == v3.uv.x &&
                                v1.uv.y == v0.uv.y && v1.uv.x == v2.uv.x)
                            {
                                // NOTE: this case is used for text quads
                                DrawRectDbg(v1, v5, COLOR_CYAN);
                                ti += 6;
                                continue;
                            }
                            // non-rectangle
                            DrawRectDbg(v1, v5, COLOR_MAGENTA);
                            ti += 6;
                        }
                        */
                        if (i0 == i3 && i2 == i4 &&
                            v5.pos.y == v4.pos.y && v5.pos.x == v3.pos.x &&
                            v1.pos.y == v0.pos.y && v1.pos.x == v2.pos.x &&
                            v5.uv.y == v4.uv.y && v5.uv.x == v3.uv.x &&
                            v1.uv.y == v0.uv.y && v1.uv.x == v2.uv.x)
                        {
                            DrawRect(v1, v5);
                            ti += 6;
                        }
                        else
                        {
                            DrawTri(v0, v1, v2);
                            ti += 3;
                        }
                    }
                }
            }

            idxOffs += idxCnt;
        }
    }
}

void draw_grayscale_image(offscreen_buffer *buffer, uint32 x0, uint32 y0,
                          const uint8 *pixels, uint32 width, uint32 height)
{
    for (uint32 r = 0; r < height; ++r)
        for (uint32 c = 0; c < width; ++c)
        {
            uint8 a = pixels[width * r + c];
            if (x0 + c < buffer->width && y0 + r < buffer->height)
                draw_pixel(buffer, x0 + c, y0 + r, color(a, a, a, 255));
        }
}

void clear_buffer(offscreen_buffer *buffer, const color &col)
{
    for (uint32 y = 0; y < buffer->height; ++y)
    {
        draw_solid_hor_line(buffer, 0, buffer->width, y, pack_color(col));
    }
}

#include "vandalism.cpp"

Vandalism *ism = nullptr;
bool firstTime;

extern "C" void setup()
{
    ImGuiIO& io = ImGui::GetIO();
    io.RenderDrawListsFn = RenderImGuiDrawLists;

    uint8 *pixels;
    int32 width, height;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

    fontpixels = new uint8[width * height];
    fontpixelswidth = width;
    fontpixelsheight = height;
    ::memcpy((void*)fontpixels, pixels, width * height);

    io.Fonts->TexID = const_cast<uint8*>(fontpixels);

    io.Fonts->ClearInputData();
    io.Fonts->ClearTexData();

    ism = new Vandalism();
    ism->setup();

    firstTime = true;
}

extern "C" void cleanup()
{
    ism->cleanup();
    delete ism;
    delete [] fontpixels;
    ImGui::Shutdown();
}

void fill_triangles(triangles *tris,
                    const std::vector<Vandalism::Point> &points,
                    const Vandalism::Visible &vis)
{
    for (size_t i = vis.startIdx + 2; i < vis.endIdx; ++i)
    {
        if (tris->size < tris->capacity)
        {
            tris->data[3 * (tris->size+0) + 0] = points[i-2].x;
            tris->data[3 * (tris->size+0) + 1] = points[i-2].y;
            tris->data[3 * (tris->size+0) + 2] = 0.00001f * vis.strokeIdx;

            tris->data[3 * (tris->size+1) + 0] = points[i-1].x;
            tris->data[3 * (tris->size+1) + 1] = points[i-1].y;
            tris->data[3 * (tris->size+1) + 2] = 0.00001f * vis.strokeIdx;

            tris->data[3 * (tris->size+2) + 0] = points[i-0].x;
            tris->data[3 * (tris->size+2) + 1] = points[i-0].y;
            tris->data[3 * (tris->size+2) + 2] = 0.00001f * vis.strokeIdx;

            tris->size += 3;
        }
    }
}

extern "C" void update_and_render(input_data *input, output_data *output)
{
    offscreen_buffer *buffer = output->buffer;

    if (firstTime)
    {
        clear_buffer(buffer, COLOR_GRAY);
        firstTime = false;
    }
    
    //draw_grayscale_image(buffer, 0, 200,
    //                     fontpixels, fontpixelswidth, fontpixelsheight);
    
    /*
    fill_color(buffer, 12, 12, 12, 255);
    uint32 midx = buffer->width / 2;
    uint32 midy = buffer->height / 2;
    uint32 maxx = buffer->width - 1;
    uint32 maxy = buffer->height - 1;
    draw_line(buffer, midx, 0,    0,    midy, 255, 255, 0,   255);
    draw_line(buffer, 0,    midy, midx, maxy, 0,   255, 255, 255);
    draw_line(buffer, midx, maxy, maxx, midy, 255, 0,   255, 255);
    draw_line(buffer, maxx, midy, midx, 0,    255, 255, 255, 255);
    */

    /*
    uint32 x0 = rand() % buffer->width;
    uint32 x1 = rand() % buffer->width;
    uint32 y0 = rand() % buffer->height;
    uint32 y1 = rand() % buffer->height;
    uint8 r = rand() % 256;
    uint8 g = rand() % 256;
    uint8 b = rand() % 256;
    draw_line(buffer, x0, y0, x1, y1, r, g, b, 255);
    */

    uint32 framex = 2 * input->nFrames % buffer->width;
    uint32 maxy = buffer->height - 1;
    double maxti = 100; // ms
    double sum = 0.0;
    double total = 0.0;
    for (uint32 ti = 0; ti < input->nTimeIntervals; ++ti)
        total += input->pTimeIntervals[ti];

    draw_line(buffer,
              (framex + 2) % buffer->width, 0,
              (framex + 2) % buffer->width, maxy,
              COLOR_BLACK);
    draw_line(buffer,
              (framex + 3) % buffer->width, 0,
              (framex + 3) % buffer->width, maxy,
              COLOR_BLACK);

    if (total > maxti)
    {
        draw_line(buffer, framex, 0, framex, maxy, COLOR_RED);
        draw_line(buffer, framex+1, 0, framex+1, maxy, COLOR_RED);
    }
    else
    {
        for (uint32 ti = 0; ti < input->nTimeIntervals; ++ti)
        {
            uint32 yfrom = (sum / maxti) * maxy;
            sum += input->pTimeIntervals[ti];
            uint32 yto = (sum / maxti) * maxy;
            uint32 tii = ti + 1;
            color col = uint32_to_color(tii);
            draw_line(buffer, framex, yfrom, framex, yto, col);
            draw_line(buffer, framex+1, yfrom, framex+1, yto, col);

            draw_line(buffer, 30 * ti, maxy, 30 * (ti + 1), maxy, col);
            draw_line(buffer, 30 * ti, maxy-1, 30 * (ti + 1), maxy-1, col);
            draw_line(buffer, 30 * ti, maxy-2, 30 * (ti + 1), maxy-2, col);
        }
    }
    for (uint32 h = 1; h < 4; ++h)
    {
        uint32 targetmsy = h * 16.666666 / maxti * maxy;
        draw_line(buffer, 0, targetmsy, buffer->width - 1, targetmsy, COLOR_CYAN);
    }

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(buffer->width, buffer->height);
    io.DeltaTime = 0.01666666f;
    io.MousePos = ImVec2(input->mousex, buffer->height - 1 - input->mousey);
    io.MouseDown[0] = input->mouseleft;

    draw_pixel(buffer, input->mousex, input->mousey, COLOR_YELLOW);

    current_buffer = buffer;


    float mx = 2.0f *
        static_cast<float>(input->mousex) /
        static_cast<float>(buffer->width) - 1.0f;

    float my = 2.0f *
        static_cast<float>(input->mousey) /
        static_cast<float>(buffer->height) - 1.0f;

    Vandalism::Input ism_input;
    ism_input.altdown = false;
    ism_input.shiftdown = false;
    ism_input.ctrldown = false;
    ism_input.mousex = mx;
    ism_input.mousey = my;
    ism_input.mousedown = input->mouseleft;

    ism->update(&ism_input);

    if (ism->visiblesChanged)
    {
        uint32 visiblesCount;
        output->bake_tris->size = 0;
        for (uint32 visIdx = 0; visIdx < ism->visibles.size(); ++visIdx)
        {
            fill_triangles(output->bake_tris, ism->points, ism->visibles[visIdx]);
        }
        output->bake_flag = true;
        // flag processed
        // TODO: make this better
        ism->visiblesChanged = false;
    }

    size_t currStart, currEnd;
    ism->get_current_stroke(currStart, currEnd);

    Vandalism::Visible curr;
    curr.startIdx = currStart;
    curr.endIdx = currEnd;
    // TODO: this is bad
    curr.strokeIdx = ism->strokes.size();

    output->curr_tris->size = 0;

    fill_triangles(output->curr_tris, ism->points, curr);
        
    ImGui::NewFrame();

    if (false)
    {
        ImGui::ShowTestWindow();
    }
    else
    {
        ImGui::SetNextWindowSize(ImVec2(200, 300), ImGuiSetCond_FirstUseEver);
        ImGui::Begin("vandalism");
        ImGui::Text("mouse-norm: (%g, %g)", mx, my);
        ImGui::Text("mouse-raw: (%g, %g)",
                    static_cast<double>(input->mousex),
                    static_cast<double>(input->mousey));
        
        ImGui::Text("Alt Ctrl Shift LMB: (%d %d %d %d)",
                    static_cast<int>(ism_input.altdown),
                    static_cast<int>(ism_input.ctrldown),
                    static_cast<int>(ism_input.shiftdown),
                    static_cast<int>(ism_input.mousedown));

        ImGui::Text("ism strokes: %lu", ism->strokes.size());
        ImGui::Text("ism points: %lu", ism->points.size());

        ImGui::Text("bake_tris (%d/%d) %d",
                    output->bake_tris->size,
                    output->bake_tris->capacity,
                    output->bake_flag);

        ImGui::Text("curr_tris (%d/%d)",
                    output->curr_tris->size,
                    output->curr_tris->capacity);

        ImGui::Text("mode: %d", ism->currentMode);

        ImGui::End();
    }

    ImGui::Render();
}
