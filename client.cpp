#include <vector>
#include "client.h"
#include "math.h"
#include "swcolor.h"
#include "swrender.h"

#include "imgui.h"

kernel_services *current_services = 0;
output_data *current_output = 0;


kernel_services::TexID font_texture_id;

std::vector<kernel_services::MeshID> ui_meshes;
std::vector<output_data::drawcall> ui_drawcalls;

void RenderImGuiDrawLists(ImDrawData *drawData)
{
    while (drawData->CmdListsCount > ui_meshes.size())
    {
        ui_meshes.push_back(current_services->create_mesh());
    }

    ui_drawcalls.clear();

    for (size_t li = 0; li < drawData->CmdListsCount; ++li)
    {
        const ImDrawList *drawList = drawData->CmdLists[li];

        const ImDrawIdx *indexBuffer = &drawList->IdxBuffer.front();
        const ImDrawVert *vertexBuffer = &drawList->VtxBuffer.front();

        current_services->update_mesh(ui_meshes[li],
                                      vertexBuffer, drawList->VtxBuffer.size(),
                                      indexBuffer, drawList->IdxBuffer.size());

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
                output_data::drawcall drawcall;
                drawcall.texture_id = *reinterpret_cast<const uint32*>(&cmd.TextureId);
                drawcall.mesh_id = ui_meshes[li];
                drawcall.offset = idxOffs;
                drawcall.count = idxCnt;

                ui_drawcalls.push_back(drawcall);

                /*
                float xmin = cmd.ClipRect.x;
                float ymin = cmd.ClipRect.y;
                float xmax = cmd.ClipRect.z;
                float ymax = cmd.ClipRect.w;
                */
            }

            idxOffs += idxCnt;
        }
    }

    current_output->ui_drawcalls = ui_drawcalls.data();
    current_output->ui_drawcall_cnt = ui_drawcalls.size();
}

#include "vandalism.cpp"

Vandalism *ism = nullptr;

bool gui_showAllViews;
int32 gui_viewIdx;
int32 gui_tool;
float gui_brush_color[4];
int32 gui_brush_diameter_units;
bool gui_mouse_occupied;
bool gui_mouse_hover;
float gui_background_color[3];
float gui_grid_bg_color[3];
float gui_grid_fg_color[3];
float gui_eraser_alpha;

const int32 cfg_min_brush_diameter_units = 1;
const int32 cfg_max_brush_diameter_units = 64;
const int32 cfg_def_brush_diameter_units = 4;
const float cfg_brush_diameter_inches_per_unit = 1.0f / 64.0f;

uint32 timingX;

ImGuiTextBuffer *viewsBuf;

extern "C" void setup(kernel_services *services)
{
    ImGuiIO& io = ImGui::GetIO();
    io.RenderDrawListsFn = RenderImGuiDrawLists;
    viewsBuf = new ImGuiTextBuffer;

    uint8 *pixels;
    int32 width, height;
    io.Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);

    font_texture_id = services->create_texture(width, height);
    services->update_texture(font_texture_id, pixels);
    io.Fonts->TexID = reinterpret_cast<void *>(font_texture_id);

    current_services = services;

    io.Fonts->ClearInputData();
    io.Fonts->ClearTexData();

    ism = new Vandalism();
    ism->setup();

    gui_showAllViews = true;
    gui_viewIdx = 0;
    gui_tool = static_cast<int>(Vandalism::DRAW);

    gui_brush_color[0] = 1.0f;
    gui_brush_color[1] = 1.0f;
    gui_brush_color[2] = 1.0f;
    gui_brush_color[3] = 1.0f;

    gui_background_color[0] = 0.3f;
    gui_background_color[1] = 0.3f;
    gui_background_color[2] = 0.3f;

    gui_grid_bg_color[0] = 0.0f;
    gui_grid_bg_color[1] = 0.0f;
    gui_grid_bg_color[2] = 0.5f;

    gui_grid_fg_color[0] = 0.5f;
    gui_grid_fg_color[1] = 0.5f;
    gui_grid_fg_color[2] = 1.0f;

    gui_brush_diameter_units = cfg_def_brush_diameter_units;

    gui_eraser_alpha = 1.0f;

    gui_mouse_occupied = false;
    gui_mouse_hover = false;

    timingX = 0;
}

