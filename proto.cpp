
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif __APPLE__
#include <mach/mach_time.h>
#endif

#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include "glcorearb.h"
#include <cstdio>
#include <cstring>

#include <vector>
#include "client.h"
#include "math.h"

const size_t GL_FUNCTIONS_CAPACITY = 100;
const char *g_names[GL_FUNCTIONS_CAPACITY];

typedef bool (*gl_func_initr)();
gl_func_initr g_initrs[GL_FUNCTIONS_CAPACITY];
size_t g_size = 0;

#define ASSERT(cond) if (!(cond)) *reinterpret_cast<volatile i32 *>(0) = 1;

#define DECL(t, n)                                                  \
bool initr_for_##n();                                               \
t delay_for_##n()                                                   \
{                                                                   \
    ASSERT(g_size < GL_FUNCTIONS_CAPACITY);                         \
    g_names[g_size] = #n;                                           \
    g_initrs[g_size] = initr_for_##n;                               \
    ++g_size;                                                       \
    return nullptr;                                                 \
}                                                                   \
t n = delay_for_##n();                                              \
bool initr_for_##n()                                                \
{                                                                   \
    n = reinterpret_cast<t>(glfwGetProcAddress(#n));                \
    return n != nullptr;                                            \
}                                                                   \

#define LOAD(t, n) DECL(PFN##t##PROC, n)

LOAD(GLENABLE, glEnable)
LOAD(GLDISABLE, glDisable)
LOAD(GLBLENDFUNC, glBlendFunc)
LOAD(GLVIEWPORT, glViewport)
LOAD(GLCLEAR, glClear)
LOAD(GLCLEARCOLOR, glClearColor)
LOAD(GLCLEARDEPTH, glClearDepth)

LOAD(GLCREATEPROGRAM, glCreateProgram)
LOAD(GLLINKPROGRAM, glLinkProgram)
LOAD(GLGETPROGRAMIV, glGetProgramiv)
LOAD(GLUSEPROGRAM, glUseProgram)
LOAD(GLDELETEPROGRAM, glDeleteProgram)

LOAD(GLCREATESHADER, glCreateShader)
LOAD(GLSHADERSOURCE, glShaderSource)
LOAD(GLCOMPILESHADER, glCompileShader)
LOAD(GLGETSHADERIV, glGetShaderiv)
LOAD(GLATTACHSHADER, glAttachShader)
LOAD(GLDELETESHADER, glDeleteShader)

LOAD(GLGETSHADERINFOLOG, glGetShaderInfoLog)
LOAD(GLGETPROGRAMINFOLOG, glGetProgramInfoLog)

LOAD(GLGENVERTEXARRAYS, glGenVertexArrays)
LOAD(GLBINDVERTEXARRAY, glBindVertexArray)
LOAD(GLDELETEVERTEXARRAYS, glDeleteVertexArrays)
LOAD(GLENABLEVERTEXATTRIBARRAY, glEnableVertexAttribArray)
LOAD(GLVERTEXATTRIBPOINTER, glVertexAttribPointer)

LOAD(GLDRAWARRAYS, glDrawArrays)
LOAD(GLDRAWELEMENTS, glDrawElements)

LOAD(GLGENBUFFERS, glGenBuffers)
LOAD(GLBINDBUFFER, glBindBuffer)
LOAD(GLBUFFERDATA, glBufferData)
LOAD(GLDELETEBUFFERS, glDeleteBuffers)
LOAD(GLMAPBUFFER, glMapBuffer)
LOAD(GLUNMAPBUFFER, glUnmapBuffer)

LOAD(GLGENTEXTURES, glGenTextures)
LOAD(GLDELETETEXTURES, glDeleteTextures)
LOAD(GLBINDTEXTURE, glBindTexture)
LOAD(GLTEXIMAGE2D, glTexImage2D)
LOAD(GLTEXSUBIMAGE2D, glTexSubImage2D)
LOAD(GLTEXPARAMETERI, glTexParameteri)


LOAD(GLGENFRAMEBUFFERS, glGenFramebuffers)
LOAD(GLDELETEFRAMEBUFFERS, glDeleteFramebuffers)
LOAD(GLGENRENDERBUFFERS, glGenRenderbuffers)
LOAD(GLDELETERENDERBUFFERS, glDeleteRenderbuffers)
LOAD(GLBINDFRAMEBUFFER, glBindFramebuffer)
LOAD(GLBINDRENDERBUFFER, glBindRenderbuffer)
LOAD(GLCHECKFRAMEBUFFERSTATUS, glCheckFramebufferStatus)
LOAD(GLFRAMEBUFFERTEXTURE, glFramebufferTexture)
LOAD(GLRENDERBUFFERSTORAGE, glRenderbufferStorage)
LOAD(GLFRAMEBUFFERRENDERBUFFER, glFramebufferRenderbuffer)

LOAD(GLDEPTHFUNC, glDepthFunc)

LOAD(GLGETUNIFORMLOCATION, glGetUniformLocation)
LOAD(GLUNIFORM1F, glUniform1f)
LOAD(GLUNIFORM2F, glUniform2f)
LOAD(GLUNIFORM3F, glUniform3f)

LOAD(GLFINISH, glFinish)
LOAD(GLGETERROR, glGetError)
LOAD(GLGETSTRING, glGetString)


bool g_printShaders = false;
bool g_printGLDiagnostics = false;
size_t g_quitFrame = 0;
bool g_noRetina = false;

bool load_gl_functions()
{
    bool result = true;
    for (size_t i = 0; i < g_size; ++i)
    {
        bool ok = (*g_initrs[i])();
        if (g_printGLDiagnostics)
        {
            ::printf("%s -> %s\n", g_names[i], (ok ? "OK" : "FAIL"));
        }
        result = result && ok;
    }
    return result;
}

void check_gl_errors(const char *tag)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
    {
        switch (err)
        {
        case GL_INVALID_ENUM:
            ::printf("GL_INVALID_ENUM");
            break;
        case GL_INVALID_VALUE:
            ::printf("GL_INVALID_VALUE");
            break;
        case GL_INVALID_OPERATION:
            ::printf("GL_INVALID_OPERATION");
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            ::printf("GL_INVALID_FRAMEBUFFER_OPERATION");
            break;
        case GL_OUT_OF_MEMORY:
            ::printf("GL_OUT_OF_MEMORY");
            break;
        case GL_STACK_UNDERFLOW:
            ::printf("GL_STACK_UNDERFLOW");
            break;
        case GL_STACK_OVERFLOW:
            ::printf("GL_STACK_OVERFLOW");
            break;
        default:
            ::printf("<unknown error>");
        }
        ::printf(" at `%s`\n", tag);
    }
}

const size_t INFOLOG_SIZE = 1024;
GLchar infolog[INFOLOG_SIZE];

void dump_source(const char *src)
{
    if (src)
    {
        int lineno = 0;
        const char *p = src;
        while (*p)
        {
            ::printf("%4d: ", ++lineno);
            do ::printf("%c", *p);
            while (*p++ != '\n' && *p);
        }
    }
    else
    {
        ::printf("<empty>\n");
    }
}

GLuint create_program(const char *vert_src, const char *geom_src, const char *frag_src)
{
    if (g_printShaders)
    {
        ::printf("--------------------vert--------------------\n");
        dump_source(vert_src);
        ::printf("--------------------geom--------------------\n");
        dump_source(geom_src);
        ::printf("--------------------frag--------------------\n");
        dump_source(frag_src);
    }

    GLuint program = glCreateProgram();

    GLenum shader_types[3] =
    {
        GL_VERTEX_SHADER,
        GL_GEOMETRY_SHADER,
        GL_FRAGMENT_SHADER
    };

    const char *shader_sources[3] =
    {
        vert_src,
        geom_src,
        frag_src
    };

    const char *shader_str[3] =
    {
        "Vertex",
        "Geometry",
        "Fragment"
    };

    for (size_t i = 0; i < 3; ++i)
    {
        if (shader_sources[i] == nullptr)
        {
            continue;
        }

        GLuint shader = glCreateShader(shader_types[i]);

        GLsizei length = static_cast<GLsizei>(::strlen(shader_sources[i]));
        glShaderSource(shader, 1, &shader_sources[i], &length);
        glCompileShader(shader);

        GLint compile_status;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
        if (compile_status == 0)
        {
            glGetShaderInfoLog(shader, INFOLOG_SIZE, nullptr, infolog);
            ::printf("%s Shader Compile Log:\n%s\n", shader_str[i], infolog);
        }
        else
        {
            glAttachShader(program, shader);
        }

        glDeleteShader(shader);
    }

    glLinkProgram(program);

    GLint link_status;
    glGetProgramiv(program, GL_LINK_STATUS, &link_status);
    if (link_status == 0)
    {
        glGetProgramInfoLog(program, INFOLOG_SIZE, nullptr, infolog);
        ::printf("Program Link Error:\n%s\n", infolog);
        glDeleteProgram(program);
        program = 0;
    }
    return program;
}

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

struct BufferPresenter
{
    void setup(FSQuad *quad, u32 width, u32 height);
    void draw(const u8 *pixels, float pX, float pY);
    void cleanup();

    GLuint m_pbo[2];
    GLuint m_tex;
    FSQuad *m_quad;
    GLuint m_fullscreenProgram;
    GLint m_scaleLoc;

    u32 m_width;
    u32 m_height;

    u32 m_writeIdx;
};

struct FSTexturePresenter
{
    void setup(FSQuad *quad);
    void draw(GLuint tex, GLenum srcFactor, GLenum dstFactor,
              float x, float y,
              float x_, float y_,
              float s, float a,
              float vpW, float vpH,
              float rtW, float rtH);
    void cleanup();

    FSQuad *m_quad;
    GLuint m_fullscreenProgram;
    GLint m_preTranslationLoc;
    GLint m_postTranslationLoc;
    GLint m_scaleLoc;
    GLint m_rotationLoc;
    GLint m_vpSizeLoc;
    GLint m_rtSizeLoc;
};

struct FSGrid
{
    void setup(FSQuad *quad);
    void draw(const float *bgcolor, const float *fgcolor);
    void cleanup();

    GLuint m_fullscreenProgram;
    GLint m_bgColorLoc;
    GLint m_fgColorLoc;
    FSQuad *m_quad;
};

struct RenderTarget
{
    void setup(u32 width, u32 height);
    void cleanup();

    void before();
    void after();

    GLuint m_fbo;
    GLuint m_depth_rbo;
    GLuint m_tex;

    u32 m_width;
    u32 m_height;
};

struct MeshPresenter
{
    void setup();
    void cleanup();

    void draw(kernel_services::MeshID mi,
              u32 offset, u32 count,
              float scaleX, float scaleY);

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
    { return reinterpret_cast<GLvoid*>(offset_(si)); }
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
    {"uvew",       1, 4, 4, GL_FLOAT, false},
    {"color",      2, 4, 4, GL_FLOAT, false}
};

QuadIndexes quad_indexes;

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
    GLuint glid;
    GLuint width;
    GLuint height;
    GLenum format;
};

