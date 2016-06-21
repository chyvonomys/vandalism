#include <vector>
#include "client.h"
#include "math.h"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wfloat-equal"
#include "imgui/imgui.h"
#pragma clang diagnostic pop
#else
#include "imgui/imgui.h"
#endif

kernel_services *current_services = 0;
output_data *current_output = 0;

kernel_services::TexID font_texture_id;

std::vector<kernel_services::MeshID> ui_meshes;
std::vector<output_data::drawcall> ui_drawcalls;

kernel_services::MeshID lines_mesh;
kernel_services::MeshID bake_mesh;
kernel_services::MeshID curr_mesh;

std::vector<ImDrawVert> lines_vb;

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

        if (ui_meshes.size() <= li)
        {
            kernel_services::MeshID mid =
            current_services->create_mesh(*current_services->ui_vertex_layout,
                                          vtxCount, idxCount);
            ui_meshes.push_back(mid);
        }

        current_services->update_mesh(ui_meshes[li],
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
    current_output->ui_drawcall_cnt = static_cast<u32>(ui_drawcalls.size());
}

#include "vandalism.cpp"

Vandalism *ism = nullptr;

bool gui_showAllViews;
i32 gui_viewIdx;
i32 gui_tool;
float gui_brush_color[4];
i32 gui_brush_diameter_units;
bool gui_mouse_occupied;
bool gui_mouse_hover;
bool gui_fake_pressure;
bool gui_debug_smoothing;
bool gui_debug_draw_rects;
bool gui_debug_draw_disks;
bool gui_debug_antialiasing;
bool gui_draw_simplify;
i32 gui_draw_smooth;
i32 gui_present_smooth;
float gui_background_color[3];
float gui_grid_bg_color[3];
float gui_grid_fg_color[3];
float gui_eraser_alpha;
float gui_smooth_error_order;
i32 gui_goto_idx;

const i32 cfg_min_brush_diameter_units = 1;
const i32 cfg_max_brush_diameter_units = 64;
const i32 cfg_def_brush_diameter_units = 4;
const float cfg_brush_diameter_inches_per_unit = 1.0f / 64.0f;
const float cfg_depth_step = 1.0f / 10000.0f;
const char* cfg_font_path = "Roboto_Condensed/RobotoCondensed-Regular.ttf";
const char* cfg_default_file = "default.ism";

ImGuiTextBuffer *viewsBuf;

std::vector<output_data::Vertex> bake_quads;
std::vector<output_data::Vertex> curr_quads;

// temp vectors, reuse to avoid reallocations
std::vector<test_point> s_sampled_points;
std::vector<test_visible> s_visibles;
std::vector<test_transform> s_transforms;

void setup(kernel_services *services)
{
    ImGuiIO& io = ImGui::GetIO();
    io.RenderDrawListsFn = RenderImGuiDrawLists;
    viewsBuf = new ImGuiTextBuffer;

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

    font_texture_id = services->create_texture(static_cast<u32>(width),
                                               static_cast<u32>(height),
                                               4);
    services->update_texture(font_texture_id, pixels);
    io.Fonts->TexID =
    reinterpret_cast<void *>(static_cast<size_t>(font_texture_id));

    current_services = services;

    const u32 BAKE_QUADS_CNT = 5000;
    const u32 CURR_QUADS_CNT = 500;

    bake_quads.reserve(BAKE_QUADS_CNT * 4);
    curr_quads.reserve(CURR_QUADS_CNT * 4);

    bake_mesh =
    current_services->create_quad_mesh(*current_services->stroke_vertex_layout,
                                       BAKE_QUADS_CNT * 4);
    curr_mesh =
    current_services->create_quad_mesh(*current_services->stroke_vertex_layout,
                                       CURR_QUADS_CNT * 4);
    lines_mesh =
    current_services->create_quad_mesh(*current_services->ui_vertex_layout, 16);

    io.Fonts->ClearInputData();
    io.Fonts->ClearTexData();

    ism = new Vandalism();
    ism->setup();

    gui_showAllViews = true;
    gui_viewIdx = 0;
    gui_tool = static_cast<i32>(Vandalism::DRAW);

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
    gui_smooth_error_order = -3.0f;

    gui_mouse_occupied = false;
    gui_mouse_hover = false;
    gui_fake_pressure = false;
    gui_debug_smoothing = false;

    gui_debug_draw_disks = true;
    gui_debug_draw_rects = true;
    gui_debug_antialiasing = true;

    gui_draw_simplify = false;
    gui_draw_smooth = static_cast<i32>(Vandalism::NONE);
    gui_present_smooth = static_cast<i32>(Vandalism::NONE);

    gui_goto_idx = 0;
}

