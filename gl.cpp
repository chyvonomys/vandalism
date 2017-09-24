struct initr_item
{
    const char *name;
    bool(*callback)();
};

std::vector<initr_item> g_initrs;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4191) // unsafe reinterpret cast
#endif

#define DECL(t, n)                                        \
bool initr_for_##n();                                     \
t delay_for_##n()                                         \
{                                                         \
	g_initrs.push_back({#n, initr_for_##n});              \
    return nullptr;                                       \
}                                                         \
t n = delay_for_##n();                                    \
bool initr_for_##n()                                      \
{                                                         \
    n = reinterpret_cast<t>(glfwGetProcAddress(#n));      \
    return n != nullptr;                                  \
}                                                         \

#define LOAD(t, n) DECL(PFN##t##PROC, n)

LOAD(GLENABLE, glEnable)
LOAD(GLDISABLE, glDisable)
LOAD(GLBLENDFUNC, glBlendFunc)
LOAD(GLBLENDFUNCSEPARATE, glBlendFuncSeparate)
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
LOAD(GLGETTEXIMAGE, glGetTexImage)
LOAD(GLTEXIMAGE2DMULTISAMPLE, glTexImage2DMultisample)
LOAD(GLTEXSUBIMAGE2D, glTexSubImage2D)
LOAD(GLTEXPARAMETERI, glTexParameteri)

LOAD(GLGENFRAMEBUFFERS, glGenFramebuffers)
LOAD(GLDELETEFRAMEBUFFERS, glDeleteFramebuffers)
LOAD(GLGENRENDERBUFFERS, glGenRenderbuffers)
LOAD(GLDELETERENDERBUFFERS, glDeleteRenderbuffers)
LOAD(GLBINDFRAMEBUFFER, glBindFramebuffer)
LOAD(GLBLITFRAMEBUFFER, glBlitFramebuffer)
LOAD(GLBINDRENDERBUFFER, glBindRenderbuffer)
LOAD(GLCHECKFRAMEBUFFERSTATUS, glCheckFramebufferStatus)
LOAD(GLFRAMEBUFFERTEXTURE, glFramebufferTexture)
LOAD(GLRENDERBUFFERSTORAGE, glRenderbufferStorage)
LOAD(GLRENDERBUFFERSTORAGEMULTISAMPLE, glRenderbufferStorageMultisample)
LOAD(GLFRAMEBUFFERRENDERBUFFER, glFramebufferRenderbuffer)

LOAD(GLDEPTHFUNC, glDepthFunc)

LOAD(GLGETUNIFORMLOCATION, glGetUniformLocation)
LOAD(GLUNIFORM1F, glUniform1f)
LOAD(GLUNIFORM2F, glUniform2f)
LOAD(GLUNIFORM3F, glUniform3f)
LOAD(GLUNIFORM4F, glUniform4f)

LOAD(GLFINISH, glFinish)
LOAD(GLGETERROR, glGetError)
LOAD(GLGETSTRING, glGetString)

#ifdef _MSC_VER
#pragma warning(pop) // unsafe reinterpret cast
#endif

bool load_gl_functions(bool printDiag)
{
    bool result = true;
    for (auto &x : g_initrs)
    {
        bool ok = (*x.callback)();
        if (printDiag)
        {
            ::printf("%s -> %s\n", x.name, (ok ? "OK" : "FAIL"));
        }
        result = result && ok;
    }
    return result;
}

template <typename I>
GLvoid* make_gl_offset(I offs)
{
    return reinterpret_cast<GLvoid*>(static_cast<size_t>(offs));
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

GLuint create_program(const char *vert_src, const char *geom_src, const char *frag_src, bool printShaders)
{
    const size_t INFOLOG_SIZE = 1024;
    static GLchar infolog[INFOLOG_SIZE];

    if (printShaders)
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
