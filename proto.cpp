#define GLFW_INCLUDE_NONE

#include <GLFW/glfw3.h>
#include <objc/objc-runtime.h>
namespace GLFWNative
{
#define GLFW_EXPOSE_NATIVE_COCOA
#define GLFW_EXPOSE_NATIVE_NSGL
#include <GLFW/glfw3native.h>
}
#include "glcorearb.h"
#include <cstdio>
#include <cstring>
#include <mach/mach_time.h>
#include <dlfcn.h>

#include "client.h"
#include "math.h"

const size_t GL_FUNCTIONS_CAPACITY = 100;
const char *g_names[GL_FUNCTIONS_CAPACITY];

typedef bool (*gl_func_initr)();
gl_func_initr g_initrs[GL_FUNCTIONS_CAPACITY];
size_t g_size = 0;

#define ASSERT(cond) if (!(cond)) *(volatile int *)0 = 1;

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
    n = (t)glfwGetProcAddress(#n);                                  \
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
LOAD(GLUNIFORM2F, glUniform2f)

LOAD(GLFINISH, glFinish)
LOAD(GLGETERROR, glGetError)
LOAD(GLGETSTRING, glGetString)

bool g_printShaders = false;
bool g_printGLDiagnostics = false;
size_t g_quitFrame = -1;

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

        GLsizei length = ::strlen(shader_sources[i]);
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


struct BufferPresenter
{
    void setup(uint32 width, uint32 height);
    void draw(const uint8 *pixels);
    void cleanup();

    GLuint m_pbo[2];
    GLuint m_tex;
    GLuint m_vbo;
    GLuint m_vao;
    GLuint m_fullscreenProgram;

    uint32 m_width;
    uint32 m_height;

    uint32 m_writeIdx;
};

struct FSTexturePresenter
{
    void setup();
    void set_translation(GLfloat x, GLfloat y);
    void draw(GLuint tex);
    void cleanup();

    GLuint m_vbo;
    GLuint m_vao;
    GLuint m_fullscreenProgram;
    GLint m_translationLoc;
    GLfloat m_translationX;
    GLfloat m_translationY;
};

struct RenderTarget
{
    void setup(uint32 width, uint32 height);
    void cleanup();

    void before();
    void after();

    GLuint m_fbo;
    GLuint m_depth_rbo;
    GLuint m_tex;

    uint32 m_width;
    uint32 m_height;
};

struct Mesh
{
    void setup();
    void cleanup();

    void update(const float *xy, uint32 vtxCnt);

    GLuint m_vao;
    GLuint m_vbo;

    uint32 m_vtxCnt;
};

struct MeshPresenter
{
    void setup();
    void cleanup();

    void draw(GLuint vao, uint32 vtxCnt);

    GLuint m_program;
};

int main(int argc, char *argv[])
{
    for (size_t i = 1; i < argc; ++i)
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
    }

    glfwInit();

    glfwWindowHint(GLFW_RESIZABLE, 0);
    glfwWindowHint(GLFW_DECORATED, 1);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    int32 windowWidthPt = 64 + 1024 + 64;
    int32 windowHeightPt = 64 + 512 + 64;

    int32 viewportWidthPt = 1024;
    int32 viewportHeightPt = 512;

    GLsizei bufferWidthPx = 1024;
    GLsizei bufferHeightPx = 512;

    GLFWwindow* pWindow;
    GLFWmonitor* pMonitor = nullptr; //glfwGetPrimaryMonitor();
    pWindow = glfwCreateWindow(windowWidthPt, windowHeightPt,"proto", pMonitor, nullptr);

    // TODO: this doesn't seem to work, no matter what is set, frame takes ~16ms
    glfwMakeContextCurrent(pWindow);
    /*
    CGLGetCurrentContext();
    CGLSetParameter();
    */
#if false
    id nsgl = (id)GLFWNative::glfwGetNSGLContext(pWindow);
    int swapInterval = 1;
    const int NSOpenGLCPSwapInterval = 222;
    objc_msgSend(nsgl, sel_registerName("setValues:forParameter:"),
                 &swapInterval, NSOpenGLCPSwapInterval);
#else
    glfwSwapInterval(1);
#endif
    
    int32 windowWidthPx, windowHeightPx;
    glfwGetFramebufferSize(pWindow, &windowWidthPx, &windowHeightPx);

    int32 viewportWidthPx = (windowWidthPx * viewportWidthPt) / windowWidthPt;
    int32 viewportHeightPx = (windowHeightPx * viewportHeightPt) / windowHeightPt;

    // viewport is centered in window
    int32 viewportLeftPx = (windowWidthPx - viewportWidthPx) / 2;
    int32 viewportBottomPx = (windowHeightPx - viewportHeightPx) / 2;


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

        FSTexturePresenter fs;
        fs.setup();

        RenderTarget rt;
        rt.setup(viewportWidthPx, viewportHeightPx);
        
        BufferPresenter blit;
        blit.setup(bufferWidthPx, bufferHeightPx);
        check_gl_errors("after setup");

        Mesh bgmesh, fgmesh;
        bgmesh.setup();
        fgmesh.setup();

        GLubyte *pixels = new GLubyte[bufferWidthPx * bufferHeightPx * 4];

        offscreen_buffer buffer;
        buffer.data = pixels;
        buffer.width = bufferWidthPx;
        buffer.height = bufferHeightPx;

        const uint32 VERTS_PER_TRI = 3;
        const uint32 VERT_DIM = 3; // xy and zindex

        const uint32 BAKE_TRIS_CNT = 5000;
        const uint32 CURR_TRIS_CNT = 500;

        GLfloat *bake_xys = new GLfloat[BAKE_TRIS_CNT * VERTS_PER_TRI * VERT_DIM];
        GLfloat *curr_xys = new GLfloat[CURR_TRIS_CNT * VERTS_PER_TRI * VERT_DIM];

        triangles bake_tris;
        bake_tris.data = bake_xys;
        bake_tris.size = 0;
        bake_tris.capacity = BAKE_TRIS_CNT * VERTS_PER_TRI;

        triangles curr_tris;
        curr_tris.data = curr_xys;
        curr_tris.size = 0;
        curr_tris.capacity = CURR_TRIS_CNT * VERTS_PER_TRI;

        MeshPresenter render;
        render.setup();
        
        input_data input;
        input.nFrames = 0;
        output_data output;
        output.bake_tris = &bake_tris;
        output.curr_tris = &curr_tris;
        output.buffer = &buffer;

        void *lib_handle = ::dlopen("client.dylib", RTLD_LAZY);

        UPDATE_AND_RENDER_FUNC upd_and_rnd = (UPDATE_AND_RENDER_FUNC)
            ::dlsym(lib_handle, "update_and_render");

        SETUP_FUNC setup = (SETUP_FUNC)::dlsym(lib_handle, "setup");
        CLEANUP_FUNC cleanup = (CLEANUP_FUNC)::dlsym(lib_handle, "cleanup");
        
        mach_timebase_info_data_t time_info;
        mach_timebase_info(&time_info);

        uint64_t frame_counter = 0;


        const uint32 TIMEPOINTS = 10;
        const uint32 INTERVALS = TIMEPOINTS-1;
        uint64_t timestamps[TIMEPOINTS] = {0};

        double intervals[INTERVALS];

#define TIME timestamps[nTimePoints++] = mach_absolute_time();

        uint32 nTimePoints = 0;

        (*setup)();

        bool reload_client = false;
        bool f9_was_up = true;

        for (;;)
        {
            if (reload_client)
            {
                (*cleanup)();
                ::dlclose(lib_handle);
                lib_handle = ::dlopen("client.dylib", RTLD_LAZY);

                upd_and_rnd = (UPDATE_AND_RENDER_FUNC)
                    ::dlsym(lib_handle, "update_and_render");

                setup = (SETUP_FUNC)::dlsym(lib_handle, "setup");
                cleanup = (CLEANUP_FUNC)::dlsym(lib_handle, "cleanup");

                (*setup)();
                reload_client = false;
                input.nFrames = 0;
            }
            
            for (uint8 i = 1; i < nTimePoints; ++i)
            {
                intervals[i-1] = 1e-6 *
                    static_cast<double>(timestamps[i] - timestamps[i-1]) *
                    time_info.numer / time_info.denom;
            }
            input.pTimeIntervals = intervals;
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

            // viewport is centered in window
            double viewportLeftPt = 0.5 * (windowWidthPt - viewportWidthPt);
            double viewportBottomPt = 0.5 * (windowHeightPt - viewportHeightPt);
            double viewportRightPt = viewportLeftPt + viewportWidthPt;
            double viewportTopPt = viewportBottomPt + viewportHeightPt;

            double mousePtX, mousePtY;
            glfwGetCursorPos(pWindow, &mousePtX, &mousePtY);

            // clamp to viewport Pt coords
            mousePtX = si_clampd(mousePtX, viewportLeftPt, viewportRightPt);
            mousePtY = si_clampd(mousePtY, viewportBottomPt, viewportTopPt);

            // to 0..widthPt/heightPt
            mousePtX = mousePtX - viewportLeftPt;
            mousePtY = mousePtY - viewportBottomPt;

            // transform to buffer Px coords
            double mousePxX = mousePtX * bufferWidthPx / viewportWidthPt;
            double mousePxY = mousePtY * bufferHeightPx / viewportHeightPt;

            input.mousex = mousePxX;
            input.mousey = mousePxY;

            if (input.mousex == bufferWidthPx)
                --input.mousex;
            if (input.mousey == bufferHeightPx)
                --input.mousey;

            // invert vertically
            input.mousey = bufferHeightPx - 1 - input.mousey;

            if (glfwWindowShouldClose(pWindow))
            {
                break;
            }

            TIME; // update ---------------------------------------------------------

            (*upd_and_rnd)(&input, &output);

            glViewport(viewportLeftPx, viewportBottomPx,
                       viewportWidthPx, viewportHeightPx);
            blit.draw(pixels);

            if (output.bake_flag)
            {
                bgmesh.update(output.bake_tris->data, output.bake_tris->size);
                // flag processed
                // TODO: ??
                output.bake_flag = false;
                rt.before();
                render.draw(bgmesh.m_vao, bgmesh.m_vtxCnt);
                rt.after();
            }

            glViewport(viewportLeftPx, viewportBottomPx,
                       viewportWidthPx, viewportHeightPx);

            fs.set_translation(output.translateX, output.translateY);
            fs.draw(rt.m_tex);

            fgmesh.update(output.curr_tris->data, output.curr_tris->size);
            render.draw(fgmesh.m_vao, fgmesh.m_vtxCnt);

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

        render.cleanup();
        bgmesh.cleanup();
        fgmesh.cleanup();
        blit.cleanup();
        rt.cleanup();
        fs.cleanup();

        delete [] bake_xys;
        delete [] curr_xys;
        delete [] pixels;

        ::dlclose(lib_handle);

        check_gl_errors("cleanup");
    }

    glfwDestroyWindow(pWindow);

    glfwTerminate();

    return 0;
}