// TODO: this runs always up
u32 next_texture_idx = 0;
const u32 MAXTEXCNT = 10;
Texture textures[MAXTEXCNT];

kernel_services::TexID create_texture(u32 w, u32 h, u32 comp)
{
    Texture &tex = textures[next_texture_idx];
    tex.width = w;
    tex.height = h;
    tex.format = (comp == 1 ? GL_RED : GL_RGBA);
    glGenTextures(1, &tex.glid);

    glBindTexture(GL_TEXTURE_2D, tex.glid);
    glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(tex.format),
                 static_cast<GLsizei>(w), static_cast<GLsizei>(h), 0,
                 tex.format, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    ++next_texture_idx;

    return next_texture_idx - 1;
}

// TODO: this is alpha only
void update_texture(kernel_services::TexID ti, const u8 *pixels)
{
    glBindTexture(GL_TEXTURE_2D, textures[ti].glid);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    static_cast<GLsizei>(textures[ti].width),
                    static_cast<GLsizei>(textures[ti].height),
                    textures[ti].format, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void delete_texture(kernel_services::TexID ti)
{
    glDeleteTextures(1, &textures[ti].glid);
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
	return v.QuadPart;
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
#endif

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

    glfwInit();

    glfwWindowHint(GLFW_RESIZABLE, 1);
    glfwWindowHint(GLFW_DECORATED, 1);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* pMonitor = glfwGetPrimaryMonitor();

    u32 monitorWidthPt, monitorHeightPt;
    const GLFWvidmode* pVideomode = glfwGetVideoMode(pMonitor);
    monitorWidthPt = static_cast<u32>(pVideomode->width);
    monitorHeightPt = static_cast<u32>(pVideomode->height);

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

    i32 initialVpWidthPt = monitorWidthPt / 2;
    i32 initialVpHeightPt = monitorHeightPt / 2;

    i32 vpPaddingPt = 64;

    i32 windowWidthPt = vpPaddingPt + initialVpWidthPt + vpPaddingPt;
    i32 windowHeightPt = vpPaddingPt + initialVpHeightPt + vpPaddingPt;

    u32 swrtWidthPx = monitorWidthPt;
    u32 swrtHeightPx = monitorHeightPt;

    GLFWwindow* pWindow;
    pWindow = glfwCreateWindow(windowWidthPt, windowHeightPt,
                               "proto", nullptr, nullptr);

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

    glfwMakeContextCurrent(pWindow);
    
    bool gl_ok = load_gl_functions();

    if (gl_ok)
    {
        if (g_printGLDiagnostics)
        {
            ::printf("----------------------------\n");
            ::printf("vendor: %s\n", glGetString(GL_VENDOR));
            ::printf("renderer: %s\n", glGetString(GL_RENDERER));
            ::printf("version: %s\n", glGetString(GL_VERSION));
            ::printf("GLSL version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
            ::printf("----------------------------\n");
        }

        FSQuad quad;
        quad.setup();

        quad_indexes.setup();

        FSTexturePresenter fs;
        fs.setup(&quad);

        RenderTarget bakeRT;
        bakeRT.setup(rtWidthPx, rtHeightPx);

        RenderTarget currRT;
        currRT.setup(rtWidthPx, rtHeightPx);
        
        BufferPresenter blit;
        blit.setup(&quad, swrtWidthPx, swrtHeightPx);

        check_gl_errors("after setup");

        GLubyte *pixels = new GLubyte[swrtWidthPx * swrtHeightPx * 4];

        offscreen_buffer buffer;
        buffer.data = pixels;
        buffer.width = swrtWidthPx;
        buffer.height = swrtHeightPx;

        MeshPresenter render;
        render.setup();

        UIPresenter uirender;
        uirender.setup();
        
        input_data input;
        input.nFrames = 0;
        output_data output;
        output.buffer = &buffer;
        
		double counter_ticks_per_ms = get_platform_counter_freq();

        const u32 TIMEPOINTS = 10;
        const u32 INTERVALS = TIMEPOINTS-1;
        u64 timestamps[TIMEPOINTS] = {0};

        double intervalsMs[INTERVALS];

#define TIME timestamps[nTimePoints++] = get_platform_counter(); 

        u32 nTimePoints = 0;

        kernel_services services;
        services.create_texture = create_texture;
        services.update_texture = update_texture;
        services.delete_texture = delete_texture;
        services.ui_vertex_layout = &ui_vertex_layout;
        services.stroke_vertex_layout = &stroke_vertex_layout;

        setup(&services);

        bool reload_client = false;
        bool f9_was_up = true;

        u32 last_scroll_updated_frame = 0;

        for (;;)
        {
            if (reload_client)
            {
                cleanup();
                setup(&services);
                reload_client = false;
                input.nFrames = 0;
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

            bool f9_is_down = (glfwGetKey(pWindow, GLFW_KEY_F9) == GLFW_PRESS);
            if (f9_is_down && f9_was_up)
            {
                reload_client = true;
            }
            f9_was_up = !f9_is_down;

            input.mouseleft =
                (glfwGetMouseButton(pWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);

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

            input.vpWidthPt = input.windowWidthPt - 2 * vpPaddingPt;
            input.vpHeightPt = input.windowHeightPt - 2 * vpPaddingPt;

            input.vpWidthPx = (input.windowWidthPx * input.vpWidthPt) / input.windowWidthPt;
            input.vpHeightPx = (input.windowHeightPx * input.vpHeightPt) / input.windowHeightPt;

            input.vpWidthIn = input.vpWidthPt / monitorHorDPI;
            input.vpHeightIn = input.vpHeightPt / monitorVerDPI;

            input.rtWidthIn = rtWidthPx / pxPerPtHor / monitorHorDPI;
            input.rtHeightIn = rtHeightPx / pxPerPtVer / monitorVerDPI;

            // viewport is centered in window
            i32 viewportLeftPx = (input.windowWidthPx - input.vpWidthPx) / 2;
            i32 viewportBottomPx = (input.windowHeightPx - input.vpHeightPx) / 2;

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

            if (output.quit_flag)
            {
                break;
            }

            if (output.bake_flag)
            {
                output.bake_flag = false;

                bakeRT.before();
                glClearColor(0.0, 0.0, 0.0, 1.0);
                glClearDepth(0.0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                render.draw(output.bgmesh, 0, output.bgmeshCnt,
                            2.0f / input.rtWidthIn, 2.0f / input.rtHeightIn);
                bakeRT.after();
            }

            if (output.change_flag)
            {
                output.change_flag = false;

                currRT.before();
                glClearColor(0.0, 0.0, 0.0, 1.0);
                glClearDepth(0.0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                fs.draw(bakeRT.m_tex, GL_ONE, GL_ZERO,
                        output.preTranslateX, output.preTranslateY,
                        output.postTranslateX, output.postTranslateY,
                        output.scale, output.rotate,
                        input.rtWidthIn, input.rtHeightIn,
                        input.rtWidthIn, input.rtHeightIn);

                render.draw(output.fgmesh, 0, output.fgmeshCnt,
                            2.0f / input.rtWidthIn, 2.0f / input.rtHeightIn);
                currRT.after();
            }

            glClearColor(output.bg_red, output.bg_green, output.bg_blue, 1.0);
            glClear(GL_COLOR_BUFFER_BIT);

            glViewport(viewportLeftPx, viewportBottomPx,
                       input.vpWidthPx, input.vpHeightPx);

            // grid.draw(output.grid_bg_color, output.grid_fg_color,
            //           output.grid_translation, output.grid_zoom);

            fs.draw(currRT.m_tex, GL_ONE, GL_SRC_ALPHA,
                    0.0f, 0.0f,
                    0.0f, 0.0f,
                    1.0f, 0.0f,
                    input.vpWidthIn, input.vpHeightIn,
                    input.rtWidthIn, input.rtHeightIn);

            blit.draw(pixels,
                      static_cast<float>(input.swWidthPx) / swrtWidthPx,
                      static_cast<float>(input.swHeightPx) / swrtHeightPx);

            for (u32 i = 0; i < output.ui_drawcall_cnt; ++i)
            {
                uirender.draw(output.ui_drawcalls[i].texture_id,
                              output.ui_drawcalls[i].mesh_id,
                              output.ui_drawcalls[i].offset,
                              output.ui_drawcalls[i].count,
                              input.vpWidthPt,
							  input.vpHeightPt);
            }

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

        (*cleanup)();

        uirender.cleanup();
        render.cleanup();
        blit.cleanup();
        bakeRT.cleanup();
        currRT.cleanup();
        fs.cleanup();
        //grid.cleanup();
        quad.cleanup();
        quad_indexes.cleanup();

        delete [] pixels;

        check_gl_errors("cleanup");
    }

    glfwDestroyWindow(pWindow);

    glfwTerminate();

    return 0;
}

void BufferPresenter::setup(FSQuad *quad, u32 width, u32 height)
{
    m_width = width;
    m_height = height;

    m_quad = quad;

    const char *vertex_src =
        "  #version 330 core                                     \n"
        "  uniform vec2 u_scale;                                 \n"
        "  out vec2 l_textureUV;                                 \n"
        "  void main()                                           \n"
        "  {                                                     \n"
        "      vec2 uv = vec2(gl_VertexID % 2, gl_VertexID / 2); \n"
        "      gl_Position.xy = 2.0f * uv - 1.0f;                \n"
        "      gl_Position.zw = vec2(0.0f, 1.0f);                \n"
        "      l_textureUV.x = u_scale.x * uv.x;                 \n"
        "      l_textureUV.y = u_scale.y * (1.0f - uv.y);        \n"
        "  }                                                     \n";

    const char *fragment_src =
        "  #version 330 core                                 \n"
        "  uniform sampler2D sampler;                        \n"
        "  in vec2 l_textureUV;                              \n"
        "  layout (location = 0) out vec4 o_color;           \n"
        "  void main()                                       \n"
        "  {                                                 \n"
        "      o_color = texture(sampler, l_textureUV);      \n"
        "  }                                                 \n";

    m_fullscreenProgram = ::create_program(vertex_src, nullptr, fragment_src);
    m_scaleLoc = glGetUniformLocation(m_fullscreenProgram, "u_scale");

    
    glGenBuffers(2, m_pbo);
    for (u32 i = 0; i < 2; ++i)
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 4 * m_width * m_height,
                     nullptr, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 static_cast<GLsizei>(m_width),
                 static_cast<GLsizei>(m_height),
                 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_writeIdx = 0;
}

void BufferPresenter::cleanup()
{
    glDeleteTextures(1, &m_tex);
    glDeleteBuffers(2, m_pbo);
    glDeleteProgram(m_fullscreenProgram);
}

void BufferPresenter::draw(const u8* pixels, float portionX, float portionY)
{
    // PBO -dma-> texture --------------------------------------------------

    // tell texture to use pixels from `read` pixel buffer
    // written on previous frame
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[(m_writeIdx + 1) % 2]);

    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    static_cast<GLsizei>(m_width),
                    static_cast<GLsizei>(m_height),
                    GL_BGRA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    // PBO update ----------------------------------------------------------

    // update `write` pixel buffer
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[m_writeIdx]);
    u8 *mappedPixels = static_cast<u8*>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER,
                                                          GL_WRITE_ONLY));
    if (mappedPixels)
    {
        ::memcpy(mappedPixels, pixels, 4 * m_width * m_height);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    m_writeIdx = (m_writeIdx + 1) % 2;
            
    // drawcall ------------------------------------------------------------

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(m_fullscreenProgram);
    glUniform2f(m_scaleLoc, portionX, portionY);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    m_quad->draw();
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
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

void RenderTarget::before()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
    glViewport(0, 0,
               static_cast<GLsizei>(m_width),
               static_cast<GLsizei>(m_height));

}

void RenderTarget::after()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void RenderTarget::cleanup()
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

    const char *fragment_src =
        "  #version 330 core                                 \n"
        "  uniform sampler2D sampler;                        \n"
        "  in vec2 l_textureUV;                              \n"
        "  layout (location = 0) out vec4 o_color;           \n"
        "  void main()                                       \n"
        "  {                                                 \n"
        "      o_color = texture(sampler, l_textureUV);      \n"
        "  }                                                 \n";

    m_fullscreenProgram = ::create_program(vertex_src, nullptr, fragment_src);
    m_preTranslationLoc = glGetUniformLocation(m_fullscreenProgram, "u_preTranslation");
    m_postTranslationLoc = glGetUniformLocation(m_fullscreenProgram, "u_postTranslation");

    m_scaleLoc = glGetUniformLocation(m_fullscreenProgram, "u_scale");
    m_rotationLoc = glGetUniformLocation(m_fullscreenProgram, "u_rotation");

    m_vpSizeLoc = glGetUniformLocation(m_fullscreenProgram, "u_vpSize");
    m_rtSizeLoc = glGetUniformLocation(m_fullscreenProgram, "u_rtSize");
}

void FSTexturePresenter::cleanup()
{
    glDeleteProgram(m_fullscreenProgram);
}

void FSTexturePresenter::draw(GLuint tex, GLenum blendSrc, GLenum blendDst,
                              float x, float y,
                              float x_, float y_,
                              float s, float a,
                              float vpW, float vpH,
                              float rtW, float rtH)
{
    glEnable(GL_BLEND);
    glBlendFunc(blendSrc, blendDst);
    glUseProgram(m_fullscreenProgram);
    glUniform2f(m_preTranslationLoc, x, y);
    glUniform2f(m_postTranslationLoc, x_, y_);
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
        "  #version 330 core                                     \n"
        "  out vec2 l_textureUV;                                 \n"
        "  void main()                                           \n"
        "  {                                                     \n"
        "      vec2 uv = vec2(gl_VertexID % 2, gl_VertexID / 2); \n"
        "      gl_Position.xy = 2.0f * uv - 1.0f;                \n"
        "      gl_Position.zw = vec2(0.0f, 1.0f);                \n"
        "      l_textureUV = uv;                                 \n"
        "  }                                                     \n";

    const char *fragment_src =
        "  #version 330 core                                    \n"
        "  in vec2 l_textureUV;                                 \n"
        "  layout (location = 0) out vec4 o_color;              \n"
        "  uniform vec3 u_bgColor;                              \n"
        "  uniform vec3 u_fgColor;                              \n"
        "  void main()                                          \n"
        "  {                                                    \n"
        "      float isLine = max(                              \n"
        "          step(abs(l_textureUV.x - 0.5f), 0.004f),     \n"
        "          step(abs(l_textureUV.y - 0.5f), 0.004f));    \n"
        "      o_color.rgb = mix(u_bgColor, u_fgColor, isLine); \n"
        "      o_color.a = 1.0f;                                \n"
        "  }                                                    \n";

    m_fullscreenProgram = ::create_program(vertex_src, nullptr, fragment_src);
    m_bgColorLoc = glGetUniformLocation(m_fullscreenProgram, "u_bgColor");
    m_fgColorLoc = glGetUniformLocation(m_fullscreenProgram, "u_fgColor");
}

void FSGrid::draw(const float *bgcolor, const float *fgcolor)
{
    glUseProgram(m_fullscreenProgram);
    glUniform3f(m_bgColorLoc, bgcolor[0], bgcolor[1], bgcolor[2]);
    glUniform3f(m_fgColorLoc, fgcolor[0], fgcolor[1], fgcolor[2]);
    m_quad->draw();
    glUseProgram(0);
}

void FSGrid::cleanup()
{
    glDeleteProgram(m_fullscreenProgram);
}

void MeshPresenter::setup()
{
    const char *vertex_src =
        "  #version 330 core                               \n"
        "  layout (location = 0) in vec3 i_msPosition;     \n"
        "  layout (location = 1) in vec4 i_uvew;           \n"
        "  layout (location = 2) in vec4 i_color;          \n"
        "  uniform vec2 u_scale;                           \n"
        "  out vec4 l_uvew;                                \n"
        "  out vec4 l_color;                               \n"
        "  void main()                                     \n"
        "  {                                               \n"
        "      gl_Position.xy = i_msPosition.xy * u_scale; \n"
        "      gl_Position.z = i_msPosition.z;             \n"
        "      gl_Position.w = 1.0f;                       \n"
        "      l_uvew = i_uvew;                            \n"
        "      l_color = i_color;                          \n"
        "  }                                               \n";

    const char *fragment_src =
        "  #version 330 core                                        \n"
        "  layout (location = 0) out vec4 o_color;                  \n"
        "  layout (location = 1) out vec4 o_color1;                 \n"
        "  in vec4 l_uvew;                                          \n"
        "  in vec4 l_color;                                         \n"
        "  void main()                                              \n"
        "  {                                                        \n"
        "      float rho = length(l_uvew.xy - vec2(0.5f, 0.5f));    \n"
        "      float w = l_uvew.w;                                  \n"
        "      float radius = mix(0.4f, 0.5f, w);                   \n"
        "      float s = mix(1.0f, 0.6f, w);                        \n"
        "      if (rho > radius) discard;                           \n"
        "      float A = l_color.a * s;                             \n"
        "      float E = l_uvew.z * s;                              \n"
        "      float SRCa = (1.0f - A) * E;                         \n"
        "      float SRC1a = (1.0f - A) * (1.0f - E);               \n"
        "      o_color.rgb = l_color.rgb * A;                       \n"
        "      o_color.a = SRCa;                                    \n"
        "      o_color1.a = SRC1a;                                  \n"
        "  }                                                        \n";

    m_program = ::create_program(vertex_src, nullptr, fragment_src);
    m_scaleLoc = glGetUniformLocation(m_program, "u_scale");
}

void MeshPresenter::cleanup()
{
    glDeleteProgram(m_program);
}

void MeshPresenter::draw(kernel_services::MeshID mi, u32 offset, u32 count,
                         float scaleX, float scaleY)
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
    glUniform2f(m_scaleLoc, scaleX, scaleY);

    glBindVertexArray(meshes[mi].vao);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshes[mi].ibuf);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_SHORT,
                   reinterpret_cast<GLvoid*>(offset * meshes[mi].isize));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glBindVertexArray(0);

    glUseProgram(0);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
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

    m_program = ::create_program(vertex_src, nullptr, fragment_src);

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
                   reinterpret_cast<GLvoid*>(offset * meshes[mi].isize));
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
        ptr[0] = idx + 0;
        ptr[1] = idx + 1;
        ptr[2] = idx + 2;
        ptr[3] = idx + 2;
        ptr[4] = idx + 3;
        ptr[5] = idx + 0;
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