void cleanup()
{
    ism->cleanup();
    delete ism;

    for (size_t i = 0; i < ui_meshes.size(); ++i)
    {
        current_services->delete_mesh(ui_meshes[i]);
    }

    current_services->delete_mesh(bake_mesh);
    current_services->delete_mesh(curr_mesh);
    current_services->delete_mesh(lines_mesh);

    ui_meshes.clear();
    ui_drawcalls.clear();

    current_services->delete_texture(font_texture_id);

    delete viewsBuf;

    ImGui::Shutdown();
}

// TODO: optimize with triangle fans/strips maybe?

void add_quad(std::vector<output_data::Vertex> &quads,
              test_point a, test_point b, test_point c, test_point d,
              float zindex, float ec,
              float rc, float gc, float bc, float ac,
              float u0, float u1)
{
    output_data::Vertex v;

    v.z = zindex;
    v.e = ec;
    v.r = rc; v.g = gc; v.b = bc; v.a = ac; v.p = 0.0f;

    v.x = a.x; v.y = a.y; v.u = u0; v.v = 0.0f; quads.push_back(v);
    v.x = b.x; v.y = b.y; v.u = u0; v.v = 1.0f; quads.push_back(v);
    v.x = c.x; v.y = c.y; v.u = u1; v.v = 1.0f; quads.push_back(v);
    v.x = d.x; v.y = d.y; v.u = u1; v.v = 0.0f; quads.push_back(v);
}

void fill_quads(std::vector<output_data::Vertex>& quads,
                const test_point *points, size_t N,
                size_t si,
                const test_transform& tform,
                const Vandalism::Brush& brush,
                float depthStep)
{
    float zindex = depthStep * si;

    size_t v0idx = quads.size();

    if (gui_debug_draw_rects)
	// rectangles between points
	for (size_t i = 1; i < N; ++i)
	{
		float2 prev = {points[i-1].x, points[i-1].y};
		float2 curr = {points[i].x, points[i].y};
		float2 dir = curr - prev;

		Vandalism::Brush cb0 = brush.modified(points[i-1].w);

		if (len(dir) > 0.001f)
		{
			Vandalism::Brush cb1 = brush.modified(points[i].w);

			float2 side = perp(dir * (1.0f / len(dir)));

			float2 side0 = 0.5f * cb0.diameter * side;
			float2 side1 = 0.5f * cb1.diameter * side;

			// x -ccw-> y
			float2 p0l = prev + side0;
			float2 p0r = prev - side0;
			float2 p1l = curr + side1;
			float2 p1r = curr - side1;

			test_point a = apply_transform_pt(tform, {p0r.x, p0r.y});
			test_point b = apply_transform_pt(tform, {p0l.x, p0l.y});
			test_point c = apply_transform_pt(tform, {p1l.x, p1l.y});
			test_point d = apply_transform_pt(tform, {p1r.x, p1r.y});

			bool eraser = (cb1.type == 1);

			// TODO: interpolate color/erase/alpha across quad
			add_quad(quads,
				a, b, c, d, zindex, (eraser ? cb1.a : 0.0f),
				cb1.r, cb1.g, cb1.b, (eraser ? 0.0f : cb1.a),
				0.5f, 0.5f);
		}
	}

    if (gui_debug_draw_disks)
	// disks at points
	for (size_t i = 0; i < N; ++i)
	{
		Vandalism::Brush cb = brush.modified(points[i].w);

		float2 curr = {points[i].x, points[i].y};
		float2 right = {0.5f * cb.diameter, 0.0f};
		float2 up = {0.0, 0.5f * cb.diameter};
		float2 bl = curr - right - up;
		float2 br = curr + right - up;
		float2 tl = curr - right + up;
		float2 tr = curr + right + up;

		test_point a = apply_transform_pt(tform, {bl.x, bl.y});
		test_point b = apply_transform_pt(tform, {tl.x, tl.y});
		test_point c = apply_transform_pt(tform, {tr.x, tr.y});
		test_point d = apply_transform_pt(tform, {br.x, br.y});

		bool eraser = (cb.type == 1);

		add_quad(quads,
			a, b, c, d, zindex, (eraser ? cb.a : 0.0f),
			cb.r, cb.g, cb.b, (eraser ? 0.0f : cb.a),
			0.0f, 1.0f);
	}

    size_t v1idx = quads.size();

    for (size_t i = v0idx; i < v1idx; ++i)
    {
        quads.push_back(quads[i]);
        quads.back().p = 1.0f;
    }

    size_t v2idx = quads.size();

    if (gui_debug_antialiasing)
    {
        for (size_t i = v1idx; i < v2idx; ++i)
        {
            quads[i].r = 1.0f;
            quads[i].g = 0.2f;
            quads[i].b = 0.2f;
        }
    }
}

