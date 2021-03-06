#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#pragma warning(push)
#pragma warning(disable: 4668)
#include <Windows.h>
#pragma warning(pop)
#elif __APPLE__
#include <mach/mach_time.h>
#elif __linux__
#include <time.h>
#endif

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include "glcorearb.h"
#include <cstdio>
#include <cstring>

#include <vector>
#include <set>
#include <algorithm>
#include <sstream>
#include "client.h"

#include "gl.cpp"

bool g_printShaders = false;
bool g_printGLDiagnostics = false;
size_t g_quitFrame = 0;
bool g_noRetina = false;

struct FSQuad
{
    void setup();
    void draw();
    void cleanup();

    GLuint m_vao;
};

struct QuadIndexes
{
    void setup();
    void cleanup();

    void before();
    void after();

    GLuint m_ibuf;
};

struct FSTexturePresenter
{
    void setup(FSQuad *quad);
    void draw(GLuint tex, GLenum srcFactor, GLenum dstFactor,
              const float2 &pre, const float2 &post,
              float s, float a,
              float vpW, float vpH,
              float rtW, float rtH);
    void draw(GLuint tex, GLenum srcFactor, GLenum dstFactor);
    void draw(GLuint tex,
              GLenum srcFactorC, GLenum dstFactorC,
              GLenum srcFactorA, GLenum dstFactorA);
    void cleanup();

    FSQuad *m_quad;
    GLuint m_fullscreenProgram;
    GLuint m_simpleFullscreenProgram;
    GLint m_preTranslationLoc;
    GLint m_postTranslationLoc;
    GLint m_scaleLoc;
    GLint m_rotationLoc;
    GLint m_vpSizeLoc;
    GLint m_rtSizeLoc;
};

struct StrokeImageTech
{
    void setup(FSQuad *quad);
    void draw(GLuint tex, const basis2r &basis,
              float alpha, const float2 &scale);
    void cleanup();

    FSQuad *m_quad;
    GLuint m_fullscreenProgram;
    GLint m_posLoc;
    GLint m_axesLoc;
    GLint m_scaleLoc;
    GLint m_alphaLoc;
};

struct FSGrid
{
    void setup(FSQuad *quad);
    void draw(const color3f &bgcolor, const color3f &fgcolor, const float2 &scale);
    void cleanup();

    GLuint m_fullscreenProgram;
    GLint m_bgColorLoc;
    GLint m_fgColorLoc;
    GLint m_scaleLoc;
    FSQuad *m_quad;
};

struct RenderTarget
{
    void setup(u32 width, u32 height);
    void cleanup();

    void begin_receive();
    void end_receive();

    void store_image(u8 *storage);

    GLuint m_fbo;
    GLuint m_depth_rbo;
    GLuint m_tex;

    u32 m_width;
    u32 m_height;
};

struct RenderTargetMS
{
    void setup(u32 width, u32 height);
    void cleanup();

    void begin_receive();
    void end_receive();

    void resolve();

    GLuint m_fbo;
    GLuint m_depth_rbo;
    GLuint m_tex;

    u32 m_width;
    u32 m_height;
};

struct MarkerBatchTech
{
    void setup();
    void cleanup();

    void draw(kernel_services::MeshID mi,
              u32 offset, u32 count,
              const float2 &scale);

    GLuint m_program;
    GLint m_scaleLoc;
};

struct UIPresenter
{
    void setup();
    void cleanup();

    void draw(kernel_services::TexID ti, kernel_services::MeshID mi,
              u32 offset, u32 count, float vpW, float vpH);

    GLuint m_program;
    GLint m_vpSizeLoc;
};

struct Mesh
{
    GLuint vao;
    GLuint ibuf;
    GLuint vbuf;
    u32 vsize;
    u32 isize;
    bool quad_mesh;
};

// TODO: this runs always up
u32 next_mesh_idx = 0;
const u32 MAXMESHCNT = 20;
Mesh meshes[MAXMESHCNT];

struct Slot
{
    const char *name;
    GLuint loc;
    GLuint dim;
    GLuint size;
    GLenum type;
    GLboolean norm;
};

struct VertexLayout
{
    virtual ~VertexLayout() {}

    virtual size_t count() const = 0;
    virtual GLsizei offset_(size_t) const = 0;
    virtual const Slot& slot(size_t) const = 0;

    GLsizei total_size() const
    { return offset_(count()); }

    GLvoid* offset(size_t si) const
    { return make_gl_offset(offset_(si)); }
};

template <size_t N>
struct VertexLayoutN : public VertexLayout
{
    Slot slots[N];

    VertexLayoutN(std::initializer_list<Slot> in)
    {
        std::copy(in.begin(), in.end(), slots);
    }

    const Slot& slot(size_t i) const override
    { return slots[i]; }

    size_t count() const override
    { return N; }

    GLsizei offset_(size_t si) const override
    {
        GLsizei result = 0;
        for (size_t i = 0; i < si; ++i)
        {
            result += slots[i].dim * slots[i].size;
        }
        return result;
    }
};

VertexLayoutN<3> ui_vertex_layout =
{
    {"msPos", 0, 2, 4, GL_FLOAT,         false},
    {"uv",    1, 2, 4, GL_FLOAT,         false},
    {"color", 2, 4, 1, GL_UNSIGNED_BYTE, true}
};

VertexLayoutN<3> stroke_vertex_layout =
{
    {"msPosition", 0, 3, 4, GL_FLOAT, false},
    {"uve",        1, 3, 4, GL_FLOAT, false},
    {"color",      2, 4, 4, GL_FLOAT, false}
};

QuadIndexes quad_indexes;

u32 capture_data_width_px = 0;
u32 capture_data_height_px = 0;
std::vector<u8> capture_data;

void normalize_capture_data()
{
    u8 *start = capture_data.data();
    u8 *finish = start + capture_data.size();
    u8 *pixel = start;
    while (pixel != finish)
    {
        // NOTE: alpha channel contains amount of bg to add
        // 1-that is alpha value of the drawing
        u8 transparency = pixel[3];
        u8 opacity = 255 - transparency;
        if (opacity > 0)
        {
            // Unpremultiply
            float alpha = opacity / 255.0f;
            pixel[0] = static_cast<u8>((pixel[0] / 255.0f) / alpha * 255.0f);
            pixel[1] = static_cast<u8>((pixel[1] / 255.0f) / alpha * 255.0f);
            pixel[2] = static_cast<u8>((pixel[2] / 255.0f) / alpha * 255.0f);
        }
        pixel[3] = opacity;
        pixel += 4;
    }
    // Flip
    const size_t rowpitch = capture_data_width_px * 4;
    std::vector<u8> swapbuffer(rowpitch);
    u8 *swaprow = swapbuffer.data();
    for (size_t row = 0; row < capture_data_height_px / 2; ++row)
    {
        u8 *thisrow = capture_data.data() + row * rowpitch;
        u8 *thatrow = capture_data.data() + (capture_data_height_px - row - 1) * rowpitch;
        ::memcpy(swaprow, thisrow, rowpitch);
        ::memcpy(thisrow, thatrow, rowpitch);
        ::memcpy(thatrow, swaprow, rowpitch);
    }
    // Crop
    // TODO:
}

