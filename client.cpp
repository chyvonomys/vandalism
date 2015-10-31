#include "client.h"
#include "math.h"
#include "swcolor.h"
#include "swrender.h"

#include "imgui.h"

offscreen_buffer *current_buffer = 0;

void DrawDot(const ImDrawVert &v)
{
    draw_pixel(current_buffer, v.pos.x, v.pos.y, v.col);
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
                  v0.col);
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
    uint32 pcol = pack_color(col);

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
                           pcol);
            }
        }
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

            draw_pixel(current_buffer, c, current_buffer->height - 1 - r,
                       pack_color(col));
        }
}

void DrawRectDbg(const ImDrawVert &c0, const ImDrawVert &c1, const color &col)
{
    uint32 x0 = c0.pos.x;
    uint32 x1 = c1.pos.x;
    uint32 y0 = c0.pos.y;
    uint32 y1 = c1.pos.y;
    uint32 c = pack_color(col);
    
    draw_line(current_buffer, x0, y0, x1, y0, c);
    draw_line(current_buffer, x0, y0, x0, y1, c);

    draw_line(current_buffer, x1, y0, x1, y1, c);
    draw_line(current_buffer, x0, y1, x1, y1, c);

    draw_line(current_buffer, x0, y0, x1, y1, c);
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

#include "vandalism.cpp"

Vandalism *ism = nullptr;

enum IsmMode
{
    ISM_DRAW = 0,
    ISM_PAN = 1,
    ISM_ZOOM = 2,
    ISM_ROTATE = 3
};

bool gui_showAllViews;
int32 gui_viewIdx;
int32 gui_mode;
float gui_brush_color[4];
float gui_brush_diameter;
bool gui_mouse_occupied;
bool gui_mouse_hover;

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

    gui_showAllViews = true;
    gui_viewIdx = 0;
    gui_mode = ISM_DRAW;

    gui_brush_color[0] = 1.0f;
    gui_brush_color[1] = 1.0f;
    gui_brush_color[2] = 1.0f;
    gui_brush_color[3] = 1.0f;

    gui_brush_diameter = 0.05f;

    gui_mouse_occupied = false;
    gui_mouse_hover = false;
}

extern "C" void cleanup()
{
    ism->cleanup();
    delete ism;
    delete [] fontpixels;
    ImGui::Shutdown();
}

inline float2 inches2snorm(const test_point& pt, float w, float h)
{
    return {2.0f * pt.x / w, 2.0f * pt.y / h};
}

void add_quad_tex(triangles *tris,
                  float2 a, float2 b, float2 c, float2 d,
                  float zindex, float rc, float gc, float bc, float ac)
{
    if (tris->size + 6 <= tris->capacity)
    {
        float *p = tris->data + tris->size * 9;

        *p++ = a.x; *p++ = a.y; *p++ = zindex; *p++ = 0.0f, *p++ = 0.0f;
        *p++ = rc; *p++ = gc; *p++ = bc; *p++ = ac;
        *p++ = b.x; *p++ = b.y; *p++ = zindex; *p++ = 0.0f, *p++ = 1.0f;
        *p++ = rc; *p++ = gc; *p++ = bc; *p++ = ac;
        *p++ = c.x; *p++ = c.y; *p++ = zindex; *p++ = 1.0f, *p++ = 1.0f;
        *p++ = rc; *p++ = gc; *p++ = bc; *p++ = ac;

        *p++ = c.x; *p++ = c.y; *p++ = zindex; *p++ = 1.0f, *p++ = 1.0f;
        *p++ = rc; *p++ = gc; *p++ = bc; *p++ = ac;
        *p++ = d.x; *p++ = d.y; *p++ = zindex; *p++ = 1.0f, *p++ = 0.0f;
        *p++ = rc; *p++ = gc; *p++ = bc; *p++ = ac;
        *p++ = a.x; *p++ = a.y; *p++ = zindex; *p++ = 0.0f, *p++ = 0.0f;
        *p++ = rc; *p++ = gc; *p++ = bc; *p++ = ac;

        tris->size += 6;
    }
}

