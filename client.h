#include <cstdint>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

struct VertexLayout;

struct kernel_services
{
    typedef u32 TexID;

    TexID create_texture(u32, u32, u32);
    void update_texture(TexID, const u8*);
    void delete_texture(TexID);

    const VertexLayout *ui_vertex_layout;
    const VertexLayout *stroke_vertex_layout;

    typedef u32 MeshID;

    MeshID create_mesh(const VertexLayout&, u32, u32);
    MeshID create_quad_mesh(const VertexLayout&, u32);
    void update_mesh(MeshID, const void*, u32, const u16*, u32);
    void update_mesh_vb(MeshID, const void*, u32);
    void delete_mesh(MeshID);

    bool check_file(const char* path);
};

struct input_data
{
    double *pTimeIntervals;
    u32 nTimeIntervals;

    u32 nFrames;

    float rawMouseXPt, rawMouseYPt;
    float vpMouseXPt, vpMouseYPt;
    bool mouseleft;
    bool scrolling;
    float scrollY;

    i32 windowWidthPx, windowHeightPx;
    i32 windowWidthPt, windowHeightPt;
    i32 windowPosXPt, windowPosYPt;

    i32 vpWidthPx, vpHeightPx;
    i32 vpWidthPt, vpHeightPt;

	float vpWidthIn, vpHeightIn;
    float rtWidthIn, rtHeightIn;

    i32 swWidthPx, swHeightPx;

	kernel_services services;
};

struct output_data
{
    // TODO: should be somehow generated from layout
    struct Vertex
    {
        float x, y, z;
        float u, v, e;
        float r, g, b, a;
    };

    bool bake_flag;
    bool change_flag;
    bool quit_flag;

    kernel_services::MeshID fgmesh;
    kernel_services::MeshID bgmesh;
    u32 fgmeshCnt;
    u32 bgmeshCnt;

    // baked texture quad manipulations
    float preTranslateX;
    float preTranslateY;
    float postTranslateX;
    float postTranslateY;
    float scale;
    float rotate;

    float bg_red;
    float bg_green;
    float bg_blue;

    float grid_bg_color[3];
    float grid_fg_color[3];
    float grid_translation[2];
    float grid_zoom;

    float zbandwidth;

    enum techid
    {
        UI,
        IMAGE,
        STROKES
    };

    struct drawcall
    {
        techid id;
        kernel_services::TexID texture_id;
        kernel_services::MeshID mesh_id;
        u32 offset;
        u32 count;
    };

    drawcall *drawcalls;
    u32 drawcall_cnt;
};

void update_and_render(input_data *input, output_data *output);
void setup(kernel_services *services);
void cleanup();