kernel_services::MeshID create_mesh_common(const VertexLayout& layout,
                                           u32 initialVCount,
                                           u32 initialICount,
                                           bool quad_mesh)
{
    Mesh &mesh = meshes[next_mesh_idx];

    u32 vertexSize = static_cast<u32>(layout.total_size());
    mesh.vsize = vertexSize;
    mesh.isize = 2;
    mesh.quad_mesh = quad_mesh;

    glGenVertexArrays(1, &mesh.vao);

    // vao setup
    glBindVertexArray(mesh.vao);

    glGenBuffers(1, &mesh.vbuf);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbuf);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(initialVCount * vertexSize),
                 nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbuf);

    for (size_t i = 0; i < layout.count(); ++i)
    {
        glVertexAttribPointer(layout.slot(i).loc,
                              static_cast<GLint>(layout.slot(i).dim),
                              layout.slot(i).type,
                              layout.slot(i).norm,
                              layout.total_size(),
                              layout.offset(i));
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    for (size_t i = 0; i < layout.count(); ++i)
    {
        glEnableVertexAttribArray(layout.slot(i).loc);
    }

    glBindVertexArray(0);

    if (mesh.quad_mesh)
    {
        mesh.ibuf = quad_indexes.m_ibuf;
    }
    else
    {
        glGenBuffers(1, &mesh.ibuf);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibuf);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(initialICount * mesh.isize),
                     nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    ++next_mesh_idx;

    return next_mesh_idx - 1;
}