void add_quad_const(triangles *tris,
                    float2 a, float2 b, float2 c, float2 d,
                    float zindex, float rc, float gc, float bc, float ac)
{
    if (tris->size + 6 <= tris->capacity)
    {
        float *p = tris->data + tris->size * 9;

        *p++ = a.x; *p++ = a.y; *p++ = zindex; *p++ = 0.5f, *p++ = 0.5f;
        *p++ = rc; *p++ = gc; *p++ = bc; *p++ = ac;
        *p++ = b.x; *p++ = b.y; *p++ = zindex; *p++ = 0.5f, *p++ = 0.5f;
        *p++ = rc; *p++ = gc; *p++ = bc; *p++ = ac;
        *p++ = c.x; *p++ = c.y; *p++ = zindex; *p++ = 0.5f, *p++ = 0.5f;
        *p++ = rc; *p++ = gc; *p++ = bc; *p++ = ac;

        *p++ = c.x; *p++ = c.y; *p++ = zindex; *p++ = 0.5f, *p++ = 0.5f;
        *p++ = rc; *p++ = gc; *p++ = bc; *p++ = ac;
        *p++ = d.x; *p++ = d.y; *p++ = zindex; *p++ = 0.5f, *p++ = 0.5f;
        *p++ = rc; *p++ = gc; *p++ = bc; *p++ = ac;
        *p++ = a.x; *p++ = a.y; *p++ = zindex; *p++ = 0.5f, *p++ = 0.5f;
        *p++ = rc; *p++ = gc; *p++ = bc; *p++ = ac;

        tris->size += 6;
    }
}

void fill_quads(triangles *tris,
                const test_point *points,
                size_t pi0, size_t pi1, size_t si,
                const test_transform& tform,
                const Vandalism::Brush& brush,
                float viewportWIn, float viewportHIn)
{
    for (size_t i = pi0 + 1; i < pi1; ++i)
    {
        float2 prev = {points[i-1].x, points[i-1].y};
        float2 curr = {points[i].x, points[i].y};
        float2 dir = curr - prev;
        if (len(dir) > 0.001f)
        {
            float2 side = brush.diameter * perp(dir * (1.0f / len(dir)));

            // x -ccw-> y
            float2 p0l = prev + side;
            float2 p0r = prev - side;
            float2 p1l = curr + side;
            float2 p1r = curr - side;

            float2 a = inches2snorm(apply_transform_pt(tform, {p0r.x, p0r.y}),
                                        viewportWIn, viewportHIn);
            float2 b = inches2snorm(apply_transform_pt(tform, {p0l.x, p0l.y}),
                                        viewportWIn, viewportHIn);
            float2 c = inches2snorm(apply_transform_pt(tform, {p1l.x, p1l.y}),
                                        viewportWIn, viewportHIn);
            float2 d = inches2snorm(apply_transform_pt(tform, {p1r.x, p1r.y}),
                                        viewportWIn, viewportHIn);

            float zindex = 0.00001f * si;
            add_quad_const(tris, a, b, c, d, zindex,
                           brush.r, brush.g, brush.b, brush.a);
        }
    }

    for (size_t i = pi0; i < pi1; ++i)
    {
        float2 curr = {points[i].x, points[i].y};
        float2 right = {brush.diameter, 0.0f};
        float2 up = {0.0, brush.diameter};
        float2 bl = curr - right - up;
        float2 br = curr + right - up;
        float2 tl = curr - right + up;
        float2 tr = curr + right + up;

        float2 a = inches2snorm(apply_transform_pt(tform, {bl.x, bl.y}),
                                viewportWIn, viewportHIn);
        float2 b = inches2snorm(apply_transform_pt(tform, {tl.x, tl.y}),
                                viewportWIn, viewportHIn);
        float2 c = inches2snorm(apply_transform_pt(tform, {tr.x, tr.y}),
                                viewportWIn, viewportHIn);
        float2 d = inches2snorm(apply_transform_pt(tform, {br.x, br.y}),
                                viewportWIn, viewportHIn);

        float zindex = 0.00001f * si;
        add_quad_tex(tris, a, b, c, d, zindex,
                     brush.r, brush.g, brush.b, brush.a);
    }
}

