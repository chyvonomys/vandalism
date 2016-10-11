#include <string>
#include <vector>
#include "client.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wfloat-equal"
#include "imgui/imgui.h"
#pragma clang diagnostic pop
#else
#include "imgui/imgui.h"
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#pragma clang diagnostic ignored "-Wcast-align"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#include "stb_image.h"
#include "stb_image_write.h"
#pragma clang diagnostic pop
#else
#pragma warning(push)
#pragma warning(disable: 4242)
#pragma warning(disable: 4244)
#pragma warning(disable: 4365)
#include "stb_image.h"
#pragma warning(disable: 4996)
#include "stb_image_write.h"
#pragma warning(pop)
#endif

kernel_services *g_services = nullptr;

kernel_services::TexID g_font_texture_id;

std::vector<kernel_services::MeshID> g_ui_meshes;

struct ImageDesc
{
    kernel_services::TexID texid;
    u32 width;
    u32 height;
};
// TODO: next two vectors have same size
// image ids in art are indexes in those vectors
std::vector<ImageDesc> g_loaded_images;
std::vector<std::string> g_loaded_image_names;
std::vector<output_data::drawcall> g_drawcalls;

kernel_services::MeshID g_lines_mesh;
kernel_services::MeshID g_bake_mesh;
kernel_services::MeshID g_curr_mesh;

std::vector<ImDrawVert> g_lines_vb;

struct CurrentImage
{
    std::string name;
    float width_in;
    float height_in;
    kernel_services::TexID texid;
    // TODO: think of better approach to avoiding texture deletion than this flag
    bool reuse;
    CurrentImage() :
        width_in(0.0f),
        height_in(0.0f),
        texid(kernel_services::default_texid),
        reuse(true)
    {}
} g_fit_img;

enum CapturingStage
{
    INACTIVE,
    SELECTION,
    CAPTURE
};

bool g_image_fitting;
CapturingStage g_image_capturing;

void clear_loaded_images()
{
    for (size_t i = 0; i < g_loaded_images.size(); ++i)
    {
        g_services->delete_texture(g_loaded_images[i].texid);
    }
    g_loaded_images.clear();
    g_loaded_image_names.clear();
}

size_t image_name_idx(const char *name)
{
    size_t result = 0;
    for (; result < g_loaded_image_names.size(); ++result)
    {
        if (g_loaded_image_names[result] == name)
            break;
    }
    return result;
}

// TODO: ImDrawIdx vs u16 in proto.cpp