void BufferPresenter::setup(uint32 width, uint32 height)
{
    m_width = width;
    m_height = height;
    
    const float fullscreenQuadVertices[4*2*2] =
    {//   X      Y       U     V
        -1.0f, -1.0f,   0.0f, 0.0f,
        -1.0f, +1.0f,   0.0f, 1.0f,
        +1.0f, +1.0f,   1.0f, 1.0f,
        +1.0f, -1.0f,   1.0f, 0.0f
    };

    const GLuint POS_LOC = 0;
    const GLuint UV_LOC = 1;

    const GLint POS_DIM = 2;
    const GLint UV_DIM = 2;

    const GLsizei vertexSize = (POS_DIM + UV_DIM) * sizeof(float);

    const char *vertex_src =
        "  #version 330 core                             \n"
        "  layout (location = 0) in vec2 i_msPosition;   \n"
        "  layout (location = 1) in vec2 i_textureUV;    \n"
        "  out vec2 l_textureUV;                         \n"
        "  void main()                                   \n"
        "  {                                             \n"
        "      gl_Position.xy = i_msPosition;            \n"
        "      gl_Position.z = 0.0f;                     \n"
        "      gl_Position.w = 1.0f;                     \n"
        "      l_textureUV = i_textureUV;                \n"
        "  }                                             \n";

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

    
    glGenBuffers(2, m_pbo);
    for (uint32 i = 0; i < 2; ++i)
    {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[i]);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 4 * m_width * m_height,
                     nullptr, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    glGenVertexArrays(1, &m_vao);

    // vao setup
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, 4 * vertexSize,
                 fullscreenQuadVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glVertexAttribPointer(POS_LOC, POS_DIM, GL_FLOAT, GL_FALSE, vertexSize, 0);
    glVertexAttribPointer(UV_LOC, UV_DIM, GL_FLOAT, GL_FALSE, vertexSize,
                          reinterpret_cast<GLvoid*>(POS_DIM * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(POS_LOC);
    glEnableVertexAttribArray(UV_LOC);

    glBindVertexArray(0);
    // end vao setup


    glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_writeIdx = 0;
}

void BufferPresenter::cleanup()
{
    glDeleteTextures(1, &m_tex);
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteBuffers(2, m_pbo);
    glDeleteProgram(m_fullscreenProgram);
}

void BufferPresenter::draw(const uint8* pixels)
{
    // PBO -dma-> texture --------------------------------------------------

    // tell texture to use pixels from `read` pixel buffer
    // written on previous frame
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[(m_writeIdx + 1) % 2]);

    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height,
                    GL_BGRA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    // PBO update ----------------------------------------------------------

    // update `write` pixel buffer
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[m_writeIdx]);
    uint8 *mappedPixels = static_cast<uint8*>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER,
                                                          GL_WRITE_ONLY));
    if (mappedPixels)
    {
        ::memcpy(mappedPixels, pixels, 4 * m_width * m_height);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    m_writeIdx = (m_writeIdx + 1) % 2;
            
    // drawcall ------------------------------------------------------------

    glClearColor(0.3, 0.3, 0.3, 1.0);

    GLbitfield clear_bits = (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClear(clear_bits);

    glUseProgram(m_fullscreenProgram);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

/*
void BufferPresenter::draw(const uint8* pixels)
{
    // PBO update ----------------------------------------------------------
    // update pixel buffer
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[0]);
    uint8 *mappedPixels = static_cast<uint8*>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER,
                                                          GL_WRITE_ONLY));
    if (mappedPixels)
    {
        ::memcpy(mappedPixels, pixels, 4 * m_width * m_height);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    // PBO -dma-> texture --------------------------------------------------

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[0]);

    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height,
                    GL_BGRA, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
            
    // drawcall ------------------------------------------------------------

    glClearColor(0.3, 0.3, 0.3, 1.0);

    GLbitfield clear_bits = (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClear(clear_bits);

    glUseProgram(m_fullscreenProgram);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}
*/

void RenderTarget::setup(uint32 width, uint32 height)
{
    m_width = width;
    m_height = height;

    glGenTextures(1, &m_tex);

    glBindTexture(GL_TEXTURE_2D, m_tex);
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenRenderbuffers(1, &m_depth_rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depth_rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_width, m_height);
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
    glViewport(0, 0, m_width, m_height);

    glClearColor(1.0, 1.0, 1.0, 0.0);
    glClearDepth(-1.0);

    GLbitfield clear_bits = (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glClear(clear_bits);
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


void FSTexturePresenter::setup()
{
    const float fullscreenQuadVertices[4*2*2] =
    {//   X      Y       U     V
        -1.0f, -1.0f,   0.0f, 0.0f,
        -1.0f, +1.0f,   0.0f, 1.0f,
        +1.0f, +1.0f,   1.0f, 1.0f,
        +1.0f, -1.0f,   1.0f, 0.0f
    };

    const GLuint POS_LOC = 0;
    const GLuint UV_LOC = 1;

    const GLint POS_DIM = 2;
    const GLint UV_DIM = 2;

    const GLsizei vertexSize = (POS_DIM + UV_DIM) * sizeof(float);

    const char *vertex_src =
        "  #version 330 core                                  \n"
        "  layout (location = 0) in vec2 i_msPosition;        \n"
        "  layout (location = 1) in vec2 i_textureUV;         \n"
        "  out vec2 l_textureUV;                              \n"
        "  uniform vec2 u_translation;                        \n"
        "  void main()                                        \n"
        "  {                                                  \n"
        "      gl_Position.xy = i_msPosition + u_translation; \n"
        "      gl_Position.z = 0.0f;                          \n"
        "      gl_Position.w = 1.0f;                          \n"
        "      l_textureUV = i_textureUV;                     \n"
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
    m_translationLoc = glGetUniformLocation(m_fullscreenProgram, "u_translation");

    glGenVertexArrays(1, &m_vao);

    // vao setup
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, 4 * vertexSize,
                 fullscreenQuadVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glVertexAttribPointer(POS_LOC, POS_DIM, GL_FLOAT, GL_FALSE, vertexSize, 0);
    glVertexAttribPointer(UV_LOC, UV_DIM, GL_FLOAT, GL_FALSE, vertexSize,
                          reinterpret_cast<GLvoid*>(POS_DIM * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(POS_LOC);
    glEnableVertexAttribArray(UV_LOC);

    glBindVertexArray(0);
    // end vao setup
}

void FSTexturePresenter::cleanup()
{
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteProgram(m_fullscreenProgram);
}

void FSTexturePresenter::set_translation(GLfloat x, GLfloat y)
{
    m_translationX = x;
    m_translationY = y;
}

void FSTexturePresenter::draw(GLuint tex)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(m_fullscreenProgram);
    glUniform2f(m_translationLoc, m_translationX, m_translationY);
    glBindTexture(GL_TEXTURE_2D, tex);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}


void Mesh::setup()
{
    const GLuint POS_LOC = 0;

    const GLuint POS_DIM = 3;

    const GLsizei vertexSize = POS_DIM * sizeof(float);

    glGenVertexArrays(1, &m_vao);

    // vao setup
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, 1500 * vertexSize,
                 0, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glVertexAttribPointer(POS_LOC, POS_DIM, GL_FLOAT, GL_FALSE, vertexSize, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(POS_LOC);

    glBindVertexArray(0);
    // end vao setup
}

void Mesh::cleanup()
{
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
}

void Mesh::update(const float *xyz, uint32 vtxCnt)
{
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vtxCnt * 3 * sizeof(float),
                 xyz, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_vtxCnt = vtxCnt;
}


void MeshPresenter::setup()
{
    const char *vertex_src =
        "  #version 330 core                             \n"
        "  layout (location = 0) in vec3 i_msPosition;   \n"
        "  void main()                                   \n"
        "  {                                             \n"
        "      gl_Position.xy = i_msPosition.xy;         \n"
        "      gl_Position.z = i_msPosition.z;           \n"
        "      gl_Position.w = 1.0f;                     \n"
        "  }                                             \n";

    const char *fragment_src =
        "  #version 330 core                                 \n"
        "  layout (location = 0) out vec4 o_color;           \n"
        "  void main()                                       \n"
        "  {                                                 \n"
        "      o_color = vec4(1.0, 1.0, 1.0, 0.5);           \n"
        "  }                                                 \n";

    m_program = ::create_program(vertex_src, nullptr, fragment_src);
}

void MeshPresenter::cleanup()
{
    glDeleteProgram(m_program);
}

void MeshPresenter::draw(GLuint vao, uint32 vtxCnt)
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_GREATER);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(m_program);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vtxCnt);
    glBindVertexArray(0);
    glUseProgram(0);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
}