void draw_timing(offscreen_buffer *buffer, uint32 frame,
                 double *intervals, uint32 intervalCnt)
{
    uint32 framex = 2 * frame % buffer->width;
    uint32 maxy = buffer->height - 1;
    double maxti = 100; // ms
    double sum = 0.0;
    double total = 0.0;
    for (uint32 ti = 0; ti < intervalCnt; ++ti)
        total += intervals[ti];

    draw_line(buffer,
              (framex + 2) % buffer->width, 0,
              (framex + 2) % buffer->width, maxy,
              pack_color(COLOR_BLACK));
    draw_line(buffer,
              (framex + 3) % buffer->width, 0,
              (framex + 3) % buffer->width, maxy,
              pack_color(COLOR_BLACK));

    if (total > maxti)
    {
        draw_line(buffer, framex, 0, framex, maxy, pack_color(COLOR_RED));
        draw_line(buffer, framex+1, 0, framex+1, maxy, pack_color(COLOR_RED));
    }
    else
    {
        for (uint32 ti = 0; ti < intervalCnt; ++ti)
        {
            uint32 yfrom = (sum / maxti) * maxy;
            sum += intervals[ti];
            uint32 yto = (sum / maxti) * maxy;
            uint32 tii = ti + 1;
            color col = uint32_to_color(tii);
            uint32 pcol = pack_color(col);
            draw_line(buffer, framex, yfrom, framex, yto, pcol);
            draw_line(buffer, framex+1, yfrom, framex+1, yto, pcol);

            draw_line(buffer, 30 * ti, maxy, 30 * (ti + 1), maxy, pcol);
            draw_line(buffer, 30 * ti, maxy-1, 30 * (ti + 1), maxy-1, pcol);
            draw_line(buffer, 30 * ti, maxy-2, 30 * (ti + 1), maxy-2, pcol);
        }
    }
    for (uint32 h = 1; h < 4; ++h)
    {
        uint32 targetmsy = h * 16.666666 / maxti * maxy;
        draw_line(buffer, 0, targetmsy, buffer->width - 1, targetmsy,
                  pack_color(COLOR_CYAN));
    }
}

void DrawTestSegment(offscreen_buffer *buffer,
                     const test_box &dst_box,
                     const test_box &src_box,
                     test_segment &seg,
                     uint32 color)
{
    uint32 x0 = lerp(dst_box.x0, dst_box.x1, invlerp(src_box.x0, src_box.x1, seg.a.x));
    uint32 x1 = lerp(dst_box.x0, dst_box.x1, invlerp(src_box.x0, src_box.x1, seg.b.x));
    uint32 y0 = lerp(dst_box.y0, dst_box.y1, invlerp(src_box.y0, src_box.y1, seg.a.y));
    uint32 y1 = lerp(dst_box.y0, dst_box.y1, invlerp(src_box.y0, src_box.y1, seg.b.y));
    draw_line(buffer, x0, y0, x1, y1, color);
}

void DrawTestBox(offscreen_buffer *buffer,
                 const test_box &dst_box,
                 const test_box &src_box,
                 test_box &box,
                 uint32 color)
{
    uint32 L = lerp(dst_box.x0, dst_box.x1, invlerp(src_box.x0, src_box.x1, box.x0));
    uint32 R = lerp(dst_box.x0, dst_box.x1, invlerp(src_box.x0, src_box.x1, box.x1));
    uint32 B = lerp(dst_box.y0, dst_box.y1, invlerp(src_box.y0, src_box.y1, box.y0));
    uint32 T = lerp(dst_box.y0, dst_box.y1, invlerp(src_box.y0, src_box.y1, box.y1));
    draw_frame_rect(buffer, L, R, B, T, color);
}