extern "C" void cleanup()
{
    ism->cleanup();
    delete ism;

    for (uint32 i = 0; i < ui_meshes.size(); ++i)
    {
        current_services->delete_mesh(ui_meshes[i]);
    }

    ui_meshes.clear();
    ui_drawcalls.clear();

    current_services->delete_texture(font_texture_id);

    delete viewsBuf;

    ImGui::Shutdown();
}

// TODO: optimize with triangle fans/strips maybe?

void add_quad_tex(std::vector<output_data::Vertex> *tris,
                  test_point a, test_point b, test_point c, test_point d,
                  float zindex, float ec,
                  float rc, float gc, float bc, float ac)
{
    output_data::Vertex v;

    v.x = a.x; v.y = a.y; v.z = zindex; v.u = 0.0f, v.v = 0.0f; v.e = ec; v.r = rc; v.g = gc; v.b = bc; v.a = ac; tris->push_back(v);
    v.x = b.x; v.y = b.y; v.z = zindex; v.u = 0.0f, v.v = 1.0f; v.e = ec; v.r = rc; v.g = gc; v.b = bc; v.a = ac; tris->push_back(v);
    v.x = c.x; v.y = c.y; v.z = zindex; v.u = 1.0f, v.v = 1.0f; v.e = ec; v.r = rc; v.g = gc; v.b = bc; v.a = ac; tris->push_back(v);
    v.x = c.x; v.y = c.y; v.z = zindex; v.u = 1.0f, v.v = 1.0f; v.e = ec; v.r = rc; v.g = gc; v.b = bc; v.a = ac; tris->push_back(v);
    v.x = d.x; v.y = d.y; v.z = zindex; v.u = 1.0f, v.v = 0.0f; v.e = ec; v.r = rc; v.g = gc; v.b = bc; v.a = ac; tris->push_back(v);
    v.x = a.x; v.y = a.y; v.z = zindex; v.u = 0.0f, v.v = 0.0f; v.e = ec; v.r = rc; v.g = gc; v.b = bc; v.a = ac; tris->push_back(v);
}

void add_quad_const(std::vector<output_data::Vertex> *tris,
                    test_point a, test_point b, test_point c, test_point d,
                    float zindex, float ec,
                    float rc, float gc, float bc, float ac)
{
    output_data::Vertex v;

    v.x = a.x; v.y = a.y; v.z = zindex; v.u = 0.5f, v.v = 0.5f; v.e = ec; v.r = rc; v.g = gc; v.b = bc; v.a = ac; tris->push_back(v);
    v.x = b.x; v.y = b.y; v.z = zindex; v.u = 0.5f, v.v = 0.5f; v.e = ec; v.r = rc; v.g = gc; v.b = bc; v.a = ac; tris->push_back(v);
    v.x = c.x; v.y = c.y; v.z = zindex; v.u = 0.5f, v.v = 0.5f; v.e = ec; v.r = rc; v.g = gc; v.b = bc; v.a = ac; tris->push_back(v);
    v.x = c.x; v.y = c.y; v.z = zindex; v.u = 0.5f, v.v = 0.5f; v.e = ec; v.r = rc; v.g = gc; v.b = bc; v.a = ac; tris->push_back(v);
    v.x = d.x; v.y = d.y; v.z = zindex; v.u = 0.5f, v.v = 0.5f; v.e = ec; v.r = rc; v.g = gc; v.b = bc; v.a = ac; tris->push_back(v);
    v.x = a.x; v.y = a.y; v.z = zindex; v.u = 0.5f, v.v = 0.5f; v.e = ec; v.r = rc; v.g = gc; v.b = bc; v.a = ac; tris->push_back(v);
}

