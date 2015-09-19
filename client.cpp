#include "client.h"
#include "math.h"
#include "swcolor.h"
#include "swrender.h"

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

void fill_quads(triangles *tris,
                const std::vector<Vandalism::Point> &points,
                const Vandalism::Visible &vis)
{
    float *xyz = tris->data + tris->size * 3;
    float zindex = 0.00001f * vis.strokeIdx;
    
    for (size_t i = vis.startIdx + 1; i < vis.endIdx; ++i)
    {
        if (tris->size >= tris->capacity)
            break;

        float2 prev = {points[i-1].x, points[i-1].y};
        float2 curr = {points[i].x, points[i].y};
        float2 dir = curr - prev;
        if (len(dir) > 0.001f)
        {
            float2 side = perp(dir * (1.0 / len(dir)));
            side.x *= 0.01f;
            side.y *= 0.02f;

            // x -ccw-> y
            float2 p0l = prev + side;
            float2 p0r = prev - side;
            float2 p1l = curr + side;
            float2 p1r = curr - side;

            xyz[3 * 0 + 0] = p0l.x;
            xyz[3 * 0 + 1] = p0l.y;
            xyz[3 * 0 + 2] = zindex;

            xyz[3 * 1 + 0] = p1r.x;
            xyz[3 * 1 + 1] = p1r.y;
            xyz[3 * 1 + 2] = zindex;

            xyz[3 * 2 + 0] = p1l.x;
            xyz[3 * 2 + 1] = p1l.y;
            xyz[3 * 2 + 2] = zindex;

            xyz += 3 * 3;
            tris->size += 3;

            xyz[3 * 0 + 0] = p0l.x;
            xyz[3 * 0 + 1] = p0l.y;
            xyz[3 * 0 + 2] = zindex;

            xyz[3 * 1 + 0] = p0r.x;
            xyz[3 * 1 + 1] = p0r.y;
            xyz[3 * 1 + 2] = zindex;

            xyz[3 * 2 + 0] = p1r.x;
            xyz[3 * 2 + 1] = p1r.y;
            xyz[3 * 2 + 2] = zindex;

            xyz += 3 * 3;
            tris->size += 3;
        }
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
        for (uint32 ti = 0; ti < intervalCnt; ++ti)
        {
            uint32 yfrom = (sum / maxti) * maxy;
            sum += intervals[ti];
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
}


extern "C" void update_and_render(input_data *input, output_data *output)
{
    offscreen_buffer *buffer = output->buffer;

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
            fill_quads(output->bake_tris, ism->points, ism->visibles[visIdx]);
            //fill_triangles(output->bake_tris, ism->points, ism->visibles[visIdx]);
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

    fill_quads(output->curr_tris, ism->points, curr);
    //fill_triangles(output->curr_tris, ism->points, curr);


    // Draw SW -----------------------------------------------------------------

    if (firstTime)
    {
        clear_buffer(buffer, COLOR_GRAY);
        firstTime = false;
    }
    
    //draw_grayscale_image(buffer, 0, 200,
    //                     fontpixels, fontpixelswidth, fontpixelsheight);
    
    draw_timing(buffer, input->nFrames,
                input->pTimeIntervals, input->nTimeIntervals);

    

    draw_pixel(buffer, input->mousex, input->mousey, COLOR_YELLOW);

    current_buffer = buffer;


    // Draw ImGui --------------------------------------------------------------
    
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(buffer->width, buffer->height);
    io.DeltaTime = 0.01666666f;
    io.MousePos = ImVec2(input->mousex, buffer->height - 1 - input->mousey);
    io.MouseDown[0] = input->mouseleft;

    ImGui::NewFrame();

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

    ImGui::Render();
}