test_box box_add_pt(const test_box &box, const test_point &p)
{
    test_box result = box;
    result.x0 = si_minf(result.x0, p.x);
    result.x1 = si_maxf(result.x1, p.x);
    result.y0 = si_minf(result.y0, p.y);
    result.y1 = si_maxf(result.y1, p.y);
    return result;
}

// TODO, this is actually an AA-box
test_box box_add_box(const test_box &box, const test_box &box2)
{
    test_box result = box;
    result = box_add_pt(result, {box2.x0, box2.y0});
    result = box_add_pt(result, {box2.x1, box2.y1});
    return result;
}

test_box box_add_seg(const test_box &box, const test_segment &seg)
{
    test_box result = box;
    result = box_add_pt(result, seg.a);
    result = box_add_pt(result, seg.b);
    return result;
}

uint32 DrawTestAll(offscreen_buffer *buffer,
                   uint32 x0, uint32 x1, uint32 y0, uint32 y1,
                   float viewportW, float viewportH,
                   const test_data &data)
{
    test_box ds_bbox = {INFINITY, -INFINITY, INFINITY, -INFINITY};
    test_transform accum_transform = id_transform();

    // ls_*** - current view's space
    // ds_*** - default view's space

    std::vector<test_box> ds_box_stack;
    std::vector<test_segment> ds_seg_stack;

    test_box ls_viewport = {-0.5f * viewportW, 0.5f * viewportW,
                            -0.5f * viewportH, 0.5f * viewportH};

    for (size_t vi = 0; vi < data.nviews; ++vi)
    {
        const auto &view = data.views[vi];

        test_transform transform = transform_from_transition(view.tr);
        accum_transform = combine_transforms(accum_transform, transform);

        for (size_t si = view.si0; si < view.si1; ++si)
        {
            size_t bi = data.strokes[si].pi0;
            size_t ei = data.strokes[si].pi1;

            if (ei > bi)
            {
                test_segment ls_segment = {data.points[bi], data.points[ei - 1]};
                test_segment ds_segment = apply_transform_seg(accum_transform,
                                                              ls_segment);
                ds_seg_stack.push_back(ds_segment);
            }
        }

        test_box ds_viewport = apply_transform_box(accum_transform,
                                                   ls_viewport);

        ds_bbox = box_add_box(ds_bbox, ds_viewport);
        ds_box_stack.push_back(ds_viewport);
    }

    draw_solid_rect(buffer, x0, x1, y0, y1, pack_color(COLOR_GRAY));

    test_box whole_box =
    {
        static_cast<float>(x0),
        static_cast<float>(x1),
        static_cast<float>(y0),
        static_cast<float>(y1)
    };

    float wb_h = y1 - y0;
    float wb_w = x1 - x0;

    float whole_box_aspect_ratio = wb_h / wb_w;
    float content_box_aspect_ratio = (ds_bbox.y1 - ds_bbox.y0) / (ds_bbox.x1 - ds_bbox.x0);

    float cx = 0.5f * (x0 + x1);
    float cy = 0.5f * (y0 + y1);

    float cb_h = wb_h;
    float cb_w = wb_w;

    if (content_box_aspect_ratio > whole_box_aspect_ratio)
    {
        cb_w = wb_h / content_box_aspect_ratio;
    }
    else
    {
        cb_h = wb_w * content_box_aspect_ratio;
    }

    test_box content_box =
    {
        cx - 0.5f * cb_w, cx + 0.5f * cb_w,
        cy - 0.5f * cb_h, cy + 0.5f * cb_h
    };

    for (size_t i = 0; i < ds_seg_stack.size(); ++i)
    {
        DrawTestSegment(buffer, content_box, ds_bbox, ds_seg_stack[i], 0xFF00AAAA);
    }

    for (size_t i = 0; i < ds_box_stack.size(); ++i)
    {
        DrawTestBox(buffer, content_box, ds_bbox, ds_box_stack[i], 0xFFEEEEEE);
    }

    DrawTestBox(buffer, content_box, ds_bbox, ds_bbox, 0xFF0000EE);

    return ds_seg_stack.size();
}