void fill_quads(std::vector<output_data::Vertex> *tris,
                const test_point *points,
                size_t pi0, size_t pi1, size_t si,
                const test_transform& tform,
                const Vandalism::Brush& brush)
{
    float radius = brush.diameter * 0.5f;

    for (size_t i = pi0 + 1; i < pi1; ++i)
    {
        float2 prev = {points[i-1].x, points[i-1].y};
        float2 curr = {points[i].x, points[i].y};
        float2 dir = curr - prev;
        if (len(dir) > 0.001f)
        {
            float2 side = radius * perp(dir * (1.0f / len(dir)));

            // x -ccw-> y
            float2 p0l = prev + side;
            float2 p0r = prev - side;
            float2 p1l = curr + side;
            float2 p1r = curr - side;

            test_point a = apply_transform_pt(tform, {p0r.x, p0r.y});
            test_point b = apply_transform_pt(tform, {p0l.x, p0l.y});
            test_point c = apply_transform_pt(tform, {p1l.x, p1l.y});
            test_point d = apply_transform_pt(tform, {p1r.x, p1r.y});

            float zindex = 0.00001f * si;
            add_quad_const(tris, a, b, c, d, zindex, brush.e,
                           brush.r, brush.g, brush.b, brush.a);
        }
    }

    for (size_t i = pi0; i < pi1; ++i)
    {
        float2 curr = {points[i].x, points[i].y};
        float2 right = {radius, 0.0f};
        float2 up = {0.0, radius};
        float2 bl = curr - right - up;
        float2 br = curr + right - up;
        float2 tl = curr - right + up;
        float2 tr = curr + right + up;

        test_point a = apply_transform_pt(tform, {bl.x, bl.y});
        test_point b = apply_transform_pt(tform, {tl.x, tl.y});
        test_point c = apply_transform_pt(tform, {tr.x, tr.y});
        test_point d = apply_transform_pt(tform, {br.x, br.y});

        float zindex = 0.00001f * si;
        add_quad_tex(tris, a, b, c, d, zindex, brush.e,
                     brush.r, brush.g, brush.b, brush.a);
    }
}

