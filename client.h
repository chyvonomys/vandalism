#include <cstddef>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

typedef int64_t i64;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;

struct offscreen_buffer
{
    u8 *data;
    u32 width;
    u32 height;
};

struct VertexLayout;

struct kernel_services
{
    typedef u32 TexID;

    typedef TexID (*CREATE_TEXTURE)(u32, u32, u32);
    typedef void (*UPDATE_TEXTURE)(TexID, const u8*);
    typedef void (*DELETE_TEXTURE)(TexID);

    CREATE_TEXTURE create_texture;
    UPDATE_TEXTURE update_texture;
    DELETE_TEXTURE delete_texture;

    const VertexLayout *ui_vertex_layout;
    const VertexLayout *stroke_vertex_layout;

    typedef u32 MeshID;

    typedef MeshID (*CREATE_MESH)(const VertexLayout&, u32, u32);
    typedef void (*UPDATE_MESH)(MeshID, const void*, u32, const u16*, u32);
    typedef void (*DELETE_MESH)(MeshID);

    CREATE_MESH create_mesh;
    UPDATE_MESH update_mesh;
    DELETE_MESH delete_mesh;
};

struct input_data
{
    double *pTimeIntervals;
    u32 nTimeIntervals;

    u32 nFrames;

    i32 swMouseXPx;
    i32 swMouseYPx;
    bool mouseleft;

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
    offscreen_buffer *buffer;

    // TODO: should be somehow generated from layout
    struct Vertex
    {
        float x, y, z;
        float u, v, e;
        float r, g, b, a;
    };

    bool bake_flag;
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

    struct drawcall
    {
        kernel_services::TexID texture_id;
        kernel_services::MeshID mesh_id;
        u32 offset;
        u32 count;
    };

    drawcall *ui_drawcalls;
    u32 ui_drawcall_cnt;
};

typedef void (*UPDATE_AND_RENDER_FUNC)(input_data *input, output_data *output);
typedef void (*SETUP_FUNC)(kernel_services *services);
typedef void (*CLEANUP_FUNC)();