uint32 DrawTestPin(offscreen_buffer *buffer,
                   uint32 x0, uint32 x1, uint32 y0, uint32 y1,
                   float viewportW, float viewportH,
                   const test_data &data, size_t pin)
{
    std::vector<test_visible> visibles;
    std::vector<test_transform> ls2ps_transforms;
    const test_box ps_viewport = {-0.5f * viewportW, 0.5f * viewportW,
                                  -0.5f * viewportH, 0.5f * viewportH};

    const test_box render_box =
    {
        static_cast<float>(x0),
        static_cast<float>(x1),
        static_cast<float>(y0),
        static_cast<float>(y1)
    };

    query(data, pin, ps_viewport, visibles, ls2ps_transforms);

    draw_solid_rect(buffer, x0, x1, y0, y1, pack_color(COLOR_GRAY));

    for (size_t i = 0; i < visibles.size(); ++i)
    {
        const test_visible &vis = visibles[i];
        uint32 pi0 = data.strokes[vis.si].pi0;
        uint32 pi1 = data.strokes[vis.si].pi1;
        if (pi1 > pi0)
        {
            test_segment ls_seg = {data.points[pi0], data.points[pi1 - 1]};
            test_segment ps_seg = apply_transform_seg(ls2ps_transforms[vis.ti],
                                                      ls_seg);
            DrawTestSegment(buffer, render_box, ps_viewport, ps_seg, 0xFFAAAA00);
        }
    }

    return visibles.size();
}