void stroke_to_quads(const test_point* begin, const test_point* end,
                     std::vector<output_data::Vertex>& quads,
                     size_t stroke_id, const test_transform& tform,
                     const Vandalism::Brush& brush)
{
    static Vandalism::Brush debug_brush
    {
        0.02f,
        1.0f, 0.0f, 0.0f, 1.0f,
        0
    };

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
                       stroke_id, tform, debug_brush,
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


void update_and_render(input_data *input, output_data *output)
{
    output->quit_flag = false;
    output->bake_flag = false;
    output->change_flag = false;

    float mxnorm = input->vpMouseXPt / input->vpWidthPt - 0.5f;
    float mynorm = input->vpMouseYPt / input->vpHeightPt - 0.5f;

    float mxin = input->vpWidthIn * mxnorm;
    float myin = input->vpHeightIn * mynorm;

    bool mouse_in_ui = gui_mouse_occupied || gui_mouse_hover;

    float pixel_height_in = input->vpHeightIn / input->vpHeightPx;

    Vandalism::Input ism_input;
    ism_input.tool = static_cast<Vandalism::Tool>(gui_tool);
    ism_input.mousex = mxin;
    ism_input.mousey = -myin;
    ism_input.negligibledistance = pixel_height_in;
    ism_input.mousedown = input->mouseleft && !mouse_in_ui;
    ism_input.fakepressure = gui_fake_pressure;
    ism_input.brushred = gui_brush_color[0];
    ism_input.brushgreen = gui_brush_color[1];
    ism_input.brushblue = gui_brush_color[2];
    ism_input.brushalpha = gui_brush_color[3];
    ism_input.eraseralpha = gui_eraser_alpha;
    ism_input.brushdiameter =
    gui_brush_diameter_units * cfg_brush_diameter_inches_per_unit;
    ism_input.scrolly = input->scrollY;
    ism_input.scrolling = input->scrolling;
    ism_input.simplify = gui_draw_simplify;
    ism_input.smooth = static_cast<Vandalism::Smooth>(gui_draw_smooth);

    ism->update(&ism_input);

    const test_data &bake_data = ism->get_bake_data();

    bool scrollViewsDown = false;

    if (ism->visiblesChanged)
    {
        // flag processed
        // TODO: make this better
        ism->visiblesChanged = false;

        bake_quads.clear();
        s_visibles.clear();
        s_transforms.clear();
        test_box viewbox = {-0.5f * input->rtWidthIn,
                            +0.5f * input->rtWidthIn,
                            -0.5f * input->rtHeightIn,
                            +0.5f * input->rtHeightIn};

        query(bake_data, ism->currentViewIdx,
              viewbox, s_visibles, s_transforms,
              pixel_height_in);

        for (u32 visIdx = 0; visIdx < s_visibles.size(); ++visIdx)
        {
            const test_visible& vis = s_visibles[visIdx];
            const test_transform& tform = s_transforms[vis.ti];
            const test_stroke& stroke = bake_data.strokes[vis.si];
            const Vandalism::Brush& brush = ism->brushes[stroke.brush_id];

            stroke_to_quads(bake_data.points + stroke.pi0,
                            bake_data.points + stroke.pi1,
                            bake_quads, vis.si, tform, brush);
        }

        u32 vtxCnt = static_cast<u32>(bake_quads.size());
        u32 idxCnt = vtxCnt / 4 * 6;

        current_services->update_mesh_vb(bake_mesh,
                                         bake_quads.data(), vtxCnt);
        output->bake_flag = true;
        output->bgmesh = bake_mesh;
        output->bgmeshCnt = idxCnt;

        ::printf("update mesh: %lu visibles\n", s_visibles.size());

        viewsBuf->clear();
        for (u32 vi = 0; vi < bake_data.nviews; ++vi)
        {
            const auto &view = bake_data.views[vi];
            auto t = view.tr.type;
            const char *typestr = (t == TZOOM ? "ZOOM" :
                                   (t == TPAN ? "PAN" :
                                    (t == TROTATE ? "ROTATE" : "ERROR")));

            bool isPinned = (view.pin_index != NPOS);
            bool isCurrent = (vi == ism->currentViewIdx);
            viewsBuf->append("%c %c %d: %s  %f/%f  (%ld..%ld)\n",
                             (isCurrent ? '>' : ' '), (isPinned ? '*' : ' '),
                             vi, typestr, view.tr.a, view.tr.b,
                             view.si0, view.si1);
        }
        scrollViewsDown = true;
    }

    output->preTranslateX  = ism->preShiftX;
    output->preTranslateY  = ism->preShiftY;
    output->postTranslateX = ism->postShiftX;
    output->postTranslateY = ism->postShiftY;
    output->scale          = ism->zoomCoeff;
    output->rotate         = ism->rotateAngle;

    output->bg_red   = gui_background_color[0];
    output->bg_green = gui_background_color[1];
    output->bg_blue  = gui_background_color[2];

    output->grid_bg_color[0] = gui_grid_bg_color[0];
    output->grid_bg_color[1] = gui_grid_bg_color[1];
    output->grid_bg_color[2] = gui_grid_bg_color[2];

    output->grid_fg_color[0] = gui_grid_fg_color[0];
    output->grid_fg_color[1] = gui_grid_fg_color[1];
    output->grid_fg_color[2] = gui_grid_fg_color[2];

    if (ism->currentChanged)
    {
        // TODO: flag processed, improve this
        ism->currentChanged = false;

        const test_data& current_data = ism->get_current_data();
        const Vandalism::Brush& currBrush = ism->get_current_brush();
        size_t currStrokeId = ism->get_current_stroke_id();

        curr_quads.clear();

        const Vandalism::Stroke& stroke = current_data.strokes[0];

        stroke_to_quads(current_data.points + stroke.pi0,
                        current_data.points + stroke.pi1,
                        curr_quads,
                        currStrokeId, id_transform(), currBrush);

        u32 vtxCnt = static_cast<u32>(curr_quads.size());
        u32 idxCnt = vtxCnt / 4 * 6;

        current_services->update_mesh_vb(curr_mesh,
                                         curr_quads.data(), vtxCnt);
        output->fgmesh = curr_mesh;
        output->fgmeshCnt = idxCnt;

        output->change_flag = true;
    }

    // Draw UI -----------------------------------------------------------------

    float uiResHor = static_cast<float>(input->vpWidthPt);
    float uiResVer = static_cast<float>(input->vpHeightPt);

    float fXPx = (ism->firstX / input->vpWidthIn + 0.5f) * uiResHor;
    float fYPx = (0.5f - ism->firstY / input->vpHeightIn) * uiResVer;

    ImDrawVert vtx;
    vtx.col = 0xFF5555FF;
    vtx.uv = ImGui::GetIO().Fonts->TexUvWhitePixel;

    lines_vb.clear();
    vtx.pos = ImVec2(fXPx-5.0f, fYPx-0.5f); lines_vb.push_back(vtx);
    vtx.pos = ImVec2(fXPx-5.0f, fYPx+0.5f); lines_vb.push_back(vtx);
    vtx.pos = ImVec2(fXPx+5.0f, fYPx+0.5f); lines_vb.push_back(vtx);
    vtx.pos = ImVec2(fXPx+5.0f, fYPx-0.5f); lines_vb.push_back(vtx);

    vtx.pos = ImVec2(fXPx-0.5f, fYPx-5.0f); lines_vb.push_back(vtx);
    vtx.pos = ImVec2(fXPx+0.5f, fYPx-5.0f); lines_vb.push_back(vtx);
    vtx.pos = ImVec2(fXPx+0.5f, fYPx+5.0f); lines_vb.push_back(vtx);
    vtx.pos = ImVec2(fXPx-0.5f, fYPx+5.0f); lines_vb.push_back(vtx);

    current_services->update_mesh_vb(lines_mesh, lines_vb.data(), 8);

    ui_drawcalls.clear();

    output_data::drawcall lines_drawcall;
    lines_drawcall.texture_id = font_texture_id;
    lines_drawcall.mesh_id = lines_mesh;
    lines_drawcall.offset = 0;
    lines_drawcall.count = 12;

    ui_drawcalls.push_back(lines_drawcall);

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
    ImGui::ColorEdit4("color", gui_brush_color);
    ImGui::ColorEdit3("background", gui_background_color);
    ImGui::SliderInt("diameter", &gui_brush_diameter_units,
                     cfg_min_brush_diameter_units, cfg_max_brush_diameter_units);
    ImGui::SliderFloat("eraser", &gui_eraser_alpha, 0.0f, 1.0f);
    ImGui::Separator();
    ImGui::Checkbox("pressure", &gui_fake_pressure);
    ImGui::Checkbox("rects", &gui_debug_draw_rects);
    ImGui::Checkbox("disks", &gui_debug_draw_disks);
    ImGui::Checkbox("antialiasing", &gui_debug_antialiasing);
    ImGui::Separator();
    ImGui::Text("drawing");
    ImGui::Checkbox("simplify", &gui_draw_simplify);
    ImGui::RadioButton("smooth none", &gui_draw_smooth,
                       static_cast<i32>(Vandalism::NONE));
    ImGui::RadioButton("smooth hermite", &gui_draw_smooth,
                       static_cast<i32>(Vandalism::HERMITE));
    ImGui::RadioButton("smooth fit bezier", &gui_draw_smooth,
                       static_cast<i32>(Vandalism::FITBEZIER));
    ImGui::Separator();
    ImGui::Text("rendering");
    ImGui::PushID("rendering");
    ImGui::RadioButton("smooth none", &gui_present_smooth,
                       static_cast<i32>(Vandalism::NONE));
    ImGui::RadioButton("smooth hermite", &gui_present_smooth,
                       static_cast<i32>(Vandalism::HERMITE));
    ImGui::RadioButton("smooth fit bezier", &gui_present_smooth,
                       static_cast<i32>(Vandalism::FITBEZIER));
    if (gui_present_smooth == static_cast<i32>(Vandalism::FITBEZIER))
    {
        ImGui::Checkbox("debug smoothing", &gui_debug_smoothing);
        ImGui::SliderFloat("error order", &gui_smooth_error_order, -7.0f, 0.0f);
    }
    ImGui::PopID();
    ImGui::Separator();
    ImGui::ColorEdit3("grid bg", gui_grid_bg_color);
    ImGui::ColorEdit3("grid fg", gui_grid_fg_color);
    ImGui::Separator();
    ImGui::RadioButton("draw", &gui_tool, static_cast<i32>(Vandalism::DRAW));
    ImGui::RadioButton("erase", &gui_tool, static_cast<i32>(Vandalism::ERASE));
    ImGui::RadioButton("pan", &gui_tool, static_cast<i32>(Vandalism::PAN));
    ImGui::RadioButton("zoom", &gui_tool, static_cast<i32>(Vandalism::ZOOM));
    ImGui::RadioButton("rot", &gui_tool, static_cast<i32>(Vandalism::ROTATE));
    ImGui::RadioButton("first", &gui_tool, static_cast<i32>(Vandalism::FIRST));
    ImGui::RadioButton("second", &gui_tool, static_cast<i32>(Vandalism::SECOND));

    if (ism->currentMode == Vandalism::IDLE)
    {
        if (ImGui::Button("Save"))
        {
            ism->save_data(cfg_default_file);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load"))
        {
            if (current_services->check_file(cfg_default_file))
            {
                ism->load_data(cfg_default_file);
            }
            else
            {
                ::printf("nothing to load, there is no default save file\n");
            }
        }
        if (ImGui::Button("Optimize views"))
        {
            ism->optimize_views();
        }
        ImGui::SameLine();
        if (ImGui::Button("Show all strokes"))
        {
            ism->show_all(input->vpWidthIn, input->vpHeightIn);
        }
        if (ism->undoable() && ImGui::Button("Undo"))
        {
            ism->undo();
        }
        if (ImGui::Button("Clear"))
        {
            ism->clear();
        }
        if (ImGui::Button("Place image"))
        {
            ism->place_image(0, 1.0f, 1.0f);
        }
        ImGui::SameLine();
        output->quit_flag = ImGui::Button("Quit");
    }

    if (!ism->append_allowed())
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
        ism->set_view(static_cast<size_t>(gui_goto_idx));
    }
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
    ImGui::Text("mouse-ui-pt: (%g, %g)", input->vpMouseXPt, input->vpMouseYPt);
    ImGui::Text("mouse-raw-pt: (%g, %g)", input->rawMouseXPt, input->rawMouseYPt);

    ImGui::Text("tool active: %d", static_cast<i32>(ism_input.mousedown));

    ImGui::Separator();

    ImGui::Text("shift-inch: (%g, %g)", ism->postShiftX, ism->postShiftY);
    ImGui::Text("zoom-coeff: %g", ism->zoomCoeff);
    ImGui::Text("rotate-angle: %g", ism->rotateAngle);

    ImGui::Separator();

    ImGui::Text("ism strokes: %lu", ism->strokes.size());
    ImGui::Text("ism points: %lu", ism->points.size());
    ImGui::Text("ism brushes: %lu", ism->brushes.size());

    ImGui::Text("bake_quads v: (%lu/%lu) %d",
                bake_quads.size(),
                bake_quads.capacity(),
                output->bake_flag);

    ImGui::Text("curr_quads v: (%lu/%lu) %d",
                curr_quads.size(),
                curr_quads.capacity(),
                output->change_flag);

    ImGui::Text("mode: %d", ism->currentMode);

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

    ImGui::End();

    current_output = output;
    ImGui::Render();

    gui_mouse_occupied = ImGui::IsAnyItemActive();
    gui_mouse_hover = ImGui::IsMouseHoveringAnyWindow();
}