void kernel_services::update_mesh_vb(kernel_services::MeshID mi,
                                     const void *vtxdata, u32 vtxcnt)
{
    glBindBuffer(GL_ARRAY_BUFFER, meshes[mi].vbuf);
    glBufferData(GL_ARRAY_BUFFER, vtxcnt * meshes[mi].vsize,
                 vtxdata, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void kernel_services::update_mesh(kernel_services::MeshID mi,
                                  const void *vtxdata, u32 vtxcnt,
                                  const u16 *idxdata, u32 idxcnt)
{
    update_mesh_vb(mi, vtxdata, vtxcnt);

    if (!meshes[mi].quad_mesh)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshes[mi].ibuf);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxcnt * meshes[mi].isize,
                     idxdata, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

kernel_services::MeshID
kernel_services::create_quad_mesh(const VertexLayout& layout,
                                  u32 initialVCount)
{
    return create_mesh_common(layout, initialVCount, 0, true);
}

kernel_services::MeshID
kernel_services::create_mesh(const VertexLayout& layout,
                             u32 initialVCount,
                             u32 initialICount)
{
    return create_mesh_common(layout, initialVCount, initialICount, false);
}

void kernel_services::delete_mesh(kernel_services::MeshID mi)
{
    glDeleteVertexArrays(1, &meshes[mi].vao);
    glDeleteBuffers(1, &meshes[mi].vbuf);
    if (!meshes[mi].quad_mesh)
    {
        glDeleteBuffers(1, &meshes[mi].ibuf);
    }
}

struct Texture
{
    bool active;
    GLuint glid;
    GLuint width;
    GLuint height;
    GLenum format;
    Texture() : active(false), glid(0), width(0), height(0), format(GL_RGBA) {}
};

const u32 MAXTEXCNT = 10;
Texture textures[MAXTEXCNT];

void create_texture_unsafe(u32 idx, u32 w, u32 h, u32 comp)
{
    Texture &tex = textures[idx];
    tex.active = true;
    tex.width = w;
    tex.height = h;
    tex.format = static_cast<GLenum>(comp == 1 ? GL_RED : GL_RGBA);
    glGenTextures(1, &tex.glid);

    glBindTexture(GL_TEXTURE_2D, tex.glid);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(tex.format),
        static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
        tex.format, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

kernel_services::TexID kernel_services::create_texture(u32 w, u32 h, u32 comp)
{
    u32 next_texture_idx = 0;
    for (; next_texture_idx < MAXTEXCNT; ++next_texture_idx)
    {
        if (!textures[next_texture_idx].active)
            break;
    }

    if (next_texture_idx < MAXTEXCNT)
    {
        create_texture_unsafe(next_texture_idx, w, h, comp);
        return next_texture_idx;
    }
    else
    {
        return kernel_services::default_texid;
    }
}

void update_texture_unsafe(kernel_services::TexID ti, const u8 *pixels)
{
    glBindTexture(GL_TEXTURE_2D, textures[ti].glid);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
        static_cast<GLsizei>(textures[ti].width),
        static_cast<GLsizei>(textures[ti].height),
        textures[ti].format, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// TODO: this is alpha only
void kernel_services::update_texture(kernel_services::TexID ti, const u8 *pixels)
{
    if (ti != kernel_services::default_texid)
    {
        update_texture_unsafe(ti, pixels);
    }
    else
    {
        ::printf("warning: attempt to update contents of default texture\n");
    }
}

void delete_texture_unsafe(kernel_services::TexID ti)
{
    glDeleteTextures(1, &textures[ti].glid);
    textures[ti] = Texture();
}

void kernel_services::delete_texture(kernel_services::TexID ti)
{
    if (ti < MAXTEXCNT)
    {
        // NOTE: do not delete default texture
        if (ti != kernel_services::default_texid)
        {
            delete_texture_unsafe(ti);
        }
    }
    else
    {
        ::printf("warning: attempt to delete default texture\n");
    }
}

const u8 *kernel_services::get_capture_data(u32 &w, u32 &h)
{
    w = capture_data_width_px;
    h = capture_data_height_px;
    return capture_data.data();
}

bool scroll_updated = false;
float scroll_y = 0.0f;
float cfg_fixed_scroll_step = 0.1f;

void scroll_callback(GLFWwindow *, double, double yscroll)
{
    scroll_updated = true;

    if (yscroll > 0.0)
        scroll_y += cfg_fixed_scroll_step;
    else if (yscroll < 0.0)
        scroll_y -= cfg_fixed_scroll_step;
}

#ifdef _WIN32
u64 get_platform_counter()
{
	LARGE_INTEGER v;
	QueryPerformanceCounter(&v);
	return static_cast<u64>(v.QuadPart);
}

double get_platform_counter_freq()
{
	LARGE_INTEGER f;
	QueryPerformanceFrequency(&f);
	return f.QuadPart / 1000.0;
}
#elif __APPLE__
u64 get_platform_counter()
{
	return mach_absolute_time();
}

double get_platform_counter_freq()
{
	mach_timebase_info_data_t time_info;
	mach_timebase_info(&time_info);

	return time_info.numer / time_info.denom * 1e6;
}
#elif __linux__
u64 get_platform_counter()
{
    timespec cnt;
    clock_gettime(CLOCK_MONOTONIC_RAW, &cnt);
    ::printf("COUNTER: %ldsec %ldnsec", cnt.tv_sec, cnt.tv_nsec);
}

double get_platform_counter_freq()
{
    timespec res;
    clock_getres(CLOCK_MONOTONIC_RAW, &res);
    ::printf("RESOLUTION: %ldsec %ldnsec", res.tv_sec, res.tv_nsec);
}
#endif

class signal_change_detector
{
public:
    signal_change_detector() : previous_value(false), current_value(false) {}

    void feed(bool value)
    {
        previous_value = current_value;
        current_value = value;
    }

    bool is_rising_edge() const
    {
        return current_value && !previous_value;
    }
private:
    bool previous_value;
    bool current_value;
};

struct Pipeline
{
    FSQuad quad;
    FSTexturePresenter fs;
    StrokeImageTech si;
    FSGrid grid;
    MarkerBatchTech markerTech;
    UIPresenter uirender;

    RenderTarget belowLayersRT;
    RenderTarget aboveLayersRT;
    RenderTarget currentLayerRT;

    RenderTarget tempRT;
    RenderTargetMS tempRTMS;

    // Make layer that contain premult color and alpha serve as amount of bg contain default correct values.
    void clearRT(RenderTarget &rt)
    {
        rt.begin_receive();
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        rt.end_receive();
    }

    void setup(u32 w, u32 h)
    {
        quad.setup();
        fs.setup(&quad);
        si.setup(&quad);
        grid.setup(&quad);
        markerTech.setup();
        uirender.setup();
        belowLayersRT.setup(w, h);
        aboveLayersRT.setup(w, h);
        currentLayerRT.setup(w, h);
        tempRT.setup(w, h);
        tempRTMS.setup(w, h);

        clearRT(belowLayersRT);
        clearRT(aboveLayersRT);
    }

    void cleanup()
    {
        tempRTMS.cleanup();
        tempRT.cleanup();
        currentLayerRT.cleanup();
        aboveLayersRT.cleanup();
        belowLayersRT.cleanup();
        uirender.cleanup();
        markerTech.cleanup();
        grid.cleanup();
        si.cleanup();
        fs.cleanup();
        quad.cleanup();
    }

    struct FrameInput
    {
        const StrokesDrawcall *strokesDCs;
        const ImageDrawcall *imageDCs;
        bool multisample_on;
        float2 in2rt;
        float2 in2vp;
    };

    // build layerRT with everything assigned to a given layer
    void build_layer(RenderTarget &rt, const LayerRecipe &recipe, FrameInput &fi)
    {
        if (fi.multisample_on)
        {
            tempRTMS.begin_receive();
        }
        else
        {
            rt.begin_receive();
        }

        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClearDepth(0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (u32 j = 0; j < recipe.items.size(); ++j)
        {
            const auto &it = recipe.items[j];
            if (it.type == LayerRecipe::ItemSpec::STROKEBATCH)
            {
                const auto &dc = fi.strokesDCs[it.index];
                markerTech.draw(dc.mesh, dc.offset, dc.count, fi.in2rt);
            }
            else if (it.type == LayerRecipe::ItemSpec::IMAGE)
            {
                const auto &dc = fi.imageDCs[it.index];
                si.draw(textures[dc.texture].glid,
                        dc.basis, 1.0f, fi.in2rt);
            }
        }
        if (fi.multisample_on)
        {
            tempRTMS.end_receive();
        }
        else
        {
            rt.end_receive();
        }

        if (fi.multisample_on)
        {
            rt.begin_receive();
            tempRTMS.resolve();
            // clear rt's depth
            // NOTE: each rt has its own depth
            glClearDepth(0.0);
            glClear(GL_DEPTH_BUFFER_BIT);
            rt.end_receive();
        }
    }
    
    void compose_layers(RenderTarget &composedRT, const std::vector<LayerRecipe> &layerRcps,
                        FrameInput &fi)
    {
        composedRT.begin_receive();
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        composedRT.end_receive();

        for (u32 i = 0; i < layerRcps.size(); ++i)
        {
            build_layer(tempRT, layerRcps[i], fi);
            composedRT.begin_receive();
            fs.draw(tempRT.m_tex,
                    GL_ONE, GL_SRC_ALPHA, GL_ZERO, GL_SRC_ALPHA);
            composedRT.end_receive();
        }
    }

    void make_frame(const input_data &input, const output_data &output,
                    bool gamma_on, bool multisample_on,
                    std::string &debugstr)
    {
        const RenderRecipe &recipe = *output.recipe;

        FrameInput fi;
        fi.strokesDCs = recipe.strokeDCs.data();
        fi.imageDCs = recipe.imageDCs.data();
        fi.multisample_on = multisample_on;
        fi.in2rt = { 2.0f / input.rtWidthIn, 2.0f / input.rtHeightIn };
        fi.in2vp = { 2.0f / input.vpWidthIn, 2.0f / input.vpHeightIn };

        std::stringstream dbgss;

        if (recipe.bakeBelow)
        {
            dbgss << "[*B]";
            compose_layers(belowLayersRT, recipe.belowBakery, fi);
        }

        if (recipe.bakeAbove)
        {
            dbgss << "[*A]";
            compose_layers(aboveLayersRT, recipe.aboveBakery, fi);
        }

        if (recipe.bakeCurrent)
        {
            dbgss << "[*C]";
            build_layer(currentLayerRT, recipe.currentBakery, fi);
        }

        if (recipe.currentStroke.count > 0)
        {
            dbgss << "[+C]";
            currentLayerRT.begin_receive();
            // NOTE: keep depth the same from previous time
            // if it was clear we would stack all iterations of
            // current stroke and transparent color would sum up.
            const auto &dc = recipe.currentStroke;
            markerTech.draw(dc.mesh, dc.offset, dc.count, fi.in2rt);
            currentLayerRT.end_receive();
        }
        
        if (recipe.capture)
        {
            dbgss << "[CAP]";
            tempRT.begin_receive();
            glClearColor(0.0, 0.0, 0.0, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);
            
            // TODO: maybe skip drawing these if bake baked nothing (empty layers)
            if (recipe.blitBelow)
                fs.draw(belowLayersRT.m_tex, GL_ONE, GL_SRC_ALPHA, GL_ZERO, GL_SRC_ALPHA);
            if (recipe.blitCurrent)
                fs.draw(currentLayerRT.m_tex, GL_ONE, GL_SRC_ALPHA, GL_ZERO, GL_SRC_ALPHA);
            if (recipe.blitAbove)
                fs.draw(aboveLayersRT.m_tex, GL_ONE, GL_SRC_ALPHA, GL_ZERO, GL_SRC_ALPHA);

            tempRT.end_receive();
            tempRT.store_image(capture_data.data());
            normalize_capture_data();
        }

        // ----------------------------------------------------------------

        if (gamma_on)
        {
            glEnable(GL_FRAMEBUFFER_SRGB);
        }

        if (recipe.drawGrid)
        {
            grid.draw(output.grid_bg_color, output.grid_fg_color, fi.in2vp);
        }
        else
        {
            glClearColor(output.bg_color.r,
                         output.bg_color.g,
                         output.bg_color.b,
                         1.0);

            glClear(GL_COLOR_BUFFER_BIT);
        }

        // viewport is centered in window
        i32 viewportLeftPx = (input.windowWidthPx - input.vpWidthPx) / 2;
        i32 viewportBottomPx = (input.windowHeightPx - input.vpHeightPx) / 2;

        glViewport(viewportLeftPx, viewportBottomPx,
                   input.vpWidthPx, input.vpHeightPx);

        // Blit drawing with possible translations/rotations/scaling
        if (recipe.blitBelow)
        {
            dbgss << "[B]";
            fs.draw(belowLayersRT.m_tex, GL_ONE, GL_SRC_ALPHA,
                output.preTranslate, output.postTranslate,
                output.scale, output.rotate,
                input.vpWidthIn, input.vpHeightIn,
                input.rtWidthIn, input.rtHeightIn);
        }
        if (recipe.blitCurrent)
        {
            dbgss << "[C]";
            fs.draw(currentLayerRT.m_tex, GL_ONE, GL_SRC_ALPHA,
                output.preTranslate, output.postTranslate,
                output.scale, output.rotate,
                input.vpWidthIn, input.vpHeightIn,
                input.rtWidthIn, input.rtHeightIn);
        }
        if (recipe.fitImage)
        {
            dbgss << "[FIT]";
            const auto &dc = recipe.fitDC;
            si.draw(textures[dc.texture].glid, dc.basis, 0.5f, fi.in2vp);
        }
        if (recipe.blitAbove)
        {
            dbgss << "[A]";
            fs.draw(aboveLayersRT.m_tex, GL_ONE, GL_SRC_ALPHA,
                output.preTranslate, output.postTranslate,
                output.scale, output.rotate,
                input.vpWidthIn, input.vpHeightIn,
                input.rtWidthIn, input.rtHeightIn);
        }

        // Draw UI passes
        for (u32 i = 0; i < recipe.guiDCs.size(); ++i)
        {
            const auto &dc = recipe.guiDCs[i];
            dbgss << "[U]";
            uirender.draw(dc.texture, dc.mesh,
                          dc.offset, dc.count,
                          static_cast<float>(input.vpWidthPt),
                          static_cast<float>(input.vpHeightPt));
        }

        if (gamma_on)
        {
            glDisable(GL_FRAMEBUFFER_SRGB);
        }
        debugstr = dbgss.str();
    }
};

void glfw_error_cb(int, const char *desc)
{
    ::printf("error: %s\n", desc);
}

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--gldiag") == 0)
        {
            g_printGLDiagnostics = true;
            g_printShaders = true;
        }
        else if (strcmp(argv[i], "--earlyquit") == 0)
        {
            g_quitFrame = 2;
        }
        else if (strcmp(argv[i], "--noretina") == 0)
        {
            g_noRetina = true;
        }
    }

    glfwSetErrorCallback(glfw_error_cb);
    glfwInit();

    glfwWindowHint(GLFW_RESIZABLE, 1);
    glfwWindowHint(GLFW_DECORATED, 1);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, 1);

    GLFWmonitor* pMonitor = glfwGetPrimaryMonitor();

    i32 monitorWidthPt, monitorHeightPt;
    const GLFWvidmode* pVideomode = glfwGetVideoMode(pMonitor);
    monitorWidthPt = pVideomode->width;
    monitorHeightPt = pVideomode->height;

    i32 monitorWidthMm, monitorHeightMm;
    glfwGetMonitorPhysicalSize(pMonitor, &monitorWidthMm, &monitorHeightMm);

    const float MM_PER_IN = 25.4f;
    float monitorWidthIn = monitorWidthMm / MM_PER_IN;
    float monitorHeightIn = monitorHeightMm / MM_PER_IN;

    float monitorHorDPI = monitorWidthPt / monitorWidthIn;
    float monitorVerDPI = monitorHeightPt / monitorHeightIn;

    ::printf("monitor: %d x %d pt\n", monitorWidthPt, monitorHeightPt);
    ::printf("monitor: %d x %d mm\n", monitorWidthMm, monitorHeightMm);
    ::printf("monitor: %f x %f in\n", monitorWidthIn, monitorHeightIn);
    ::printf("monitor: %f x %f dpi\n", monitorHorDPI, monitorVerDPI);

    i32 initialVpWidthPt = 9 * monitorWidthPt / 10;
    i32 initialVpHeightPt = 9 * monitorHeightPt / 10;

    i32 vpPaddingPt = 2;

    i32 windowWidthPt = vpPaddingPt + initialVpWidthPt + vpPaddingPt;
    i32 windowHeightPt = vpPaddingPt + initialVpHeightPt + vpPaddingPt;

    GLFWwindow* pWindow;
    pWindow = glfwCreateWindow(windowWidthPt, windowHeightPt,
                               "proto", nullptr, nullptr);

    bool gl_ok = (pWindow != nullptr);

    if (gl_ok)
        glfwMakeContextCurrent(pWindow);

    gl_ok = gl_ok && load_gl_functions(g_printGLDiagnostics);

    if (gl_ok)
    {
        glfwSetScrollCallback(pWindow, &scroll_callback);

        i32 windowWidthPx, windowHeightPx;
        glfwGetFramebufferSize(pWindow, &windowWidthPx, &windowHeightPx);
        if (g_noRetina)
        {
            windowWidthPx = windowWidthPt;
            windowHeightPx = windowHeightPt;
        }

        float pxPerPtHor = static_cast<float>(windowWidthPx) / windowWidthPt;
        float pxPerPtVer = static_cast<float>(windowHeightPx) / windowHeightPt;

        u32 rtWidthPx = static_cast<u32>(monitorWidthPt * pxPerPtHor);
        u32 rtHeightPx = static_cast<u32>(monitorHeightPt * pxPerPtVer);

        ::printf("rendertarget: %d x %d px\n", rtWidthPx, rtHeightPx);

        if (g_printGLDiagnostics)
        {
            ::printf("----------------------------\n");
            ::printf("vendor: %s\n", glGetString(GL_VENDOR));
            ::printf("renderer: %s\n", glGetString(GL_RENDERER));
            ::printf("version: %s\n", glGetString(GL_VERSION));
            ::printf("GLSL version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
            ::printf("----------------------------\n");
        }

        create_texture_unsafe(kernel_services::default_texid, 1, 1, 4);
        u8 ones[4] = { 255, 255, 255, 255 };
        update_texture_unsafe(kernel_services::default_texid, ones);

        quad_indexes.setup();

        Pipeline p;
        p.setup(rtWidthPx, rtHeightPx);

        capture_data_width_px = rtWidthPx;
        capture_data_height_px = rtHeightPx;
        capture_data.resize(rtWidthPx * rtHeightPx * 4);

        check_gl_errors("after setup");

        input_data input;
        ::memset(&input, 0, sizeof(input_data));
        output_data output;
        
		double counter_ticks_per_ms = get_platform_counter_freq();

        const u32 TIMEPOINTS = 20;
        const u32 INTERVALS = TIMEPOINTS-1;
        u64 timestamps[TIMEPOINTS] = {0};

        double intervalsMs[INTERVALS];

#define TIME timestamps[nTimePoints++] = get_platform_counter(); 

        u32 nTimePoints = 0;

        kernel_services services;
        services.ui_vertex_layout = &ui_vertex_layout;
        services.stroke_vertex_layout = &stroke_vertex_layout;

        setup(&services);

        signal_change_detector f9_signal, g_signal, m_signal;

        bool reload_client = false;
        bool gamma_enabled = true;
        bool multisample_enabled = true;

        u32 last_scroll_updated_frame = 0;

        for (;;)
        {
            if (reload_client)
            {
                cleanup();
                setup(&services);
                reload_client = false;
                ::memset(&input, 0, sizeof(input_data));
            }
            
            for (u8 i = 1; i < nTimePoints; ++i)
            {
				intervalsMs[i - 1] = static_cast<double>(timestamps[i] - timestamps[i - 1]) / counter_ticks_per_ms;
            }
            input.pTimeIntervals = intervalsMs;
            input.nTimeIntervals = (nTimePoints == 0 ? 0 : nTimePoints - 1);
            ++input.nFrames;
            nTimePoints = 0;

            TIME; // reading input -------------------------------------------------------

            glfwPollEvents();

            f9_signal.feed(glfwGetKey(pWindow, GLFW_KEY_F9) == GLFW_PRESS);
            g_signal.feed(glfwGetKey(pWindow, GLFW_KEY_G) == GLFW_PRESS);
            m_signal.feed(glfwGetKey(pWindow, GLFW_KEY_M) == GLFW_PRESS);

            if (f9_signal.is_rising_edge())
            {
                reload_client = true;
            }

            if (g_signal.is_rising_edge())
            {
                gamma_enabled = !gamma_enabled;
            }

            if (m_signal.is_rising_edge())
            {
                multisample_enabled = !multisample_enabled;
                input.forceUpdate = true;
            }

            input.mouseleft =
                glfwGetMouseButton(pWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            input.shiftkey =
                glfwGetKey(pWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                glfwGetKey(pWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

            if (input.scrolling)
            {
                input.scrollY = scroll_y;
            }

            if (input.nFrames - last_scroll_updated_frame > 5)
            {
                input.scrolling = false;
            }

            if (scroll_updated)
            {
                scroll_updated = false;
                input.scrolling = true;
                // keep the input.scrollY at its previous value
                last_scroll_updated_frame = input.nFrames;
            }

            glfwGetFramebufferSize(pWindow,
                                   &input.windowWidthPx,
                                   &input.windowHeightPx);

            glfwGetWindowPos(pWindow,
                             &input.windowPosXPt,
                             &input.windowPosYPt);

            glfwGetWindowSize(pWindow,
                              &input.windowWidthPt,
                              &input.windowHeightPt);

            input.vpWidthPt = (input.windowWidthPt == 0 ? 0 :
                input.windowWidthPt - 2 * vpPaddingPt);
            input.vpHeightPt = (input.windowHeightPt == 0 ? 0 :
                input.windowHeightPt - 2 * vpPaddingPt);

            input.vpWidthPx = (input.windowWidthPt == 0 ? 0 :
                (input.windowWidthPx * input.vpWidthPt) / input.windowWidthPt);
            input.vpHeightPx = (input.windowHeightPt == 0 ? 0 :
                (input.windowHeightPx * input.vpHeightPt) / input.windowHeightPt);

            input.vpWidthIn = input.vpWidthPt / monitorHorDPI;
            input.vpHeightIn = input.vpHeightPt / monitorVerDPI;

            input.rtWidthIn = rtWidthPx / pxPerPtHor / monitorHorDPI;
            input.rtHeightIn = rtHeightPx / pxPerPtVer / monitorVerDPI;

            // viewport is centered in window
            double viewportLeftPt = 0.5 * (input.windowWidthPt - input.vpWidthPt);
            double viewportBottomPt = 0.5 * (input.windowHeightPt - input.vpHeightPt);
            double viewportRightPt = viewportLeftPt + input.vpWidthPt;
            double viewportTopPt = viewportBottomPt + input.vpHeightPt;

            double mousePtX, mousePtY;
            glfwGetCursorPos(pWindow, &mousePtX, &mousePtY);

            input.rawMouseXPt = static_cast<float>(mousePtX);
            input.rawMouseYPt = static_cast<float>(mousePtY);

            // clamp to viewport Pt coords
            mousePtX = si_clampd(mousePtX, viewportLeftPt, viewportRightPt);
            mousePtY = si_clampd(mousePtY, viewportBottomPt, viewportTopPt);

            // to 0..widthPt/heightPt
            mousePtX = mousePtX - viewportLeftPt;
            mousePtY = mousePtY - viewportBottomPt;

            input.vpMouseXPt = static_cast<float>(mousePtX);
            input.vpMouseYPt = static_cast<float>(mousePtY);

            input.swWidthPx = input.vpWidthPt;
            input.swHeightPx = input.vpHeightPt;

            // NOTE: all mouse coords have Y axis pointing down.

            if (glfwWindowShouldClose(pWindow))
            {
                break;
            }

            TIME; // update ---------------------------------------------------------

            update_and_render(&input, &output);

            input.forceUpdate = false;

            if (output.quit_flag)
            {
                break;
            }

            p.make_frame(input, output,
                         gamma_enabled, multisample_enabled,
                         input.debugstr);

            TIME; // finish ---------------------------------------------------------
            
            glFinish();
            check_gl_errors("frame finish");
            
            TIME; // swap -----------------------------------------------------------
            
            glfwSwapBuffers(pWindow);
            
            TIME; // end ------------------------------------------------------------

            if (input.nFrames == g_quitFrame)
            {
                break;
            }
        }

        cleanup();

        p.cleanup();

        quad_indexes.cleanup();
        delete_texture_unsafe(kernel_services::default_texid);

        // TODO: check if all textures/meshes are deleted properly

        check_gl_errors("cleanup");
    }

    glfwDestroyWindow(pWindow);

    glfwTerminate();

    return 0;
}

void RenderTarget::setup(u32 width, u32 height)
{
    m_width = width;
    m_height = height;

    glGenTextures(1, &m_tex);

    glBindTexture(GL_TEXTURE_2D, m_tex);
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     static_cast<GLsizei>(m_width),
                     static_cast<GLsizei>(m_height),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &m_depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24,
                          static_cast<GLsizei>(m_width),
                          static_cast<GLsizei>(m_height));

    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    
    glGenFramebuffers(1, &m_fbo);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    {
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_tex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, m_depth_rbo);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        bool ok = (status == GL_FRAMEBUFFER_COMPLETE);
        if (!ok)
        {
            ::printf("!! framebuffer incomplete\n");
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTarget::begin_receive()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
    glViewport(0, 0,
               static_cast<GLsizei>(m_width),
               static_cast<GLsizei>(m_height));
}

void RenderTarget::end_receive()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void RenderTarget::store_image(u8 *storage)
{
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, storage);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void RenderTarget::cleanup()
{
    glDeleteFramebuffers(1, &m_fbo);
    glDeleteRenderbuffers(1, &m_depth_rbo);
    glDeleteTextures(1, &m_tex);
}


void RenderTargetMS::setup(u32 width, u32 height)
{
    m_width = width;
    m_height = height;

    glGenTextures(1, &m_tex);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_tex);
    {
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGBA,
                                static_cast<GLsizei>(m_width),
                                static_cast<GLsizei>(m_height),
                                true);
    }
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);

    glGenRenderbuffers(1, &m_depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depth_rbo);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH_COMPONENT24,
                                     static_cast<GLsizei>(m_width),
                                     static_cast<GLsizei>(m_height));

    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &m_fbo);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    {
        glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_tex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, m_depth_rbo);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        bool ok = (status == GL_FRAMEBUFFER_COMPLETE);
        if (!ok)
        {
            ::printf("!! framebuffer incomplete\n");
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderTargetMS::begin_receive()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
    glViewport(0, 0,
               static_cast<GLsizei>(m_width),
               static_cast<GLsizei>(m_height));
}

void RenderTargetMS::end_receive()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void RenderTargetMS::resolve()
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
    // TODO: receiving side should have the same dimensions
    GLint w = static_cast<GLint>(m_width);
    GLint h = static_cast<GLint>(m_height);
    glBlitFramebuffer(0, 0, w, h,
                      0, 0, w, h,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void RenderTargetMS::cleanup()
{
    glDeleteFramebuffers(1, &m_fbo);
    glDeleteRenderbuffers(1, &m_depth_rbo);
    glDeleteTextures(1, &m_tex);
}


void FSTexturePresenter::setup(FSQuad *quad)
{
    m_quad = quad;

    const char *vertex_src =
        "  #version 330 core                                  \n"
        "  out vec2 l_textureUV;                              \n"
        "  uniform vec2 u_preTranslation;                     \n"
        "  uniform vec2 u_postTranslation;                    \n"
        "  uniform float u_scale;                             \n"
        "  uniform float u_rotation;                          \n"
        "  uniform vec2 u_rtSize;                             \n"
        "  uniform vec2 u_vpSize;                             \n"
        "  void main()                                        \n"
        "  {                                                  \n"
        "      vec2 uv = vec2(gl_VertexID % 2,                \n"
        "                     gl_VertexID / 2);               \n"
        "      vec2 msPos = 2.0f * uv - 1.0f;                 \n"
        "      vec2 xy_i = msPos * u_rtSize / 2;              \n"
        "      xy_i += u_preTranslation;                      \n"
        "      float s = sin(u_rotation);                     \n"
        "      float c = cos(u_rotation);                     \n"
        "      vec2 X = u_scale * vec2(c, s);                 \n"
        "      vec2 Y = u_scale * vec2(-s, c);                \n"
        "      xy_i = xy_i.x * X + xy_i.y * Y;                \n"
        "      xy_i += u_postTranslation;                     \n"
        "      gl_Position.xy = 2 * xy_i / u_vpSize;          \n"
        "      gl_Position.z = 0.0f;                          \n"
        "      gl_Position.w = 1.0f;                          \n"
        "      l_textureUV = uv;                              \n"
        "  }                                                  \n";

    const char *simple_vertex_src =
        "  #version 330 core                                  \n"
        "  out vec2 l_textureUV;                              \n"
        "  void main()                                        \n"
        "  {                                                  \n"
        "      vec2 uv = vec2(gl_VertexID % 2,                \n"
        "                     gl_VertexID / 2);               \n"
        "      vec2 msPos = 2.0f * uv - 1.0f;                 \n"
        "      gl_Position.xy = msPos;                        \n"
        "      gl_Position.z = 0.0f;                          \n"
        "      gl_Position.w = 1.0f;                          \n"
        "      l_textureUV = uv;                              \n"
        "  }                                                  \n";

    const char *fragment_src =
        "  #version 330 core                                 \n"
        "  uniform sampler2D sampler;                        \n"
        "  in vec2 l_textureUV;                              \n"
        "  layout (location = 0) out vec4 o_color;           \n"
        "  void main()                                       \n"
        "  {                                                 \n"
        "      o_color = texture(sampler, l_textureUV);      \n"
        "  }                                                 \n";

    m_fullscreenProgram = ::create_program(vertex_src, nullptr, fragment_src, g_printShaders);
    m_preTranslationLoc = glGetUniformLocation(m_fullscreenProgram, "u_preTranslation");
    m_postTranslationLoc = glGetUniformLocation(m_fullscreenProgram, "u_postTranslation");

    m_scaleLoc = glGetUniformLocation(m_fullscreenProgram, "u_scale");
    m_rotationLoc = glGetUniformLocation(m_fullscreenProgram, "u_rotation");

    m_vpSizeLoc = glGetUniformLocation(m_fullscreenProgram, "u_vpSize");
    m_rtSizeLoc = glGetUniformLocation(m_fullscreenProgram, "u_rtSize");
    m_simpleFullscreenProgram = ::create_program(simple_vertex_src, nullptr,
                                                 fragment_src, g_printShaders);
}

void FSTexturePresenter::cleanup()
{
    glDeleteProgram(m_fullscreenProgram);
    glDeleteProgram(m_simpleFullscreenProgram);
}

void FSTexturePresenter::draw(GLuint tex, GLenum blendSrc, GLenum blendDst,
                              const float2 &pre, const float2 &post,
                              float s, float a,
                              float vpW, float vpH,
                              float rtW, float rtH)
{
    glEnable(GL_BLEND);
    glBlendFunc(blendSrc, blendDst);
    glUseProgram(m_fullscreenProgram);
    glUniform2f(m_preTranslationLoc, pre.x, pre.y);
    glUniform2f(m_postTranslationLoc, post.x, post.y);
    glUniform1f(m_scaleLoc, s);
    glUniform1f(m_rotationLoc, a);
    glUniform2f(m_rtSizeLoc, rtW, rtH);
    glUniform2f(m_vpSizeLoc, vpW, vpH);
    glBindTexture(GL_TEXTURE_2D, tex);
    m_quad->draw();
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

void FSTexturePresenter::draw(GLuint tex, GLenum blendSrc, GLenum blendDst)
{
    glEnable(GL_BLEND);
    glBlendFunc(blendSrc, blendDst);
    glUseProgram(m_simpleFullscreenProgram);
    glBindTexture(GL_TEXTURE_2D, tex);
    m_quad->draw();
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

void FSTexturePresenter::draw(GLuint tex, GLenum blendSrcC, GLenum blendDstC,
                              GLenum blendSrcA, GLenum blendDstA)
{
    glEnable(GL_BLEND);
    glBlendFuncSeparate(blendSrcC, blendDstC, blendSrcA, blendDstA);
    glUseProgram(m_simpleFullscreenProgram);
    glBindTexture(GL_TEXTURE_2D, tex);
    m_quad->draw();
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

void FSQuad::setup()
{
    glGenVertexArrays(1, &m_vao);
}

void FSQuad::draw()
{
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void FSQuad::cleanup()
{
    glDeleteVertexArrays(1, &m_vao);
}

void FSGrid::setup(FSQuad *quad)
{
    m_quad = quad;

    const char *vertex_src =
        "  #version 330 core                                    \n"
        "  uniform vec2 u_scale;                                \n"
        "  out vec2 l_xy_i;                                     \n"
        "  void main()                                          \n"
        "  {                                                    \n"
        "      vec2 uv = vec2(gl_VertexID % 2,                  \n"
        "                     gl_VertexID / 2);                 \n"
        "      vec2 msPos = 2.0f * uv - 1.0f;                   \n"
        "      gl_Position.xy = msPos;                          \n"
        "      gl_Position.z = 0.0f;                            \n"
        "      gl_Position.w = 1.0f;                            \n"
        "      l_xy_i = msPos / u_scale;                        \n"
        "  }                                                    \n";

    const char *fragment_src =
        "  #version 330 core                                    \n"
        "  in vec2 l_xy_i;                                      \n"
        "  layout (location = 0) out vec4 o_color;              \n"
        "  uniform vec3 u_bgColor;                              \n"
        "  uniform vec3 u_fgColor;                              \n"
        "  void main()                                          \n"
        "  {                                                    \n"
        "      vec2 dist0 = fract(l_xy_i);                      \n"
        "      vec2 dist1 = vec2(1.0f, 1.0f) - dist0;           \n"
        "      vec2 dist = min(dist0, dist1);                   \n"
        "      float isvert = smoothstep(0.0f, 0.01f, dist.x);  \n"
        "      float ishorz = smoothstep(0.0f, 0.01f, dist.y);  \n"
        "      float isLine = 1.0f - min(isvert, ishorz);       \n"
        "      o_color.rgb = mix(u_bgColor, u_fgColor, isLine); \n"
        "      o_color.a = 1.0f;                                \n"
        "  }                                                    \n";

    m_fullscreenProgram = ::create_program(vertex_src, nullptr, fragment_src, g_printShaders);
    m_bgColorLoc = glGetUniformLocation(m_fullscreenProgram, "u_bgColor");
    m_fgColorLoc = glGetUniformLocation(m_fullscreenProgram, "u_fgColor");
    m_scaleLoc = glGetUniformLocation(m_fullscreenProgram, "u_scale");
}

void FSGrid::draw(const color3f &bgcolor, const color3f &fgcolor, const float2 &scale)
{
    glUseProgram(m_fullscreenProgram);
    glUniform3f(m_bgColorLoc, bgcolor.r, bgcolor.g, bgcolor.b);
    glUniform3f(m_fgColorLoc, fgcolor.r, fgcolor.g, fgcolor.b);
    glUniform2f(m_scaleLoc, scale.x, scale.y);
    m_quad->draw();
    glUseProgram(0);
}

void FSGrid::cleanup()
{
    glDeleteProgram(m_fullscreenProgram);
}

void MarkerBatchTech::setup()
{
    // TODO: many fields are duplicated in each vertex
    //       they are basically constant per stroke
    //       maybe it makes sense to keep them elsewhere and pass stroke id
    //       then do fetch in vertex shader
    //       such fields are: basecolor, zoffset, alpha, eraser
    const char *vertex_src =
        "  #version 330 core                               \n"
        "  layout (location = 0) in vec3 i_msPosition;     \n"
        "  layout (location = 1) in vec3 i_uve;            \n"
        "  layout (location = 2) in vec4 i_color;          \n"
        "  uniform vec2 u_scale;                           \n"
        "  out vec3 l_uve;                                 \n"
        "  out vec4 l_color;                               \n"
        "  void main()                                     \n"
        "  {                                               \n"
        "      gl_Position.xy = i_msPosition.xy * u_scale; \n"
        "      float zindex = i_msPosition.z;              \n"
        "      gl_Position.z = 2.0f * zindex - 1.0f;       \n"
        "      gl_Position.w = 1.0f;                       \n"
        "      l_uve = i_uve;                              \n"
        "      l_color = i_color;                          \n"
        "  }                                               \n";

    const char *fragment_src =
        "  #version 330 core                                        \n"
        "  layout (location = 0) out vec4 o_color;                  \n"
        "  layout (location = 1) out vec4 o_color1;                 \n"
        "  in vec3 l_uve;                                           \n"
        "  in vec4 l_color;                                         \n"
        "  void main()                                              \n"
        "  {                                                        \n"
        "      float rho = length(l_uve.xy - vec2(0.5f, 0.5f));     \n"
        "      if (rho > 0.5f) discard;                             \n"
        "      float A = l_color.a;                                 \n"
        "      float E = l_uve.z;                                   \n"
        "      float SRCa = (1.0f - A) * E;                         \n"
        "      float SRC1a = (1.0f - A) * (1.0f - E);               \n"
        "      o_color.rgb = l_color.rgb * A;                       \n"
        "      o_color.a = SRCa;                                    \n"
        "      o_color1.a = SRC1a;                                  \n"
        "  }                                                        \n";

    m_program = ::create_program(vertex_src, nullptr, fragment_src, g_printShaders);
    m_scaleLoc = glGetUniformLocation(m_program, "u_scale");
}

void MarkerBatchTech::cleanup()
{
    glDeleteProgram(m_program);
}

void MarkerBatchTech::draw(kernel_services::MeshID mi, u32 offset, u32 count, const float2 &scale)
{
    if (count == 0)
    {
        return;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_SRC1_ALPHA);
    glUseProgram(m_program);
    glUniform2f(m_scaleLoc, scale.x, scale.y);

    glBindVertexArray(meshes[mi].vao);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshes[mi].ibuf);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_SHORT,
                   make_gl_offset(offset * meshes[mi].isize));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    glUseProgram(0);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
}

void StrokeImageTech::setup(FSQuad *quad)
{
    m_quad = quad;

    const char *vertex_src =
        "  #version 330 core                                  \n"
        "  out vec2 l_textureUV;                              \n"
        "  uniform vec2 u_pos;                                \n"
        "  uniform vec4 u_axes;                               \n"
        "  uniform vec2 u_scale;                              \n"
        "  void main()                                        \n"
        "  {                                                  \n"
        "      vec2 uv = vec2(gl_VertexID % 2,                \n"
        "                     gl_VertexID / 2);               \n"
        "      vec2 X = vec2(u_axes.x, u_axes.y);             \n"
        "      vec2 Y = vec2(u_axes.z, u_axes.w);             \n"
        "      vec2 xy_i = uv.x * X + uv.y * Y;               \n"
        "      xy_i += u_pos;                                 \n"
        "      gl_Position.xy = xy_i * u_scale;               \n"
        "      gl_Position.z = 0.0f;                          \n"
        "      gl_Position.w = 1.0f;                          \n"
        "      l_textureUV = vec2(uv.x, -uv.y);               \n"
        "  }                                                  \n";

    const char *fragment_src =
        "  #version 330 core                                 \n"
        "  uniform sampler2D sampler;                        \n"
        "  uniform float u_alpha;                            \n"
        "  in vec2 l_textureUV;                              \n"
        "  layout (location = 0) out vec4 o_color;           \n"
        "  layout (location = 1) out vec4 o_color1;          \n"
        "  void main()                                       \n"
        "  {                                                 \n"
        "      vec4 color = texture(sampler, l_textureUV);   \n"
        "      float A = color.a * u_alpha;                  \n"
        "      float E = 0.0f;                               \n"
        "      float SRCa = (1.0f - A) * E;                  \n"
        "      float SRC1a = (1.0f - A) * (1.0f - E);        \n"
        "      o_color.rgb = color.rgb * A;                  \n"
        "      o_color.a = SRCa;                             \n"
        "      o_color1.a = SRC1a;                           \n"
        "  }                                                 \n";

    m_fullscreenProgram = ::create_program(vertex_src, nullptr, fragment_src, g_printShaders);
    m_posLoc = glGetUniformLocation(m_fullscreenProgram, "u_pos");
    m_axesLoc = glGetUniformLocation(m_fullscreenProgram, "u_axes");
    m_scaleLoc = glGetUniformLocation(m_fullscreenProgram, "u_scale");
    m_alphaLoc = glGetUniformLocation(m_fullscreenProgram, "u_alpha");
}

void StrokeImageTech::cleanup()
{
    glDeleteProgram(m_fullscreenProgram);
}

void StrokeImageTech::draw(GLuint tex, const basis2r &basis,
                           float alpha, const float2 &scale)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_SRC1_ALPHA);
    glUseProgram(m_fullscreenProgram);
    glUniform2f(m_posLoc, basis.o.x, basis.o.y);
    glUniform4f(m_axesLoc, basis.x.x, basis.x.y, basis.y.x, basis.y.y);
    glUniform2f(m_scaleLoc, scale.x, scale.y);
    glUniform1f(m_alphaLoc, alpha);
    glBindTexture(GL_TEXTURE_2D, tex);
    m_quad->draw();
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}


// TODO: make this more generic by introducing scale translate
// TODO: then it will be a reusable piece, refactor
void UIPresenter::setup()
{
    const char *vertex_src =
        "  #version 330 core                               \n"
        "  layout (location = 0) in vec2 i_ssPosition;     \n"
        "  layout (location = 1) in vec2 i_uv;             \n"
        "  layout (location = 2) in vec4 i_color;          \n"
        "  uniform vec2 u_vpSize;                          \n"
        "  out vec2 l_uv;                                  \n"
        "  out vec4 l_color;                               \n"
        "  void main()                                     \n"
        "  {                                               \n"
        "      gl_Position.xy = i_ssPosition.xy / u_vpSize;\n"
        "      gl_Position.xy -= vec2(0.5, 0.5);           \n"
        "      gl_Position.xy *= vec2(2.0, -2.0);          \n"
        "      gl_Position.z = 0.0f;                       \n"
        "      gl_Position.w = 1.0f;                       \n"
        "      l_uv = i_uv;                                \n"
        "      l_color = i_color;                          \n"
        "  }                                               \n";

    const char *fragment_src =
        "  #version 330 core                                        \n"
        "  layout (location = 0) out vec4 o_color;                  \n"
        "  uniform sampler2D sampler;                               \n"
        "  in vec2 l_uv;                                            \n"
        "  in vec4 l_color;                                         \n"
        "  void main()                                              \n"
        "  {                                                        \n"
        "      o_color = l_color * texture(sampler, l_uv);          \n"
        "  }                                                        \n";

    m_program = ::create_program(vertex_src, nullptr, fragment_src, g_printShaders);

    m_vpSizeLoc = glGetUniformLocation(m_program, "u_vpSize");
}

void UIPresenter::cleanup()
{
    glDeleteProgram(m_program);
}

void UIPresenter::draw(kernel_services::TexID ti, kernel_services::MeshID mi,
                       u32 offset, u32 count,
                       float vpWidth, float vpHeight)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(m_program);
    glUniform2f(m_vpSizeLoc, vpWidth, vpHeight);
    glBindTexture(GL_TEXTURE_2D, textures[ti].glid);

    glBindVertexArray(meshes[mi].vao);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshes[mi].ibuf);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_SHORT,
                   make_gl_offset(offset * meshes[mi].isize));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

// TODO: samplers are not set up (uniform)

bool kernel_services::check_file(const char *path)
{
	FILE *f = nullptr;

#ifdef _WIN32
	::fopen_s(&f, path, "r");
#elif __APPLE__
    f = ::fopen(path, "r");
#endif

    if (f)
    {
        ::fclose(f);
        return true;
    }
    return false;
}

void QuadIndexes::setup()
{
    std::vector<u16> buffer;
    const size_t vtx_cnt = 65536;
    const size_t quad_cnt = vtx_cnt / 4;
    const size_t idx_cnt = quad_cnt * 6;
    buffer.resize(idx_cnt);

    u16 *ptr = buffer.data();
    u16 idx = 0;
    for (size_t i = 0; i < quad_cnt; ++i)
    {
		ptr[0] = idx;
		ptr[1] = static_cast<u16>(idx + 1);
		ptr[2] = static_cast<u16>(idx + 2);
		ptr[3] = static_cast<u16>(idx + 2);
		ptr[4] = static_cast<u16>(idx + 3);
		ptr[5] = idx;
        ptr += 6;
        idx += 4;
    }

    glGenBuffers(1, &m_ibuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibuf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u16) * idx_cnt,
                 buffer.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void QuadIndexes::cleanup()
{
    glDeleteBuffers(1, &m_ibuf);
}

void QuadIndexes::before()
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibuf);
}

void QuadIndexes::after()
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