extern "C" void update_and_render(input_data *input, output_data *output)
{
    offscreen_buffer *buffer = output->buffer;

    float mxnorm = static_cast<float>(input->mousex) /
                   static_cast<float>(buffer->width) - 0.5f;

    float mynorm = static_cast<float>(input->mousey) /
                   static_cast<float>(buffer->height) - 0.5f;

    float mxin = output->bufferWidthIn * mxnorm;
    float myin = output->bufferHeightIn * mynorm;

    bool mouse_in_ui = gui_mouse_occupied || gui_mouse_hover;

    Vandalism::Input ism_input;
    ism_input.altdown = (gui_mode == ISM_ROTATE);
    ism_input.shiftdown = (gui_mode == ISM_PAN);
    ism_input.ctrldown = (gui_mode == ISM_ZOOM);
    ism_input.mousex = mxin;
    ism_input.mousey = myin;
    ism_input.mousedown = input->mouseleft && !mouse_in_ui;
    ism_input.brushred = gui_brush_color[0];
    ism_input.brushgreen = gui_brush_color[1];
    ism_input.brushblue = gui_brush_color[2];
    ism_input.brushalpha = gui_brush_color[3];
    ism_input.brushdiameter = gui_brush_diameter;

    ism->update(&ism_input);

    const test_data &debug_data = ism->get_debug_data();

    if (ism->visiblesChanged)
    {
        output->bake_tris->size = 0;
        std::vector<test_visible> visibles;
        std::vector<test_transform> transforms;
        test_box viewbox = {-0.5f * output->bufferWidthIn,
                            0.5f * output->bufferWidthIn,
                            -0.5f * output->bufferHeightIn,
                            0.5f * output->bufferHeightIn};

        query(debug_data, debug_data.nviews - 1, viewbox, visibles, transforms);
        for (uint32 visIdx = 0; visIdx < visibles.size(); ++visIdx)
        {
            const test_visible& vis = visibles[visIdx];
            const test_transform& tform = transforms[vis.ti];
            const test_stroke& stroke = debug_data.strokes[vis.si];
            const Vandalism::Brush& brush = ism->brushes[stroke.brush_id];
            fill_quads(output->bake_tris, debug_data.points,
                       stroke.pi0, stroke.pi1, vis.si, tform, brush,
                       output->bufferWidthIn, output->bufferHeightIn);
        }

        output->bake_flag = true;
        // flag processed
        // TODO: make this better
        ism->visiblesChanged = false;
        ::printf("update mesh: %lu visibles\n", visibles.size());
    }

    output->translateX = 2.0f * ism->shiftX / output->bufferWidthIn;
    output->translateY = 2.0f * ism->shiftY / output->bufferHeightIn;
    output->scale = ism->zoomCoeff;

    size_t currStart, currEnd;
    size_t currId = ism->get_current_stroke(currStart, currEnd);

    output->curr_tris->size = 0;

    Vandalism::Brush currBrush;
    if (ism->get_current_brush(currBrush))
    {
        fill_quads(output->curr_tris, debug_data.points,
                   currStart, currEnd, currId, id_transform(), currBrush,
                   output->bufferWidthIn, output->bufferHeightIn);
    }

    // Draw SW -----------------------------------------------------------------

    if (input->nFrames == 1)
    {
        clear_buffer(buffer, COLOR_GRAY);
    }

    //draw_grayscale_image(buffer, 0, 200,
    //                     fontpixels, fontpixelswidth, fontpixelsheight);

    draw_timing(buffer, input->nFrames,
                input->pTimeIntervals, input->nTimeIntervals);

    if (!mouse_in_ui)
    {
        draw_pixel(buffer,
                   input->mousex, input->mousey,
                   pack_color(COLOR_YELLOW));
    }

    uint32 seg_cnt = 0;

    if (gui_showAllViews)
    {
        seg_cnt = DrawTestAll(buffer,
                              0, 400, 0, 400,
                              output->bufferWidthIn, output->bufferHeightIn,
                              debug_data);
    }
    else
    {
        seg_cnt = DrawTestPin(buffer,
                              0, 400, 0, 400,
                              output->bufferWidthIn, output->bufferHeightIn,
                              debug_data, gui_viewIdx);
    }

    current_buffer = buffer;

    // Draw ImGui --------------------------------------------------------------

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(buffer->width, buffer->height);
    io.DeltaTime = 0.01666666f;
    io.MousePos = ImVec2(input->mousex, buffer->height - 1 - input->mousey);
    io.MouseDown[0] = input->mouseleft;

    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(100, 200), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("brush");
    ImGui::ColorEdit4("color", gui_brush_color);
    ImGui::SliderFloat("diameter", &gui_brush_diameter, 0.01f, 0.1f);
    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(200, 300), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("debug");
    ImGui::Text("mouse-inch: (%g, %g)", mxin, myin);
    ImGui::Text("mouse-norm: (%g, %g)", mxnorm, mynorm);
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

    ImGui::RadioButton("draw", &gui_mode, ISM_DRAW); ImGui::SameLine();
    ImGui::RadioButton("pan", &gui_mode, ISM_PAN); ImGui::SameLine();
    ImGui::RadioButton("zoom", &gui_mode, ISM_ZOOM); ImGui::SameLine();
    ImGui::RadioButton("rot", &gui_mode, ISM_ROTATE);

    ImGui::Text("test_segments: %d", seg_cnt);

    ImGui::Checkbox("all views", &gui_showAllViews);
    ImGui::SliderInt("view", &gui_viewIdx, 0, debug_data.nviews - 1);

    ImGui::Text("mouse occupied: %d", static_cast<int>(gui_mouse_occupied));
    ImGui::Text("mouse hover: %d", static_cast<int>(gui_mouse_hover));

    ImGui::End();

    ImGui::Render();

    gui_mouse_occupied = ImGui::IsAnyItemActive();
    gui_mouse_hover = ImGui::IsMouseHoveringAnyWindow();
}