void RenderImGuiDrawLists(ImDrawData *drawData)
{
    for (size_t li = 0; li < static_cast<size_t>(drawData->CmdListsCount); ++li)
    {
        const ImDrawList *drawList = drawData->CmdLists[li];

        const ImDrawIdx *indexBuffer = &drawList->IdxBuffer.front();
        const ImDrawVert *vertexBuffer = &drawList->VtxBuffer.front();

        u32 vtxCount = static_cast<u32>(drawList->VtxBuffer.size());
        u32 idxCount = static_cast<u32>(drawList->IdxBuffer.size());

        if (g_ui_meshes.size() <= li)
        {
            kernel_services::MeshID mid =
            g_services->create_mesh(*g_services->ui_vertex_layout,
                                    vtxCount, idxCount);
            g_ui_meshes.push_back(mid);
        }

        g_services->update_mesh(g_ui_meshes[li],
                                vertexBuffer, vtxCount,
                                indexBuffer, idxCount);

        i32 cmdCnt = drawList->CmdBuffer.size();

        u32 idxOffs = 0;

        for (i32 ci = 0; ci < cmdCnt; ++ci)
        {
            const ImDrawCmd &cmd = drawList->CmdBuffer[ci];
            u32 idxCnt = cmd.ElemCount;

            if (cmd.UserCallback)
            {
                cmd.UserCallback(drawList, &cmd);
            }
            else
            {
                output_data::drawcall drawcall;
                size_t texId = reinterpret_cast<size_t>(cmd.TextureId);
                drawcall.texture_id = static_cast<kernel_services::TexID>(texId);
                drawcall.mesh_id = g_ui_meshes[li];
                drawcall.offset = idxOffs;
                drawcall.count = idxCnt;
                drawcall.id = output_data::UI;
                drawcall.layer_id = 0xFF;

                g_drawcalls.push_back(drawcall);

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

}

#include "vandalism.cpp"

const char *g_roman_numerals[11] =
{
    "0", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"
};

Vandalism *g_ism = nullptr;

bool gui_showAllViews;
i32 gui_viewIdx;
i32 gui_tool;
color4f gui_brush_color;
i32 gui_brush_diameter_units;
i32 gui_brush_spread_units;
i32 gui_brush_angle;
bool gui_mouse_occupied;
bool gui_mouse_hover;
bool gui_fake_pressure;
bool gui_debug_smoothing;
bool gui_debug_draw_rects;
bool gui_debug_draw_disks;
bool gui_draw_simplify;
i32 gui_draw_smooth;
i32 gui_present_smooth;
color3f gui_background_color;
color3f gui_grid_bg_color;
color3f gui_grid_fg_color;
bool gui_grid_enabled;
float gui_eraser_alpha;
float gui_smooth_error_order;
i32 gui_goto_idx;
bool gui_layer_active[255];
i32 gui_layer_cnt;
i32 gui_current_layer;

u32 g_dclogidx = 0;
const u32 DCLOGENTRIES = 10;
std::string g_dclog[DCLOGENTRIES];
u32 g_dclogtimes[DCLOGENTRIES] = {0};

const i32 cfg_min_brush_diameter_units = 1;
const i32 cfg_max_brush_diameter_units = 64;
const i32 cfg_def_brush_diameter_units = 4;
const float cfg_brush_diameter_inches_per_unit = 1.0f / 64.0f;
const i32 cfg_max_strokes_per_buffer = 8192;
const float cfg_depth_step = 1.0f / cfg_max_strokes_per_buffer;
const char* cfg_font_path = "Roboto_Condensed/RobotoCondensed-Regular.ttf";
const char* cfg_default_image_file = "default_image.jpg";
const char* cfg_default_capture_file = "default_capture.png";
const char* cfg_default_file = "default.ism";

const float cfg_capture_width_in = 4.0f;
const float cfg_capture_height_in = 4.0f;

ImGuiTextBuffer *g_viewsBuf;

std::vector<output_data::Vertex> g_bake_quads;
std::vector<output_data::Vertex> g_curr_quads;

void setup(kernel_services *services, u32 nLayers)
{
    ImGuiIO& io = ImGui::GetIO();
    io.RenderDrawListsFn = RenderImGuiDrawLists;
    g_viewsBuf = new ImGuiTextBuffer;

    u8 *pixels;
    i32 width, height;

	if (services->check_file(cfg_font_path))
	{
		ImFontConfig config;
		config.OversampleH = 3;
		config.OversampleV = 3;
		io.Fonts->AddFontFromFileTTF(cfg_font_path, 16.0f, &config);
	}

    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    g_font_texture_id = services->create_texture(static_cast<u32>(width),
                                                 static_cast<u32>(height),
                                                 4);
    services->update_texture(g_font_texture_id, pixels);
    io.Fonts->TexID =
    reinterpret_cast<void *>(static_cast<size_t>(g_font_texture_id));

    g_services = services;

    const u32 BAKE_QUADS_CNT = 5000;
    const u32 CURR_QUADS_CNT = 500;

    g_bake_quads.reserve(BAKE_QUADS_CNT * 4);
    g_curr_quads.reserve(CURR_QUADS_CNT * 4);

    g_bake_mesh =
    g_services->create_quad_mesh(*g_services->stroke_vertex_layout,
                                 BAKE_QUADS_CNT * 4);
    g_curr_mesh =
    g_services->create_quad_mesh(*g_services->stroke_vertex_layout,
                                 CURR_QUADS_CNT * 4);
    g_lines_mesh =
    g_services->create_quad_mesh(*g_services->ui_vertex_layout, 16);

    io.Fonts->ClearInputData();
    io.Fonts->ClearTexData();

    g_ism = new Vandalism();
    g_ism->setup();

    gui_showAllViews = true;
    gui_viewIdx = 0;
    gui_tool = static_cast<i32>(Vandalism::DRAW);

    gui_brush_color = { 1.0, 1.0f, 1.0f, 1.0f };
    gui_background_color = { 0.3f, 0.3f, 0.3f };
    gui_grid_bg_color = { 0.0f, 0.0f, 0.5f };
    gui_grid_fg_color = { 0.5f, 0.5f, 1.0f };

    gui_grid_enabled = false;

    gui_brush_diameter_units = cfg_def_brush_diameter_units;
    gui_brush_spread_units = 0;

    gui_eraser_alpha = 1.0f;
    gui_smooth_error_order = -3.0f;

    gui_mouse_occupied = false;
    gui_mouse_hover = false;
    gui_fake_pressure = false;
    gui_debug_smoothing = false;

    gui_debug_draw_disks = true;
    gui_debug_draw_rects = true;

    gui_draw_simplify = false;
    gui_draw_smooth = static_cast<i32>(Vandalism::NONE);
    gui_present_smooth = static_cast<i32>(Vandalism::NONE);

    gui_goto_idx = 0;

    for (u8 i = 0; i < nLayers; ++i)
    {
        gui_layer_active[i] = true;
    }
    gui_current_layer = 0;
    gui_layer_cnt = static_cast<i32>(nLayers);

    g_image_fitting = false;
    g_image_capturing = INACTIVE;
}

void cleanup()
{
    g_ism->clear();
    clear_loaded_images();

    delete g_ism;

    for (size_t i = 0; i < g_ui_meshes.size(); ++i)
    {
        g_services->delete_mesh(g_ui_meshes[i]);
    }

    g_services->delete_mesh(g_bake_mesh);
    g_services->delete_mesh(g_curr_mesh);
    g_services->delete_mesh(g_lines_mesh);

    g_ui_meshes.clear();
    g_drawcalls.clear();

    g_services->delete_texture(g_font_texture_id);

    delete g_viewsBuf;

    ImGui::Shutdown();
}

// TODO: optimize with triangle fans/strips maybe?

void add_quad(std::vector<output_data::Vertex> &quads,
              float2 a, float2 b, float2 c, float2 d,
              float zindex, float ec,
              float rc, float gc, float bc, float ac,
              float u0, float u1)
{
    output_data::Vertex v;

    v.z = zindex;
    v.e = ec;
    v.r = rc; v.g = gc; v.b = bc; v.a = ac;

    v.x = a.x; v.y = a.y; v.u = u0; v.v = 0.0f; quads.push_back(v);
    v.x = b.x; v.y = b.y; v.u = u0; v.v = 1.0f; quads.push_back(v);
    v.x = c.x; v.y = c.y; v.u = u1; v.v = 1.0f; quads.push_back(v);
    v.x = d.x; v.y = d.y; v.u = u1; v.v = 0.0f; quads.push_back(v);
}

void add_rect(std::vector<output_data::Vertex>& quads,
              float2 p0, float2 p1, float2 side0, float2 side1,
              const basis2s& tform,
              const Masterpiece::Brush &brush,
              float zindex)
{
    float2 A = point_in_basis(tform, p0 + side0);
    float2 B = point_in_basis(tform, p0 - side0);
    float2 C = point_in_basis(tform, p1 - side1);
    float2 D = point_in_basis(tform, p1 + side1);

    add_quad(quads,
             A, B, C, D, zindex, (brush.type == 1 ? brush.color.a : 0.0f),
             brush.color.r, brush.color.g, brush.color.b, (brush.type == 1 ? 0.0f : brush.color.a),
             0.5f, 0.5f);
}

void add_disk(std::vector<output_data::Vertex>& quads,
              float2 center, float2 right, float2 up,
              const basis2s& tform,
              const Masterpiece::Brush &brush,
              float zindex)
{
    float2 bl = center - right - up;
    float2 br = center + right - up;
    float2 tl = center - right + up;
    float2 tr = center + right + up;

    float2 A = point_in_basis(tform, bl);
    float2 B = point_in_basis(tform, tl);
    float2 C = point_in_basis(tform, tr);
    float2 D = point_in_basis(tform, br);

    add_quad(quads,
             A, B, C, D, zindex, (brush.type == 1 ? brush.color.a : 0.0f),
             brush.color.r, brush.color.g, brush.color.b, (brush.type == 1 ? 0.0f : brush.color.a),
             0.0f, 1.0f);
}

void modify_brush(Masterpiece::Brush b, float w)
{
    b.color.a *= w;
    b.diameter *= w;
    b.spread *= w;
}

void fill_quads(std::vector<output_data::Vertex>& quads,
                const test_point *points, size_t N,
                size_t si,
                const basis2s& tform,
                const Masterpiece::Brush& brush,
                float depthStep)
{
    if (N == 0)
    {
        return;
    }

    float zindex = depthStep * (si + 1);

    if (gui_debug_draw_rects)
    {
        Masterpiece::Brush b1 = brush;
        modify_brush(b1, points[0].w);

        if (b1.spread > 0.0)
        {
            float2 spr1n = {si_cosf(b1.angle), si_sinf(b1.angle)};
            float2 spr1 = b1.spread * spr1n;
            float2 side = 0.5f * b1.diameter * perp(spr1n);

            float2 p1 = {points[0].x, points[0].y};
            add_rect(quads, p1 - spr1, p1 + spr1,
                     side, side,
                     tform, b1, zindex);
        }

        // rectangles between points
        for (size_t i = 1; i < N; ++i)
        {
            float2 prevC = {points[i-1].x, points[i-1].y};
            float2 currC = {points[i].x, points[i].y};
            float2 dirC = currC - prevC;

            if (len(dirC) > 0.001f)
            {
                Masterpiece::Brush cb0 = brush;
                modify_brush(cb0, points[i-1].w);
                Masterpiece::Brush cb1 = brush;
                modify_brush(cb1, points[i].w);

                if (cb0.spread > 0.0 || cb1.spread > 0.0)
                {
                    float2 spr0n = {si_cosf(cb0.angle), si_sinf(cb0.angle)};
                    float2 spr0 = cb0.spread * spr0n;

                    float2 spr1n = {si_cosf(cb1.angle), si_sinf(cb1.angle)};
                    float2 spr1 = cb1.spread * spr1n;

                    float2 prevL = prevC - spr0;
                    float2 currL = currC - spr1;
                    float2 dirL = currL - prevL;
                    // TODO: this is inaccurate,
                    // multiplying this with prev and curr sizes
                    // will not produce real tangents to circles
                    float2 sideL = norm(perp(dirL));

                    // TODO: interp color/alpha across rect
                    // left edge
                    add_rect(quads, prevL, currL,
                             sideL * cb0.diameter * 0.5f,
                             sideL * cb1.diameter * 0.5f,
                             tform, cb0, zindex);

                    float2 prevR = prevC + spr0;
                    float2 currR = currC + spr1;
                    float2 dirR = currR - prevR;
                    // TODO: this is inaccurate,
                    // multiplying this with prev and curr sizes
                    // will not produce real tangents to circles
                    float2 sideR = norm(perp(dirR));

                    // TODO: interp color/alpha across rect
                    // right edge
                    add_rect(quads, prevR, currR,
                             sideR * cb0.diameter * 0.5f,
                             sideR * cb1.diameter * 0.5f,
                             tform, cb0, zindex);

                    // TODO: interp color/alpha across rect
                    // fill
                    add_rect(quads, prevC, currC,
                             spr0, spr1,
                             tform, cb0, zindex);

                    // across edge
                    float2 side = 0.5f * cb1.diameter * perp(spr1n);
                    add_rect(quads, currC - spr1, currC + spr1,
                             side, side,
                             tform, cb1, zindex);
                }
                else
                {
                    float2 side = norm(perp(dirC));
                    add_rect(quads, prevC, currC,
                             side * cb0.diameter * 0.5f,
                             side * cb1.diameter * 0.5f,
                             tform, brush, zindex);
                }
            }
        }
    }

    if (gui_debug_draw_disks)
    {
        // disks at points
        for (size_t i = 0; i < N; ++i)
        {
            Masterpiece::Brush cb = brush;
            modify_brush(cb, points[i].w);

            float2 curr = {points[i].x, points[i].y};
            float2 right = {0.5f * cb.diameter, 0.0f};
            float2 up = {0.0, 0.5f * cb.diameter};

            if (cb.spread > 0.0)
            {
                float2 side = {cb.spread * si_cosf(cb.angle),
                               cb.spread * si_sinf(cb.angle)};

                add_disk(quads, curr + side, right, up,
                         tform, cb, zindex);

                add_disk(quads, curr - side, right, up,
                         tform, cb, zindex);
            }
            else
            {
                add_disk(quads, curr, right, up,
                         tform, cb, zindex);
            }
        }
    }
}

void stroke_to_quads(const test_point* begin, const test_point* end,
                     std::vector<output_data::Vertex>& quads,
                     size_t stroke_id, const basis2s& tform,
                     const Masterpiece::Brush& brush)
{
    static Masterpiece::Brush s_debug_brush =
    {
        0.02f, 0.0f, 0.0f,
        {1.0f, 0.0f, 0.0f, 1.0f},
        false
    };
    static std::vector<test_point> s_sampled_points;

    if (gui_present_smooth == Vandalism::FITBEZIER ||
        gui_present_smooth == Vandalism::HERMITE)
    {
        s_sampled_points.clear();
        smooth_stroke(begin, static_cast<size_t>(end - begin),
                      s_sampled_points,
                      si_powf(10.0f, gui_smooth_error_order),
                      gui_present_smooth == Vandalism::FITBEZIER);

        if (gui_debug_smoothing)
        {
            fill_quads(quads, begin, static_cast<size_t>(end - begin),
                       stroke_id, tform, s_debug_brush,
                       cfg_depth_step);
        }

        fill_quads(quads,
                   s_sampled_points.data(), s_sampled_points.size(),
                   stroke_id, tform, brush,
                   cfg_depth_step);
    }
    else
    {
        fill_quads(quads, begin, static_cast<size_t>(end - begin),
                   stroke_id, tform, brush,
                   cfg_depth_step);
    }
}

void collect_bake_data(const test_data& bake_data,
                       float width_in, float height_in,
                       float pixel_height_in,
                       u8 layer_id)
{
    static std::vector<test_visible> s_visibles;
    static std::vector<basis2s> s_transforms;

    s_visibles.clear();
    s_transforms.clear();
    box2 viewbox = {-0.5f * width_in,
                    +0.5f * width_in,
                    -0.5f * height_in,
                    +0.5f * height_in};

    query(layer_id, bake_data, g_ism->currentView,
          viewbox, s_visibles, s_transforms,
          pixel_height_in);

    for (u32 visIdx = 0; visIdx < s_visibles.size(); ++visIdx)
    {
        const test_visible& vis = s_visibles[visIdx];
        const basis2s& tform = s_transforms[vis.tform_id];
        if (vis.ty == test_visible::STROKE)
        {
            const test_stroke& s = bake_data.strokes[vis.obj_id];
            const Masterpiece::Brush& brush = g_ism->art.get_brush(s.brush_id);

            size_t before = g_bake_quads.size();
            if (g_drawcalls.empty() ||
                g_drawcalls.back().id != output_data::BAKEBATCH ||
                (g_drawcalls.back().id == output_data::BAKEBATCH &&
                 g_drawcalls.back().layer_id != layer_id))
            {
                output_data::drawcall dc;
                dc.id = output_data::BAKEBATCH;
                dc.mesh_id = g_bake_mesh;
                dc.texture_id = 0; // not used
                dc.offset = static_cast<u32>(before / 4 * 6);
                dc.count = 0;
                dc.layer_id = layer_id;

                g_drawcalls.push_back(dc);
            }

            stroke_to_quads(bake_data.points + s.pi0,
                            bake_data.points + s.pi1,
                            g_bake_quads, vis.obj_id, tform, brush);
            size_t after = g_bake_quads.size();

            size_t idxCnt = (after - before) / 4 * 6;
            g_drawcalls.back().count += static_cast<u32>(idxCnt);
        }
        else if (vis.ty == test_visible::IMAGE)
        {
            const test_image &img = bake_data.images[vis.obj_id];

            output_data::drawcall dc;
            dc.id = output_data::IMAGE;
            dc.mesh_id = 0; // not used
            dc.texture_id = g_loaded_images[img.nameidx].texid;
            dc.offset = 0; // not used
            dc.count = 0; // not used
            dc.layer_id = layer_id;

            float2 pos = point_in_basis(tform, img.basis.o);

            float2 ox = point_in_basis(tform, img.basis.o + img.basis.x);
            float2 oy = point_in_basis(tform, img.basis.o + img.basis.y);

            // TODO: this is ugly
            dc.params[0] = pos.x;
            dc.params[1] = pos.y;

            dc.params[2] = ox.x - pos.x;
            dc.params[3] = ox.y - pos.y;

            dc.params[4] = oy.x - pos.x;
            dc.params[5] = oy.y - pos.y;

            g_drawcalls.push_back(dc);
        }
    }

    if (s_visibles.empty())
    {
        output_data::drawcall dc;
        dc.id = output_data::BAKEBATCH;
        dc.mesh_id = g_bake_mesh;
        dc.texture_id = 0; // not used
        dc.offset = 0;
        dc.count = 0;
        dc.layer_id = layer_id;

        g_drawcalls.push_back(dc);
    }

    std::cout << "layer #" << static_cast<u32>(layer_id) << " update mesh: " << s_visibles.size() << " visibles" << std::endl;
}

bool load_image(const char *filename, ImageDesc &desc)
{
    if (g_services->check_file(filename))
    {
        i32 image_w, image_h, image_comp;
        float *image_data = stbi_loadf(filename, &image_w, &image_h, &image_comp, 4);
        if (image_comp > 0 && image_w > 0 && image_h > 0 && image_data != nullptr)
        {
            desc.width = static_cast<u32>(image_w);
            desc.height = static_cast<u32>(image_h);
            size_t pixel_cnt = desc.width * desc.height * 4;
            std::vector<u8> udata(pixel_cnt);
            const float *pIn = image_data;
            u8 *pOut = udata.data();
            for (size_t i = 0; i < pixel_cnt; ++i)
            {
                *(pOut++) = static_cast<u8>(*(pIn++) * 255.0f);
            }
            stbi_image_free(image_data);

            desc.texid = g_services->create_texture(desc.width, desc.height, 4);
            g_services->update_texture(desc.texid, udata.data());
            return true;
        }
    }
    return false;
}

void build_view_dbg_buffer(ImGuiTextBuffer *buffer, const test_data &bake_data)
{
    buffer->clear();
    std::stringstream ss;
    for (u32 vi = 0; vi < bake_data.nviews; ++vi)
    {
        const auto &view = bake_data.views[vi];

        ss << '#' << vi << " L:" << view.li;

        float2 t = view.tr.o;
        float s = len(view.tr.x);
        float a = si_atan2(view.tr.x.y, view.tr.x.x);

        if (s == 1.0f && t.x == 0.0f && t.y == 0.0f && a == 0.0f)
            ss << " ID";
        else if (s == 1.0f && a == 0.0f)
            ss << " PAN " << t.x << ',' << t.y;
        else if (t.x == 0.0f && t.y == 0.0f && a == 0.0f)
            ss << " ZOOM " <<  s;
        else if (s == 1.0f && t.x == 0.0f && t.y == 0.0f)
            ss << " ROTATE " << 180.0f * a / 3.1415926535f;
        else ss << " COMPLEX " << t.x << ',' << t.y
                         << " /" << 180.0f * a / 3.1415926535f
                         << " x" << s;

        if (view.has_strokes())
            ss << " [" << view.si0 << ".." << view.si1 << ')';

        if (view.has_image())
            ss << " i:" << view.ii;

        ss << "\n";
    }
    buffer->append("%s", ss.str().c_str());
}

void update_and_render(input_data *input, output_data *output)
{
    output->quit_flag = false;

    g_drawcalls.clear();

    float mxnorm = input->vpMouseXPt / input->vpWidthPt - 0.5f;
    float mynorm = input->vpMouseYPt / input->vpHeightPt - 0.5f;

    float mxin = input->vpWidthIn * mxnorm;
    float myin = input->vpHeightIn * mynorm;

    bool mouse_in_ui = gui_mouse_occupied || gui_mouse_hover;

    float pixel_height_in = input->vpHeightIn / input->vpHeightPx;

    Vandalism::Input ism_input;
    ism_input.tool = (input->shiftkey ? Vandalism::PAN : static_cast<Vandalism::Tool>(gui_tool));
    ism_input.mousePos.x = mxin;
    ism_input.mousePos.y = -myin;
    ism_input.negligibledistance = pixel_height_in;
    ism_input.mousedown = input->mouseleft && !mouse_in_ui;
    ism_input.fakepressure = gui_fake_pressure;
    ism_input.brushcolor = gui_brush_color;
    ism_input.eraseralpha = gui_eraser_alpha;
    ism_input.brushdiameter =
    gui_brush_diameter_units * cfg_brush_diameter_inches_per_unit;
    ism_input.brushangle = 3.1415926535f * gui_brush_angle / 180.0f;
    ism_input.brushspread =
    gui_brush_spread_units * cfg_brush_diameter_inches_per_unit;
    ism_input.scrolly = input->scrollY;
    ism_input.scrolling = input->scrolling;
    ism_input.simplify = gui_draw_simplify;
    ism_input.smooth = static_cast<Vandalism::Smooth>(gui_draw_smooth);

    // TODO: move all input to this point
    g_ism->set_current_layer(static_cast<u8>(gui_current_layer));
    g_ism->update(&ism_input);

    bool scrollViewsDown = false;

    const test_data &bake_data = g_ism->get_bake_data();

    if (input->forceUpdate || g_ism->visiblesChanged)
    {
        // TODO: this should not happen once per every stroke
        // implement some intermediate 'uncommited' strokes

        // flag processed
        // TODO: make this better
        g_ism->visiblesChanged = false;

        g_bake_quads.clear();

        // TODO: collect only active layers
        for (u8 layerIdx = 0; layerIdx < gui_layer_cnt; ++layerIdx)
        {
            collect_bake_data(bake_data,
                              input->rtWidthIn, input->rtHeightIn,
                              pixel_height_in,
                              layerIdx);
        }

        u32 vtxCnt = static_cast<u32>(g_bake_quads.size());

        g_services->update_mesh_vb(g_bake_mesh,
                                   g_bake_quads.data(), vtxCnt);
        
        build_view_dbg_buffer(g_viewsBuf, bake_data);
        scrollViewsDown = true;
    }

    output->preTranslate  = g_ism->preShift;
    output->postTranslate = g_ism->postShift;
    output->scale          = g_ism->zoomCoeff;
    output->rotate         = g_ism->rotateAngle;

    output->bg_color = gui_background_color;
    output->grid_bg_color = gui_grid_bg_color;
    output->grid_fg_color = gui_grid_fg_color;

    output->zbandwidth = cfg_depth_step;

    if (input->forceUpdate || g_ism->currentChanged)
    {
        // TODO: this overwrites whole stroke on each update
        // maybe it can be only appended?

        // TODO: flag processed, improve this
        g_ism->currentChanged = false;

        const test_data& current_data = g_ism->get_current_data();
        const Masterpiece::Brush& currBrush = g_ism->get_current_brush();
        size_t currStrokeId = g_ism->get_current_stroke_id();

        g_curr_quads.clear();

        const Masterpiece::Stroke& stroke = current_data.strokes[0];

        stroke_to_quads(current_data.points + stroke.pi0,
                        current_data.points + stroke.pi1,
                        g_curr_quads,
                        currStrokeId, default_basis(), currBrush);

        u32 vtxCnt = static_cast<u32>(g_curr_quads.size());
        u32 idxCnt = vtxCnt / 4 * 6;

        g_services->update_mesh_vb(g_curr_mesh,
                                   g_curr_quads.data(), vtxCnt);

        output_data::drawcall dc;
        dc.id = output_data::CURRENTSTROKE;
        dc.mesh_id = g_curr_mesh;
        dc.texture_id = 0; // not used
        dc.offset = 0;
        dc.count = idxCnt;
        dc.layer_id = g_ism->get_current_layer_id();

        g_drawcalls.push_back(dc);
    }

    // Draw UI -----------------------------------------------------------------

    float uiResHor = static_cast<float>(input->vpWidthPt);
    float uiResVer = static_cast<float>(input->vpHeightPt);

    float fXPx = (g_ism->firstPos.x / input->vpWidthIn + 0.5f) * uiResHor;
    float fYPx = (0.5f - g_ism->firstPos.y / input->vpHeightIn) * uiResVer;

    // TODO: do not update this each frame
    ImDrawVert vtx;
    vtx.col = 0xFF5555FF;
    vtx.uv = ImGui::GetIO().Fonts->TexUvWhitePixel;

    g_lines_vb.clear();
    vtx.pos = ImVec2(fXPx-5.0f, fYPx-0.5f); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(fXPx-5.0f, fYPx+0.5f); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(fXPx+5.0f, fYPx+0.5f); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(fXPx+5.0f, fYPx-0.5f); g_lines_vb.push_back(vtx);

    vtx.pos = ImVec2(fXPx-0.5f, fYPx-5.0f); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(fXPx+0.5f, fYPx-5.0f); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(fXPx+0.5f, fYPx+5.0f); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(fXPx-0.5f, fYPx+5.0f); g_lines_vb.push_back(vtx);

    float cX0 = (0.5f - 0.5f * cfg_capture_width_in / input->vpWidthIn) * uiResHor;
    float cX1 = (0.5f + 0.5f * cfg_capture_width_in / input->vpWidthIn) * uiResHor;

    float cY0 = (0.5f + 0.5f * cfg_capture_height_in / input->vpHeightIn) * uiResVer;
    float cY1 = (0.5f - 0.5f * cfg_capture_height_in / input->vpHeightIn) * uiResVer;

    vtx.col = 0xFF0000FF;
    vtx.pos = ImVec2(cX0-0.5f, cY0); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(cX0+0.5f, cY0); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(cX0+0.5f, cY1); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(cX0-0.5f, cY1); g_lines_vb.push_back(vtx);

    vtx.pos = ImVec2(cX1-0.5f, cY0); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(cX1+0.5f, cY0); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(cX1+0.5f, cY1); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(cX1-0.5f, cY1); g_lines_vb.push_back(vtx);

    vtx.pos = ImVec2(cX0, cY0-0.5f); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(cX0, cY0+0.5f); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(cX1, cY0+0.5f); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(cX1, cY0-0.5f); g_lines_vb.push_back(vtx);

    vtx.pos = ImVec2(cX0, cY1-0.5f); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(cX0, cY1+0.5f); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(cX1, cY1+0.5f); g_lines_vb.push_back(vtx);
    vtx.pos = ImVec2(cX1, cY1-0.5f); g_lines_vb.push_back(vtx);

    g_services->update_mesh_vb(g_lines_mesh, g_lines_vb.data(), 24);

    output_data::drawcall lines_drawcall;
    lines_drawcall.texture_id = g_font_texture_id;
    lines_drawcall.mesh_id = g_lines_mesh;
    lines_drawcall.offset = 0;
    lines_drawcall.count = 12;
    lines_drawcall.id = output_data::UI;
    lines_drawcall.layer_id = 0xFF;

    g_drawcalls.push_back(lines_drawcall);

    // Draw ImGui --------------------------------------------------------------

    ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(uiResHor, uiResVer);
    io.DeltaTime = 0.01666666f;
	io.MousePos = ImVec2(input->vpMouseXPt, input->vpMouseYPt);
    io.MouseDown[0] = input->mouseleft;

	struct Timing
	{
		static float time_getter(void *data, int idx)
		{
			const double *ts = static_cast<input_data*>(data)->pTimeIntervals;
			return static_cast<float>(ts[idx]);
		}
	};

	ImGui::NewFrame();

    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.Colors[ImGuiCol_Text]             = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        style.Colors[ImGuiCol_FrameBg]          = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
        style.Colors[ImGuiCol_TitleBg]          = ImVec4(0.4f, 0.5f, 0.1f, 1.0f);
        style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.5f, 0.5f, 0.3f, 0.5f);
        style.Colors[ImGuiCol_TitleBgActive]    = ImVec4(0.4f, 0.5f, 0.0f, 1.0f);
        style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.4f, 0.5f, 0.1f, 1.0f);
        style.Colors[ImGuiCol_Button]           = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
        style.Colors[ImGuiCol_ButtonHovered]    = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
        style.Colors[ImGuiCol_ButtonActive]     = ImVec4(0.4f, 0.5f, 0.1f, 1.0f);

        style.AntiAliasedLines = false;
        style.AntiAliasedShapes = false;
        style.WindowPadding = ImVec2(4, 4);
        style.WindowRounding = 0;
        style.WindowTitleAlign = ImGuiAlign_Center;
        style.FramePadding = ImVec2(3, 2);
        style.ItemInnerSpacing = ImVec2(3, 3);
        style.ItemSpacing = ImVec2(3, 3);
        style.ScrollbarRounding = 0;
        style.WindowFillAlphaDefault = 0.9f;
    }

	if (input->nTimeIntervals > 1)
	{
		ImGui::Begin("performance");
		ImGui::PlotHistogram("frame time", Timing::time_getter, input,
                             static_cast<int>(input->nTimeIntervals));
		ImGui::End();
	}

    ImGui::SetNextWindowSize(ImVec2(100, 200), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("vandalism");

    // TOOLS
    ImGui::RadioButton("draw", &gui_tool, static_cast<i32>(Vandalism::DRAW));
    ImGui::SameLine();
    ImGui::RadioButton("erase", &gui_tool, static_cast<i32>(Vandalism::ERASE));
    if (gui_tool == Vandalism::DRAW || gui_tool == Vandalism::ERASE)
    {
        if (gui_tool == Vandalism::DRAW)
        {
            ImGui::ColorEdit4("color", &gui_brush_color.r);
        }
        else
        {
            ImGui::SliderFloat("eraser", &gui_eraser_alpha, 0.0f, 1.0f);
        }
        ImGui::SliderInt("diameter", &gui_brush_diameter_units,
                         cfg_min_brush_diameter_units, cfg_max_brush_diameter_units);
        ImGui::SliderInt("spread", &gui_brush_spread_units,
                         cfg_min_brush_diameter_units, cfg_max_brush_diameter_units);
        ImGui::SliderInt("angle", &gui_brush_angle, 0, 180);
    }
    ImGui::RadioButton("pan", &gui_tool, static_cast<i32>(Vandalism::PAN));
    ImGui::SameLine();
    ImGui::RadioButton("zoom", &gui_tool, static_cast<i32>(Vandalism::ZOOM));
    ImGui::SameLine();
    ImGui::RadioButton("rotate", &gui_tool, static_cast<i32>(Vandalism::ROTATE));
    ImGui::RadioButton("move center", &gui_tool, static_cast<i32>(Vandalism::FIRST));
    ImGui::SameLine();
    ImGui::RadioButton("pivot", &gui_tool, static_cast<i32>(Vandalism::SECOND));

    Undoee undoee = g_ism->undoable();
    const char *undolabels[] = { "", "Undo last stroke", "Undo image insertion", "Undo view change" };
    if (undoee != NOTHING)
    {
        ImGui::SameLine();
        if (ImGui::Button(undolabels[undoee]))
        {
            g_ism->undo();
        }
    }

    ImGui::Separator();
    // BACKGROUND

    ImGui::ColorEdit3("background", &gui_background_color.r);
    ImGui::Checkbox("grid", &gui_grid_enabled);
    if (gui_grid_enabled)
    {
        ImGui::ColorEdit3("grid bg", &gui_grid_bg_color.r);
        ImGui::ColorEdit3("grid fg", &gui_grid_fg_color.r);

        output_data::drawcall dc;
        dc.id = output_data::GRID;
        dc.texture_id = 0; // not used
        dc.mesh_id = 0; // not used
        dc.offset = 0; // not used
        dc.count = 0; // not used
        dc.layer_id = 0xFF;

        g_drawcalls.push_back(dc);
    }

    ImGui::Separator();
    // STROKE SETTINGS

    ImGui::Checkbox("fake pressure", &gui_fake_pressure);
    ImGui::SameLine();
    ImGui::Checkbox("rectangles", &gui_debug_draw_rects);
    ImGui::SameLine();
    ImGui::Checkbox("disks", &gui_debug_draw_disks);

    ImGui::Separator();
    // SMOOTHING/SIMPLIFICATION

    ImGui::Text("drawing");
    ImGui::Checkbox("simplify", &gui_draw_simplify);
    ImGui::Text("smooth: ");
    ImGui::SameLine();
    ImGui::RadioButton("none", &gui_draw_smooth,
                       static_cast<i32>(Vandalism::NONE));
    ImGui::SameLine();
    ImGui::RadioButton("hermite", &gui_draw_smooth,
                       static_cast<i32>(Vandalism::HERMITE));
    ImGui::SameLine();
    ImGui::RadioButton("fit bezier", &gui_draw_smooth,
                       static_cast<i32>(Vandalism::FITBEZIER));
    ImGui::Separator();
    ImGui::Text("rendering");
    ImGui::PushID("rendering");
    ImGui::Text("smooth: ");
    ImGui::SameLine();
    ImGui::RadioButton("none", &gui_present_smooth,
                       static_cast<i32>(Vandalism::NONE));
    ImGui::SameLine();
    ImGui::RadioButton("hermite", &gui_present_smooth,
                       static_cast<i32>(Vandalism::HERMITE));
    ImGui::SameLine();
    ImGui::RadioButton("fit bezier", &gui_present_smooth,
                       static_cast<i32>(Vandalism::FITBEZIER));
    if (gui_present_smooth == Vandalism::FITBEZIER)
    {
        ImGui::Checkbox("debug smoothing", &gui_debug_smoothing);
        ImGui::SliderFloat("error order", &gui_smooth_error_order, -7.0f, 0.0f);
    }
    ImGui::PopID();

    ImGui::Separator();
    // LAYERS

    for (u8 i = 0; i < gui_layer_cnt; ++i)
    {
        if (i > 0) ImGui::SameLine();
        ImGui::Checkbox(g_roman_numerals[i], &gui_layer_active[i]); 
    }

    ImGui::PushID("curlayer");
    for (u8 i = 0; i < gui_layer_cnt; ++i)
    {
        if (i > 0) ImGui::SameLine();
        ImGui::RadioButton(g_roman_numerals[i], &gui_current_layer, i);
    }
    ImGui::PopID();

    ImGui::Separator();
    // BUTTONS

    if (g_ism->currentMode == Vandalism::IDLE)
    {
        if (ImGui::Button("Clear canvas"))
        {
            g_ism->clear();
            clear_loaded_images();
        }
        ImGui::SameLine();
        if (ImGui::Button("Show all strokes"))
        {
            g_ism->show_all(input->vpWidthIn, input->vpHeightIn);
        }

        ImGui::Text("[%s]", cfg_default_file);
        ImGui::SameLine();
        if (ImGui::Button("Save"))
        {
            g_ism->save_data(cfg_default_file, g_loaded_image_names);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load"))
        {
            if (g_services->check_file(cfg_default_file))
            {
                clear_loaded_images();

                g_ism->load_data(cfg_default_file, g_loaded_image_names);

                for (size_t i = 0; i < g_loaded_image_names.size(); ++i)
                {
                    ImageDesc desc;
                    if (load_image(g_loaded_image_names[i].c_str(), desc))
                    {
                        g_loaded_images.push_back(desc);
                    }
                    else
                    {
                        // TODO: some indication of missing file
                        ImageDesc invalid_desc{ g_font_texture_id, 0, 0 };
                        g_loaded_images.push_back(invalid_desc);
                    }
                }
            }
            else
            {
                ::printf("nothing to load, there is no default save file\n");
            }
        }

        ImGui::Text("[%s]", cfg_default_capture_file);
        if (g_image_capturing == INACTIVE)
        {
            ImGui::SameLine();
            if (ImGui::Button("Capture"))
            {
                g_image_capturing = SELECTION;
            }
        }
        else if (g_image_capturing == SELECTION)
        {
            output_data::drawcall dc;
            dc.texture_id = g_font_texture_id;
            dc.mesh_id = g_lines_mesh;
            dc.offset = 12;
            dc.count = 24;
            dc.id = output_data::UI;
            dc.layer_id = 0xFF;

            g_drawcalls.push_back(dc);

            ImGui::SameLine();
            if (ImGui::Button("Cancel capture"))
            {
                g_image_capturing = INACTIVE;
            }

            ImGui::SameLine();
            if (ImGui::Button("Save capture"))
            {
                output_data::drawcall captdc;
                captdc.id = output_data::CAPTURE;
                captdc.params[0] = cfg_capture_width_in;
                captdc.params[1] = cfg_capture_height_in;
                captdc.texture_id = 0;
                captdc.mesh_id = 0;
                captdc.offset = 0;
                captdc.count = 0;
                captdc.layer_id = static_cast<u8>(gui_current_layer);

                g_drawcalls.push_back(captdc);

                g_image_capturing = CAPTURE;
            }
        }
        else if (g_image_capturing == CAPTURE)
        {
            u32 width, height;
            const u8 *data = g_services->get_capture_data(width, height);
            stbi_write_png(cfg_default_capture_file,
                           static_cast<i32>(width),
                           static_cast<i32>(height),
                           4, data, 0);
            g_image_capturing = INACTIVE;
        }

        ImGui::Text("[%s]", cfg_default_image_file);
        if (!g_image_fitting)
        {
            ImGui::SameLine();
            if (ImGui::Button("Insert image"))
            {
                g_fit_img.name = cfg_default_image_file;
                size_t ism_img_name_idx = image_name_idx(g_fit_img.name.c_str());
                if (ism_img_name_idx < g_loaded_image_names.size())
                {
                    const ImageDesc &desc = g_loaded_images[ism_img_name_idx];
                    float image_aspect = static_cast<float>(desc.height) / static_cast<float>(desc.width);

                    g_fit_img.texid = desc.texid;
                    g_fit_img.width_in = 0.5f * input->vpWidthIn;
                    g_fit_img.height_in = g_fit_img.width_in * image_aspect;
                    g_fit_img.reuse = true;

                    g_image_fitting = true;
                }
                else
                {
                    ImageDesc desc;
                    if (load_image(g_fit_img.name.c_str(), desc))
                    {
                        g_loaded_images.push_back(desc);
                        float image_aspect = static_cast<float>(desc.height) / static_cast<float>(desc.width);

                        g_fit_img.texid = desc.texid;
                        g_fit_img.width_in = 4.0f;
                        g_fit_img.height_in = g_fit_img.width_in * image_aspect;
                        g_fit_img.reuse = false;

                        g_image_fitting = true;
                    }
                }
            }
        }
        else
        {
            ImGui::SameLine();
            if (ImGui::Button("Cancel fitting"))
            {
                if (!g_fit_img.reuse)
                {
                    g_services->delete_texture(g_fit_img.texid);
                }
                g_fit_img = CurrentImage();

                g_image_fitting = false;
            }

            ImGui::SameLine();
            if (ImGui::Button("Place image"))
            {
                g_ism->place_image(image_name_idx(g_fit_img.name.c_str()),
                                   g_fit_img.width_in, g_fit_img.height_in);

                g_image_fitting = false;
            }
        }
    }

    if (g_image_fitting)
    {
        output_data::drawcall dc;
        dc.texture_id = g_fit_img.texid;
        dc.id = output_data::IMAGEFIT;
        dc.layer_id = g_ism->get_current_layer_id();

        dc.params[0] = -0.5f * g_fit_img.width_in;
        dc.params[1] = -0.5f * g_fit_img.height_in;

        dc.params[2] = g_fit_img.width_in;
        dc.params[3] = 0.0f;

        dc.params[4] = 0.0f;
        dc.params[5] = g_fit_img.height_in;

        g_drawcalls.push_back(dc);
    }

    output_data::drawcall pdc;
    ::memset(&pdc, 0, sizeof(output_data::drawcall));
    pdc.id = output_data::PRESENT;

    for (u8 i = 0; i < gui_layer_cnt; ++i)
    {
        if (gui_layer_active[i])
        {
            pdc.layer_id = i;
            g_drawcalls.push_back(pdc);
        }
    }
    
    output->quit_flag = ImGui::Button("Quit");

    if (!g_ism->append_allowed())
    {
        ImGui::TextColored(ImColor(1.0f, 0.0f, 0.0f), "read only mode");
    }

    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(200, 300), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("views");
    ImGui::SliderInt("view", &gui_goto_idx, 0,
                     static_cast<i32>(bake_data.nviews-1));
    if (ImGui::Button("Goto view"))
    {
        g_ism->set_view(static_cast<size_t>(gui_goto_idx));
    }
    ImGui::TextUnformatted(g_viewsBuf->begin());
    if (scrollViewsDown)
    {
        ImGui::SetScrollHere(1.0f);
    }
    ImGui::End();

    ImGui::SetNextWindowSize(ImVec2(200, 300), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("debug");
    ImGui::Text("mouse-inch: (%g, %g)", mxin, myin);
    ImGui::Text("mouse-norm: (%g, %g)", mxnorm, mynorm);
    ImGui::Text("mouse-ui-pt: (%g, %g)", input->vpMouseXPt, input->vpMouseYPt);
    ImGui::Text("mouse-raw-pt: (%g, %g)", input->rawMouseXPt, input->rawMouseYPt);

    ImGui::Text("tool active: %d", static_cast<i32>(ism_input.mousedown));

    ImGui::Separator();

    ImGui::Text("shift-inch: (%g, %g)", g_ism->postShift.x, g_ism->postShift.y);
    ImGui::Text("zoom-coeff: %g", g_ism->zoomCoeff);
    ImGui::Text("rotate-angle: %g", g_ism->rotateAngle);

    ImGui::Separator();

    // TODO: multilayers support
    ImGui::Text("ism strokes: %d", static_cast<u32>(g_ism->art.num_strokes()));
    ImGui::Text("ism points: %d", static_cast<u32>(g_ism->art.num_points()));
    ImGui::Text("ism brushes: %d", static_cast<u32>(g_ism->art.num_brushes()));

    ImGui::Text("bake_quads v: (%d/%d)",
                static_cast<u32>(g_bake_quads.size()),
                static_cast<u32>(g_bake_quads.capacity()));

    ImGui::Text("curr_quads v: (%d/%d)",
                static_cast<u32>(g_curr_quads.size()),
                static_cast<u32>(g_curr_quads.capacity()));

    ImGui::Text("mode: %d", g_ism->currentMode);

    ImGui::Text("mouse occupied: %d", static_cast<i32>(gui_mouse_occupied));
    ImGui::Text("mouse hover: %d", static_cast<i32>(gui_mouse_hover));

    ImGui::Separator();
    ImGui::Text("vp size %d x %d pt", input->vpWidthPt, input->vpHeightPt);
    ImGui::Text("vp size %d x %d px", input->vpWidthPx, input->vpHeightPx);
    ImGui::Text("vp size %g x %g in", input->vpWidthIn, input->vpHeightIn);
    ImGui::Text("win size %d x %d pt",
                input->windowWidthPt, input->windowHeightPt);
    ImGui::Text("win size %d x %d px",
                input->windowWidthPx, input->windowHeightPx);
    ImGui::Text("win pos %d, %d pt",
                input->windowPosXPt, input->windowPosYPt);

    ImGui::Separator();
    std::stringstream ds;
    for (size_t i = 0; i < g_drawcalls.size(); ++i)
    {
        u32 lid = g_drawcalls[i].layer_id;
        switch (g_drawcalls[i].id)
        {
        case output_data::UI: ds << "U"; break;
        case output_data::IMAGE: ds << "I" << lid; break;
        case output_data::BAKEBATCH: ds << "B" << lid; break;
        case output_data::CURRENTSTROKE: ds << "S" << lid; break;
        case output_data::GRID: ds << "G"; break;
        case output_data::IMAGEFIT: ds << "F" << lid; break;
        case output_data::CAPTURE: ds << "C" << lid; break;
        case output_data::PRESENT: ds << "P" << lid; break;
        }
        if (i + 1 < g_drawcalls.size())
        {
            ds << " ";
        }
    }
    if (ds.str() != g_dclog[g_dclogidx % DCLOGENTRIES])
    {
        ++g_dclogidx;
        g_dclog[g_dclogidx % DCLOGENTRIES] = ds.str();
        g_dclogtimes[g_dclogidx % DCLOGENTRIES] = 1;
    }
    else
    {
        ++g_dclogtimes[g_dclogidx % DCLOGENTRIES];
    }
    for (u32 i = 1; i <= DCLOGENTRIES; ++i)
    {
        u32 id = (g_dclogidx + i) % DCLOGENTRIES;
        ImGui::Text("%s (%d times)", g_dclog[id].c_str(), g_dclogtimes[id]);
    }

    ImGui::End();
    ImGui::Render();

    gui_mouse_occupied = ImGui::IsAnyItemActive();
    gui_mouse_hover = ImGui::IsMouseHoveringAnyWindow();

    output->drawcalls = g_drawcalls.data();
    output->drawcall_cnt = static_cast<u32>(g_drawcalls.size());
}