uint32 draw_timing(offscreen_buffer *buffer, uint32 framex,
                 double *intervals, uint32 intervalCnt)
{
    uint32 maxy = buffer->height - 1;
    double maxti = 200; // ms
    double sum = 0.0;
    double total = 0.0;
    for (uint32 ti = 0; ti < intervalCnt; ++ti)
        total += intervals[ti];

    draw_line(buffer,
              (framex + 2) % buffer->width, 0,
              (framex + 2) % buffer->width, maxy,
              0x00000000);
    draw_line(buffer,
              (framex + 3) % buffer->width, 0,
              (framex + 3) % buffer->width, maxy,
              0x00000000);

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

    return framex + 2;
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

    query(data, pin, ps_viewport, visibles, ls2ps_transforms, 1000.0f);

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

    float mxnorm = static_cast<float>(input->swMouseXPx) /
                   static_cast<float>(input->swWidthPx) - 0.5f;

    float mynorm = static_cast<float>(input->swMouseYPx) /
                   static_cast<float>(input->swHeightPx) - 0.5f;

    mynorm = -mynorm;

    float mxin = input->vpWidthIn * mxnorm;
    float myin = input->vpHeightIn * mynorm;

    bool mouse_in_ui = gui_mouse_occupied || gui_mouse_hover;

    float pixel_height_in = input->vpHeightIn / input->vpHeightPx;

    Vandalism::Input ism_input;
    ism_input.tool = static_cast<Vandalism::Tool>(gui_tool);
    ism_input.mousex = mxin;
    ism_input.mousey = myin;
    ism_input.negligibledistance = pixel_height_in;
    ism_input.mousedown = input->mouseleft && !mouse_in_ui;
    ism_input.brushred = gui_brush_color[0];
    ism_input.brushgreen = gui_brush_color[1];
    ism_input.brushblue = gui_brush_color[2];
    ism_input.brushalpha = gui_brush_color[3];
    ism_input.eraseralpha = gui_eraser_alpha;
    ism_input.brushdiameter = gui_brush_diameter_units * cfg_brush_diameter_inches_per_unit;

    ism->update(&ism_input);

    const test_data &debug_data = ism->get_debug_data();

    bool scrollViewsDown = false;

    if (ism->visiblesChanged)
    {
        output->bake_tris->clear();
        std::vector<test_visible> visibles;
        std::vector<test_transform> transforms;
        test_box viewbox = {-0.5f * input->rtWidthIn,
                            +0.5f * input->rtWidthIn,
                            -0.5f * input->rtHeightIn,
                            +0.5f * input->rtHeightIn};

        query(debug_data, debug_data.nviews - 1,
              viewbox, visibles, transforms,
              pixel_height_in);
        for (uint32 visIdx = 0; visIdx < visibles.size(); ++visIdx)
        {
            const test_visible& vis = visibles[visIdx];
            const test_transform& tform = transforms[vis.ti];
            const test_stroke& stroke = debug_data.strokes[vis.si];
            const Vandalism::Brush& brush = ism->brushes[stroke.brush_id];
            fill_quads(output->bake_tris, debug_data.points,
                       stroke.pi0, stroke.pi1, vis.si, tform, brush);
        }

        output->bake_flag = true;
        // flag processed
        // TODO: make this better
        ism->visiblesChanged = false;
        ::printf("update mesh: %lu visibles\n", visibles.size());

        viewsBuf->clear();
        for (uint32 vi = 0; vi < debug_data.nviews; ++vi)
        {
            const auto &view = debug_data.views[vi];
            auto t = view.tr.type;
            const char *typestr = (t == TZOOM ? "ZOOM" :
                                   (t == TPAN ? "PAN" :
                                    (t == TROTATE ? "ROTATE" : "ERROR")));
            viewsBuf->append("%d: %s  %f/%f  (%ld..%ld)\n",
                            vi, typestr, view.tr.a, view.tr.b, view.si0, view.si1);
        }
        scrollViewsDown = true;
    }

    output->preTranslateX = ism->preShiftX;
    output->preTranslateY = ism->preShiftY;
    output->postTranslateX = ism->postShiftX;
    output->postTranslateY = ism->postShiftY;
    output->scale      = ism->zoomCoeff;
    output->rotate     = ism->rotateAngle;

    output->bg_red   = gui_background_color[0];
    output->bg_green = gui_background_color[1];
    output->bg_blue  = gui_background_color[2];

    output->grid_bg_color[0] = gui_grid_bg_color[0];
    output->grid_bg_color[1] = gui_grid_bg_color[1];
    output->grid_bg_color[2] = gui_grid_bg_color[2];

    output->grid_fg_color[0] = gui_grid_fg_color[0];
    output->grid_fg_color[1] = gui_grid_fg_color[1];
    output->grid_fg_color[2] = gui_grid_fg_color[2];

    size_t currStart, currEnd;
    size_t currId = ism->get_current_stroke(currStart, currEnd);

    output->curr_tris->clear();

    Vandalism::Brush currBrush;
    if (ism->get_current_brush(currBrush))
    {
        fill_quads(output->curr_tris, debug_data.points,
                   currStart, currEnd, currId, id_transform(), currBrush);
    }

    // Draw SW -----------------------------------------------------------------

    if (input->nFrames == 1)
    {
        clear_buffer(buffer, COLOR_GRAY);
        timingX = 0;
    }

    timingX = draw_timing(buffer, timingX,
                input->pTimeIntervals, input->nTimeIntervals);

    timingX = timingX % input->swWidthPx;

    if (!mouse_in_ui)
    {
        draw_pixel(buffer,
                   input->swMouseXPx, input->swMouseYPx,
                   pack_color(COLOR_YELLOW));
    }

    int fXPx = (ism->firstX / input->vpWidthIn + 0.5f) * input->swWidthPx;
    int fYPx = (0.5f - ism->firstY / input->vpHeightIn) * input->swHeightPx;

    draw_line(buffer, fXPx-2, fYPx, fXPx+2, fYPx, pack_color(COLOR_RED));
    draw_line(buffer, fXPx, fYPx-2, fXPx, fYPx+2, pack_color(COLOR_RED));

#ifdef DEBUG_CASCADE
    uint32 seg_cnt = 0;

    if (gui_showAllViews)
    {
        seg_cnt = DrawTestAll(buffer,
                              0, 400, 0, 400,
                              input->rtWidthIn, input->rtHeightIn,
                              debug_data);
    }
    else
    {
        seg_cnt = DrawTestPin(buffer,
                              0, 400, 0, 400,
                              input->rtWidthIn, input->rtHeightIn,
                              debug_data, gui_viewIdx);
    }
#endif

    current_output = output;

    // Draw ImGui --------------------------------------------------------------

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(input->swWidthPx, input->swHeightPx);
    io.DeltaTime = 0.01666666f;
    io.MousePos = ImVec2(input->swMouseXPx, input->swMouseYPx);
    io.MouseDown[0] = input->mouseleft;

    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(100, 200), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("vandalism");
    ImGui::ColorEdit4("color", gui_brush_color);
    ImGui::ColorEdit3("background", gui_background_color);
    ImGui::SliderInt("diameter", &gui_brush_diameter_units,
                     cfg_min_brush_diameter_units, cfg_max_brush_diameter_units);
    ImGui::SliderFloat("eraser", &gui_eraser_alpha, 0.0f, 1.0f);
    ImGui::Separator();
    ImGui::ColorEdit3("grid bg", gui_grid_bg_color);
    ImGui::ColorEdit3("grid fg", gui_grid_fg_color);
    ImGui::Separator();
    ImGui::RadioButton("draw", &gui_tool, static_cast<int>(Vandalism::DRAW));
    ImGui::RadioButton("erase", &gui_tool, static_cast<int>(Vandalism::ERASE));
    ImGui::RadioButton("pan", &gui_tool, static_cast<int>(Vandalism::PAN));
    ImGui::RadioButton("zoom", &gui_tool, static_cast<int>(Vandalism::ZOOM));
    ImGui::RadioButton("rot", &gui_tool, static_cast<int>(Vandalism::ROTATE));
    ImGui::RadioButton("first", &gui_tool, static_cast<int>(Vandalism::FIRST));
    ImGui::RadioButton("second", &gui_tool, static_cast<int>(Vandalism::SECOND));

    if (ism->currentMode == Vandalism::IDLE)
    {
        if (ImGui::Button("Save"))
        {
            ism->save_data("debug.ism");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load"))
        {
            ism->load_data("debug.ism");
        }
        ImGui::SameLine();
        if (ism->undoable() && ImGui::Button("Undo"))
        {
            ism->undo();
        }
    }

    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(200, 300), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("views");
    ImGui::TextUnformatted(viewsBuf->begin());
    if (scrollViewsDown)
    {
        ImGui::SetScrollHere(1.0f);
    }
    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(200, 300), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("debug");
    ImGui::Text("mouse-inch: (%g, %g)", mxin, myin);
    ImGui::Text("mouse-norm: (%g, %g)", mxnorm, mynorm);
    ImGui::Text("mouse-ui-px: (%g, %g)",
                static_cast<double>(input->swMouseXPx),
                static_cast<double>(input->swMouseYPx));

    ImGui::Text("tool active: %d", static_cast<int>(ism_input.mousedown));

    ImGui::Separator();

    ImGui::Text("shift-inch: (%g, %g)", ism->postShiftX, ism->postShiftY);
    ImGui::Text("zoom-coeff: %g", ism->zoomCoeff);
    ImGui::Text("rotate-angle: %g", ism->rotateAngle);

    ImGui::Separator();

    ImGui::Text("ism strokes: %lu", ism->strokes.size());
    ImGui::Text("ism points: %lu", ism->points.size());

    ImGui::Text("bake_tris (%lu/%lu) %d",
                output->bake_tris->size(),
                output->bake_tris->capacity(),
                output->bake_flag);

    ImGui::Text("curr_tris (%lu/%lu)",
                output->curr_tris->size(),
                output->curr_tris->capacity());

    ImGui::Text("mode: %d", ism->currentMode);

#ifdef DEBUG_CASCADE
    ImGui::Text("test_segments: %d", seg_cnt);

    ImGui::Checkbox("all views", &gui_showAllViews);
    ImGui::SliderInt("view", &gui_viewIdx, 0, debug_data.nviews - 1);
#endif

    ImGui::Text("mouse occupied: %d", static_cast<int>(gui_mouse_occupied));
    ImGui::Text("mouse hover: %d", static_cast<int>(gui_mouse_hover));

    ImGui::Separator();
    ImGui::Text("vp size %d x %d pt", input->vpWidthPt, input->vpHeightPt);
    ImGui::Text("vp size %d x %d px", input->vpWidthPx, input->vpHeightPx);
    ImGui::Text("vp size %g x %g in", input->vpWidthIn, input->vpHeightIn);
    ImGui::Text("win size %d x %d pt", input->windowWidthPt, input->windowHeightPt);
    ImGui::Text("win size %d x %d px", input->windowWidthPx, input->windowHeightPx);
    ImGui::Text("win pos %d, %d pt", input->windowPosXPt, input->windowPosYPt);

    ImGui::End();

    ImGui::Render();

    gui_mouse_occupied = ImGui::IsAnyItemActive();
    gui_mouse_hover = ImGui::IsMouseHoveringAnyWindow();
}
